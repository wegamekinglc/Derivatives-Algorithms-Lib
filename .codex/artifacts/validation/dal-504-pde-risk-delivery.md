# DAL-504 S5 owning PDE risk delivery

S5 supplies component-owned examples, standalone core/installed consumer
checks and scoped [performance evidence](../performance/dal-504-pde-risk.md):
P1/P2 pass both frozen rounds and P3 preserves native counters/payload.
Independent focused review and coordinator acceptance remain required;
this author receipt does not claim merge or full-feature acceptance.

Baseline: `1216e03aaf71071e8e36aef1f24082a1255380f8` (merged S4).
Executable/tool content: `66702eb0dfa1077a960242e97f553401fa421d9d`.
Documentation successors are not new executable producers.
The [identity manifest](dal504/identity.json) binds exact source subtrees,
dependencies, configurations, compiler, platform, commands and binary hashes.
The [raw archive manifest](dal504/raw-archive.json) binds logs, caches,
compile/link commands, all gate stdout/stderr and replayable native timing
binaries delivered with the issue handoff.

## Scope and design

- `pde_perf --risk CASE MODE REPEATS` runs the evaluator or an independent
  scenario loop. The reference constructs markets, copies inputs,
  previews/builds a common plan, retains the base/plan and independently
  extracts the same scenario diagnostics. It does not call the risk
  evaluator or its difference/availability assembly. Preflight compares
  actual grids, dated nodes, chronological steps, successful scenario
  fields, prices, statuses, capacities, solves and work.
- The existing default/`--early-exercise` workloads remain intact.
  Selected risk modes are separate from the formal European allowlist
  verdict. No theta, solve, AAD, planner, risk-assembly or binding product
  source changes are made.
- New core program `early_exercise_pde_risk` lives under
  `dal-cpp/examples/methods/` with core dependencies only. Separate public
  program `early_exercise_pde_risk_public` calls the facade and shares the
  core presentation helper in the permitted direction. Both have CTest
  and install entries; existing example paths/targets are retained.
- Python `019.early_exercise_pde_risk.py` and Excel
  `012.early_exercise_pde_risk.md` show full-risk tables and PARTIAL handling.
  Every example checks row availability. Method/language/component guides
  and CHANGELOG cover finite-scale Gamma, decimal units, native carry
  partials, cumulative budgets, ownership and caller tape.

The existing benchmark executable is extended rather than adding a formal
allowlist entry. Standalone public static libraries explicitly enable PIC
for Python consumption. Frozen numerical/resource/interface semantics remain.

## N1–N12 and B1–B10 exits

