# DAL Benchmark Regression Workflow

Use this reference after correctness tests pass to compare a finished branch with a baseline or
to advise on benchmark coverage. Performance measurement is an out-of-band quality sweep, not a
substitute for correctness review.

## Contents

- [Project benchmark context](#project-benchmark-context)
- [Regression gate and module map](#regression-gate-and-module-map)
- [Baseline and isolation](#baseline-and-isolation)
- [Release builds](#release-builds)
- [Paired measurement](#paired-measurement)
- [Current CI reproduction](#current-ci-reproduction)
- [Threshold and verdict](#threshold-and-verdict)
- [Report contract](#report-contract)
- [Coverage advisory](#coverage-advisory)
- [Boundaries and cleanup](#boundaries-and-cleanup)

## Project Benchmark Context

- DAL is a C++17 quantitative-finance library with Automatic Adjoint Differentiation.
- `dal-cpp/benchmarks/` contains standalone executables, one directory per target.
- The targets are registered with CTest under the `benchmark` label, but the normal
  `build_linux.sh` test pass excludes that label and does not execute them.
- `DAL_CPP_BUILD_BENCHMARKS` defaults to `ON` in `dal-cpp/CMakeLists.txt`, but the shared `base`
  CMake preset overrides it to `OFF`. Every preset-based performance build must therefore pass
  `-DDAL_CPP_BUILD_BENCHMARKS=ON` explicitly.
- The Linux CI job builds and smoke-runs all current benchmark targets. Its paired PR regression
  gate is the nine-target closed set in
  `.github/scripts/check_benchmark_regressions.py`.
- Installed binaries under `build/stage/.../bin/` change only after `cmake --install`. They can
  be stale after a rebuild and are not valid for branch-versus-baseline comparison.

Read current shared build options in `CLAUDE.md`, the benchmark target inventory in
`dal-cpp/benchmarks/CMakeLists.txt`, and the executable gate in
`.github/scripts/check_benchmark_regressions.py`. These are current repository files; no legacy
Claude artifact is required.

## Regression Gate And Module Map

The regression gate is exactly these nine executables:

- `tape_perf`: native AAD tape clear, rewind, zero-adjoint, and propagation operations.
- `jacobian_perf`: curve-calibration Jacobian sweeps and dense/row-width harvesting.
- `pde_perf`: `ThetaScheme_` Crank-Nicolson rollback for a 200 by 200 European-call workload.
- `rng_perf`: Sobol normal and uniform filling over 100K paths.
- `interp_perf`: cubic interpolation with 50 knots and 10K query points.
- `krylov_perf`: conjugate-gradient solve for a 500 by 500 SPD tridiagonal system.
- `banded_perf`: banded tridiagonal matrix-vector multiplication at size 10K.
- `cholesky_perf`: dense Cholesky decomposition of a 200 by 200 SPD matrix.
- `rate_risk_perf`: rate pricing, node-risk aggregation, and steady-state quote-risk aggregation
  for single-curve, joint-XCCY, and staged-XCCY-basis provenance.

Do not add another executable to the regression verdict ad hoc. The paired gate rejects names
outside this allowlist.

All other benchmark targets are smoke/coverage evidence, not part of this regression verdict.
That includes the historically excluded `matrix_perf` and `script_perf`, plus newer targets such
as `script_mc_perf`, `curve_calibration_perf`, `xccy_perf`, `ycinstrument_perf`, and
`threadpool_perf`. Run them as informational evidence only when the changed path or user request
calls for them.

## Baseline And Isolation

Use the merge-base of the branch under test and `master`, unless the user names a specific
baseline. Capture immutable commit IDs before building:

```bash
head_ref=$(git rev-parse HEAD)
baseline_ref=$(git merge-base "$head_ref" master)
```

If the user supplies a baseline, resolve it with `git rev-parse <baseline>` and record that SHA
instead. Do not compare against a moving branch name after measurement begins.

Create separate detached source worktrees and separate out-of-tree build directories:

```bash
perf_root=$(mktemp -d)
head_source="$perf_root/head-source"
baseline_source="$perf_root/baseline-source"
head_build="$perf_root/head-build"
baseline_build="$perf_root/baseline-build"

git worktree add --detach "$head_source" "$head_ref"
git worktree add --detach "$baseline_source" "$baseline_ref"
```

Use equivalent initialized dependencies in both source worktrees. If submodules are absent,
report the build as blocked or initialize both worktrees identically when that checkout mutation
is authorized. Never reuse the active branch build directory for either side.

Record both source SHAs, compiler identity, CMake version, generator, CPU model, AAD backend,
thread settings, and relevant environment variables. Use the same machine and settings for both
builds.

## Release Builds

Configure both sides as Release builds with benchmarks explicitly enabled. Keep `-S` and `-B`
visible so there is no ambiguity about which source produced which binary:

```bash
cmake --preset=Release-linux -S "$head_source" -B "$head_build" -DDAL_CPP_BUILD_BENCHMARKS=ON
cmake --preset=Release-linux -S "$baseline_source" -B "$baseline_build" -DDAL_CPP_BUILD_BENCHMARKS=ON

cmake --build "$head_build" --parallel "$(nproc)"
cmake --build "$baseline_build" --parallel "$(nproc)"
```

Build both sides on the same machine without changing compiler, CPU governor, native-architecture
flags, AAD backend, thread count, or other cache options between them. If a special option is
needed, pass the identical value to both configure commands and record it.

Confirm benchmark enablement in both `CMakeCache.txt` files:

```bash
rg '^DAL_CPP_BUILD_BENCHMARKS:BOOL=ON$' \
  "$head_build/CMakeCache.txt" "$baseline_build/CMakeCache.txt"
```

Run only build-tree binaries:

```text
<build-root>/dal-cpp/benchmarks/<benchmark>/<benchmark>
```

For example:

```bash
"$head_build/dal-cpp/benchmarks/tape_perf/tape_perf"
"$baseline_build/dal-cpp/benchmarks/tape_perf/tape_perf"
```

Do not use `bin/<benchmark>`, `build/stage/.../bin/<benchmark>`, or a path from an earlier build.

## Paired Measurement

Single process runs are diagnostic only. Never issue a regression verdict from one run.

For every gated executable:

1. Run the branch and baseline on the same otherwise-idle machine.
2. Collect at least ten process-level samples per side.
3. Interleave the sides and alternate which side runs first, cancelling slow environmental
   drift.
4. Retain every executable output and every parsed case duration.
5. Reduce each side to its best-of-N minimum. Timing samples are right-skewed; the minimum is
   least contaminated by scheduler, cache, and page-fault delays.
6. If a result is borderline, collect more paired samples and inspect distribution shape or a
   corroborating statistical test. Do not replace the gate reduction with mean or median.

Set `DAL_NUM_THREADS` consistently for both sides. Current CI uses `DAL_NUM_THREADS=4`.

On a shared runner, VM, WSL2 host, thermally unstable machine, or visibly busy workstation,
record the environment as noisy. If additional samples do not stabilize the minima, the verdict
is `inconclusive`, not `regression`.

## Current CI Reproduction

The current executable gate defaults to ten samples, two confirmation rounds, and a 4% threshold.
The Linux PR workflow passes those values explicitly. Two rounds of ten means twenty interleaved
process samples per case and side.

Reproduce it against the isolated build roots:

```bash
python3 "$head_source/.github/scripts/check_benchmark_regressions.py" \
  --base-root "$baseline_build" \
  --head-root "$head_build" \
  --output-dir "$perf_root/paired-results" \
  --samples 10 \
  --confirmation-rounds 2 \
  --threshold-percent 4
```

The script:

- alternates base/head first position on every outer sample;
- validates complete sample counts;
- reduces each confirmation round and the combined samples with `min`;
- fails a comparable case only when every confirmation round exceeds +4%;
- treats base-only case removal/renaming as a hard coverage failure;
- reports head-only cases as new informational coverage;
- applies the current Sobol precise-opt-in/fast ratio ceiling; and
- writes raw command outputs, `results.json`, and `summary.md` under the output directory.

Use the script's nonzero exit as the current repository gate result, but still inspect and report
its raw samples and failures.

## Threshold And Verdict

The repository's calibrated policy is a strict +4% threshold in every one of two independent
best-of-ten confirmation rounds. Historical same-binary calibration found roughly 1% average
best-of-N noise and a maximum just under 4%; shared-runner single-process swings can be around
plus or minus 6%. The 4% two-round rule operationalizes that evidence conservatively.

Classify each comparable benchmark case:

- `regression`: every confirmation round is above +4%, or the executable gate reports another
  hard acceptance failure.
- `no-change`: the gate passes and neither direction has a sustained material delta.
- `improvement`: branch minima are consistently and materially lower.
- `inconclusive`: the build, dependency state, machine noise, incomplete samples, or changing
  environment prevents a defensible comparison.

Do not relabel a passing 2-4% movement as a regression. Do not let one large run override stable
best-of-N minima.

## Report Contract

The report must contain:

- branch and baseline SHAs and how the baseline was selected;
- isolated source and build paths;
- Release configuration, benchmark enablement, compiler, CPU, AAD backend, thread count, and
  whether the machine was quiet;
- sample count per side, interleaving order, reduction (`min`), round count, and threshold;
- for every gated benchmark and case: baseline minimum, branch minimum, percentage delta, both
  confirmation-round deltas, and verdict;
- paths to retained raw outputs, `results.json`, and `summary.md`;
- any informational benchmark results outside the nine-target gate;
- the coverage advisory; and
- one overall verdict: `no regression`, `regression found` with named cases, or `inconclusive`
  with the blocking reason.

Never hide noisy or missing measurements behind an overall pass. Distinguish a failed performance
acceptance gate from an environment that could not produce a valid measurement.

## Coverage Advisory

Re-read the branch diff and identify changed hot paths: AAD sweeps, calibration loops, matrix
kernels, interpolation queries, random/path generation, PDE stepping, script evaluation, and
thread-pool operations.

Map each path first to the nine-target module map above, then to real current targets in
`dal-cpp/benchmarks/CMakeLists.txt`. If a current executable already exercises the path, advise
extending its cases or workload rather than adding a near-duplicate target.

When no executable covers the path, advise a focused target under
`dal-cpp/benchmarks/<name>_perf/` with a concrete problem size, iteration count, and expected
duration comparable to neighboring targets. Coverage advice is non-blocking unless the user or
controlling acceptance criteria make it a requirement.

Do not edit or create the benchmark in advisory mode. If implementation is requested, hand off to
`dal-implementer` and preserve its isolated-worktree and test discipline.

## Boundaries And Cleanup

- Do not benchmark before correctness tests pass.
- Do not compare one run, non-interleaved machines, different build types, or different compiler
  and CPU settings.
- Do not use stale installed binaries or reuse one side's build directory for the other.
- Do not gate on targets outside the nine-target allowlist.
- Do not edit production or benchmark code during pure measurement.
- Do not commit, push, change PR state, submit a review, resolve threads, or merge.
- Do not assert a regression when the environment is noisy or the samples are incomplete.

After the report is durable, remove the detached worktrees with `git worktree remove` and remove
only the validated temporary performance root. Preserve raw results when the user wants them
retained.
