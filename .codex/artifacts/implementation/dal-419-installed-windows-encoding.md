## DAL-419 result

The default native Windows installed-consumer gate passes **3/3**, and the original
installed-Excel driver passes its tests-OFF XLL/audit and tests-ON **259/259** gate
under **CP936**, with `CL` and `_CL_` unset. No compiler encoding override, code-page
change, package/export modification, test removal or acceptance reduction was used.
Unicode and ASCII rendering retain their original bytes.

This is a local implementation handoff for independent tester/reviewer acceptance.
Nothing was pushed; no PR was opened, readied or merged. DAL-418 / DAL-408 / PR #620
were not modified. Coordinator owns the subsequent separate DAL-419 PR, review,
final-head CI and integration. DAL-419 should enter `in_review`, not `done`.

## Baseline, scope and design

- Fresh isolated checkout: `agent/dal-implementer/3103ab041f74`.
- Actual latest `origin/master`, checked against `git ls-remote` at start and after
  verification: **`bf4cabea8575206375047e51993a5aa5aeb54c0c`**.
- Test-only commit: **`a4ab9bef1dff4ac580d6bc9c6680bb7e9caacd1c`**.
- Tested production fix: **`1bbb195aab407ed6e63f6b9dd5490c28d2f4066d`**.
- The following evidence commit changes only this report and its evidence subtree.
- Active DAL issues and all three open PRs (#620, #621, #615) were checked for
  duplication; #621's complete changed-file list contained no competing header fix.

Only two product/test files differ from master:

- `dal-cpp/dal/script/visitor/debugger.hpp`: spell the 23 non-ASCII tree symbol
  literals as explicit UTF-8 byte escapes, retaining the existing `const char*`
  C++17 interface. One local comment explains the encoding constraint.
- `tests/installed-consumer/main.cpp`: add a check of all **33 symbols in each
  mode**, using ASCII source and byte-exact UTF-8/ASCII expectations. It runs inside
  the existing consumer, so CTest remains three entries.

The change is representation-only: `audit_literals.py` verifies that all **421**
string literal occurrences retain identical bytes, and all non-literal C++ tokens
are unchanged. The ASCII style, renderer logic, defaults and error text are unchanged.
No CMake, export, dependency, public/Python/Excel product API, pricing or BG-1 file
was modified. No generated enum is involved.

The controlling DAL-419 scope explicitly permits this byte-preserving compatibility
repair without reopening the business spec/API/critique chain. Existing style,
test, boundary and implementer contracts and the debugger source/tests were read;
`docs/methodology/script_engine.md` supplies the existing rendering contract. The
current API note does not require a new decision for unchanged output bytes.
No published documentation/changelog change is needed for this routine build repair.

## Actual RED and GREEN

Every invocation was collected in the foreground to its terminal exit. Native
commands below are stored in the corresponding `.cmd` files, including VS setup,
CP936 observation and guards rejecting `CL` / `_CL_`. The native root was a new,
task-owned `D:\multica-builds\dal419-3103ab04` directory. Parent export/build trees
were only read for dependency provenance and were never reused as writable output.

The master source and all five pinned dependencies were exported first. A fresh
Release native installation was built with MSVC, native AAD, static CRT, public ON,
core/public tests OFF, examples OFF, Excel/Python OFF. The original master consumer
and Excel drivers then ran against that installation, before the fix was staged
into the native source/installation.

- `master-probe.cmd`: **exit 2**, C2001 at original `debugger.hpp:308/309`. This
  focused include/compile RED was collected on actual master before the local
  production edit. An earlier probe setup lacked `WIN32`; that setup failure is
  separately preserved and is not the accepted RED.
- `consumer-red.cmd`: original `tests/installed-consumer/run.cmake`, **exit 1**;
  configure succeeds, consumer compilation fails before linking with the same
  installed-header C2001.
- `excel-red.cmd`: original `tests/optional-excel/windows-installed.cmake`,
  **exit 1**; tests-OFF XLL/audit passes, tests-ON compilation fails with C2001.
- `consumer-test-red.cmd`: copy only the committed regression test into that
  source, build `dal_installed_consumer --parallel 1`, **exit 1**, same C2001.
  The original master production header and installation were still unchanged.
- `green.cmd`: stage only the fixed header and committed test, rebuild affected
  native production objects, reinstall, and run the original consumer driver in
  a fresh build directory. **Exit 0, 3/3 pass**, 0.47 seconds CTest time.
- `excel-green.cmd`: original installed-Excel driver in a fresh build directory.
  **Exit 0**; tests-OFF XLL/audit passes; tests-ON XLL/audit passes and
  **259/259 pass**, 4.13 seconds CTest time. No cases were filtered or removed.

The unchanged acceptance-driver invocations use:

```text
cmake -DDAL_INSTALL_PREFIX=D:/multica-builds/dal419-3103ab04/install
      -DDAL_CONSUMER_BINARY_DIR=D:/multica-builds/dal419-3103ab04/consumer-{red|green}
      -DDAL_BUILD_CONFIG=Release -DDAL_GENERATOR=Ninja
      -P tests/installed-consumer/run.cmake

cmake -DDAL_INSTALL_PREFIX=D:/multica-builds/dal419-3103ab04/install
      -DDAL_OPTIONAL_EXCEL_BINARY_DIR=D:/multica-builds/dal419-3103ab04/excel-{red|green}
      -P tests/optional-excel/windows-installed.cmake
```

The stored Ninja rules prove that neither `dal_installed_consumer` nor
`dal_excel_tests` receives `/utf-8` or a charset option. Their Release flags are
`/DWIN32 /D_WINDOWS /EHsc /O2 /Ob2 /DNDEBUG -std:c++17 -MT`. Excel's XLL keeps its
pre-existing target flags. The original audit verifies installed core/public
packages, real installed GTest, imported targets and matching static CRT.

The new regression test was not weakened. Formatting was restricted to the new
symbol initializer/test block. No additional production refactor was needed.

## Rendering and affected caller checks

Actual direct includes occur in core `script/event.cpp`, `script/diagnostics.cpp`
and `script/visitor/all.hpp`; the latter reaches public/binding callers. Fresh
native and Linux library builds compiled the affected production objects. The
installed consumer exercises inclusion and direct inline symbol access; installed
Excel covers the second failing downstream compilation path.

A small, recorded core-only test harness builds the unchanged
`test_debugger.cpp` and `test_exercise_parse.cpp` with the normal core test runner.
All **42/42** tests pass on original master Windows, fixed Windows, and fixed Linux.
They cover legacy text, JSON escaping/errors, Unicode/ASCII trees, width expansion,
FIX/date preservation, empty products, and exercise rendering. The harness links
only `DAL::cpp` and GTest; it introduces no reverse public dependency.

`compat.cpp` writes all 33 symbols in both modes and an arithmetic/conditional
product at widths **1, 20, 125**, and verifies empty-product output. The Windows
master baseline is compiled with an explicit `/utf-8` **only to obtain the original
Unicode byte oracle**. Fixed Windows `compat` compiles with default encoding flags.
The existing Unicode-literal test source is compiled with `/utf-8` in both focused
test runs; it is not an installed default-gate substitute.

Native before/after outputs are exactly **3,658 identical bytes**:
`sha256:96d36cc6aa6479f06f786fa615dd02da86f25a8052226cec38ce22fb8922fd9b`.
Fixed Linux output is **3,456 bytes**, identical after Windows CRLF translation:
`sha256:c26f900342a333236173718a246e2899915f617b6af9d8871340d8841e665d96`.

Linux GCC14 Release freshly builds/installs core and public, then passes the same
42 focused tests and the original installed consumer **3/3**. Linux uses the
shared-core/default build configuration; native Windows uses static libraries.
No performance algorithm, loop or branch changed. The selected renderer output
cases cover affected math, modes and width boundaries. Unchanged numerical/AAD/
Monte Carlo/calibration benchmark matrices were not repeated; no new timing or
numerical-performance PASS is claimed.

## Provenance and evidence

Native host: Windows **10.0.26200.9711**, VS2022 **17.14.35**, MSVC
**19.44.35228**, VS-selected CMake **3.27.6** / Ninja, CP936, C++17 Release/native AAD,
static CRT, lifetime diagnostics/profiling OFF, six build jobs/four DAL threads.
Linux: WSL2/Ubuntu26.04, GCC **14.3.0**, CMake **4.2.3**, Unix Makefiles,
C++17 Release/native AAD, six library build jobs/four focused build jobs. The native
CMake version differs from the parent's 4.2.3; it satisfies the existing 3.21 minimum.
Both native RED/GREEN use the same recorded CMake executable and default flags.

Dependency pins are the same immutable sources as the parent validation:

```text
eigen       3147391d946bb4b6c68edd901f2add6ac1f31f8c
googletest  d72f9c8aea6817cdf1ca0ac10887f328de7f3da2
machinist   e76b0ef243ca9bd506bb2f3743d8c9b52ee8d01d
pybind11    8a099e44b3d5f85b20f05828d919d2332a8de841
rapidjson   ea152ded01708ff1657b275ed0bcbf0681d64519
```

`snapshot.json` records master/dependency archive SHA-256 values. Before/after
export audits check **5,532 Git blobs**: all match master except the two explicitly
staged files in GREEN. Five Windows scripts have only `.gitattributes`-mandated
CRLF conversion. The unused nested RapidJSON test GTest gitlink is recorded and
not exported/built. The fixed source and installed debugger headers are raw-byte
identical; the regression source also matches the local test commit.

`manifest.json` inventories **19** library/executable hashes plus raw output,
source audits, CMake caches, compiler commands/Ninja rules, CTest logs and XML.
`raw-evidence.zip` preserves **125** files without output normalization, including
RED/GREEN drivers, source patch, all standalone Excel step logs, focused sources,
preparation/audit scripts and auxiliary harness setup failures. Those setup failures
(missing include/support paths and an initial flags override) were corrected only
in the evidence harness; they are not product RED or passing results. The initial
Linux Ninja configure failed because Ninja was unavailable; the unchanged product
then built and passed with Unix Makefiles. The native Ninja gates used real Ninja.

Raw archive: **1,169,118 bytes**,
`sha256:c78cc50718a93f6b35352876c7ac28b318a60736dfa0d77c1c47e1756580ffa1`.
The source patch contains only the test and encoding commits, without BG-1 or #621.

## Limits and remaining acceptance

Default Excel compilation still reports existing C4819 warnings from UTF-8 comments
in this and other headers. The observed default builds succeed; this does not claim
warning-free `/WX` consumption. The repair intentionally stays within the failing
literal representation and preserves output bytes.

The parent's `dal-418-rate-option-literals.md`, integration
`6372ede9452b2ec912234a373666dda9b239a1bc`, and its hashed manifest/raw archive
remain historical evidence for Linux3883/Windows3967/Python/examples on that
different integration. They were not relabeled as tests of DAL-419. The five pins
are reused as source provenance; all affected libraries/executables above were
freshly built. Full scripts, Python, live Excel registration/workbook smoke tests,
all example/numerical matrices and final-head CI were not rerun here.

No build/export or component contract changed, so the conditional requirement for
a new core-only/public-disabled build matrix is not triggered. Independent tester
and code-review acceptance, any coordinator-directed documentation decision, the
separate PR and its required final-head gates remain outstanding. No implementation
blocker remains in the two required default Windows installed-package paths.
