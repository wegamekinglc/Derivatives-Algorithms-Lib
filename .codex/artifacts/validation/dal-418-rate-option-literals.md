## DAL-418 independent validation

Validator: dal-tester. Captured 2026-10-22 UTC / 2026-10-23 Asia/Shanghai.

**BG-1 AC1–AC7 pass on the integrated source. The complete delivery gate remains blocked by default native Windows installed-consumer and installed-Excel-tests builds under code page 936.** Their diagnostic `/utf-8` controls pass but do not replace the default gates. Independent reviewer, doc-writer and final pushed-head CI remain coordinator-owned and pending. DAL-418 and DAL-408 remain `in_progress`; PR #620 remains draft and unpushed.

### Source and executable provenance

- Original checkout and branch retained: `agent/dal-implementer/e2075e8549aa`, existing PR [#620](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/620). Author head `0ba2e15412aa3ca8184d9e704fed84b0f03f0c79`; real incremental parent `9c547ab64ff2a53ef444ac928440e443fc720c24`.
- Tested integration head **`6372ede9452b2ec912234a373666dda9b239a1bc`**, normal conflict-free merge with parents `0ba2e15412aa3ca8184d9e704fed84b0f03f0c79` and `bf4cabea8575206375047e51993a5aa5aeb54c0c`. Only two files were imported from accepted master: `dal-cpp/tests/model/test_gsr_script_acceptance.cpp` and `.codex/artifacts/validation/dal-413-gsr-script-numerical-validation.md`. Production/dependency/build configuration trees did not change during integration. Final `git ls-remote` still reports master `bf4cabea…`, branch remote `9c547ab6…`.
- Linux: Ubuntu 26.04 in WSL2, GCC 14.3, C++17 Release, native AADET, six build jobs, four DAL threads. Python 3.13.9, pybind11 3.1.0, NumPy 2.5.4, pytest 9.1.1.
- Real Windows: native `cmd.exe`, Windows 10.0.26200.9711, Visual Studio 2022 17.14.35 / MSVC 19.44.35228, CMake 4.2.3 / Ninja, Release/native AADET. Dedicated NTFS source export `D:\multica-builds\dal418-eb7d672c679c-6372ede9\source`; no shim or portable substitute. Python 3.12.13, pybind11 3.1.0, NumPy 2.5.4, pytest 9.1.1.
- Windows source plus all five dependency exports were audited against **5,555 Git blobs**. No content mismatch; 21 EOL-only exports comprise five Windows scripts and 16 generated `auto/` files normalized by the original generator. All changed production/test files match raw Git bytes. The six native base libraries/test executables/XLL retained identical hashes after enabling workspace Python.
- `dal-418-validation-evidence/manifest.json` records source, dependency, CMake cache, compile-command, library, executable, extension and raw-evidence SHA-256 values. `windows-snapshot.json` records Git/dependency archive hashes; `windows-source-audit.json` records the export audit. These source identities apply to all fresh results below. The later validation artifact commit changes evidence only.

| Dependency | Exact Git pin |
| --- | --- |
| eigen | `3147391d946bb4b6c68edd901f2add6ac1f31f8c` |
| googletest | `d72f9c8aea6817cdf1ca0ac10887f328de7f3da2` |
| machinist | `e76b0ef243ca9bd506bb2f3743d8c9b52ee8d01d` |
| pybind11 | `8a099e44b3d5f85b20f05828d919d2332a8de841` |
| rapidjson | `ea152ded01708ff1657b275ed0bcbf0681d64519` |

### Author evidence audit and independent focused checks

`audit_author.py` verified 55 manifest entries, the author's raw evidence archive, actual-parent production/test input blobs and five latest-master RED inputs. The initial-master, actual-parent and latest-master XML each contain three expected parser failures: Caplet, fixed Swaption and floating Swaption `1e-7`, all owning `InvalidScriptInstrumentDefinition` with `Not a valid number string`. Expanded parent RED is 7 failures / 9 cases; author's post-fix target GREEN is 121/121.

The initial RED manifest correction is valid and narrowly documented. Its original test hash `6ecfcac8f28d4bc0fa03942dacb0c9457ce4d1809506d0d6d17ecd431bf23e16` was the later expanded test file present when hashing completed. Corrected hash `fb005d5cf6412bc137bb56e44963e7b5042414c8b2780283b48c82a14b83df63` equals both the actual isolated RED translation unit and test commit `0ac14fb74696b1d0e80e2426e6270555028410fe`. Only that hash and a correction note changed; original manifest, commands, binary and XML were preserved. See `author-audit.json` and the unchanged implementation evidence.

Independent executions:

```bash
DAL_NUM_THREADS=1 build/dal418-master/latest_rate_tests \
  --gtest_filter='GSRRateOptionScriptTest.TestTiny*' \
  --gtest_output=xml:.codex/artifacts/validation/dal-418-validation-evidence/independent-latest-red.xml
# exit 1: three expected parser failures on bf4cabea production

DAL_NUM_THREADS=1 build/dal418-parent/rate_tests \
  --gtest_filter=GSRRateOptionScriptTest.TestAccrualCompatibilitySnapshots \
  --gtest_output=xml:.codex/artifacts/validation/dal-418-validation-evidence/independent-parent-snapshots.xml
# exit 0: actual-parent retained binary, verified SHA-256 before execution

DAL_NUM_THREADS=1 build/Release-linux/dal-cpp/dal_cpp_tests \
  --gtest_filter='GSRRateOptionScriptTest.TestTiny*:GSRRateOptionScriptTest.TestAccrual*:GSRRateOptionScriptTest.TestSubnormalAccrualUsesIndependentDecimalParserBoundary:GSRRateOptionScriptTest.TestInvalidAccrualRetainsOwningInputErrors' \
  --gtest_output=xml:.codex/artifacts/validation/dal-418-validation-evidence/fresh-linux-ac.xml
# exit 0: nine formal regression cases from the freshly rebuilt integration
```

The fresh Linux and native Windows snapshot XML each exactly match the freshly rerun actual-parent **504 properties** (36 cases × 14 properties), including row keys/text, seven identity categories and portable JSON. The pinned combined fingerprint is `sha256:e6449292ee247ec0e27d808d028b63133a23a39d62b761d6126e41b5526ef8b0`. Windows additionally reran the same-runtime independent subnormal oracle: 2/2 snapshot/subnormal cases pass. This compares real parent results with the new builds, rather than comparing two runs of the fixed formatter.

| Acceptance | Independent assessment |
| --- | --- |
| AC1 | PASS: audited formal RED inputs/XML and independently reran latest-master RED, all three positions; fresh integrated GREEN. Original assertions retained. |
| AC2 | PASS: CALL/PUT in all three positions, exact literal/AST/descriptor constant, FastGaussian preparation and independent decimal/native prices; relative nonzero oracle and exact zero oracle. |
| AC3 | PASS: 13 normal boundary inputs × three positions, exact single `NodeConst_`, exponent-free bounded literal; independent subnormal decimal/error comparison on Linux and Windows. Extreme normal values test import/AST without demanding unsupported prices. |
| AC4 | PASS: 504 parent/Linux/Windows properties equal; ordinary/threshold accrual, scientific N/K rows, zero/negative K protected. Event exponent grammar is unchanged. |
| AC5 | PASS: JSON and DAL archive round trips, new binding leaves original immutable, changed source/binding/context rejects stale plans; compatibility suites pass in full runs. |
| AC6 | PASS: 18 invalid accrual cases retain owning input errors; existing AST, date/index/convention/fee/lag, archive and parser negative cases run unchanged. |
| AC7 | PASS: full rate-option suite covers tree/compiled/AAD/CRN/domain/tape lifecycle. Tiny tree/compiled payments and numeraire use identical input paths, including zero and observable nonzero amounts. No budget, threshold or tolerance edits. |
| AC8 | BLOCKED/PENDING: default Windows installed-consumer and installed-Excel tests fail as detailed below; complete release gate cannot pass. Reviewer, documentation decision and actual final-head CI are pending. |

### Fresh complete-script and component results

All invocations were foreground-collected to termination. Existing root `test_output.txt` was moved aside before each new capture; the records below are this run's output. Counts are configuration-specific CTest entries, not a fixed historical test total.

| Invocation | Exit / fresh result | Raw evidence |
| --- | --- | --- |
| Initial `bash ./build_linux.sh`, GCC14, inherited host PATH | 8; 3,879/3,882 pass, three configuration timeouts; 333.35 s | `linux-initial-test_output.txt` |
| Full Linux script, Linux-only tool PATH, Python ON | 0; **3,883/3,883 PASS**, 48.93 s | `linux-full-test_output.txt` |
| Separate Linux fast example label | 0; **37/37 PASS**, 6.38 s | `linux-examples.log` |
| Linux installed CMake consumer | 0; **3/3 PASS** | `linux-installed-consumer.log` |
| Original native `build_windows.bat` | 0; **3,967/3,967 PASS**, 1,142.42 s | `windows-full-test_output.txt`, `windows-full-LastTest.log` |
| Native Windows workspace Python CTest entry | 0; **1,461 passed, 3 skipped**, 89.45 s pytest; CTest 1/1 | `windows-python.log` |
| Native Windows standalone installed-DAL Python | Python CTest 1/1; **1,460 passed, 4 skipped**, 104.90 s pytest. Combined driver later exits 1 at the separate consumer gate. | `windows-standalone-python.log` |
| Native Windows default installed C++ consumer | **FAIL**, repeatable pre-link `debugger.hpp:308` C2001 under CP936 | `windows-consumer-default-red.log` |
| Native Windows installed consumer diagnostic `CL=/utf-8` | 0; **3/3 PASS**, 0.47 s; default gate remains failed | `windows-consumer-utf8-build.log`, `windows-consumer-utf8-tests.log` |
| Native Windows standalone installed Excel, original driver | 1; tests-OFF XLL build/audit passes; tests-ON compile **FAIL**, same CP936/C2001 | `windows-installed-excel.log`, `installed-excel-*.log` |
| Native Windows installed Excel diagnostic `CL=/utf-8` | 0; XLL and tests build; **259/259 PASS**, 1.36 s; default gate remains failed | `windows-installed-excel-utf8-build.log`, `windows-installed-excel-utf8-tests.log` |
| Changed production TU, GCC14 and GCC15, existing CI warning policy | both 0, `-fsyntax-only -Wall -Wextra -Wpedantic -Werror` with existing CI suppressions | `strict-source-checks.json` |

Linux exact successful full-script command:

```bash
PATH=/home/wegamekinglc/anaconda3/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
NUM_CORES=6 DAL_NUM_THREADS=4 CC=gcc-14 CXX=g++-14 \
ADDITIONAL_CMAKE_FLAGS=-DDAL_BUILD_PYTHON=ON \
bash ./build_linux.sh > test_output.txt 2>&1
```

The unchanged script configures/builds/installs core and facade plus portable Excel and Python. Its CTest selection excludes examples and benchmark labels. Both newly accepted `GSRScriptAcceptanceTest` cases are discovered and pass on Linux and Windows (`accepted-test-discovery.txt` and both full logs). Linux Python's aggregate pytest CTest entry passes; this capture does not preserve an individual Linux pytest case count.

```bash
# With the same Linux-only PATH:
DAL_NUM_THREADS=4 ctest --test-dir build/Release-linux \
  -L '^examples$' --output-on-failure --parallel 6
CC=gcc-14 CXX=g++-14 cmake \
  -DDAL_INSTALL_PREFIX="$PWD/build/stage/Release-linux" \
  -DDAL_CONSUMER_BINARY_DIR="$PWD/build/dal418-installed-consumer" \
  -DDAL_BUILD_CONFIG=Release -P tests/installed-consumer/run.cmake
```

The initial Linux failures were `dal_native_aad_configuration`, `dal_aad_profiling_configuration` (120-second child configure timeouts) and `dal_optional_excel_configuration` (300 seconds). Their logs show CMake tool discovery traversing inherited Windows PATH entries. Each unchanged driver passed in a narrow rerun with the Linux tool PATH (about 0.3–0.4 seconds), then all three passed in the complete script rerun. Original failure output is retained; no CMake timeout, source, test or acceptance rule was modified. See the three `*-configuration-narrow.log` records.

Native Windows command was `cmd.exe /d /c D:\multica-builds\dal418-eb7d672c679c-6372ede9\run-validation.cmd`. Its retained driver sets `DAL_DIR`, `NUMBER_OF_PROCESSORS=6`, `DAL_NUM_THREADS=4`, then calls the **original `build_windows.bat`** and captures its exit. Core/public, native XLL and Excel tests build; 37 fast examples and the existing slow European MC example execute. The slow example took 795.84 seconds and passed without changing its budget. This is an exit-status observation, not a performance benchmark claim. No live Excel workbook/registration smoke test is claimed.

The retained `run-python.cmd` enables `DAL_BUILD_PYTHON=ON` on the same native Windows Release build with explicit Python executable, rebuilds/installs and runs `ctest --verbose --output-on-failure --no-tests=error -C Release -R "^dal_python_pytest$"`. `run-standalone-python.cmd` separately configures `-S dal-python` against that installed DAL, builds, and runs its complete Python entry. The three common skips are two optional mpmath oracles and the Windows huge-allocation resource-limit case; the fourth standalone-only skip is the intentionally unavailable native opaque test helper. Workspace mode includes that helper and the case passes. Existing skips are reported, not hidden by new filtering.

### Unresolved native installed-package gates

The original `tests/installed-consumer/run.cmake` configures successfully on native Windows, then `dal_installed_consumer` compilation fails at installed `dal/script/visitor/debugger.hpp:308` with **C2001: newline in constant** and cascading syntax errors. `chcp` is 936. The affected header has UTF-8 tree-rendering string literals; the monorepo supplies `/utf-8`, while the standalone installed consumer's default compiler invocation does not.

Narrow reproduction is preserved in `run-consumer-repro.cmd`: after `VsDevCmd.bat`, build only `dal_installed_consumer --parallel 1` in the already configured consumer tree, capture nonzero exit, then use command-local `CL=/utf-8` for the unchanged tree. The control builds all three consumers and CTest is 3/3. The script uses `setlocal`; no persistent environment, source, exported package or build configuration repair was made. The wrapper's exit 0 means expected RED plus diagnostic control succeeded; it does not mean the default consumer passed.

`consumer-parent-identity.json` records exact equal Git blobs at real parent `9c547ab6` and tested head for `debugger.hpp`, consumer CMakeLists and consumer `main.cpp`. The failure occurs before linking and has no call to the changed private importer. This evidence identifies an existing encoding/configuration interaction outside the permitted BG-1 production write scope; a separate full parent consumer build was not performed. **A baseline failure still blocks this gate.** Coordinator must route its resolution or obtain an explicit acceptance decision; this validation does not waive it.

The additional native installed-Excel CI driver was also executed unchanged:

```text
cmake -DDAL_INSTALL_PREFIX=<native-source>/build/stage/Release-windows
      -DDAL_OPTIONAL_EXCEL_BINARY_DIR=<native-root>/installed-excel
      -P tests/optional-excel/windows-installed.cmake
```

Tests-OFF XLL configure/build/audit passes. Tests-ON configures, then Excel test translation units fail with the same `debugger.hpp:308/309` C2001 and C4819 on CP936. The XLL target itself supplies `/utf-8`; the standalone tests target lacks it. `run-installed-excel-control.cmd` reproduces one failed object (`CMakeFiles/dal_excel_tests.dir/tests/test_curvature_request.cpp.obj`, one build job), then command-local `CL=/utf-8` builds the unchanged tests-ON XLL/tests and runs **259/259 PASS**. Default driver exit 1 and narrow RED are preserved separately; control exit 0 does not waive the failed default. Relevant Excel CMake/test/driver blobs and root CMakeLists also equal the actual parent, as recorded in the identity manifest. Both package failures share the observed compiler-encoding cause; no additional product repair is included.

### Scope, outstanding gates and artifacts

Existing tests were run and author tests were audited. **New tests authored: zero. Failures repaired in production/tests/build files: zero.** No actual coverage gap in AC1–AC7 required adding redundant tests. Only local integration and this validation artifact subtree were written; spec/API/critique/implementation records, shared documentation and public APIs remain unchanged.

The private formatter executes during import, outside pricing/AAD/MC loops. Caller search finds the existing single-fixing calibration test (`dal-cpp/tests/model/test_gsr_script_calibration.cpp`), included in both full suites; no example, facade, Python, Excel or nine benchmark callers of this importer were found. No unrelated full benchmark run or unmeasured performance PASS is claimed.

The full scripts cover the active Release/native-AADET configurations. They do not reproduce every CI leg: six Linux compilers, four lifetime/profiling combinations, warning-clean/core-only-example jobs, sanitizer runtimes, Python wheels/macOS and Windows diagnostic variants remain final-head CI obligations. A GCC15 syntax check does not count as its full compiler leg. This production-only importer change does not modify examples/build files, so the special example-change core-independence workflow is not used to claim extra local coverage. Existing configuration checks pass, but are not equivalent to all diagnostic production tests.

No push, ready-for-review action or GitHub merge was performed. The current remote head's old review/CI cannot prove the local integrated fix. After the outstanding local gate is resolved, coordinator still needs reviewer → doc-writer, final content integration, final-head push/CI and protected merge checks. This report is a **partial gate success with an explicit blocker**, not merge readiness.

`dal-418-validation-evidence/raw-evidence.zip` preserves raw output/XML/drivers without whitespace normalization; `manifest.json` inventories per-file hashes, and `archive.json` records archive hash and size. Audit/preparation/capture scripts are included for reproducibility. No executables or full source exports are bundled. Report and evidence archive are delivered as issue attachments; local paths in logs are provenance only.
