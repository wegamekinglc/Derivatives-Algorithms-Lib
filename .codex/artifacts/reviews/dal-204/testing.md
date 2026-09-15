# DAL-204 F7 independent testing

## Current result: R1 independent verification, 2026-09-15

**PASS for the tested Linux scope at HEAD `3b57a10c23a9d68f95c3edd3888a4f477f12bb04`, tree `a8e2bfdb09648ee481e44508ba6d61745d13e67d`.** The eight foreign string-enum failures reproduce on both old modules and become eight contextual TypeError rejections on both independently rebuilt wheels. Failed setters retain different previous valid values. Legal settings and shared event/model-binding text remain compatible. No actionable product failure remains in this verification scope.

Draft [PR #375](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/375), branch `feature/dal-204-python-fix-settings`, remains open at that exact head. Product/test commit: `27de3544d4497a77ee2c6d67743f147bbeb65d53`. This report awaits parent acceptance, the documentation decision, and mandatory re-review; it is not merge approval.

### Scope and provenance

**Running existing tests:** the accepted regression suites ran without modifications. **Authoring tests:** no repository tests added; the authenticated reviewer probe was replayed unchanged, with a local StrEnum variant and existing assertion-free example/numeric probes. **Repairing failures:** no product or test repair. One initial probe command had an extra repository prefix and exited 127 before Python started; the corrected invocation and both expected RED runs are preserved separately.

Only this report changes in the repository. No commit, push, PR update, or other repository file change is needed. The parent can incorporate the attached report in the existing serial workflow. The complete previous S3 report is preserved below as history; its earlier PASS did not detect R1 and is superseded by this section.

Authenticated implementation report/evidence hashes match the current issue contract: `6d5de94f7ec4c6c5ef69e1de1405d813335c226e3737109d4cb8cc01b23b4ba0` and `20cca69ae651fee775ea631765066c9182f41d3cc35378602b13bc77a0f61cfd`. All 115 implementation evidence manifest entries were verified. The embedded reviewer evidence matches `a0a541cd7d6cc6a2e211fd3f0ecc4cb9fbe4b157d07ee2e0d4dddf4569c40ca3`; its original probe is the RED/GREEN input. These inherited results are not counted as independent tests.

`input-identity.json` and `source-sha256.json` verify 771 source/test/build inputs against the prior independent S3 inventory. Exactly four inputs differ: `scriptsettings.hpp`, `script.cpp`, `value.cpp`, and `test_script_settings.py`. Core/public sources, CMake inputs, gitlinks, Python wrappers, and the example are unchanged. All **266 installed headers** match current source. Both installed Release static libraries match the original S3 hashes; both sanitizer static libraries are unchanged before/after the binding rebuild. Build caches, flags, link commands, source compilation lists, and final binary hashes are attached.

The RED wheels were originally built at `607584aaeba0596edf8e384a8137e5e518a3ddaa`; their production inputs are byte-identical to the reviewed `f2125ea96b668fc32b66061241cec67213d0ea28`. Their hashes are `42bc9389441fd440d42c5b85b3d49128a12845b6c7a6e378471370e249181965` (cp39) and `cd0bac5e72a36ced892d81e3135d24e182bcb8c7551d251ce2f7e80c7227ce24` (cp313). Logs explicitly identify these old binaries even though the checkout already contains the new regression tests.

### Fresh builds and runtime identity

Linux x86_64 / WSL2, glibc 2.43, GCC **15.2.0**, CMake **4.2.3**, C++17, **AADET**. The existing isolated Python **3.9.25** and **3.13.9** environments both use actual **pybind11 2.11.1**. Each environment's 31 headers match upstream gitlink `8a099e44b3d5f85b20f05828d919d2332a8de841`. NumPy/pytest versions remain 2.0.2/8.4.2 and 2.5.3/9.1.1 respectively; host dependencies were untouched.

Both standalone wheels compile all nine binding translation units in fresh build directories against the verified installed DAL public package, then install into the isolated environments. Release uses `-O3 -DNDEBUG` and LTO. The sanitizer rebuild compiles the two changed translation units and relinks against the existing instrumented core/public libraries, with `-O1 -g0 -DNDEBUG`, address+undefined instrumentation, and IPO disabled.

| Fresh tested module | SHA256 |
| --- | --- |
| Installed cp39 wheel, `dal-python/.venv/lib/python3.9/site-packages/dal/_dal.cpython-39-x86_64-linux-gnu.so` | `638bb810d7a3a469b638ac95631d5e1c13f1b8b4343f03e4238a9b23318f917c` |
| Installed cp313 wheel, task `evidence/venv313/lib/python3.13/site-packages/dal/_dal.cpython-313-x86_64-linux-gnu.so` | `7b62ad3284ffad876c96b29d8fe596fa42a8358763a0361214e76868fb61f426` |
| Sanitized cp39, `build/s3-asan/dal-python/dal/_dal.cpython-39-x86_64-linux-gnu.so` | `f5ee59514118c34773e615f85c26385b85b0b7523dec1324216f007b9e15210f` |

`run_python.py` validates the loaded module directory, exact pybind version, all current source hashes, and loaded Python helper bytes before invoking pytest/probes in the same process. It records module hash, source SHA/tree, versions, and `ldd`. Guards remain active under `python -O`. Installed extension hashes also match their actual wheel members; wheel metadata/member hashes are in `summary.json`.

### Fresh results

All GREEN commands exited **0**. The expected old reviewer probes and eight-test RED run exited **1**. Each operation has exact argv, cwd, UTC timestamp, duration, exit code, and raw output in its JSON/log; XML records every executed test name.

| Verification | Actual result | Evidence |
| --- | --- | --- |
| Original reviewer probe, old cp39 and cp313 | Eight erroneous acceptances each; six legal controls each | `old-reviewer39-replay`, `old-reviewer313` |
| Narrow eight-test regression, old/new cp39 | **8 failed → 8 passed**, all RED failures are DID NOT RAISE TypeError | `old-focused39`, `focused39` |
| Original reviewer probe, new cp39 and cp313 | Eight TypeError rejections and six legal controls each | `reviewer39`, `reviewer313` |
| Python 3.13 StrEnum variant | Eight rejections and six legal controls | `strenum313` |
| Installed cp39 complete Python suite | **640 passed / 1 skipped**, 19.21 s | `python39-full` |
| Installed cp313 complete Python suite | **640 passed / 1 skipped**, 19.15 s | `python313-full` |
| ASan+UBSan related Python suite | **299 passed**, 4.39 s, no sanitizer diagnostics | `sanitizer-related` |
| Public script/archive contracts, unchanged verified S3 executable | **23/23 freshly executed**, 0.22 s | `public-script-tests` |
| Fresh installed C++ consumer builds | **2/2**, legacy/typed and historical AAD | `consumer-configure`, `consumer-build`, `consumer-tests` |
| Complete FIX example, cp39/cp313, normal and optimized | Four passes, PV `260.00000000000006`, d_SCALE `80.0` | `example39-*`, `example313-*` |
| Five corrupted example outputs under cp39 `-O` | All rejected: PV, risk, extra key, product schema, valuation schema | `example39-negative-*` |
| NumPy integer seam, invalid aliases, independent historical oracle and C++ comparison | Passed on cp39/cp313; PV160 and d_SCALE80 | `supplementary39`, `supplementary313` |

Each complete suite collects **641** tests, including **161 settings**, **78 FIX valuation**, **239 F7**, and **299 related** cases. The eight event/model-bindings scenarios are included in both full suites and the sanitizer suite: high/low API × plain str/normal str subclass/DAL String_/foreign string enum. Each scenario checks constructor and setter binding conversion, event conversion, and the independent **PV100** oracle. The other new legal controls cover 24 text constructor/setter cases and four genuine policy cases. These are included counts, not extra tests to add to the totals.

The only skip in each wheel is `test_joint_quote_risk.py:177`, because standalone wheels do not ship the private `_dal_quote_risk_test` fixture. No F7 or related case skips. The fixture's prior monorepo pass is historical evidence, not a current wheel pass.

### Boundary audit and original F7 matrix

All helper call sites were inspected. `SettingStringInput` serves only default_index constructor/setter (`script.cpp:23,29`), today policy (`value.cpp:64`), and method (`value.cpp:102`). Genuine `TodayFixingPolicy_` remains accepted before the string guard. Generic `StringInput` remains unchanged for events (`script.cpp:49`) and model-binding keys/values (`value.cpp:79,80`). NUL rejection, original case, legacy Value conversion, native settings copies, and GIL release boundaries are unchanged and covered by the passing existing tests.

The eight regression cases use different prior values: the opposite valid today policy, `EQ[OLD]`, and `mrg32`. They verify setter preservation and error class/field/value/identifier; the reviewer output audit additionally requires the exact non-enum string/NUL constraint, and both policy names plus `InvalidTodayFixingPolicy` for today errors.

The existing F7 oracle matrix was re-executed on both wheels and under sanitizer: T07's 48 combinations (BS/Dupire, three RNGs, BB, tree/compiled, double/AAD), T15's explicit 80→90→80 snapshot repricing, T18 historical PV/rate/SCALE derivatives across 1/257/8193 paths, future observation/payment dates, exact/fuzzy independent PV160/PV120 and derivatives, hard history, T27 legacy/alias/default deduplication, T28 conversion/copy/errors, and T29 diagnostic schemas/request IDs. Native public tests freshly re-execute the global-history/no-cache, explicit-empty snapshot, archive v1/v2, no-I/O and no-worker cases that Python does not expose. The detailed original mapping below remains valid; native core-only test results outside those 23 are inherited.

### Reproduction and limits

The attached `README.md`, capture JSON files, scripts, source inventory, and build metadata give complete commands. From the repository, representative commands are:

```bash
env PYTHONPATH= dal-python/.venv/bin/python ../evidence-r1/run_python.py \
  dal-python/.venv/lib/python3.9/site-packages dal-python/tests -q -rs \
  --junitxml=../evidence-r1/python39-full.xml
cmake --build build/s3-asan --target _dal -j 2
env PYTHONPATH=build/s3-asan/dal-python \
  LD_PRELOAD=/usr/lib/gcc/x86_64-linux-gnu/15/libasan.so:/usr/lib/gcc/x86_64-linux-gnu/15/libstdc++.so \
  ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  dal-python/.venv/bin/python ../evidence-r1/run_python.py build/s3-asan/dal-python \
  dal-python/tests/test_script_settings.py dal-python/tests/test_fix_valuation.py \
  dal-python/tests/test_script.py dal-python/tests/test_value.py dal-python/tests/test_api.py \
  dal-python/tests/test_xccy_resettable.py dal-python/tests/test_curve_pricing.py -q
```

No full core rebuild/full canonical Linux CTest run was repeated: its inputs and native libraries are unchanged and verified. Prior native1778, old sanitizer255, old wheel597-collection and S2 CoDi597 results remain historical. This run freshly builds both wheels, the changed sanitized binding and consumers, and freshly executes the results above. Leak detection is disabled for the unsanitized Python host; address/undefined diagnostics remain enabled. Windows/XLL, macOS, manylinux repair/portability, Python3.10–3.12, alternate AAD backends, and release upload were not independently executed.

The sole CI snapshot at **08:52:54 UTC** matches the exact tested head: **47 checks, 45 successful, one skipped, one running, zero failures**. It is an external snapshot, not independently reproduced platform coverage. No watch/poll, merge, final Closes, or successor/F8 dispatch occurred.

---

## Historical S3 report — tested at 607584aa, before R1

The following original report is preserved verbatim. Its references to “fresh,” “this run,” 195 F7 cases, and the earlier PASS describe only the previous S3 run and do not override the current R1 result above.

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
