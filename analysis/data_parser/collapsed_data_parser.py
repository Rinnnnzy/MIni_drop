import os
import subprocess
from pathlib import Path
from typing import Tuple

_TOOLS_DIR = os.path.join(os.path.dirname(__file__), "..", "tools")
_COLLAPSE_SCRIPT = os.path.join(_TOOLS_DIR, "stackcollapse-perf.pl")
_FLAMEGRAPH_SCRIPT = os.path.join(_TOOLS_DIR, "flamegraph.pl")


def parse(perf_data_path: str, work_dir: str) -> Tuple[str, str]:
    """
    Run the perf data pipeline:
        perf script → stackcollapse-perf.pl → flamegraph.pl

    Returns (collapsed_path, flamegraph_svg_path).
    Raises RuntimeError on any step failure.
    """
    perf_data_path = str(Path(perf_data_path).resolve())
    wd = Path(work_dir)

    # Step 1: perf script → raw trace text
    raw_txt = str(wd / "perf.txt")
    _run_to_file(["perf", "script", "-i", perf_data_path], raw_txt)

    # Step 2: stackcollapse-perf.pl → folded stacks
    collapsed = str(wd / "perf.folded")
    _pipe_perl(_COLLAPSE_SCRIPT, raw_txt, collapsed)

    # Step 3: flamegraph.pl → SVG
    svg = str(wd / "flamegraph.svg")
    _pipe_perl(_FLAMEGRAPH_SCRIPT, collapsed, svg)

    return collapsed, svg


def _run_to_file(cmd: list, out_path: str):
    with open(out_path, "w") as fout:
        result = subprocess.run(cmd, stdout=fout, stderr=subprocess.PIPE)
    if result.returncode != 0:
        raise RuntimeError(
            f"{cmd[0]} exited {result.returncode}: {result.stderr.decode(errors='replace')}"
        )


def _pipe_perl(script: str, in_path: str, out_path: str):
    with open(in_path, "r", errors="replace") as fin, open(out_path, "w") as fout:
        result = subprocess.run(
            ["perl", script], stdin=fin, stdout=fout, stderr=subprocess.PIPE
        )
    if result.returncode != 0:
        raise RuntimeError(
            f"perl {os.path.basename(script)} exited {result.returncode}: "
            f"{result.stderr.decode(errors='replace')}"
        )
