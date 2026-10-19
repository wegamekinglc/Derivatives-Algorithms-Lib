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
At the initial implementation handoff, CI was left to run normally after push;
no watch, review dispatch or merge was included in that first delivery.

## Authorized merge and full benchmark follow-up

The subsequent user instruction requests merge and a successful complete
post-merge benchmark, authorizing foreground CI/result collection. Reuse the
independent **Approve** for exact head
`d36f747837daffbbb1722944d374d164d0ca85f7`; its retained report also verifies all
35 smoke source hashes and the three worker identities. No production or test
source changed after that review.

Before merge, initial and immediately pre-merge paginated audits covered all
review threads, inline comments, PR comments, reviews, exact-head check runs and
legacy commit statuses. Both audits found zero review threads, zero inline
comments and zero GitHub reviews; the sole PR comment is Codacy's zero-issue
report. The independent Approve is recorded in the Multica review thread.
Final-head **36/36 check runs passed**, including `Linux CI gate` and
`Windows CI gate`. Master protection requires both contexts from app 15368,
strict up-to-date checks, and zero mandatory GitHub approvals. These settings
were preserved. The final PR state was `MERGEABLE` / `CLEAN`, with unchanged head.
The older local CLI initially reports no `--required` checks before the gate
jobs exist; the actual master protection and final raw check records are used
for acceptance instead.

The single foreground `gh pr checks 605 --watch --interval 60` exits 0. After
the final audits and another head check, merge with:

```sh
gh pr merge 605 --squash \
  --match-head-commit d36f747837daffbbb1722944d374d164d0ca85f7 \
  --body-file ../dal387-merge-body.md
```

[PR #605](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/605)
is merged at `2026-10-19T19:06:19Z`; merge commit:
`0f1feabee193d0976ab7a5336c9e9e29cb2aa28d`. No admin bypass, auto-merge,
check-policy change, unresolved-thread dismissal or production repair is needed.

Dispatch the original, unchanged workflow on `master`:

```sh
gh workflow run benchmarks.yml --ref master
gh run watch 39308227048 --interval 60 --exit-status
```

[Run 39308227048](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/runs/39308227048)
is created at `2026-10-19T19:06:48Z`, event `workflow_dispatch`, branch `master`,
actual head SHA exactly `0f1feabee193d0976ab7a5336c9e9e29cb2aa28d`.
The run is distinguished from all pre-dispatch manual runs, and a fetched Git
ancestry check confirms the merge commit is contained. This explicitly requested
post-merge full workflow expands the original development smoke scope; all
sampling, confirmation rounds, noise controls, numerical checks and thresholds
remain intact. The issue was kept `in_progress` until the complete successful
run and its evidence were collected.

Merge audits and review evidence are retained in `dal387-merge-audit/`;
dispatch identity, foreground watch output and complete raw benchmark artifacts
are retained in `dal387-postmerge-benchmark/`. The final handoff attaches the
updated record and evidence, with each job and gate's actual conclusion.

### Final result

The foreground run watch exits **0**. The complete workflow concludes
**success** at `2026-10-19T20:55:26Z`, approximately 108 minutes 38 seconds
after dispatch. Linux completes successfully at `20:55:25Z`; Windows completes
successfully at `19:52:06Z`. The actual run head remains the merge SHA above,
and both platforms' retained `environment.json` identify baseline
`2887adc8f50ce1b028d6deed91ec8b54733033f3` and that exact merged head.
The approved head and squash merge have identical Git tree
`d6276042abe8511a45e5d8cb507295b1afff4a6f`; their full tree diff is empty.

All three required comparison steps report `success`:

- **Compare base and head Python performance:** all 98 cases pass, zero
  failures and zero unconfirmed signals. Retained raw arrays contain 20 samples
  per case and side: two rounds of ten, best-of-N minima, strict +4% in both
  rounds, plus the unchanged nine-of-ten paired confirmation rule.
- **Compare Python pricing and AAD risk with QuantLib and rateslib:** full
  mode (`smoke: false`), two rounds of ten processes per backend, two warmups,
  all 60 worker reports complete. Each reports 43 ordered cases: DAL 43 supported,
  QuantLib 39 supported/four declared unsupported, rateslib 25 supported/18
  declared unsupported. Numerical validation and completeness pass throughout.
  This closes the initial smoke-only acceptance boundary; timing ratios remain
  informational, with no competitor speed requirement invented.
- **Compare base and head performance:** all nine allowlisted native targets
  and all 100 comparable cases pass, with zero failures. Every case and side
  retains 20 process samples and two round deltas. All six generic-joint
  overhead checks also pass. All nine static base/head executable pairs are
  byte-identical; the configured identical-binary retry is **not triggered**.
  The original +4%, two independent best-of-ten rounds and absolute-cost guards
  remain intact. Overall native/Python verdict: **no regression** under the
  existing gates.

Both Linux and Windows retain 24 smoke command statuses, all zero. Python
workload correctness on both native builds and the comparison pytest step
also report success. Conditional A/A diagnosis and reproduction steps remain
skipped because the Python performance gate succeeds; no required gate is skipped.

Linux measurement environment: AMD EPYC 7763 64-Core Processor, GCC 14.2.0,
CMake 3.31.6, Release/static, native architecture tuning enabled, native AAD
(workflow metadata `aadet`), CPython 3.13, `DAL_NUM_THREADS=4`; comparison workers
retain OMP/OpenBLAS/MKL thread counts of one. Head/base run on the same hosted
runner with the original interleaving and noise controls. The final report is
based on these complete calibrated samples, not the local smoke or a single run.

Independently check every comparison source digest against the merged Git
revision: all **35** match. All 60 workers identify source head `0f1feabe...`,
the selected build-tree package, and the same native SHA-256:
`095cdfcfa17de1336a19929264fea8c72774e2805b9a295dce780af6d1fd6466`.
No stale installed module or local smoke binary is substituted. `validation.json`
records the completed result, case/sample/worker counts and source identities.

The merge commit's own final GitHub snapshot has **27/27 successful check runs**,
including `Linux CI gate`, `Windows CI gate` and both benchmark jobs. This is
separate from the 36/36 successful pre-merge checks on the approved PR head.
The Multica link table confirms PR #605 is merged. There are no remaining
code findings, failed gates or run-owned foreground processes to hand off.

The evidence attachment includes complete per-case minima, percentages and both
round deltas in `linux-artifact/paired/summary.md` and
`linux-artifact/python-paired/summary.md`, full comparison tables in
`linux-artifact/python-third-party/summary.md`, all raw process/worker reports,
platform metadata/statuses, both job logs, merge audits and final API snapshots.
This final update changes only the existing implementation record and its index;
no further production fix, new repair PR, numerical adjustment, sampling change
or threshold change is needed.

The final evidence-only snapshot is archived on
`agent/dal-implementer/dal-387-final-evidence`, based on the tested merge commit.
This keeps PR #605's approved head intact while publishing the post-merge record;
its exact archive commit is linked in the final issue handoff.
