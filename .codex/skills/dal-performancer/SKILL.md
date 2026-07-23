---
name: dal-performancer
description: Run or advise on DAL benchmark regression checks and benchmark coverage. Use after implementation and passing tests when the user asks about performance, benchmark regressions, CI benchmark noise, hot paths, or where to add `*_perf` coverage.
---

# DAL Performancer

Treat benchmark noise as the dominant risk. Do not flag regressions from single runs.
Run performance measurement only after correctness tests pass.

## Regression Set

Gate only on these eight benchmarks:

- `tape_perf`
- `jacobian_perf`
- `pde_perf`
- `rng_perf`
- `interp_perf`
- `krylov_perf`
- `banded_perf`
- `cholesky_perf`

Other benchmark targets, including `matrix_perf` and `script_perf`, are informational unless the
user explicitly expands the scope.

## Measurement Workflow

Before measuring or advising on coverage, load and follow the complete
[benchmark regression workflow](references/benchmark-workflow.md). Its required contract is:

1. Compare the branch with its merge-base against `master`, unless the user names a baseline.
2. Use isolated branch and baseline source worktrees and isolated Release build directories.
3. Configure both with explicit `-S`, `-B`, and `-DDAL_CPP_BUILD_BENCHMARKS=ON`.
4. Run binaries from each build tree, never a potentially stale install/stage `bin/`.
5. Measure on the same quiet machine with paired, interleaved samples and at least ten samples
   per side; reduce each set to best-of-N minimum.
6. Reproduce the current CI policy with
   `.github/scripts/check_benchmark_regressions.py`: two rounds of ten samples and a 4% threshold.
7. Report raw samples, environment, configuration, per-benchmark verdicts, and an overall
   `no regression`, `regression found`, or `inconclusive` verdict.

## Coverage Advisory

Map changed hot paths to the inline module map in the benchmark workflow. Suggest a new target or
extension only when no current benchmark covers the path. Coverage advice is non-blocking.

## Boundary

Pure measurement is read-only: do not edit production code, benchmark sources, Git history, pull
requests, or merge state. If the user asks to add coverage or fix a regression, use
`dal-implementer` in an isolated worktree. Never merge; hand the measured verdict back to the
requesting workflow.

## References

- [Benchmark regression workflow](references/benchmark-workflow.md): complete measurement,
  threshold, reporting, coverage, and cleanup procedure.
- [Build commands and options](../../../CLAUDE.md#build-commands): canonical shared CMake
  configuration.
- [Paired regression gate](../../../.github/scripts/check_benchmark_regressions.py): executable
  current CI policy.
