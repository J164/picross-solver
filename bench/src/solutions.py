import hashlib
from pathlib import Path

HASH_HEX_DIGITS = 16

def solution_hash(rows: list[str]) -> str:
    return hashlib.sha256("/".join(rows).encode()).hexdigest()[:HASH_HEX_DIGITS]

def read_non_clues(path: str) -> dict[str, list[list[int]]]:
    clues: dict[str, list[list[int]]] = {"rows": [], "columns": []}
    current = None
    for line in Path(path).read_text().splitlines():
        word = line.strip()
        if word in clues:
            current = clues[word]
        elif not word:
            if current:
                current = None
        elif current is not None:
            current.append([int(n) for n in word.split(",") if int(n) > 0])
    return clues

def runs_of(cells: str) -> list[int]:
    return [len(run) for run in cells.split("0") if run]

def is_solution(clues: dict[str, list[list[int]]], rows: list[str]) -> bool:
    if len(rows) != len(clues["rows"]) or any(len(row) != len(clues["columns"]) or set(row) - {"0", "1"} for row in rows):
        return False
    columns = ["".join(row[x] for row in rows) for x in range(len(clues["columns"]))]
    return [runs_of(row) for row in rows] == clues["rows"] and [runs_of(col) for col in columns] == clues["columns"]

def summarize(path: str, solutions: list[dict]) -> dict:
    clues = read_non_clues(path)
    return {
        "nsol": len(solutions),
        "solution_hashes": [("partial:" if s.get("partial") else "") + solution_hash(s["rows"]) for s in solutions],
        "solutions_valid": all(not s.get("partial") and is_solution(clues, s["rows"]) for s in solutions),
    }
