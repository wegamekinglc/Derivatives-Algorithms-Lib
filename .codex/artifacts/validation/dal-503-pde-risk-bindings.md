# DAL-503 S4 owning PDE risk projections

Implementation source: `7b3e5e0b309867c6d388d7c2c8f483fba36e8053`,
on baseline `cf0895477e6b9ddefdd2ef41865d3213c24aaeec` (merged S2/S3).
This record accompanies the same dedicated S4 branch/PR. Subsequent documentation
commits do not change that implementation. The
[identity manifest](dal-503-pde-risk-bindings-identity.json) binds source files,
dependencies, configurations, linked libraries and executables.

Local projection checks pass. **Windows/MSVC/XLL remains NOT EXECUTED**;
this is an implementation handoff for focused review, not full feature
acceptance. S5 examples/performance/release work remains separate.

## Implementation and scope

Public exports are aliases in `dal-public/src/earlyexercisepderisk.hpp`.
Python registers the settings/request, all six enums and every owning result,
plan, record, scenario, stencil and execution field. Inputs are strict;
constructor settings are keyword-only, getters detach, properties are readonly,
copy/deepcopy owns values, and evaluation releases the GIL after input copying.

Excel adds three storable value types and all 22 frozen factories/getters,
strict scalar/bump tables, detached handles, atomic handle replacement and
44 Machinist-generated registration/help files. Optional values remain blank;
unavailable scalar access raises and risk handles reject archiving.

No numerical source, core enum, S3 reference tool, dependency pin, existing
example or target is changed. New installed-header smoke consumers belong in
`tests/installed-consumer`, not in the examples reserved for S5. A short
current-state projection guide and API status update accompany the bindings.
There is no schema, threshold or numerical-method deviation.

## Environment and applicability

Linux x86_64; GCC 15.2.0, CMake 4.2.3, C++17 Release, AADET native backend,
Eigen enabled and native architecture tuning OFF. Core is a shared library;
public is static. Python is CPython 3.13.9 with **selected system pybind11
3.0.4**. The pinned pybind11 2.11.1 submodule is present but is not the selected
header package. All submodule SHAs and exact flags are in the manifest.
Benchmarks were configured ON by the existing default; no benchmark target or
performance sample was requested or executed.

The numerical qualification is reused from the
[final S3 entry](dal-502-pde-risk-final.md), including its bounded BBS scope,
producer identities and historical FAIL/INCOMPLETE records. The complete
`dal-cpp/dal` source tree and S3 tools retain their baseline Git objects.
New S4 binaries retain their own hashes; they do not impersonate S3 producers.
N1–N12 are not re-executed here, and historical #602 evidence is not substituted.

## RED, GREEN and harness corrections

- Public RED: `g++ -std=c++17 -I. -Idal-cpp -fsyntax-only dal-public/tests/test_early_exercise_pde_risk.cpp`
  failed because the new facade header did not exist. After alias export,
  syntax checking and `PublicEarlyExercisePdeRiskTest.TestAliasesAndOwningEvaluation`
  passed.
- Python RED: the focused risk test failed with missing
  `EarlyExercisePdeRiskRequest_` before registration. After registration,
  the initial 110 risk cases passed. A separate keyword-error test then failed
  because pybind's default `risk_value` argument error lacked the required
  identifier. Explicit argument parsing made that test pass.
- Excel RED: the new focused translation unit failed on its missing binding
  header. Factories, settings and projections made the initial contract tests
  pass. Separate semantic-coordinate and NUL risk-ID tests exposed missing
  row/column and argument context; each failed before its production fix and
  passed afterward.
- One Excel comparison helper initially interpreted a native enum's
  `const char*` string as Boolean. An explicit string overload corrected the
  harness; the production string assertion was retained.
- The first Python tape observer was linked into pybind's hidden-visibility
  module. Its `RecordingScope` failed before the callback with
  `tape must be the calling thread's default tape`. Moving the observer to
  a separate test module with core visibility made it share the actual
  linked core library's inline TLS tape. The six native comparison/tape
  checks then passed. No core source or tape policy was changed.

## Reproduction and final selected verification

Commands run from the repository root; `python3` denotes the selected
CPython interpreter above. Raw command outputs are delivered with the issue
handoff archive, including earlier failed checkpoints.

```bash
git submodule update --init --recursive
cmake -S . -B build/s4 -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-15 \
  -DDAL_ENABLE_NATIVE_ARCH=OFF -DDAL_CPP_BUILD_EXAMPLES=ON \
  -DDAL_PUBLIC_BUILD_EXAMPLES=OFF -DDAL_BUILD_PYTHON=ON \
  -DPython3_EXECUTABLE="$(command -v python3)" -DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON
cmake --build build/s4 --target _dal dal_public_tests dal_excel_portable_tests -j 6
ctest --test-dir build/s4 \
  -R '^(PublicEarlyExercisePde|ExcelEarlyExercisePde|dal_binding_boundary)' --output-on-failure
python3 -S dal-python/tests/run_build_pytest.py build/s4/dal-python \
  /home/wegamekinglc/anaconda3/lib/python3.13/site-packages \
  dal-python/tests/test_early_exercise_pde_risk.py dal-python/tests/test_early_exercise_pde.py -q
cmake --build build/s4 --target dal_check_generated -j 4
```

