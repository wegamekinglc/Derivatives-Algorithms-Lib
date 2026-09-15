# DAL-204 F7 independent testing

## Result

**PASS for the independently tested Linux scope. No actionable product failures found.** The final implementation builds and loads with the actual wheel pin, pybind11 **2.11.1**, on Python **3.9.25** and **3.13.9**. Both standalone wheels pass all 195 F7 cases and all other available Python tests. Native regression, installed C++ consumers, the complete example, and focused ASan/UBSan checks also pass.

This is the S3 test result, not a merge approval. Windows, manylinux portability, intermediate Python versions, and alternate AAD backends were not independently executed here. The wheel suites each skip one unrelated, unpublished quote-risk test fixture; details follow.

### Source and delivery identity

- Issue: DAL-240; parent: DAL-204.
- Existing draft PR: https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/375
- Shared branch: `feature/dal-204-python-fix-settings`.
- Tested HEAD: `607584aaeba0596edf8e384a8137e5e518a3ddaa`.
- Tested tree: `d9dfc7b593681dcea3d8389f6882e9e66fae532e`.
- F6 ancestor: `a98bf9b07e9bd0fee23faa4cc9edca737f709654`; ancestry check passed.
- Local checkout, remote branch, and GitHub PR head matched before testing and again before delivery. This task used its independent Multica checkout, `agent/dal-tester/89410784e62c`.
- Accepted F7 API note SHA256: `d158902e5fae1def90ae844ab1e70eaec0a9d62b80499a06c135f11c6f21b605`.
- The example correction evidence downloaded through Multica matches `f3676a0ce5649803bf6297f9121f4522a86f330337765835c5b27b6a516a01b6`. Only its assertion-free negative probe was reused; all results below used newly built modules.

**Running existing tests:** the suites and fixtures below were executed without modification. **Authoring tests:** no repository tests added; local supplementary probes cover NumPy integer conversion, invalid aliases, and cross-language comparison. **Repairing failures:** no product or test repairs. Initial configuration failures were missing checkout submodules; initialization resolved them. Their failed commands remain in the evidence.

The only repository write is this report. It is delivered as an attachment for the existing PR; there is no new commit, push, PR, product change, build/CI change, or test change in this run. The parent can incorporate the exact report during the next authorized serial update.

## Fresh environment and module provenance

Linux x86_64 / WSL2 `5.15.167.4-microsoft-standard-WSL2`, glibc 2.43; GCC 15.2.0; CMake 4.2.3; C++17; native **AADET** backend. Release uses `-O3 -DNDEBUG`; the sanitizer build uses `-O1 -g0 -DNDEBUG`, address+undefined instrumentation, and interprocedural optimization disabled. Native builds use Unix Makefiles; wheel builds use Ninja 1.13.2 and scikit-build-core 1.0.3.

| Isolated environment | Python | pybind11 | NumPy | pytest |
| --- | --- | --- | --- | --- |
| `dal-python/.venv` | 3.9.25, uv-managed CPython | 2.11.1 | 2.0.2 | 8.4.2 |
| task-local `evidence/venv313` | 3.13.9, Anaconda CPython | 2.11.1 | 2.5.3 | 9.1.1 |

No host packages were replaced. Both environments' 31 pybind11 headers match the checked-out upstream v2.11.1 gitlink `8a099e44b3d5f85b20f05828d919d2332a8de841` byte for byte. `environment39.log` and `environment313.log` record versions, paths, all gitlinks, and this comparison. Minimal compile/load probes also pass, but are not used in place of the real DAL builds.

`run_python.py` imports the actual extension, rejects an unexpected module directory, records source SHA/tree, module SHA256, Python/package versions and `ldd`, then invokes pytest in that same process. The full native CTest independently runs the registered `python -m pytest .../dal-python/tests -v` command. Full CTest output, including its Python 597-case result, is retained.

