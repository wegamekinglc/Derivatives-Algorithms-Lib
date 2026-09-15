# DAL-205 Excel FIX Documentation — DAL-246

## Outcome and Publication

Documentation now describes the implemented Excel FIX surface and the executable
workbook. The CHANGELOG records the new capability. This is the S4 documentation
handoff; parent acceptance and DAL-247 independent review remain separate.

- Existing draft PR: https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/376.
- Destination branch: `feature/dal-205-excel-fix-settings`; base: `master`.
- Source/S3 input HEAD: `71a0cbf6aad773856a946f73c82283712ba792c0`.
- Source/S3 input tree: `7f9294a1a78ff0750cb5759ff1753fc467b64509`.
- Required F7 ancestor: `9e6a55f58228a2c5b8101e08b6ab3551ae99a119`, verified.
- Publication HEAD/tree and the one post-push CI snapshot are supplied in the
  final issue comment and attached `delivery-identity.json`; this committed
  report cannot embed its own commit hash.

The initial Multica checkout was clean and matched GitHub's head/tree. All
changes remain on the existing PR. No product, generated, test, executable
fixture, API-note, implementation-report, or testing-report changes are included.

## Documentation Decisions and Discrepancies

Read the complete target public API guide, script-engine methodology, Excel
README, documentation index, and CHANGELOG before editing. Reconciled the
approved API note with current native/public settings and entry points, Excel
markup/parsers/immutable wrappers/JSON transport, actual generated registrations,
Python projections, portable and raw Windows tests, and the complete workbook
manifest/generator/runner/C++ consumer/Python verifier.

| Discrepancy                                                                   | Current documentation                                                                                                                                                     |
|-------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Public API and methodology said Excel had no settings or Describe/Explain     | Both describe the seven implemented functions and link the detailed guide.                                                                                                |
| Excel README only showed the legacy valuation path                            | Adds the settings/diagnostic workflow and executable workbook links.                                                                                                      |
| Snapshot documentation omitted the empty constructor and intraday distinction | Documents three optional parallel arrays, exact fractional timestamps, first-blank termination, and authoritative explicit empty snapshots.                               |
| No published workbook settings/default/error contract                         | Adds `docs/excel-script-settings.md` with exact names/order, two-column ranges, nullable handles, date bounds, booleans, physical row/column errors, and ownership.       |
| Recalculation and diagnostic transport were undocumented for Excel            | Specifies nonvolatile calls, fresh preparation, zero-history Describe, zero-worker Explain, no cache, and lossless JSON chunk concatenation.                              |
| No user-facing executed-fixture recipe or independent numeric reference       | Documents the real manifest/runner and consumers, exact `Values!M20` formula, historical PV160/d_SCALE80, discounted history, and distinct future fixing/payment oracles. |

The API note's opening “not implemented” and suggested workbook layout are S1
history. Current source and manifest take precedence: the committed fixture is
JSON, static `.xlsx` is generated, and the COM runner creates and saves the
executed workbook while adding long-output cases. Its actual zero-rate
historical formula occupies `Values!M20`. Worksheet names have no added `DA.`
prefix. Published docs contain current behavior, not delivery chronology.

The new guide is a worksheet/API guide under `docs/`, not a new methodology.
`docs/README.md` links it and updates the existing script-engine summary. No
methodology file was added, removed, or renamed, so the `CLAUDE.md` methodology
list needs no change. Existing methodology semantics are retained; only the
stale Excel paragraph and a cross-reference change.

**CHANGELOG: update warranted.** This is a significant public capability:
Excel gains settings-based named FIX valuation and contract/valuation
diagnostics, plus an explicit empty snapshot constructor. The dated entry
records those capabilities and compatibility. It makes no performance or
test-coverage claim. Prior dated C++/Python entries remain historical records.
Routine implementation cleanup and documentation formatting receive no entries.

## Validation Performed in This Documentation Run

Commands run from the repository root unless marked workspace. Exact outputs,
the audit driver, source identities, and publication checks accompany this
report in the documentation evidence attachment.

- `python3 .github/scripts/check_docs.py`: baseline passed for 67 Markdown
  files; final passed for 69, adding this report and the Excel guide. The check covers
  local links/anchors, table shape, trailing whitespace, metadata, math macros,
  current-state artifact placement, and repository workflow documentation.
- Workspace `python3 documentation-audit.py`: all seven documented registrations
  match actual names, argument order/optional markers, and nonvolatile metadata.
  The guide's complete formula matches the committed fixture exactly.
