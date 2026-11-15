import argparse
import json
import random
import sys
from pathlib import Path

from src.cache_puzzles import get_puzzles
from src.trials import DEFAULT_DRIVER, SOLVER_FLAGS, driver_flags, run_trials

SOURCES = ("sample_simpson", "rand30", "tournament")

def select_puzzles(args: argparse.Namespace) -> list:
    puzzles = get_puzzles()
    if args.source is not None:
        puzzles = [puzzle for puzzle in puzzles if puzzle["source"] == args.source]

    groups = {source: [puzzle for puzzle in puzzles if puzzle["source"] == source] for source in ([args.source] if args.source is not None else SOURCES)}

    rng = random.Random(args.seed)
    n = args.num_puzzles
    base_share = n // len(groups)
    shares = {group: min(len(puzzles), base_share) for group, puzzles in groups.items()}

    leftover = n - sum(shares.values())
    growable = [group for group in groups if shares[group] < len(groups[group])]
    i = 0
    while leftover > 0 and len(growable) > 0:
        group = growable[i % len(growable)]
        shares[group] += 1
        leftover -= 1
        if shares[group] >= len(groups[group]):
            growable.remove(group)
        else:
            i += 1

    if leftover > 0:
        total = sum(len(puzzles) for puzzles in groups.values())
        print(f"only {total} puzzles are available but {n} were requested", file=sys.stderr)

    selected = []
    for group, puzzles in groups.items():
        selected += rng.sample(puzzles, shares[group])
    return selected

def cmd_run(args: argparse.Namespace) -> None:
    binary = Path(args.binary) if args.binary is not None else DEFAULT_DRIVER
    if not binary.exists():
        raise SystemExit(f"binary not found: {binary}")

    config: dict = {name: True for name in SOLVER_FLAGS if getattr(args, name)}
    config["observer"] = args.command == "phases"
    if args.max_search_nodes is not None:
        if args.command != "phases":
            raise SystemExit("--max-search-nodes is only supported by the phases command\n")
        config["max_search_nodes"] = args.max_search_nodes
    try:
        driver_flags(config)
    except ValueError as e:
        raise SystemExit(str(e))

    for puzzle in select_puzzles(args):
        for row in run_trials(str(binary), puzzle, config, args.timeout, args.max_solutions, args.trials, extra_fields={"cfg": args.command}):
            print(json.dumps(row), flush=True)

def add_common_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--trials", type=int, default=5, help="trials per puzzle; a trial that does not finish ends the series (default: 5)")
    parser.add_argument("--timeout", type=float, default=10.0, help="solve timeout in seconds (default: 10)")
    parser.add_argument("--num-puzzles", type=int, default=10, help="number of puzzles randomly selected to solve (default: 10)")
    parser.add_argument("--seed", type=int, default=None, help="seed for --num-puzzles (default: None)")
    parser.add_argument("--source", type=str, default=None, choices=list(SOURCES), help="source to select puzzles from (default: all)")
    parser.add_argument("--max-solutions", type=int, default=2, help="stop after this many solutions (default: 2)")
    parser.add_argument("--dp-line-solver", action="store_true", help="enable the DP line solver")
    parser.add_argument("--freedom", action="store_true", help="enable the Chen-Lin freedom-parameter line solver")
    parser.add_argument("--bitboard", action="store_true", help="enable portable multiword bitboard line operations")
    parser.add_argument("--fp1", action="store_true", help="enable the FP1 probing optimization")
    parser.add_argument("--fp2", action="store_true", help="enable the FP2 probing optimization")
    parser.add_argument("--pixel-branching", action="store_true", help="enable pixel branching")
    parser.add_argument("--incremental-propagation", action="store_true", help="propagate with the full reduction only, and keep the FP1 probing grids between probing rounds")
    parser.add_argument("--max-search-nodes", type=int, default=None, help="maximum number of search nodes the solver can try")
    parser.add_argument("--binary", type=str, default=None, help=f"phase_driver binary (default: {DEFAULT_DRIVER})")

def main() -> None:
    parser = argparse.ArgumentParser(prog="bench.py")
    subparsers = parser.add_subparsers(dest="command", required=True)

    timing_parser = subparsers.add_parser("timing", help="Time the solver on each puzzle")
    add_common_args(timing_parser)
    timing_parser.set_defaults(func=cmd_run)

    phases_parser = subparsers.add_parser("phases", help="Time the solver and record its phases and counters on each puzzle")
    add_common_args(phases_parser)
    phases_parser.set_defaults(func=cmd_run)

    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()