| Tested module | Location relative to repository/task workdir | SHA256 |
| --- | --- | --- |
| Native Python 3.9 | repository `build/Release-linux/dal-python/dal/_dal.cpython-39-x86_64-linux-gnu.so` | `624b36b8fbba3674a4bd1e6a892f5e1edcb60d59815695e469a30aa992790420` |
| Installed cp39 wheel | repository `dal-python/.venv/lib/python3.9/site-packages/dal/_dal.cpython-39-x86_64-linux-gnu.so` | `42bc9389441fd440d42c5b85b3d49128a12845b6c7a6e378471370e249181965` |
| Installed cp313 wheel | task `evidence/venv313/lib/python3.13/site-packages/dal/_dal.cpython-313-x86_64-linux-gnu.so` | `cd0bac5e72a36ced892d81e3135d24e182bcb8c7551d251ce2f7e80c7227ce24` |
| Sanitized Python 3.9 | repository `build/s3-asan/dal-python/dal/_dal.cpython-39-x86_64-linux-gnu.so` | `2adb887220f0ea0c924123388682a84d829612468a63b1e6a529fd42b640be89` |

The normal extensions link system libstdc++, libm, libgcc and libc; DAL core/public are static. The sanitizer extension additionally loads libasan and libubsan. Exact absolute paths and loaded libraries are in the corresponding test logs. `source-sha256.json` records 771 source/test/build input hashes; `build-metadata/` preserves caches and compilation commands. `wheel-identities.json` records wheel metadata and every packaged member hash.

## Executed tests

All successful commands below exited **0**. Each command's exact argument vector, cwd, timestamp, exit code and duration is in its companion JSON/log. Names refer to files inside the evidence attachment.

| Verification | Fresh result | Evidence |
| --- | --- | --- |
| Canonical Linux build/install/test script, Python 3.9 and pinned pybind11 | **1778/1778 CTests**, 19.86 s; embedded Python **597 passed** | `full-linux39-final.log`, `test_output_fresh.txt`, `ctest39-LastTest.log` |
| Full Python 3.9 suite with same-process module identity | **597 passed**, 18.79 s | `python39-full.log/.xml` |
| Focused Python script/settings/value/snapshot regression | **255 passed**, 2.74 s | `python39-related.log/.xml` |
| Standalone cp39 wheel build against installed DAL::public | Built, installed and loaded; **596 passed, 1 skipped**, 8.66 s | `wheel39.log`, `wheel-install39.log`, `wheel39-tests.log/.xml` |
| Standalone cp313 wheel build against installed DAL::public | Built, installed and loaded; **596 passed, 1 skipped**, 8.93 s | `wheel313.log`, `wheel-install313.log`, `wheel313-tests.log/.xml` |
| ASan+UBSan build of current core/public/Python and focused regression | **255 passed**, 4.03 s; no sanitizer diagnostics | `configure-sanitizer-ready.log`, `build-sanitizer.log`, `sanitizer-tests.log/.xml` |
| Separate installed C++ consumers, legacy/typed and historical AAD | **2/2** | `consumer-configure.log`, `consumer-build.log`, `consumer-tests.log` |
| Full FIX example, normal and optimized Python | Passed on native cp39 and both installed wheels; PV `260.00000000000006`, d_SCALE `80.0` | `example39*.log`, `wheel39-smoke*.log`, `wheel313-smoke*.log` |
| Five corrupted example outputs under `python -O` | All rejected: PV, risk, extra key, product schema, valuation schema | `example39-negative-*.log` |
| Supplementary Python/C++ comparison and conversion probes | Passed on native cp39 and installed cp313 | `cross-language39.log`, `wheel313-supplementary.log` |

The 195 F7 cases comprise `test_script_settings.py` **117** and `test_fix_valuation.py` **78**, all passing under native cp39, both wheels, and the sanitizer build. XML-derived counts are in `summary.json`.

Each wheel's only skip is `test_joint_quote_risk.py:177`: `_dal_quote_risk_test` is a private monorepo fixture which standalone wheels do not build or ship. It passes in the complete native Python run. No F7, script/value, or snapshot case was skipped. This is reported as 596+1, not as 597 wheel passes.

### Reproduction commands

Run from the repository. `REPO` below means its absolute directory; the capture logs contain the actual expanded paths. `PY39=dal-python/.venv/bin/python`, `PY313=../evidence/venv313/bin/python`.