Results: **130 Python PASS**, including 119 new risk cases and 11 existing
price cases; **17 CTest PASS**, including eight new portable Excel cases,
the new public test, existing price contracts and the binding boundary check.
The Windows-only raw risk-ID cell test is outside these Linux counts.
Machinist generation/check succeeds without tracked or untracked drift.
Documentation checks pass for 138 Markdown files; `git diff --check` passes.
Raw archive: `dal-503-bindings-evidence.tar.gz`, 59626 bytes, SHA256
`7a715733eda770dc9c4e623169a6226bb6dce05e6f918e34fe701f01e628cf68` (55 log files plus build identity).

After the final Windows-only input-markup change, the two affected Excel
translation units were also rebuilt using their CMake-generated make rules
and relinked using `cmake -E cmake_link_script`. The complete target build
above is the final build check, not a substitution of stale executables.

## B-control coverage

- B1: source/list mutation and release, public copy/move, Python copy/deepcopy,
  detached nested vectors/records/requests, readonly properties and repeated
  reads with unchanged work/solve/payload counters.
- B2: Excel recording/ready/legacy graphs and Python native recording/ready
  graphs; node/event/argument/derivative counts, channel mode/width, seeds and
  subsequent Reverse. Distinct concurrent requests agree with serial calls.
- B3: all flag/real/count families, finite/range/sequence/typed/keyword errors
  with exact prefixes/paths. Barrier-coordinated GIL heartbeat uses a long
  Python switch interval to require progress during native evaluation.
- B4: default blanks, strict canonical/duplicate/unknown keys, Boolean/numeric/
  date cell distinction, exact integer bounds, independent default bump columns,
  holes, length validation, all table headers/ordering, unavailable blanks,
  scalar errors, atomic recovery and archive rejection.
- B5: all plan/record/scenario/execution fields compared directly with core in
  Python and every Excel table cell against native records, using one linked
  core library. COMPLETE/PARTIAL/BASE_FAILED/WORK_LIMIT and two carry segments
  are covered. Same-build comparisons use exact equality, stronger than the
  permitted scalar tolerance; they are schema/arithmetic checks, not new
  continuous-model accuracy qualification.
- B6: full/one-less scenario and retained/workspace budgets, shared spot
  scenarios, two carry segments, unsigned conversion rejection, zero-solve
  success and exact work caps 1326/1325/390/78/77. Work totals match scenarios;
  unrun rows carry zero observed counts and no solver observations.
  Unchanged core checked arithmetic, large-layout, smaller underlying limits
  and capacity proofs retain their accepted S2/S3 scope.
- B7: repeated malformed/preflight failures followed by success, real
  LCP_RESIDUAL_LIMIT followed by its successful control, global interruption
  and projection of the accepted private Rate[0]:F+ failure seam. Completed
  independent risks and diagnostics survive.
- B10: all new registration/help files are generated and drift-checked.
  Windows raw-cell test is authored; actual Windows/MSVC/XLL build,
  registration/help and worksheet COMPLETE/PARTIAL remain unexecuted.

## Independent core and installed consumers

```bash
cmake -S . -B build/s4-core-only -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++-15 -DDAL_ENABLE_NATIVE_ARCH=OFF \
  -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_PYTHON=OFF -DDAL_BUILD_EXCEL=OFF \
  -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DDAL_CPP_BUILD_TESTS=OFF -DDAL_CPP_BUILD_EXAMPLES=ON
cmake --build build/s4-core-only --target dal_cpp european_fd european_aad_fd -j 6
ctest --test-dir build/s4-core-only -R '^european_(aad_)?fd$' --output-on-failure
cmake -DCMAKE_INSTALL_LOCAL_ONLY=TRUE -DCMAKE_INSTALL_PREFIX="$PWD/../s4-stage" \
  -P build/s4-core-only/dal-cpp/cmake_install.cmake
cmake -DCMAKE_INSTALL_LOCAL_ONLY=TRUE -DCMAKE_INSTALL_PREFIX="$PWD/../s4-stage" \
  -P build/s4/dal-public/cmake_install.cmake
cmake -S tests/installed-consumer -B build/s4-consumer -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++-15 -DCMAKE_PREFIX_PATH="$PWD/../s4-stage"
cmake --build build/s4-consumer --target dal_installed_core_pde_risk dal_installed_public_pde_risk -j 4
ctest --test-dir build/s4-consumer -R pde_risk --output-on-failure
```

Fresh independent core library and both existing PDE examples build;
**2/2 example tests PASS**. Both installed consumers build and **2/2 PASS**.
Their include flags contain only the staged include directory; `ldd`
resolves core from the staged library directory. Main, core-only and installed
core library SHA256 all equal
`6e4279ffd6b7ef31835951471eeba9bd68381e66e58c3f452ec7e7aef6f63dae`.
The component-owned S5 public example/full B9 delivery remains separate.

## Remaining gates and exclusions

The Linux host has no MSVC/Excel runtime; CMake explicitly skips XLL.
Portable success does not replace B10. Independent focused review and actual
head CI remain pending at publication; no merge or auto-merge is enabled.

P1/P4 numerical callers are unchanged; applicable accepted S3 evidence is
reused. P2/P3 risk/projection performance, full formal examples and remaining
S5 release delivery are not executed here. Full unrelated unit/benchmark
matrices are not repeated for an additive projection change.
