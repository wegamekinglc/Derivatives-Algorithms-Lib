# DAL-418 / DAL-408 final integration and publication

PR #620 continues on `agent/dal-implementer/e2075e8549aa`. This record supersedes
historical stage-status and caller-availability statements without changing the
original implementation, validation, review or documentation reports.

## Source and review applicability

- Tested normal merge: `fb82515e6d0af75824b592e7121ceb2e5e24c1c0`, tree
  `62ce9228892d05976b29f4a91ff6aad1c77c460d`.
- Actual parents: local `b3589ed93b9c8adef9296a6dbb081b9f66f1fcc8` and accepted
  master `4ed1084de1fe582d46fd38d9436021044c2e7166` (merged PR #624, including #621).
- The only conflicts were CHANGELOG and artifact-index additions. Their resolution
  is an exact union of both accepted texts; the methodology guide is the automatic
  merge. Three-way inputs and staged resolution are preserved in the attachment.
- Both BG-1 code/test blobs equal reviewed `378ca2f5`. The seven independent XCCY
  files, two preparation files and XCCY regression equal accepted `9c547ab6`.
  All 135 upstream-only paths equal the actual master. There is no new semantic
  product or formal-test diff requiring another review/documentation stage.
- Original review SHA-256 remains
  `cfb1e9ffa503c79a7f7ac256d0e0ee3bb86be3545cbe7e465e1bf6d00789e556`.
  The five original activity reports remain byte-identical.
- The existing test-first commits `0ac14fb7` / `4b5e49e7` and fix `a4386247`
  remain in normal history. Original parent and actual-master RED, independent
  decimal/subnormal controls and 504-property parent snapshots are retained.
  Current master importer equals the original RED parent; parser and lexer are
  unchanged. This phase does not relabel harness/setup errors as product RED.

## Current DAL-407 callers

The old statement that no public/Python/Excel caller exists describes the earlier
source only. Accepted #621 now forwards through:

- `dal-public/src/calibrationinstrument.cpp`: native variant and import settings
  map to the existing specific importer, with the public pillar/settings boundary.
- `dal-python/src/bindings/calibrationinstrument.cpp`: owned option/settings copies
  survive the GIL release and forward to the public facade.
- `dal-excel/src/__calibrationinstrument.cpp`: typed native option and parsed
  settings forward to the same facade.

An attachment-only joint probe exercises ordinary, tiny `1e-7` and full-mantissa
`1.2345678901234567e-7` accruals at caplet/fixed/floating locations, both CALL/PUT:
18 cases. On each OS, C++ facade and actual Excel wrapper equal core full JSON;
source/definition/binding/instrument/compiler/context/plan identities, portable
JSON/DAL archive, Excel chunk/load, native pricing and stale-binding rejection
are checked. Each OS passes three probe tests, including owned invalid-input
reports after inputs die, optional pillar and rate-option-only settings rejection.
Across OSes the full JSON and seven identities match; maximum observed price
difference is `1.3478801408339791e-15`, not a cross-platform precision promise.
All four freshly built workspace/installed Python modules independently match the
18 C++ records and preserve three owned errors, with their actual module paths
checked to exclude an older editable installation.

Existing formal XCCY/general-MC isolation, owned reports, rate-option AAD/CRN,
subnormal/parser and identity regressions run in the complete fresh suites.
`XccyScriptIntegrationTest.TestGeneralMcAdmissionDoesNotEscapeIntoXccyContext`
continues to pass. No numerical budgets, sampling, acceptance tolerances,
timeouts, quality rules or dependency pins change.

## Fresh local verification

Both OS builds use fresh exports of the immutable merge plus all five pinned
dependencies. Main archive SHA-256:
`9746edd0550cdebbac40fcbb1612761861c8c7a04edbd524d784b13689344132`.
The attachment records every one of 5,700 actual source/dependency inputs:
Linux is byte-identical; Windows has only 16 generator-produced CRLF/LF changes.
No semantic input mismatch exists. Build caches, compile commands, actual tool
versions, libraries, executables and extension/XLL SHA-256 identities are retained.

| Gate | Actual result |
| --- | --- |
| Original `build_linux.sh`, Python ON, GCC 14.3, CMake 4.2.3 | 3,896/3,896; exit 0; CTest 68.31 s, including workspace Python aggregate |
| Linux fast example label | 38/38; exit 0 |
| Linux original installed-consumer driver | 3/3; exit 0 |
| Linux standalone installed-DAL Python | 1,473 passed / 3 skipped; CTest 1/1; exit 0 |
| Original native `build_windows.bat`, MSVC 19.44, CMake 3.27.6 | 3,981/3,981; exit 0; CTest 1,362.53 s |
| Native workspace Python | 1,473 passed / 3 skipped; CTest 1/1; exit 0; pytest 96.14 s |
| Native standalone installed-DAL Python | 1,472 passed / 4 skipped; CTest 1/1; exit 0 |
| Native CMake 3.27.6 default installed consumer / Excel | 3/3 and 263/263; exit 0 |
| Native CMake 4.2.3 default installed consumer / Excel | 3/3 and 263/263; exit 0 |
| Attachment-only C++/Excel joint probe, each OS | 3/3; exit 0 |
| Attachment-only Python joint probe, each of four modules | 18 cases + 3 owned errors; exit 0 |

Linux uses a Linux-only tool PATH, `CC=gcc-14`, `CXX=g++-14`, four build/DAL
threads and `ADDITIONAL_CMAKE_FLAGS=-DDAL_BUILD_PYTHON=ON`, then calls the original
script. Extra gates use the original CMake installed-consumer driver, unmodified
standalone `dal-python` project against the fresh install, and CTest's existing
example/Python entries. Exact argv, cwd, environments, times and exits are attached.

Windows uses real native cmd/MSVC and the original full script, including its slow
European MC example. The unchanged slow example passes in 792.28 s; this is an exit-status observation, not a benchmark claim. The workspace Python extension is then enabled,
rebuilt/installed and tested; all seven already-tested native library/test/XLL
outputs retain identical hashes. Standalone Python separately consumes the new
installed DAL. Both default installed-package versions prove CP936 and empty
`CL`/`_CL_`; all 72 consumer/Excel-test compile commands use the exported static
CRT with no `/utf-8`, source-charset or execution-charset override. The XLL retains
its existing target-owned settings. No live workbook/registration smoke is claimed.

The historical validation report's Windows CMake 4.2.3 claim was corrected with
evidence by DAL-419: that original native tool was 3.27.6. This phase actually tests
both versions; it does not rewrite that report or its original failed default
installation/diagnostic-control facts. Optional mpmath oracle skips, the Windows
huge-allocation resource-limit skip and the standalone-only native opaque-helper
skip are reported as skips, never passes. Linux workspace Python has the original
aggregate CTest result; no unpreserved per-case count is invented.

The accrual formatter remains import-only. New callers forward into that same
path; no pricing/MC/AAD hot loop changed. Existing numerical and smoke workloads
pass with their original budgets. No unrelated full benchmark matrix or local
reproduction of every CI compiler/wheel/diagnostic leg is claimed. Setup-only
driver/command-selector corrections are retained in the raw setup note.

## Recoverable evidence and final delivery tree

Before removal, the two task-created directories were proved absent from accepted
master, archived using exact Git bytes/modes/blob IDs/SHA-256, CRC-checked and
actually extracted to disk. All 36 tracked files and 60 runtime files restore
exactly; nested original raw archives and all historical reports are preserved.

Recovery attachment: `DAL-418-recoverable-evidence-fb82515e.zip`, SHA-256
`2698132e975e6a2cfd40c56f20f4c8c65f5ae9580476becedaac980a72d39b2a`.
It contains `INVENTORY.json`, original paths, reports and runtime inventory.
Alternatively recover in a disposable checkout from immutable history:

```bash
git restore --source=fb82515e6d0af75824b592e7121ceb2e5e24c1c0 -- \
  .codex/artifacts/implementation/dal-418-evidence \
  .codex/artifacts/validation/dal-418-validation-evidence
```

The normal cleanup commit removes only those 36 tracked run assets and adds this
publication record plus its index entry. All other tested tree entries remain
identical, including formal regressions and accepted upstream changes. New probe,
audit and publication tools/logs stay in the final issue evidence attachment.
Nothing is force-pushed, and no Codacy rule/threshold or maintained code is edited
to suppress findings. Old report paths/commands refer to their immutable source
commit and are recoverable as above.

The same PR retains DAL-408 and adds `Closes DAL-418`. After local completion it
is pushed and made ready. Exact final-head CI is collected once in the foreground
with `gh pr checks 620 --watch`; its actual complete outcome is delivered in the
final issue attachment and PR checks, without creating another code head solely
to record CI. This record makes no advance claim that CI or merge gates pass.
The author does not merge; DAL-418/DAL-408 and the parent stay in progress for the
coordinator's final acceptance and protected match-head merge.
