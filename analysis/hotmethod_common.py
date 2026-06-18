from dataclasses import dataclass
from typing import List


@dataclass
class FuncStat:
    name: str
    count: int  # inclusive sample count (times the function appears across all stacks)


def parse_collapsed(text: str) -> List[FuncStat]:
    """Parse a collapsed-stack file and return functions sorted by inclusive count."""
    counts: dict = {}
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        parts = line.rsplit(" ", 1)
        if len(parts) != 2:
            continue
        stack, n_str = parts
        try:
            n = int(n_str)
        except ValueError:
            continue
        for func in stack.split(";"):
            func = func.strip()
            if func:
                counts[func] = counts.get(func, 0) + n

    result = [FuncStat(name=k, count=v) for k, v in counts.items()]
    result.sort(key=lambda x: x.count, reverse=True)
    return result


def top_n(text: str, n: int = 10) -> List[FuncStat]:
    return parse_collapsed(text)[:n]
