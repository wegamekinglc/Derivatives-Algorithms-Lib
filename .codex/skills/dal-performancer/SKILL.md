---
name: dal-performancer
description: Run or advise on DAL benchmark regression checks and benchmark coverage. Use after implementation and passing tests when the user asks about performance, benchmark regressions, CI benchmark noise, hot paths, or where to add `*_perf` coverage.
---

# DAL Performancer

Treat benchmark noise as the dominant risk. Do not flag regressions from single runs.

## Regression Set

Gate on these eight benchmarks:

- `tape_perf`
- `jacobian_perf`
- `pde_perf`
- `rng_perf`
- `interp_perf`
- `krylov_perf`
- `banded_perf`
- `cholesky_perf`

Do not gate on `matrix_perf` or `script_perf`; report them only if asked.

## Measurement Workflow

1. Identify baseline: merge-base against `master` unless the user names a ref.
2. Build branch and baseline in Release with benchmarks enabled.
3. Run each benchmark from the build tree path, not stale `bin/`.
4. Use paired, interleaved runs with N at least 10 for each binary.
5. Reduce to best-of-N min.
6. Flag regression only when branch min exceeds baseline min by more than the calibrated noise floor, roughly 2-4 percent.

Build:

```bash
mkdir -p build
cd build
cmake --preset=Release-linux ..
make -j$(nproc)
```

## Report Table

| Benchmark | Baseline min (ms) | Branch min (ms) | Delta | Verdict | Notes |
|-----------|-------------------|-----------------|-------|---------|-------|

Include sample count, environment quietness, and whether the result is inconclusive.

## Coverage Advisory

Map changed hot paths to existing benchmarks. Suggest a new target or extension only when no benchmark covers the path.