- The existing `generate-script-fix-workbook.py --output <workspace-output>`
  executes with Python 3.13.9/openpyxl 3.1.5. All 106 populated manifest cells,
  text-valued constant definition, manual calculation, and the 1900 date system
  match. This is static workbook generation, not a fresh Excel execution.
- Download hashes match the supplied latest testing report, implementation
  report, and S3 archive. All 132 archive manifest entries verify. All 1307
  baseline Git blobs reproduce the source hashes using Git's checkout filters;
  this includes prescribed CRLF batch-file conversion. An initial raw-blob
  comparison exposed that conversion distinction and was corrected in the
  evidence-only audit, without changing repository data.
- Recomputed 57 independent PV/AAD comparisons against the saved C++/Python/Excel
  S3 outputs using standard date arithmetic and exponentials. Reassembled all
  four saved Excel JSON columns without separators and compared complete
  decoded native documents. Long document lengths are 281617 and 127403.
  These are fresh audits of saved evidence, not new DAL pricing executions.
- New tables are padded according to the repository style. Changed files have
  final newlines and no trailing whitespace. `git diff --check` passes; exact
  staged name/scope and whitespace output is in the attached publication evidence.

An initial documentation check found a nonexistent Linux installation anchor;
the guide now links the real `#linux-workspace-build` section. No product or
example defect was found, and no implementation repair is requested.

## S3 Evidence Scope and Inheritance

The current tester attachment, rather than the historical report committed in
the source tree, is authoritative for execution results:

- `testing.md` attachment `01a0a52f-28f8-704c-930c-58d7381876a3`, SHA256
  `6fd8ff169eec3d1bdc238fa31f68862d75520a51e8c016602730f1f103ae47c4`.
- S3 archive `01a0a530-89a3-7138-9447-3a8f21b3eaf6`, SHA256
  `b6afeada1a2389573864b6a4dbe8462443eacdb9afbdbbddd2b431304b50c404`.
- Actual workbook `01a0a530-8e02-7de4-8511-39533cb874e6`, SHA256
  `9036cf77bcc7ca93f78c770b1cf8a25dee77876498d05e8056e4092c46c2228d`.
- Latest implementation report `01a0a518-abe8-77ef-a4a0-6f039e774a8b`, SHA256
  `456b04a980ca55db95fbe92ac40cba3c52dbbb49653204918aaf904b2d4bf1a6`.

S3 executed on `71a0cbf6`: Linux CTest 1795/1795, including the Python suite
with 641 cases; portable 46/46; Clang ASan/UBSan portable 46/46; generated
source drift and installed consumer 1/1; Windows XLL 60/60; actual Excel
283 assertions/47 outputs. These overlapping counts are not summed.
PSScriptAnalyzer 1.25.0 full defaults found zero Warning/Error and retained
30 Information diagnostics; all 16 per-site cleanup negative controls failed
as expected. S3 also audited 121 saved cells and four complete JSON documents.

The tester incrementally rebuilt affected Windows sources and relinked its own
XLL on that source head. Its SHA256 is
`7c07cfd32f40d62d98547ac71f1535a1e2fd10ad86487937b4cd1ee395ed8c67`.
Excel 16.0.20326.20144 x64 executed at 2026-09-15T12:57:34Z; registration,
pass, save, and own-process cleanup are evidenced. Full Windows core/public
suites were not run; full core/public tests ran on Linux. The earlier GCC
Debug sanitizer `Composite_` typeinfo link issue was not retried; the passing
sanitizer evidence is Clang. Unicode evidence exercises output through the
existing byte-oriented UTF-8 input transport and does not establish general
Unicode worksheet input support. The maximum-row guard was not stress-tested.

This documentation run does not rebuild or relabel that XLL. The delivered diff
from `71a0cbf6` is limited to the seven documentation/report files listed in the
publication evidence. Native/public/Python/Excel sources, registration sources
and generated files, build configuration, tests, executable manifest and
consumers, runner, dependency pointers, and other active reports are unchanged.
Their Git tree identities and an empty diff outside the allowlist are supplied
for the reviewer to determine inherited coverage. Full product suites were not
repeated for these documentation-only changes.

## Remaining Handoff

Keep PR #376 draft and omit closing keywords for GitHub #362 and DAL-205.
Parent DAL-205 accepts this report, closes the report stage, and supplies the
published head to DAL-247 for mandatory independent review and final gates.
Performance remains advisory. Take one nonblocking CI snapshot after publication;
pending results do not establish merge readiness. No reviewer/F9 dispatch,
merge, parent closure, or CI watch/poll is part of this run. Deliver one final
DAL-246 comment with this report and evidence, set DAL-246 `in_review --no-start`,
and recall parent DAL-205 only if its active-run read is empty.
