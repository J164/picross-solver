import argparse
import json
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

BENCH_DIR = Path(__file__).resolve().parent
REPO_DIR = BENCH_DIR.parent
sys.path.insert(0, str(BENCH_DIR))
from src.trials import driver_flags

GRID_ROW = re.compile(r"^\s+[.#?]+$")

ALL = {"freedom": True, "bitboard": True, "pixel_branching": True, "incremental_propagation": True}
CONFIGS = {
    "baseline": {},
    "dp": {"dp_line_solver": True},
    "freedom": {"freedom": True},
    "bitboard": {"bitboard": True},
    "fp1": {"fp1": True},
    "fp2": {"fp2": True},
    "pixel_branching": {"pixel_branching": True},
    "incremental": {"incremental_propagation": True},
    "dp+bitboard": {"dp_line_solver": True, "bitboard": True},
    "freedom+bitboard": {"freedom": True, "bitboard": True},
    "all_fp1": {**ALL, "fp1": True},
    "all_fp2": {**ALL, "fp2": True},
}

def parse(output: str) -> dict:
    grids: dict[str, dict] = {}
    current = None
    rows: list[str] = []

    def flush() -> None:
        if current is not None and rows:
            grids[current]["solutions"].append("\n".join(rows))
        rows.clear()

    for line in output.splitlines():
        if line.startswith("GRID "):
            flush()
            current = line.strip()
            grids[current] = {"solutions": [], "count": None, "messages": [], "aborted": False}
        elif current is None:
            if line.strip():
                grids.setdefault("<file>", {"solutions": [], "count": None, "messages": [], "aborted": False})["messages"].append(line.strip())
        elif "Solution nb" in line:
            flush()
        elif GRID_ROW.match(line):
            rows.append(line.strip())
        else:
            flush()
            stripped = line.strip()
            if stripped == "Solver aborted":
                grids[current]["aborted"] = True
            elif stripped.startswith("Number of solutions found"):
                grids[current]["count"] = stripped
            elif stripped and not line.startswith("  ") and not stripped.startswith(">"):
                grids[current]["messages"].append(stripped)
    flush()
    for grid in grids.values():
        grid["solutions"].sort()
    return grids

def reference_for(path: Path) -> Path | None:
    rel = path.relative_to(REPO_DIR / "inputs")
    for candidate in (REPO_DIR / "outputs" / rel.with_suffix(".txt"), REPO_DIR / "outputs" / rel):
        if candidate.exists():
            return candidate
    return None

def check(cli: str, path: Path, ref: Path, cfg: str, flags: list[str], timeout: int) -> dict:
    expected = parse(ref.read_text())
    cmd = [cli, "--no-timing", "--timeout", str(timeout), *flags, str(path)]
    result = {"input": str(path.relative_to(REPO_DIR)), "cfg": cfg, "grids": len(expected)}
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout * len(expected) + 60)
    except subprocess.TimeoutExpired:
        return {**result, "result": "timeout", "timed_out_grids": sorted(expected), "diffs": []}

    got = parse(proc.stdout)
    diffs, timed_out = [], []
    for name in sorted(set(got) | set(expected)):
        if name not in got or name not in expected:
            diffs.append(f"{name}: {'missing' if name not in got else 'unexpected'}")
        elif got[name]["aborted"]:
            timed_out.append(name)
        else:
            diffs += [f"{name}: {key} differs" for key in ("count", "solutions", "messages") if got[name][key] != expected[name][key]]
    status = "DIFF" if diffs else ("timeout" if len(timed_out) == len(expected) else ("partial" if timed_out else "ok"))
    return {**result, "result": status, "timed_out_grids": timed_out, "diffs": diffs}

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", default=str(REPO_DIR / "build-release" / "bin" / "picross_solver_cli"))
    parser.add_argument("--timeout", type=int, default=60, help="per-grid solver timeout in seconds")
    parser.add_argument("--jobs", type=int, default=3)
    args = parser.parse_args()

    configs = CONFIGS
    inputs = sorted(p for p in (REPO_DIR / "inputs").rglob("*") if p.suffix in (".txt", ".non") and reference_for(p))
    tasks = [(args.cli, p, reference_for(p), cfg, driver_flags(c), args.timeout) for p in inputs for cfg, c in configs.items()]
    print(f"{len(inputs)} inputs with reference outputs x {len(configs)} configs = {len(tasks)} runs", file=sys.stderr)

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(lambda t: check(*t), tasks))

    out_dir = BENCH_DIR / "results" / "regression"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "summary.jsonl").write_text("".join(json.dumps(r) + "\n" for r in results))

    counts = {status: sum(r["result"] == status for r in results) for status in ("ok", "partial", "timeout", "DIFF")}
    grids_total = sum(r["grids"] for r in results)
    grids_timed_out = sum(len(r["timed_out_grids"]) for r in results)
    print(f"runs: {counts['ok']} match, {counts['partial']} match on the grids that finished, {counts['timeout']} timed out, {counts['DIFF']} differ")
    print(f"grids: {grids_total - grids_timed_out}/{grids_total} checked, {grids_timed_out} timed out (> {args.timeout}s)")

    sys.exit(1 if counts["DIFF"] else 0)

if __name__ == "__main__":
    main()