The [S3 final map](dal-502-pde-risk-final.md#final-n1n12-evidence-map) owns
**12 PASS / 0 FAIL / 0 INCOMPLETE**, under its bounded adopted references.
Every N row retains the linked raw/reference/resource records and original
producers. `git diff` from production source
`015885814288b8f884594430b03a557c4bdae865` is empty for `dal-cpp/dal`.
S5 performs no independent-reference or refinement rerun.

| Exit   | Evidence and applicability                                                                                                |
|--------|---------------------------------------------------------------------------------------------------------------------------|
| N1–N12 | Each S3 final-map row retains its adopted scope and original product/reference/audit identity.                            |
| B1     | S4 owning/copy/detached/lifetime checks; S5 P3 also checks repeated reads with fixed native counters/payload.             |
| B2     | Accepted S2/S3 core and S4 recording/ready/legacy/multi-channel/concurrency receipts; product source unchanged.           |
| B3     | S4 strict Python inputs, readonly records and barrier-coordinated GIL evidence; S5 runs one readonly regression.          |
| B4     | S4 canonical Excel inputs, tables/blanks/errors, atomic handles/archive rejection and native-export repair.               |
| B5     | S4 same-library all-field equivalence for COMPLETE/PARTIAL/BASE_FAILED/WORK_LIMIT and two carry segments.                 |
| B6     | Accepted exact scenario/work/byte/overflow/underlying-limit checks; P2 records capacities within envelopes.               |
| B7     | Accepted malformed/preflight/numerical/global-work and private failure-seam recovery evidence.                            |
| B8     | Fresh S5 core build: public/Python/Excel/portable OFF, examples ON; old PDE targets and new core example build/run.       |
| B9     | Separate public example, CTest/install, both installed examples and consumers; installed headers/imported libraries only. |
| B10    | S4 generation check and actual Windows/MSVC/XLL/worksheet execution, retained at producer `20401a29`.                     |

B1–B7/B10 details are in the [S4 receipt](dal-503-pde-risk-bindings.md#b-control-coverage)
and [manifest](dal-503-pde-risk-bindings-identity.json). `git diff` from
Windows producer `20401a29539606384723ae1b1cce9dd6708bdf1d` is empty for
core/public/Python/Excel product sources and generated Excel stubs.
Windows B10 producer remains **dal-tester / `20401a29`**, XLL SHA256
`d1cfc3dff6ecefbac8c77578d2fd399c06f8bcc9daab4711b3673d0a528ec945`.
Its 10 native risk tests, 22 registrations/help entries and actual
COMPLETE/PARTIAL worksheet formulas are reused, not relabeled as S5 runs.
The Linux Excel probe measures typed spill construction, not COM latency.

## RED and GREEN

Initial focused compile:

```bash
g++-15 -std=c++17 -Idal-cpp/externals/googletest/googletest/include \
  -Idal-cpp -fsyntax-only dal-cpp/tests/math/pde/test_pde_risk_benchmark.cpp
```

RED: missing `../../../benchmarks/pde_risk_perf/reference.hpp`, exit 1.
After implementation, the direct focused executable passes 2/2 tests.
An early input retained the middle Bermudan date while asserting the
maturity-only B6 arithmetic; the input was corrected to frozen B6 while
retaining **1326 work / 34 solves** assertions. No threshold was loosened.

D-profile integration then failed with `reference step mismatch`:
internal rollback steps use execution order; the public plan is chronological.
The adapter now reverses the internal index and additionally checks exact
source/target times and indices. Production planning is unchanged.
Final core CTest selection: **6/6 PASS**.

```bash
cmake --preset Release-linux -S . -B build/s5-core -DCMAKE_CXX_COMPILER=g++-15 \
  -DBUILD_SHARED_LIBS=OFF -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_PYTHON=OFF \
  -DDAL_BUILD_EXCEL=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF \
  -DDAL_CPP_BUILD_BENCHMARKS=ON
cmake --build build/s5-core --target dal_cpp_tests pde_perf \
  early_exercise_pde_risk european_fd european_aad_fd -j 4
ctest --test-dir build/s5-core \
  -R '^(PdeRiskBenchmarkTest\.|early_exercise_pde_risk|european_fd|european_aad_fd|pde_risk_perf_identity)' \
  --output-on-failure
cmake -DCMAKE_INSTALL_LOCAL_ONLY=TRUE -DCMAKE_INSTALL_PREFIX="$PWD/../s5-stage" \
  -P build/s5-core/dal-cpp/cmake_install.cmake
cmake --install build/s5-core/dal-cpp/examples/methods/early_exercise_pde_risk \
  --prefix "$PWD/../s5-stage"

cmake -S dal-public -B build/s5-public -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++-15 -DCMAKE_PREFIX_PATH="$PWD/../s5-stage" \
  -DDAL_PUBLIC_BUILD_TESTS=OFF -DDAL_PUBLIC_BUILD_EXAMPLES=ON \
  -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
cmake --build build/s5-public --target early_exercise_pde_risk_public -j 4
ctest --test-dir build/s5-public -R '^early_exercise_pde_risk_public$' --output-on-failure
cmake -DCMAKE_INSTALL_LOCAL_ONLY=TRUE -DCMAKE_INSTALL_PREFIX="$PWD/../s5-stage" \
  -P build/s5-public/cmake_install.cmake
cmake --install build/s5-public/examples/early_exercise_pde_risk --prefix "$PWD/../s5-stage"

cmake -S tests/installed-consumer -B build/s5-consumers -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++-15 -DCMAKE_PREFIX_PATH="$PWD/../s5-stage"
cmake --build build/s5-consumers --target dal_installed_core_pde_risk dal_installed_public_pde_risk -j 2
DAL_NUM_THREADS=4 ctest --test-dir build/s5-consumers -R 'pde_risk$' --output-on-failure
```

Public example: **1/1 PASS**. Installed consumers: **2/2 PASS** after
final PIC relink. Both installed examples run and their complete output
matches. Consumer flags contain only `s5-stage/include`; links use installed
`libdal_cpp.a` and, for the public consumer, `libdal_public.a`.

The initial standalone public archive lacked PIC; the Python shared link
failed with `R_X86_64_TPOFF32`. Its log is retained. Public reconfiguration,
reinstall and affected Python/consumer/Excel relinks pass. This is a
configuration correction, not a solver repair.

Standalone Python Release links installed public/core archives.
Readonly/detached regression: **1/1 PASS**, 118 deselected. The new example
runs Bermudan COMPLETE, American COMPLETE and boundary PARTIAL. P3 executes
eight Python and eight portable Excel measurements with invariant checks.
Full S3/S4 numerical/three-end matrices are reused, not rerun.

```bash
DAL_NUM_THREADS=4 python3 -S dal-python/tests/run_build_pytest.py build/s5-python \
  /home/wegamekinglc/anaconda3/lib/python3.13/site-packages \
  dal-python/tests/test_early_exercise_pde_risk.py -k readonly_detached -q
DAL_NUM_THREADS=4 PYTHONPATH="$PWD/build/s5-python" \
  python3 -S dal-python/examples/019.early_exercise_pde_risk.py
```

## Stage history and handoff

| Stage        | PR                                                          | Actual merge SHA                           |
|--------------|-------------------------------------------------------------|--------------------------------------------|
| S0 / DAL-499 | #682                                                        | `78d301d35249ef39614bf788a0d306ea63620a29` |
| S1 / DAL-500 | #683                                                        | `ca88add3f553c5250f19015dca1511a2fafd90da` |
| S2 / DAL-501 | #684                                                        | `1b8c92e8f23320b2723628fd37b399455c918e23` |
| S3 / DAL-502 | #685                                                        | `cf0895477e6b9ddefdd2ef41865d3213c24aaeec` |
| S4 / DAL-503 | #687                                                        | `1216e03aaf71071e8e36aef1f24082a1255380f8` |
| S5 / DAL-504 | Dedicated Draft PR on `agent/dal-orchestrator/82cd1c13880a` | Not merged; reviewer handoff               |

DAL-506/#686 documentation integration (`cd90d13e`) remains in the baseline.
No new dispatch, merge or auto-merge is performed. Exact-head CI is reported
as one nonblocking issue snapshot; prior checks do not certify a successor.
Coordinator arranges focused S5 review.
