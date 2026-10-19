DAL-387 repairs comparison startup on `agent/dal-orchestrator/aa0a6f07eca6`,
against immutable baseline `2887adc8f50ce1b028d6deed91ec8b54733033f3`.
The orchestrator owns independent review and acceptance after this handoff.

## Failure and design

The latest scheduled run,
[39236247937](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/runs/39236247937/job/118005899404),
fails in “Compare Python pricing and AAD risk with QuantLib and rateslib”.
The previous run,
[39171282067](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/runs/39171282067/job/117804684655),
fails identically. Both complete 339 comparison tests before the driver fails.
The latest run also completes the native regression gate with zero failures
and the Python gate with 98 comparisons and zero failures. These historical
results classify the failure; they do not certify this repair's head.

The workflow supplies `PYTHONPATH` only to pytest, then launches the documented
driver with `--dal-package`. Its import chain is
`compare -> evidence -> scenarios -> exercise_scenarios -> exercise_pde -> dal`.
The public PDE migration in `0be45b26` introduced the native dependency before
argument parsing and before the existing worker build selector can run.
The resulting exception is `ModuleNotFoundError: No module named 'dal'`.
Workers deliberately clear inherited `PYTHONPATH`, so changing only the
workflow environment would leave their startup broken.

Defer importing the PDE adapter until the first surface request. Initialize
the controller through the existing `load_dal` validator after parsing arguments,
before report validation or reference generation. Workers retain their existing
validated build selection. Both controller and workers reject missing packages
before creating evidence directories; help requires no native backend.
The controller needs DAL too: checking worker reports and aggregating results
evaluate the public PDE reference in that process.

The controlling requirements are the issue handoff, benchmark README and
`.codex/references/benchmark-workflow.md`; no separate DAL-387 public API/spec
or numerical critique was supplied. The component boundary contract, public
early-exercise PDE method and archived DAL-379 benchmark migration record were
also inspected. No core numerical or public compatibility change is required.

## Scope and case mapping

- `dal-python/benchmarks/dal_comparisons/compare.py`: controller startup for
  all 43 comparison workloads and three worker backends; use the selected build
  for controller-side reference validation.
- `dal-python/benchmarks/dal_comparisons/exercise_scenarios.py`: eight
  `mc_{bermudan,american}_{price,greeks}_{16384,65536}` reference paths; only
  adapter import timing changes, outside workload timing and behind the existing
  surface cache. Exact dates, grids, refinement, bumps and tolerances are intact.
- `dal-python/benchmarks/tests/test_comparison_cli.py`: clean subprocess CLI
  regressions, with native imports explicitly blocked even if a wheel is installed.
  Covers help and missing selected packages for controller and all three workers.
- This record and its index document the repair and handoff.

The actual smoke invocation exercises the complete 43-case inventory because
the driver validates complete worker reports and exposes no case selector.
It is execution/correctness evidence, not a performance acceptance sample.
Native nine-target timings, unrelated algorithm matrices and the full product
test suite are excluded: no timed algorithm, C++/binding code, benchmark workload,
build file, example, dependency pin or target closure changes. Consequently no
new `DAL_BUILD_PUBLIC=OFF` example/build certification is required or claimed.
The Python benchmark correctly depends on the public binding.

Full-mode policy remains at least ten processes per backend in two rounds,
rotating backend order and reversing the second round, with two warmups and
unchanged provenance/completeness checks. Native/Python base-head regression
gates, their +4% thresholds, confirmation and noise controls are unchanged.
There is no new timing verdict from this shared WSL host.

## RED and GREEN

Before production edits, this command exits 1 with the same import chain as CI:

```sh
env -u PYTHONPATH PYTHONNOUSERSITE=1 python3 -s \
  dal-python/benchmarks/run_comparisons.py \
  --dal-package build/benchmark-head/dal-python \
  --output-dir ../dal387-baseline-repro --samples 10 --rounds 2
```

The focused help test first fails on the premature native import, then passes
after deferring the PDE adapter. Adding missing-package coverage exposes the
controller's absent selector; it passes after initializing the controller.
The test was moved while green from `.github/scripts/tests` to the existing
comparison test directory, so it runs in the workflow's comparison pytest step.
The final test is also copied unchanged into a detached `2887adc8` source
worktree at `../dal387-baseline`; both test methods fail there (five failing
subcases). No baseline binary is needed for this import-order reproduction.
The detached reproduction worktree was removed after retaining its logs.

```sh
# RED: in ../dal387-baseline, exit 1; GREEN: in the task checkout, exit 0.
../dal387-venv/bin/python -m unittest discover \
  -s dal-python/benchmarks/tests -p test_comparison_cli.py -v
```