```bash
git submodule update --init --recursive
env NUM_CORES=8 ADDITIONAL_CMAKE_FLAGS="-Dpybind11_DIR=$REPO/dal-python/.venv/lib/python3.9/site-packages/pybind11/share/cmake/pybind11" \
  bash ./build_linux.sh --python 3.9
env PYTHONPATH=build/Release-linux/dal-python "$PY39" ../evidence/run_python.py \
  build/Release-linux/dal-python dal-python/tests -q --junitxml=../evidence/python39-full.xml
```

The build script was captured through `tee test_output.txt` with pipefail after deleting the old bootstrap output, as required by `run-tests.md`. Its exact wrapper is in `full-linux39-final.json`; the fresh file was copied into the attachment. Benchmarks were disabled and excluded from CTest.

Each wheel was built with the following command, substituting `39` or `313` in the output/build directory and the corresponding isolated Python/pybind directory:

```bash
env CMAKE_BUILD_PARALLEL_LEVEL=4 "$PY313" -m build --wheel --no-isolation \
  --outdir ../evidence/wheels313 -Cbuild-dir="$REPO/build/s3-wheel313" \
  -Ccmake.define.DAL_INSTALL_PREFIX="$REPO/build/stage/Release-linux" \
  -Ccmake.define.pybind11_DIR="$REPO/../evidence/venv313/lib/python3.13/site-packages/pybind11/share/cmake/pybind11" dal-python
uv pip install --python "$PY313" ../evidence/wheels313/dal_python-2026.9.5-cp313-cp313-linux_x86_64.whl
env PYTHONPATH= "$PY313" ../evidence/run_python.py \
  ../evidence/venv313/lib/python3.13/site-packages dal-python/tests -q -rs \
  --junitxml=../evidence/wheel313-tests.xml
```

`--no-isolation` here reuses the already isolated task venv with both exact build pins installed; it does not substitute a host pybind11. The standalone CMake logs explicitly confirm installed `DAL::public` linkage and pybind11 2.11.1. Wheels are local `linux_x86_64` artifacts, not repaired manylinux wheels. The workflow's cp39/cp313 PR selection and installed-public construction were inspected; the manylinux container, `build_native.py` distribution build, Windows wheel workflow, and release upload were not executed.

Sanitizer configuration/build:

```bash
cmake --preset Release-linux -S . -B build/s3-asan \
  -DDAL_BUILD_PYTHON=ON -DDAL_CPP_BUILD_TESTS=OFF -DDAL_PUBLIC_BUILD_TESTS=OFF \
  -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF \
  -DDAL_ENABLE_SANITIZERS=address,undefined '-DCMAKE_CXX_FLAGS_RELEASE=-O1 -g0 -DNDEBUG' \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
  -DPython3_EXECUTABLE="$REPO/dal-python/.venv/bin/python" \
  -Dpybind11_DIR="$REPO/dal-python/.venv/lib/python3.9/site-packages/pybind11/share/cmake/pybind11"
cmake --build build/s3-asan --target _dal _dal_quote_risk_test -j 4
env PYTHONPATH=build/s3-asan/dal-python \
  LD_PRELOAD=/usr/lib/gcc/x86_64-linux-gnu/15/libasan.so:/usr/lib/gcc/x86_64-linux-gnu/15/libstdc++.so \
  ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  "$PY39" ../evidence/run_python.py build/s3-asan/dal-python \
  dal-python/tests/test_script_settings.py dal-python/tests/test_fix_valuation.py \
  dal-python/tests/test_script.py dal-python/tests/test_value.py dal-python/tests/test_api.py \
  dal-python/tests/test_xccy_resettable.py dal-python/tests/test_curve_pricing.py \
  -q --junitxml=../evidence/sanitizer-tests.xml
```

The consumer uses the attached `consumer/CMakeLists.txt`, `find_package(dal-public CONFIG REQUIRED)`, and only `DAL::public`. Exact configure/build/CTest commands are in `consumer-*.json`. Example commands and all five negative cases are individually captured; no assertions were removed or tolerances changed.

## Acceptance mapping

