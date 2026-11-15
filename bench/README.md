# Benchmarks

## 1. Build (Release)

```
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DPICROSS_BUILD_CLI=ON -DPICROSS_BUILD_BENCH=ON -DPICROSS_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS="-Wno-error=free-nonheap-object"
cmake --build build-release -j
```

## 2. Puzzles

The first benchmark run downloads and indexes the puzzles into `bench/cache/`:

| source | puzzles | notes |
|---|---|---|
| `sample_simpson` | 31 | Wolter's survey sample set |
| `rand30` | 5,000 | random 30x30 from the survey |
| `tournament` | 30,000 | 25x25, 1000 per TAAI/ICGA/TCGA tournament 2011-2023 |

## 3. Running benchmarks

`bench.py` runs one config on a random selection of puzzles and prints one JSON row per trial (the measurement
layer is `src/trials.py`): status, wall and CPU time, peak memory, search and probing counters, phase times, and the
solutions' fingerprints and validity.

```
python bench/bench.py phases # counters and phase times
python bench/bench.py timing # just timing
```