Raw logs retain both incremental cycles, the final baseline reproduction and
the direct CI-command reproduction. Test assertions were not weakened.

## Verification and delivery

Fresh native build: GCC 15.2.0, CMake 4.2.3, C++17 Release/static, native AAD,
native-architecture tuning off, CPython 3.13.9, pybind11 3.0.4. The benchmark-only
hash-locked dependencies are installed unchanged in `../dal387-venv`, including
QuantLib 1.43 and rateslib 2.7.1. All pinned submodule identities are unchanged.
The host is a shared WSL VM on an Intel i9-13900HX. No timing comparison is made
between this configuration and the historical GCC-14 GitHub runner.

Commands run from the task checkout (the selected interpreter is passed as an
absolute path to CMake):

```sh
python3 .github/scripts/checkout_dependencies.py
uv venv ../dal387-venv --python /home/wegamekinglc/anaconda3/bin/python3
uv pip install --python ../dal387-venv/bin/python --require-hashes \
  -r dal-python/benchmarks/requirements-comparisons.txt
uv pip install --python ../dal387-venv/bin/python pytest pybind11==3.0.4
cmake -S . -B build/dal387-python -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF -DDAL_BUILD_PUBLIC=ON -DDAL_PUBLIC_BUILD_TESTS=OFF \
  -DDAL_CPP_BUILD_TESTS=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF \
  -DDAL_CPP_BUILD_BENCHMARKS=OFF -DDAL_BUILD_PYTHON=ON \
  -DPython3_EXECUTABLE="$PWD/../dal387-venv/bin/python" \
  -DDAL_ENABLE_NATIVE_ARCH=OFF
cmake --build build/dal387-python --target _dal --parallel 6
python3 -m unittest discover -s .github/scripts/tests -v
DAL_NUM_THREADS=4 PYTHONPATH="$PWD/build/dal387-python/dal-python" \
  ../dal387-venv/bin/python -m pytest dal-python/benchmarks/tests \
  dal-python/tests/test_early_exercise_pde.py -q
env -u PYTHONPATH PYTHONNOUSERSITE=1 DAL_NUM_THREADS=4 \
  OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1 \
  MPLCONFIGDIR="$PWD/../dal387-matplotlib" ../dal387-venv/bin/python -s \
  dal-python/benchmarks/run_comparisons.py \
  --dal-package build/dal387-python/dal-python \
  --output-dir ../dal387-comparison-smoke --smoke
python3 .github/scripts/check_docs.py
git diff --check
```

All commands exit 0. Script verification: 208 tests run, 202 passed and six
existing Windows/pwsh/.NET-only skips. Pytest: 352 passed (341 comparison tests
including the two new methods, plus 11 binding tests), four successful unittest
subtests and the existing rateslib licence notice. No skips in this pytest run.
This includes the unchanged PDE convergence, independent CRR/European checks,
all Greek bumps and public-reference routing assertions.

Actual clean-environment driver: controller status `passed`, 43 complete rows
per worker; DAL 43 passed, QuantLib 39 passed/four declared unsupported, rateslib
25 passed/18 declared unsupported. All three workers resolve the newly built
module inside `build/dal387-python/dal-python`; its SHA-256 is
`8cb74dcd029bb869ff0ea670eb9b363d1c51eb9b2e2fecb5e0a97472d35e986e`,
also rechecked after execution. The controller successfully validates and
aggregates the early-exercise references with no inherited `PYTHONPATH`.
The smoke uses the existing one-round/one-sample profile and two warmups;
it is not substituted for a full performance measurement.

Raw evidence is retained in the workspace's `dal387-*.log`,
`dal387-comparison-smoke/{results.json,summary.md,round-1/}`, dependency versions
and native SHA files. The final issue attachment bundles those files, the
historical Linux logs and the downloaded failed-run artifact, plus the tested
CMake cache and this report. These runtime paths describe evidence locations,
not reader-accessible links. No run-owned process is left running.

No numerical tolerance, PDE configuration, public API, component placement or
performance-gate deviation is needed. Local correctness/startup verification
passes; required exact-head CI and independent review remain external acceptance
steps. Full performance remeasurement is outside this import-only repair.

Delivery: production/test commit
`68618e6ff57fe2a7933cefed45866d8cb8e1ea10`,
[PR #605](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/605)
from `agent/dal-orchestrator/aa0a6f07eca6` into `master`. The following commit
adds only this report and its active-artifact index. No production or test
source changed after the successful verification. The fresh checkout had no
Git author configured; commits use the turn's specified Cheng Li identity
through per-command Git configuration, without modifying shared/global identity.
CI is left to run normally after push; no watch, polling, review dispatch or
merge is part of this implementation handoff.