All native cases below are present and passed in the fresh 1778-case run. The 23 public `ScriptApiTest`, `ScriptContractTest`, and `ScriptArchiveTest` cases and relevant core case names are extracted in `summary.json`; they are not inherited S2 results.

| Contract | Executed coverage and independent expectation |
| --- | --- |
| T28 high/low signatures and compatibility | `test_api_keyword_boundaries_and_settings_types`, `test_legacy_calls_and_default_deduplication`, existing `test_api.py`, `test_script.py`, `test_value.py`; high `events_dates` / low `dates`, Cell passthrough, str/String events, old 3–8 positional calls and keywords, default None, new keyword-only APIs/constructors, unknown/duplicate kwargs and whole-settings type rejection. Installed C++ consumer also exercises old 3–8 calls and typed settings. |
| T28 types, copies and errors | `test_settings_values_and_copy_protocol`, transactional setter matrix, strict booleans, date type/reset/invalid-date cases, exact policy strings/enum/unnamed enum, Unicode/NUL, invalid RNG/smoothing and binding shape/semantics. Copy/deepcopy are independent settings values; dict/date getters detach; snapshot remains a native immutable handle. Bad setters preserve previous values. Error assertions retain identifiers, fields and constraints. |
| T28 path validation | Both native Value entries reject bool/enum/float/None/string, zero/negative/overflow, and a 5001-digit integer without losing context. Valid `__index__` and INT_MAX reach the next null-product precondition without allocating billions of paths. Supplementary probes confirm NumPy int32/int64. A Python dict cannot retain repeated identical keys; case-distinct `spot`/`SPOT` remains observable and triggers native DuplicateModelBinding. |
| T07 today policy | `test_today_policy_independent_oracles`: 48 combinations of BS/Dupire, three RNGs, BB, tree/compiled and double/AAD. Explicit D=2026-09-12 differs from global D; Model returns 100, RequireHistorical returns 80, missing history fails. C++ `ScriptApiTest.TestTodayPolicy` and preparation/adapter cases verify positive read-count controls and zero history for Model. |
| T15 refresh, snapshot and no cache | Python `test_historical_aad_batches_and_repricing` runs explicit 80→90→80 snapshots and 1/257/8193 paths. Actual **global** refresh is independently covered by `ScriptApiTest.TestRepricingAndExplainNeverCache`, `TestExplainDoesNotFreezeGlobalMarket`, `ScriptFixingPreparationTest.TestRepricing`, and `TestExplicitSnapshotNeverFallsBackToPresentGlobalHistory`: global80→90→old snapshot80, old plan remains80, repeated prepare counts, Explain80 then Value90, explicit empty snapshot fails despite present global data. Python input-map mutation does not alter a constructed snapshot. |
| T18 historical AAD | Python historical tests use D=09-12, H=09-11 midnight, P=09-22, SCALE=2 and r=.05. Independent PV=`160*exp(-r*10/365)`, d_SCALE=`80*exp(-r*10/365)`, d_rate=`-(10/365)*PV`, spot/vol zero and exactly the permitted PV/d_ risk keys; tree/compiled and batch sizes pass. Native `ScriptPastReplayTest` and `ScriptContractTest.TestArchiveStaysContractOnlyAfterExplainAndAadRepricing` provide independent analytic and tape-lifetime coverage. |
| Future observation / exact and fuzzy | Python `test_future_observation_and_payment_have_distinct_dates` uses an analytic deterministic drift/discount oracle at F=09-15 and P=09-22, ignores future snapshot999, and tests duplicate F cancellation. `test_exact_fuzzy_and_hard_history_have_separate_oracles` checks exact PV160, fuzzy PV120/d_SCALE60/d_K−800, same-width finite difference, and hard-history PV160/d_K0, each in tree/compiled. Native `ScriptCompiledParityTest.TestIndexFixingsSamePathAndArtifactLifetime` assigns F120/P999 and checks fixed expected values; `TestIndexFixingsAnalyticPathRisks` separately checks both evaluators against analytic path risks. |
| T27 legacy | Original future SPOT fixtures run unchanged; bound historical SPOT/FIX deduplicates to one request. Historical unbound SPOT and mixed unbound future SPOT/FIX preserve their distinct errors. Native `TestDefaultSpotDeduplicatesAndLegacyGuards` and fresh supplementary Python probes reject parameterized SPOT and FIX(). |
| T29 archive and diagnostics | Native `ScriptArchiveTest.TestIndexVersioning` reads real v1 golden through the registry, writes v2, preserves raw/default/macro/delivery identity, and accepts absent/empty default. `ScriptContractTest.TestArchiveStaysContractOnlyAfterExplainAndAadRepricing` proves no runtime pollution after real valuation. Python checks high dict/low raw JSON, schemas, exact request/history/model slot/node IDs and inverse FX value1.25. Native `TestDescribeContractOnly`, `TestExplainPreparation`, interleaved-ID and counting-model cases establish Describe no market reads, Explain independent default preparation with no worker/path generation, and real request-use alignment. |
| GIL, ownership, recovery | Read `script.cpp`, `scriptsettings.hpp`, `value.cpp`, high-level wrappers and public forwarding. Python input/event/settings/handle copies precede `gil_scoped_release`; workers receive native data, not callbacks or dicts. `test_settings_are_copied_before_gil_release` mutates caller settings from a live Python thread while pricing still returns PV160/d_SCALE80. Existing GIL/date-lock and concurrent pricing tests pass; expired/empty/failure recovery cases also pass. |
| Full example | Real model, initialization, unquoted index literals and DateTime(date,0); normal and optimized execution pass. Five local negative probes confirm PV abs_tol2.6e−10, d_SCALE abs_tol1e−10, rel_tol0, PV/d_ keys, and the two exact schemas remain enforced under optimization. |

