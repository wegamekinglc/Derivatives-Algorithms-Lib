# DAL-419 integration and publication record

The original DAL-419 branch now contains the accepted master integration and passes
the affected default installed-package gates. The two reviewed code files are
unchanged. Publication and final-head CI are authorized by the current DAL-418
dispatch; the author does not merge. Final PR/head and CI results are retained in
the dispatch attachment after publication, without making another head-changing
commit solely to record CI.

## Integration identity

- Branch: `agent/dal-implementer/3103ab041f74`.
- Reviewed source: `909d59d3c49b3a116e2835511cf6ebaf6fa96840`.
- Accepted master: `acadd8ee44fdb6a0bb2e70de9d559db59de066f8`.
- Normal merge: `38f7c2837927094dd4be51f41aecc941fcfc54aa`, with precisely those
  two parents in that order. Git's ort merge completed without conflicts.
- The complete merge tree equals accepted master plus the original branch changes
  from its `bf4cabea` base. The accepted upstream delta has 132 files; no conflict
  resolution or new semantic change was introduced.
- `dal-cpp/dal/script/visitor/debugger.hpp` retains reviewed blob
  `76d8a7866ed1eb93b355b65932083bd08a05114e`.
- `tests/installed-consumer/main.cpp` retains reviewed blob
  `efb9ebcc20b87adc744caace9fd7b6e8aabeda82`.
- The reviewer report is preserved verbatim, SHA-256
  `f5852d6fc9a23575530acc6ad3690da73c280364881be4a08bc18d41de655c86`,
  and committed as an active task record. Its Approve is scoped to the reviewed
  fix; the mechanical integration and fresh gates below establish applicability.

Only the two original code files differ from current master outside DAL-419
artifacts. This phase adds integration/validation metadata and preserves the
review; it changes no product, formal test, CMake/export, dependency or public
contract. PR #620 and its checkout are not modified. No new design or
documentation stage is needed for this representation-preserving repair.

## Fresh affected local verification

The tested merge was exported to a new task-owned native Windows directory with
all five pinned dependencies. `source-audit.json` verifies **5,685 Git blobs**;
there is no content mismatch. Five Windows scripts have only the established
archive CRLF conversion. The unused nested RapidJSON GTest gitlink is not built.
The installed debugger header is byte-identical to the reviewed source.

Native CMake **4.2.3** freshly built and installed core/public, using MSVC
19.44.35228, Release C++17, native AAD, static CRT, six build jobs and four DAL
threads. Independent fresh consumer/Excel directories then ran the original
drivers with both native CMake versions:

| Native CMake | Installed consumer | Installed Excel                               |
|--------------|--------------------|-----------------------------------------------|
| 4.2.3        | 3/3 PASS           | 263/263 PASS; tests-OFF/ON XLL and audit PASS |
| 3.27.6       | 3/3 PASS           | 263/263 PASS; tests-OFF/ON XLL and audit PASS |

The 263 count is the actual new-master discovery, four more than the previous
259-case base. No case or acceptance condition was removed. Every driver observes
system and both console code pages **936**, and rejects nonempty CL/_CL_ before
and after VS setup. No code-page change or compiler charset override was used.
Actual Ninja rules for **73** consumer, Excel-test and compat compilations were
audited; all default-consumption rules lack UTF-8/source/execution charset options.
The XLL's existing private setting and the legacy Unicode golden test target's
existing setting are not used as substitutes for default consumption.

Native debugger/exercise **42/42 PASS**. All 66 symbol fields, widths 1/20/125 and
empty-product output preserve the real old-master oracle: **3,658 native bytes**,
SHA-256 `96d36cc6aa6479f06f786fa615dd02da86f25a8052226cec38ce22fb8922fd9b`.
The independent 384-literal payload/token audit is reused through the exact
reviewed header blob identity; no new token edit occurred in this phase.

Linux GCC14/CMake4.2.3 core/public were rebuilt and installed from the integrated
source. The original installed consumer passes **3/3**, and freshly linked
debugger/exercise tests pass **42/42**. Their **3,456-byte** renderer output equals
the original Windows oracle after CRLF translation, SHA-256
`c26f900342a333236173718a246e2899915f617b6af9d8871340d8841e665d96`.

The original native master C2001 RED and previous default GREEN remain in the
unchanged implementation/validation archives. Historical native Windows CMake was
**3.27.6**, as independently corrected and reviewed; 4.2.3 was the historical
Linux version. This phase explicitly runs both native versions and does not alter
those old reports or relabel their source/binaries as new integration results.
Existing C4819 warnings remain visible; warning-free default `/WX` consumption is
not claimed.

## Reproducible commands and evidence

The following are historical commands from immutable commit
`fd1996a4bfd122d9bfed0c1b4451e234d3eca7d9`. Their one-time tools and raw evidence
have been moved out of the delivery tree; recover them as described below before
replaying these commands in a separate checkout.

