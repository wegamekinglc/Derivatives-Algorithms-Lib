# DAL-202 F5 documentation

Current-state documentation now covers named tree/compiled exact-double and
fuzzy-AAD valuation, shared observation loads, typed historical state, and
dependency-aware folding. This is a documentation report for parent acceptance
and subsequent independent DAL-231 review, not master delivery.

## Revision and scope

- Accepted tester head: `fbe2e9fb98ab15b6b9481d059050e1ef98581c0f`.
- Accepted tree: `2c2227e3f6a70b38e7cfb3ddcefd181961c5c0ca`.
- Fixed F4 base: `5c954ca2fdded2ca35ad15c7fef44d90a002007d`.
- Branch: `feature/dal-202-compiled-observations`.
- Draft PR: https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/372.
- PR base remains `feature/dal-201-historical-aad-state`; dependencies #371/#369.

The checkout's initial SHA/tree exactly matched the accepted tester publication.
The final issue comment and attached `publication.json` record the resulting
publication SHA/tree and verified remote PR head without embedding a
self-referential commit hash here.

Changed files are only `docs/methodology/script_engine.md`,
`docs/methodology/aad.md`, `docs/README.md`, `CHANGELOG.md`, and this report.
No C++, test, generated file, executable example, build configuration, submodule,
or dependency branch changed. No methodology document was added, removed, or
renamed, so the CLAUDE methodology list needs no edit. The docs index required
an update because it still listed named AAD/compiled/fuzzy as unavailable.

## Reconciliation

Read the complete target documents, current issue and parent descriptions and
bounded comment history, implementation/testing artifacts, role/style and full
publication contracts. Checked preparation, observation-plan reads, compiler
and stream ownership, domain/constant/condition passes, simulation guards,
public facade and binding signatures, executable script example, and nearby
analytic/parity, history, failure, and allocation tests.

Non-trivial stale claims corrected:

- Named compiled and prepared AAD compiled valuation were described as rejected.
  Both are supported by model-aware preparation; raw FIX and raw historical AAD
  guards remain. The guide distinguishes low-level fuzzy-double references from
  the exact-double Monte Carlo entry.
- Prepared execution was said to skip domain/constant-condition folding.
  It now documents prefetch before pruning, historical dependency domains,
  live script parameters, conservative prepared fuzzy kernels, eager AND/OR,
  and final IF/constant metadata before requested bytecode construction.
- Observation access and recording descriptions were restricted to tree
  evaluation. They now cover `LoadObservation`, separate payment sample mapping,
  shared plan ownership, hard historical bytecode, discarded settled PAYS,
  typed seed reconstruction per recording, and unchanged payoff-root lifetimes.
- The domain discussion assumed every variable starts at zero and all fuzzy
  conditions use the same domain-folding rules. It now distinguishes historical
  initial domains, prepared fuzzy runtime kernels, and legacy discrete bounds.
- The mode discussion did not include the prepared compiled-mode guard.
  It now requires matching type, effective compiled flag, and AAD epsilon, with
  fresh preparation for changed date, history, parameters, or execution settings.

Retained limitations: explicit spot binding to one ordinary EQ in BS/Dupire;
historical EQ/FX semantics; unsupported future FX/IR/composite/delivery/multi-asset
outputs; exact midnight and today policy; no historical fallback; non-atomic
global snapshot capture; history-only nonexpired preparation is not executable.
Named settings remain unavailable through dal-public/Python/Excel valuation,
default-index archives, and JSON debug schema `/1`. No F6-F8 surface is advertised.

CHANGELOG decision: **add an entry** under 2026-09-14. Named compiled valuation
and safe dependency/fuzzy optimization are a significant capability and
methodology shift under the documentation role contract. Prior entries remain
historical records; no testing or delivery narrative is added to published docs.

## Fresh documentation validation

Run from the repository root:

```bash
python3 .github/scripts/check_docs.py
git diff --check
git diff fbe2e9fb98ab15b6b9481d059050e1ef98581c0f --name-status
git diff --exit-code fbe2e9fb98ab15b6b9481d059050e1ef98581c0f -- dal-cpp dal-public dal-python dal-excel .github cmake CMakeLists.txt CMakePresets.json
```

The documentation checker passes for 70 Markdown files, including local
links/anchors, Markdown table structure, trailing whitespace, math macros, and
current documentation placement. The attached `validate_docs.py` additionally
audits final newlines and exact table padding in changed documents and asserts
the complete changed-path allowlist. Run it from the repository root with
`python3 ../evidence/validate_docs.py` after extracting the evidence alongside
the checkout.
The staged path list and whitespace check are captured immediately before
commit. Publication verification checks clean status, exact head/tree, ancestry,
remote branch/PR identity, and an empty code/test/configuration diff against the
tester head. Fresh logs and the reproduction script accompany this report.

C++ tests were not rerun for this documentation-only change. Accepted DAL-229
evidence remains the independent Linux build/install/CTest 1730/1730,
native/Adept/CoDiPack targeted 440/440 each, XAD 439/439, legacy parity/fuzz
33/33, and isolated allocation 2/2. These are inherited test results, not fresh
documentation-stage executions.

## Evidence limits and reviewer handoff

Allocation measurements cover native-double exact/fuzzy tree/compiled repeated
evaluation after buffers are constructed, not AAD tape allocations, arbitrary
C malloc calls, or every expression shape. Constant-time observation reads were
source-inspected, not dynamically counted. Compiler failure injection occurs
immediately before bytecode construction, not inside arbitrary opcodes. No
Windows, Python, sanitizer, or performance validation is claimed. DAL-223 stays
deferred; no optimization experiment or gate change was made.

The parent's 2026-09-14T08:34:22Z CI snapshot was available and not stale:
Codacy Static Code Analysis failed, 25 checks passed, and 17 were running.
This unresolved integration work carries forward. One post-push snapshot is
attached as `ci-snapshot.json`; it does not replace independent review or grant
merge permission. CI is not watched or polled. The coordinator must accept this
report, start existing DAL-231, and resolve integration gates before master
delivery. No close intent, merge, reviewer dispatch, or F6 start is performed.