Deterministic PV tests retain `1e-12*max(1,abs(expected))`, analytic AAD `1e-10`, fixed-path comparisons `1e-8`. Fuzzy finite differences retain the same path and width; hard switching points are not treated as smooth derivatives. No assertion or tolerance was weakened.

The supplementary C++/Python comparison constructs equal snapshot contents in separate native processes, verifies the independent PV160/d_SCALE80 oracle, and compares the full default explanation. It does not claim one pointer is shared across processes, and does not replace the independent oracles above. Python has no public global-fixing writer or product archive API; those exact boundaries are tested by current C++ fixtures instead of new public APIs.

## Findings, inherited evidence and limits

- **No actionable F7 failure found.** The previously unverified pybind11 pin now has independent native/installed-wheel evidence at both supported Python endpoints on this Linux host.
- Bootstrap failures: missing googletest, then XAD example/Machinist sources; logs `full-linux39.log`, `full-linux39-ready.log`, `configure-sanitizer.log` retain exit1. `git submodule update --init --recursive` populated the pinned sources; subsequent builds passed without code/config changes. The wheel linker emitted a serial-LTO warning but succeeded.
- Wheel tests each skip the one unpublished quote-risk fixture as described above. No compatibility or test failure required a baseline counterexample or implementer repair route.
- Sanitizer evidence covers the 255 affected Python cases with freshly instrumented DAL libraries/binding. Host-Python leak detection was disabled; this is not a leak audit or a full sanitized C++ run.
- Python 3.10/3.11/3.12, Windows/MSVC/XLL, macOS, manylinux container portability, and CoDiPack/Adept/XAD were not independently run. The 29 Linux portable Excel cases are not XLL evidence.
- S2's reported CoDiPack597/native1778/sanitizer255/consumer2 at `fd9c42d07b54506366652f6559ddf1e0fff093e8` and pybind11 3.0.4 remain historical implementer evidence, referenced through the accepted implementation report. They are not counted as this tester's executions. The old binary was never loaded for these results.
- One Multica CI snapshot was read: fetched **2026-09-15 06:59:41 UTC**, 46 checks, 43 passed, 3 pending, 0 failed, PR draft. No check watch/poll or CI completion claim; no Windows result is inferred from this rollup. No performance sidecar or gate was introduced.

S3 testing is ready for parent acceptance. DAL-241 documentation and DAL-242 mandatory review remain the parent's serial next stages. This run does not merge, add final Closes, or start F8. The report and fresh evidence are attached to the sole final DAL-240 comment; parent active-run check/rerun follows delivery.
