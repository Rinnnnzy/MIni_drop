#!/usr/bin/env python3
"""
hotmethod_analyzer.py — main entry point for the analysis service.

Invoked by apiserver after the agent uploads perf.data to MinIO:
    python3 hotmethod_analyzer.py --task-id <tid> [--task-type <int>]

Required environment variables: MINIO_*, PG_DSN.
Optional:  LLM_API_KEY, LLM_BASE_URL, LLM_MODEL (enable AI suggestions).
"""

import argparse
import logging
import os
import sys

# Ensure sibling modules (storage, error, …) are importable regardless of cwd.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import psycopg2

from storage import StorageClient
from error import AnalysisError
from data_parser.collapsed_data_parser import parse as run_perf_pipeline
from hotmethod_common import top_n
from analysis_advisor import run_analysis

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s [%(name)s] %(message)s",
)
log = logging.getLogger("hotmethod_analyzer")

_STATUS_RUNNING = 1
_STATUS_DONE    = 2
_STATUS_FAILED  = 3


def _set_analysis_status(tid: str, status: int):
    dsn = os.environ.get("PG_DSN", "")
    if not dsn:
        return
    try:
        conn = psycopg2.connect(dsn)
        with conn:
            with conn.cursor() as cur:
                cur.execute(
                    "UPDATE hotmethod_task SET analysis_status = %s WHERE tid = %s",
                    (status, tid),
                )
        conn.close()
    except Exception as e:
        log.error(f"failed to update analysis_status for {tid}: {e}")


def main():
    parser = argparse.ArgumentParser(description="Mini-Drop hotmethod analyzer")
    parser.add_argument("--task-id",   required=True,  help="task TID")
    parser.add_argument("--task-type", type=int, default=0, help="task type (unused currently)")
    args = parser.parse_args()

    tid = args.task_id
    log.info(f"analysis started  tid={tid}  task_type={args.task_type}")
    _set_analysis_status(tid, _STATUS_RUNNING)

    work_dir = f"/tmp/drop_{tid}_analysis"
    os.makedirs(work_dir, exist_ok=True)

    try:
        storage = StorageClient()

        # 1. Download perf.data from MinIO
        cos_key    = f"{tid}/perf.data"
        local_perf = os.path.join(work_dir, "perf.data")
        log.info(f"downloading {cos_key} → {local_perf}")
        storage.download(cos_key, local_perf)

        # 2. perf script → collapsed stacks + flamegraph SVG
        log.info("running perf data pipeline")
        collapsed_path, svg_path = run_perf_pipeline(local_perf, work_dir)

        # 3. Upload flamegraph SVG back to MinIO
        svg_cos_key = f"{tid}/flamegraph.svg"
        log.info(f"uploading flamegraph → {svg_cos_key}")
        storage.upload(svg_path, svg_cos_key)

        # 4. Parse collapsed stacks → Top-10 functions
        with open(collapsed_path, "r", errors="replace") as f:
            collapsed_text = f.read()
        top_funcs = top_n(collapsed_text, n=10)
        log.info(f"top functions: {[s.name for s in top_funcs]}")

        # 5. Rule engine + optional LLM → write to PostgreSQL
        enable_llm = bool(os.environ.get("LLM_API_KEY", "").strip())
        run_analysis(tid, top_funcs, enable_llm=enable_llm)

        _set_analysis_status(tid, _STATUS_DONE)
        log.info(f"analysis complete  tid={tid}")

    except AnalysisError as e:
        log.error(f"analysis error [{e.code}]: {e.msg}")
        _set_analysis_status(tid, _STATUS_FAILED)
        sys.exit(1)
    except Exception as e:
        log.exception(f"unexpected error: {e}")
        _set_analysis_status(tid, _STATUS_FAILED)
        sys.exit(1)


if __name__ == "__main__":
    main()
