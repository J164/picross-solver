import json
import subprocess
import time
from collections.abc import Iterator
from pathlib import Path

from .cache_puzzles import PUZZLE_DIR, Puzzle
from .solutions import summarize

REPO_DIR = Path(__file__).resolve().parent.parent.parent
DEFAULT_DRIVER = REPO_DIR / "build-release" / "bin" / "phase_driver"

SOLVER_FLAGS = ("dp_line_solver", "freedom", "bitboard", "fp1", "fp2", "pixel_branching", "incremental_propagation")
CONFIG_KEYS = set(SOLVER_FLAGS) | {"max_solutions", "observer", "max_search_nodes"}
HARD_KILL_GRACE_S = 15.0

def driver_flags(config: dict) -> list[str]:
    return [f"--{name.replace('_', '-')}" for name in SOLVER_FLAGS if config.get(name)]

def run_driver_trial(driver: str, puzzle_path: str, timeout: float, config: dict, max_solutions: int) -> dict:
    cmd = [driver, puzzle_path, "--max-solutions", str(max_solutions), "--timeout", str(timeout), *driver_flags(config)]
    if not config.get("observer", True):
        cmd.append("--no-observer")
    if config.get("max_search_nodes"):
        cmd += ["--max-search-nodes", str(config["max_search_nodes"])]

    start = time.perf_counter()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + HARD_KILL_GRACE_S)
    except subprocess.TimeoutExpired:
        return {"status": "timeout", "solver_status": "KILLED", "wall_s": time.perf_counter() - start}

    try:
        out = json.loads(proc.stdout)
    except json.JSONDecodeError:
        return {"status": "error", "solver_status": f"exit {proc.returncode}", "stderr": proc.stderr}

    statuses = {"OK": "ok", "TIMEOUT": "timeout", "CONTRADICTORY_GRID": "contradictory", "BUDGET_REACHED": "budget"}
    return {
        "status": statuses.get(out["status"], "error"),
        "solver_status": out["status"],
        "wall_s": out["wall_time_s"],
        "cpu_s": out["cpu_time_s"],
        "peak_rss_kb": out["peak_rss_kb"],
        **summarize(puzzle_path, out["solutions"]),
        "search_nodes": out["search_nodes"],
        "probing_rounds": out["probing_rounds"],
        "max_depth": out["max_depth"],
        "line_solves": out["line_solves"],
        "phases_s": out["phases_s"],
        "depth_split_s": out["depth_split_s"],
    }

def puzzle_path(puzzle: Puzzle) -> str:
    return str(PUZZLE_DIR / f"{puzzle['sha']}.non")

def puzzle_fields(puzzle: Puzzle) -> dict:
    return {key: puzzle[key] for key in ("sha", "name", "source", "source_group", "width", "height")}

def run_trials(driver: str, puzzle: Puzzle, config: dict, timeout: float, max_solutions: int, trials: int,
               stop_if_unsolved: bool = True, first_trial: int = 1, extra_fields: dict | None = None) -> Iterator[dict]:
    max_solutions = config.get("max_solutions", max_solutions)
    for trial in range(first_trial, first_trial + trials):
        result = run_driver_trial(driver, puzzle_path(puzzle), timeout, config, max_solutions)
        yield {**puzzle_fields(puzzle), **(extra_fields or {}), "trial": trial, **result, "finished_at": time.time()}
        if stop_if_unsolved and result["status"] not in ("ok", "contradictory"):
            break

def run_job(job: dict) -> list[dict]:
    return list(run_trials(job["driver"], job["puzzle"], job["config"], job["timeout"], job["max_solutions"], job["trials"], job["stop_if_unsolved"], job["first_trial"], job["extra_fields"]))