```bash
python3 .codex/artifacts/implementation/dal-419-publication-evidence/prepare.py
python3 .codex/artifacts/implementation/dal-419-publication-evidence/run_native.py build consumer-423 excel-423 focused consumer-327 excel-327
python3 .codex/artifacts/implementation/dal-419-publication-evidence/make_audit.py
python3 .codex/artifacts/implementation/dal-419-publication-evidence/run_native.py audit
python3 .codex/artifacts/implementation/dal-419-publication-evidence/audit_source.py
python3 .codex/artifacts/implementation/dal-419-publication-evidence/capture_local.py
```

Preparation preserves existing Windows trees and refuses to overwrite its export.
All native commands execute synchronously to terminal exit, recorded in
`run-build.json` / `run-audit.json`. `local-manifest.json` records source,
environment, actual compile counts, renderer bytes, **64 binary identities**, and
hashes for **98 raw files**. `raw-evidence.zip` preserves unmodified commands,
logs/XML, caches, Ninja rules, installed audits and byte specimens; it excludes
source exports, tool installations and executables. Linux commands/results are in
the same archive. Incremental diff and changed-line clang-format checks pass.

The change is constant spelling with identical payloads and code flow. No
numerical/AAD/Monte Carlo timing case is affected; no unrelated benchmark or full
product matrix was repeated solely for handoff. Fresh local scope is the affected
installation/caller path above. Final compiler, diagnostics, sanitizer and wheel
coverage belongs to the explicitly required final-head CI.

## Evidence migration and input preservation

The first publication head, `fd1996a4bfd122d9bfed0c1b4451e234d3eca7d9`, completed
all 36 checks: 35 SUCCESS and Codacy ACTION_REQUIRED, with watch exit 1. Codacy
check-run `119992959037` reported 173 new issues against its unchanged zero
threshold. The paginated GitHub API exposes only 50 annotations, all in this
task's one-time evidence Python scripts. The other 123 findings are unclassified;
this cleanup does not claim to classify or individually fix all 173.

The three task-created directories below were absent from accepted master and
are removed from the delivery tree after archiving their exact tracked bytes:

- `.codex/artifacts/implementation/dal-419-encoding-evidence/`
- `.codex/artifacts/implementation/dal-419-publication-evidence/`
- `.codex/artifacts/validation/dal-419-validation-evidence/`

The DAL-418 dispatch attachment `DAL-419-recoverable-evidence-fd1996a4.zip` contains
all **45** tracked files at their original paths, including tools, manifests and
the three original raw ZIPs. Its size is **1,789,961 bytes**, SHA-256
`61bcd9157b2943d21991eb97f2c0406e5adbd1673e8812abcc583f40855711da`.
`INVENTORY.json` records each path, Git mode/blob, size and SHA-256. All 45 members
were read back and matched to their original Git blobs; all 72 archive members
passed CRC checks. The archive also preserves the previous full CI watch log,
actual exit, provider snapshots, available annotations and historical reports.
It contains no dependency checkout, executable or build tree.

Normal Git history retains the source commit. In a disposable checkout, restore
the original directories with:

```bash
git restore --source=fd1996a4bfd122d9bfed0c1b4451e234d3eca7d9 -- .codex/artifacts/implementation/dal-419-encoding-evidence .codex/artifacts/implementation/dal-419-publication-evidence .codex/artifacts/validation/dal-419-validation-evidence
```

Alternatively extract those three subtrees from the attachment and verify them
against `INVENTORY.json`. Paths and commands in the unchanged implementation,
independent validation and review reports refer to this immutable evidence.

This phase changes only those 45 delivery entries and this publication report.
Every other tracked path, mode and blob matches `fd1996a4`, including all product
code, formal tests, build/export files, configuration, workflows and dependency
pins. The two code blobs also match approved `909d59d3` as recorded above. Thus
the independent review and the fresh integrated default-installation/caller
results remain applicable; no product or installation matrix is repeated for
this evidence-only change. Recovery, scope, report/link and staged-diff checks
are the local validation for this phase. No quality rule, exclusion, threshold,
required check or maintained-code behavior is changed. The new publication head
still requires its own complete foreground CI result, including Codacy; the old
35 successes are not substituted for that result.

## Publication boundary

Push only after the local gates above pass. The unique independent PR targets
master, includes DAL-419 in its title and `Closes DAL-419` in its body, and does
not close the parent task. Publish one final head, then collect its complete CI
terminal result with one foreground `gh pr checks <PR> --watch` invocation.
Preserve any failure/cancellation/timeout/unknown result; do not merge, submit a
GitHub review, resolve threads, dispatch experts or start #620 integration.
DAL-419 enters in_review on delivery; DAL-418/DAL-408 remain in_progress and #620
remains draft. The coordinator owns exact-head final acceptance and merge.
