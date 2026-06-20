#!/usr/bin/env python3
"""
poll_worker.py — Docker 部署模式下的 analysis 触发器。

问题背景：
  apiserver (Go/Alpine 容器) 无法直接 exec Python 脚本（不同容器）。
  本 worker 替代 apiserver 的 triggerAnalysis() 逻辑：
  每 5 秒轮询 DB，找到"采集完成但未分析"的任务，调用 hotmethod_analyzer.py。

轮询条件：
  status = 3 (collection done) AND analysis_status IN (0, 3)
  0 = pending (apiserver 写完 cos_key 后设置)
  3 = failed  (apiserver 的 triggerAnalysis goroutine exec 失败后设置)
  二者都是"需要分析"的状态。
"""

import logging
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from dotenv import load_dotenv
load_dotenv(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env"))

import psycopg2

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s [poll_worker] %(message)s",
)
log = logging.getLogger("poll_worker")

POLL_INTERVAL = 5  # seconds between each DB poll

# analysis_status constants — must match apiserver/server/const_enum.go
_A_PENDING = 0
_A_RUNNING = 1
_A_DONE    = 2
_A_FAILED  = 3

# hotmethod_task.status: 3 = collection complete, perf.data uploaded
# (0=pending 1=running 2=uploading 3=done 4=failed — apiserver/server/const_enum.go)
_TASK_DONE = 3


def _get_pending(conn):
    """Return list of (tid, task_type) that need analysis."""
    with conn.cursor() as cur:
        cur.execute(
            """
            SELECT tid, type FROM hotmethod_task
            WHERE  status          = %s
              AND  analysis_status IN (%s, %s)
              AND  deleted_at      IS NULL
            ORDER  BY create_time ASC
            LIMIT  5
            """,
            (_TASK_DONE, _A_PENDING, _A_FAILED),
        )
        return cur.fetchall()


def _set_status(conn, tid: str, status: int):
    with conn.cursor() as cur:
        cur.execute(
            "UPDATE hotmethod_task SET analysis_status = %s WHERE tid = %s",
            (status, tid),
        )
    conn.commit()


def _run_analyzer(tid: str, task_type: int) -> bool:
    script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "hotmethod_analyzer.py")
    cmd = [sys.executable, script, "--task-id", tid, "--task-type", str(task_type)]
    log.info("launching analyzer  tid=%s  type=%d", tid, task_type)
    result = subprocess.run(cmd, env=os.environ.copy())
    if result.returncode != 0:
        log.error("analyzer failed  tid=%s  exit=%d", tid, result.returncode)
        return False
    log.info("analyzer finished  tid=%s", tid)
    return True


def main():
    dsn = os.environ.get("PG_DSN", "")
    if not dsn:
        log.error("PG_DSN not set — exiting")
        sys.exit(1)

    log.info("poll_worker started  interval=%ds", POLL_INTERVAL)

    while True:
        try:
            with psycopg2.connect(dsn) as conn:
                rows = _get_pending(conn)

            for tid, task_type in rows:
                # Open a fresh connection per task so a crash doesn't roll back others.
                try:
                    with psycopg2.connect(dsn) as conn:
                        _set_status(conn, tid, _A_RUNNING)

                    success = _run_analyzer(tid, task_type)

                    # hotmethod_analyzer.py writes DONE/FAILED itself.
                    # If the subprocess crashed before writing, mark FAILED here.
                    if not success:
                        with psycopg2.connect(dsn) as conn:
                            _set_status(conn, tid, _A_FAILED)

                except Exception as e:
                    log.exception("error processing tid=%s: %s", tid, e)

        except psycopg2.OperationalError as e:
            log.warning("db connection error (will retry): %s", e)
        except Exception as e:
            log.exception("unexpected error in poll loop: %s", e)

        time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    main()
