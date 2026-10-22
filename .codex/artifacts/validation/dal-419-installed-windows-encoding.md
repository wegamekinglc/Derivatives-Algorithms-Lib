## DAL-419 independent installed-package validation

Validator: dal-tester, 2026-10-22 UTC / 2026-10-23 Asia/Shanghai. This is the focused DAL-419 validation dispatched from DAL-418, separate from the already accepted BG-1 review/documentation work.

**PASS for the requested SHA's DAL-419 validation:** both original default installed-package gates pass under native Windows CMake 4.2.3 and 3.27.6, without encoding/environment workarounds, and all checked rendering bytes are compatible. No implementation blocker remains in this tested scope. Master advanced during validation; its additional product integration is untested, as detailed below. Independent reviewer, separate PR/final-head gates and master integration remain pending. No product, build/export, dependency or formal test file was modified by the validator. No push, PR creation, ready action or merge was performed. DAL-419 remains `in_review`; DAL-418 and DAL-408 remain `in_progress`; #620 remains draft.

### Exact source, tools and isolation

- Original DAL-419 checkout and branch retained: `agent/dal-implementer/3103ab041f74`. Tested source head **`696edf742f4c9024e7745f55c65923b56d98abca`**, whose product/test content equals fix commit `1bbb195aab407ed6e63f6b9dd5490c28d2f4066d`. Formal test commit: `a4ab9bef1dff4ac580d6bc9c6680bb7e9caacd1c`; base/master: `bf4cabea8575206375047e51993a5aa5aeb54c0c`.
- Only two non-artifact files differ from master: `dal-cpp/dal/script/visitor/debugger.hpp` and `tests/installed-consumer/main.cpp`. No BG-1/#620, #621, CMake/export/API/default/error/dependency changes were imported. The original #620 checkout was read for historical provenance and not written.
- New task-owned Windows root: `D:\multica-builds\dal419-tester-eb7d672c-696edf74`. Source and all five pinned dependencies were freshly exported; **5,550 Git blobs** verified with no content mismatch. Five Windows scripts have only export-mandated CRLF conversion. Dependency contents were audited separately from their gitlink entries; RapidJSON's unused nested test GTest gitlink was not built.
- Real native Windows 10.0.26200.9711, VS2022 17.14.35 / MSVC 19.44.35228, Ninja, C++17 Release, native AADET, static CRT, default lifetime/profiling OFF. Six build jobs/four DAL threads. Fresh core/public libraries were built and installed with native CMake **4.2.3**; their affected callers were rebuilt rather than taking old installed libraries as new results.
- CMake 4.2.3 was installed side by side inside the validation root from the [official Kitware release](https://github.com/Kitware/CMake/releases/tag/v4.2.3). Windows x64 ZIP SHA-256 `eb4ebf5155dbb05436d675706b2a08189430df58904257ae5e91bcba4c86933c` matches the release's checksum file. Existing system CMake **3.27.6** was preserved and used for separate fresh consumer/Excel directories against the same newly installed fixed libraries.
- `native-snapshot.json`, `source-audit.json` and `manifest.json` record archive/dependency identity, source audit, compiler/tool hashes, CMake caches/Ninja commands and fresh binary identities. New validation outputs are separate from both the author's and DAL-418's historical trees.

Dependency pins, unchanged from the author and parent provenance:

```text
eigen       3147391d946bb4b6c68edd901f2add6ac1f31f8c
googletest  d72f9c8aea6817cdf1ca0ac10887f328de7f3da2
machinist   e76b0ef243ca9bd506bb2f3743d8c9b52ee8d01d
pybind11    8a099e44b3d5f85b20f05828d919d2332a8de841
rapidjson   ea152ded01708ff1657b275ed0bcbf0681d64519
```

### Correction to the parent tool-version record

My previous DAL-418 validation report incorrectly described **native Windows CMake as 4.2.3**. The original captured Windows cache's SHA-256 is still exactly `2d489b1c77bf2f6175d4b0f9e34ed3b73dd8114082c36c718207ca8b75bfb40e`, matching the original DAL-418 manifest. Its internal cache version and CMake root are **3.27.6**, also matching the current native executable. **4.2.3 was the Linux version.** Thus the author did not clear the original failure by switching native CMake versions: the native parent and author both used 3.27.6.

`audit_author.py` verifies this against the unchanged historical file and recorded hash. This report corrects that version attribution without writing to the frozen parent artifact. The new 4.2.3 and 3.27.6 gate runs explicitly test both versions, rather than relying on the erroneous earlier description or a tool-version change as a repair.

### Audit of the author's RED/GREEN and bytes

Verified the author's raw archive SHA-256 `c78cc50718a93f6b35352876c7ac28b318a60736dfa0d77c1c47e1756580ffa1`, all **125 raw-evidence hashes**, seven metadata hashes and **19 binary hashes** at their recorded locations. Master/green export audits each verified 5,532 blobs with no unexpected mismatch; original master/default-consumer/default-Excel/test-only RED logs contain the actual C2001, not just a same-blob inference. Default gate drivers guard CL/_CL_ and contain no encoding option or code-page change. Three recorded focused XML files each contain 42 cases and zero failures. Setup-error logs remain separate from accepted RED evidence.

Independent lexical comparison strips comments, handles quoted strings/characters and decodes C++ escapes before comparing payload bytes. **All 384 actual string-literal tokens retain identical bytes; 23 literal representations change; every other code token is identical.** The author's simpler regex counts 421 quoted occurrences, including comment text. The count difference does not hide a changed code literal; `author-audit.json` records the independent count and result. ASCII mode, renderer control flow, defaults and owning diagnostics remain byte-compatible.

For an independent runtime RED, the actual `bf4cabea` header was exported into a separate include overlay and compiled by the same native MSVC/default CP936 process against the fresh installation. `master-probe.cmd` fails with the expected **C2001 at lines 308/309**, exit 1 from the driver. No `/utf-8` was used. The author's already audited two complete master installed-driver failures were reused; the validator did not repeat those full failing builds.

### Fresh independent results

All commands were collected in the foreground to termination. The original installed drivers and acceptance cases were preserved; no test was filtered out to obtain their counts.

| Check | Result | Evidence |
| --- | --- | --- |
| Native 4.2.3 fresh core/public build and install | exit 0 | `native/build-fixed.log` |
| Native 4.2.3 original installed consumer | **3/3 PASS**, exit 0 | `native/consumer-default.log` |
| Native 4.2.3 original installed Excel | **259/259 PASS**, exit 0; tests-OFF and tests-ON XLL/audit pass | `native/excel-default.log`, `native/excel-default/excel-ON-ctest.log` |
| Native 3.27.6 original installed consumer | **3/3 PASS**, exit 0 | `native/consumer-327.log` |
| Native 3.27.6 original installed Excel | **259/259 PASS**, exit 0; tests-OFF and tests-ON XLL/audit pass | `native/excel-327.log`, `native/excel-327/excel-ON-ctest.log` |
| Native 4.2.3 existing debugger/exercise cases | **42/42 PASS**, exit 0 | `native/focused-fixed.xml`, `native/focused-fixed.log` |
| Default-encoding actual rendering | **3,658 bytes exactly equal to original master oracle** | `native/compat-fixed.bin`, `author-windows-baseline.bin` |
| Actual master header default CP936 compilation | expected RED, driver exit 1 / C2001 | `native/master-probe.log` |

Each driver guards CL/_CL_ **before and after** VS setup. The native environment probe records system ACP **936**, input/output console code pages **936**, and both variables null. Drivers query `chcp` without changing it. CMake/CTest versions and compiler paths are captured per invocation. Actual consumer and Excel-test compile commands are independently extracted with Ninja and audited for absence of `/utf-8`, `/source-charset` and `/execution-charset` overrides. The XLL keeps its existing private `/utf-8` option; it is the separate consumer and `dal_excel_tests` commands that establish the repaired default consumption.

Build/install command, using the new native CMake 4.2.3 on the driver-local PATH:

```text
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_INSTALL_PREFIX=<native-root>/install -DMSVC_RUNTIME=static
  -DDAL_BUILD_PUBLIC=ON -DDAL_CPP_BUILD_TESTS=OFF
  -DDAL_CPP_BUILD_BENCHMARKS=OFF -DDAL_PUBLIC_BUILD_TESTS=OFF
  -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_EXCEL=OFF
  -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF
cmake --build build/native --parallel 6
cmake --install build/native
```

The two original acceptance-driver commands, independently run with **each** native CMake version, are:

```text
cmake -DDAL_INSTALL_PREFIX=<native-root>/install
  -DDAL_CONSUMER_BINARY_DIR=<native-root>/consumer-{default|327}
  -DDAL_BUILD_CONFIG=Release -DDAL_GENERATOR=Ninja
  -P tests/installed-consumer/run.cmake

cmake -DDAL_INSTALL_PREFIX=<native-root>/install
  -DDAL_OPTIONAL_EXCEL_BINARY_DIR=<native-root>/excel-{default|327}
  -P tests/optional-excel/windows-installed.cmake
```

Exact runnable `.cmd` files, their terminal exits, raw logs, CMake caches, Ninja rules and installed runtime/package audits are in the evidence archive. The environment probe uses an existing native Python solely to query Win32 code pages; it is not a Python product test or a compiler-setting override.

### Renderer/caller coverage and evidence reuse

The fresh focused runner compiles the unchanged `test_debugger.cpp` and `test_exercise_parse.cpp` with the normal test main, linking **only DAL::cpp and GTest**. These 42 cases cover legacy text/JSON, Unicode and ASCII trees, width branching, date/FIX/exercise rendering and error boundaries. The existing Unicode golden test source retains its test-target `/utf-8`; that runner is not used as a substitute for default installed consumption.

The compatibility executable is separately compiled **without an encoding option**. It emits all 33 symbols in both modes, an arithmetic/conditional product at widths **1, 20, 125**, and checks the empty product. Its entire output equals the hash-verified original master Windows oracle, not an oracle generated from the fixed helper:

```text
3,658 bytes
sha256:96d36cc6aa6479f06f786fa615dd02da86f25a8052226cec38ce22fb8922fd9b
```

The original master oracle was deliberately compiled with `/utf-8` to express the old supported UTF-8 behavior; this exception belongs only to the old byte oracle. New default-gate builds and new compatibility executable have no encoding override. Author Windows/Linux byte equality after CRLF translation was audited as well.

Direct changed-header callers are core `script/event.cpp`, `script/diagnostics.cpp` and `script/visitor/all.hpp`; the last is included by parser/event headers and reaches downstream public/binding code. Fresh core/public libraries rebuild these callers. Consumer and installed Excel tests compile the two previously failing downstream paths; no package/export or dependency direction changed. The conditional example/build-change core-without-public workflow is not needed for this representation-only header fix, and no additional core-independence matrix is claimed.

The author's Linux GCC14 build, 42 focused cases and installed consumer 3/3 are **reused audited evidence**, not fresh validator runs. They have the same fixed product/test content, immutable pins and verified binary identities. Parent DAL-418's Linux3883/Windows3967/Python/examples results belong to its different `6372ede9` integration and are not relabeled as DAL-419 results. No parent full matrix was repeated solely for handoff.

Performance scope: this change alters spelling of constant bytes only, with identical code tokens, literal payloads and renderer output. It adds no numerical/AAD/Monte Carlo branch, loop or pricing call. No numerical/performance benchmark is selected or claimed passed. Existing C4819 warnings from UTF-8 comments remain visible in the successful default builds; this is not a warning-free `/WX` consumption claim.

### Delivery and remaining work

**Existing tests run:** the two original installed gates on two native tool versions plus the 42 existing targeted cases and compatibility executable. **Formal tests authored: zero. Product/test/build failures repaired by validator: zero.** The existing symbol regression plus independent token/runtime audits cover the requested boundary; no uncovered behavior required a new test.

Only this validation report/evidence subtree was written. Scope-limited success does not finish DAL-418 AC8 or authorize #620 merge. Coordinator still needs DAL-419 independent reviewer, any necessary documentation decision, separate PR and final-head gates, actual master merge, then normal #620 integration and its required local/final-head verification. No such publication or acceptance decision was performed here.

**Live master change:** at start, `git ls-remote` confirmed base `bf4cabea`. At final capture it reported **`acadd8ee44fdb6a0bb2e70de9d559db59de066f8`**, the newly merged #621/DAL-407 product commit. A safe fetch and complete file-list comparison identify calibration protocol/curve changes, public CMake/facade/example/test additions, Python CMake/bindings/tests, Excel generated entries/source/tests and documentation. The debugger header, both installed-consumer sources/driver, installed-Excel driver/CMake, root CMake and five dependency pins are unchanged by that commit. Nevertheless its new public and Excel production/test inputs affect an integrated installation, so this report does **not** certify a current-master integrated build or preserve the old test count for it. No merge/rebase or product import was attempted. `master-advance.json` and the complete changed-file list hand the specific integration scope to the coordinator before the independent PR/final-head gates.

The repository documentation checker passed for 91 Markdown files; validation artifact whitespace is checked with the staged diff. A post-run evidence selector initially assumed MSVC `/c` rather than CMake's emitted `-c`; its correction is recorded separately and changed no product, test invocation or result.

`dal-419-validation-evidence/raw-evidence.zip` preserves raw logs/XML/drivers, cache/rule records and byte specimens; `manifest.json` inventories source/config/binary identity and `archive.json` hashes the archive/report. Source exports, tool installers and executables are excluded from delivery. Runtime paths are provenance only; the report and lightweight evidence are attached to the dispatch reply.
