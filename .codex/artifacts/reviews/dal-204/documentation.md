# DAL-204 Python FIX documentation

## Outcome and identity

S4 documentation is ready for parent acceptance and mandatory S5 review.
The published guides describe the implemented Python FIX surface and reference
the complete executable example. No product, build, test, generated, or example
source was changed. This report is the doc-writer's work; the archived S3 report
remains the tester's unmodified work.

- Issue: DAL-241; parent: DAL-204.
- Existing draft PR: [#375](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/375).
- Shared branch: `feature/dal-204-python-fix-settings`.
- Source input: `607584aaeba0596edf8e384a8137e5e518a3ddaa`;
  tree `d9dfc7b593681dcea3d8389f6882e9e66fae532e`.
- F6 ancestor: `a98bf9b07e9bd0fee23faa4cc9edca737f709654`, verified.
- Documentation/content commit: `a48befb9d2e35926b15545825a8704adaf7691da`;
  tree `869e8409c108cfeeeb5c3cfa7114260e5357cd16`.
- The following report-only commit records this document. Exact final delivery
  HEAD/tree, remote/PR equality, draft state, clean tree and the single post-push
  CI snapshot are in the attached `delivery.json` / `pr-state.json` and final
  issue comment. This avoids claiming that a commit can contain its own hash.

Multica checked out the explicit source ref into a clean independent workspace.
The existing remote/PR head matched that source. The family check showed only
this writer and the parent coordinator active; the other specialist stages
were not writing the branch. The checkout then followed the same feature branch.

## Reconciliation and changes

Read the target documents in full, the accepted F6/F7 API notes, implementation
and testing reports, current public/core settings and entry points, Python
bindings/helpers/wrappers, complete example, and direct script/settings/value/
snapshot tests. Source controls current behavior; API notes and earlier stage
reports retain their original delivery-time language.

Nontrivial discrepancies corrected:

- `docs/methodology/script_engine.md` said Python lacked the new settings,
  explicit dates/snapshots, bindings and diagnostics. It now identifies the
  current Python entries, shared preparation, result layers, and example while
  retaining the actual Excel limitations.
- `docs/public-api.md` listed only legacy Python valuation and said the new
  projections were not bound. It now includes the settings and diagnostic
  workflows, copies, strict paths, errors, schemas and links to full contracts.
- `dal-python/README.md` used the low-level `dates` spelling for high-level
  Product_New, omitted IRN from valuation RNGs, described paths only as positive,
  and omitted FIX settings and diagnostic boundaries. The reference now records
  the actual signatures, defaults, property input/output types, copies and
  failures, plus runnable complete-example instructions.
- The PR description still marked independent final-head tests and pybind11
  2.11.1 compatibility unverified. Its complete Summary/Test plan is rewritten
  using accepted S3 evidence and the actual S4 checks, with S5 still pending.

Contract coverage in the published text:

- Keyword-only settings and all three constructors; high-level `events_dates`,
  low-level `dates`/Cell rules; old 3–8 positional/keyword calls; settings objects
  versus dictionaries; None defaults; strict flags, paths and smoothing.
- Exact policy names and enum members, str/String binding dictionaries,
  dictionary duplicate-key limits, property copies, transactional setters,
  copy/deepcopy, native snapshot sharing and GIL ownership.
- Unquoted FIX, literal/event dates, midnight DateTime with explicit hour 0,
  past/today/future sources, no lookahead or missing-history fallback, explicit
  single ordinary-EQ future binding, and legacy SPOT identity rules.
- Global capture versus authoritative empty/old explicit snapshots, sequential
  capture and concurrent-write limitation, fresh preparation with no future-PV
  cache, hard historical AAD state and fuzzy future conditions, tree/compiled
  selection, already-normalized PV/d_ results and no fixing-risk keys.
- Describe's pure contract/no-market-I/O behavior, Explain's independent default
  exact/tree preparation with no workers, high dict/low JSON schemas and actual
  ID mappings, old DebugJson /1 and absence of a public Python product archive.

**CHANGELOG decision: add one entry under 2026-09-15.** Python access to named
valuation configuration and diagnostics is a significant public capability;
the stricter path contract also deserves an explicit compatibility note. No
test counts, CI history or delivery narrative were added to published docs.
Earlier changelog entries retain their historical scope.

No methodology document was added, removed or renamed, so `docs/README.md` and
the CLAUDE methodology list require no index change. No Excel projection or F8
work was introduced. Performance is advisory and no benchmark gate was used.

## Fresh S4 validation

All recorded verification commands exited 0. Exact expanded argument vectors,
UTC times, cwd, environment overrides and exit codes are in companion JSON/logs
inside the fresh evidence attachment.

- `python .github/scripts/check_docs.py`: passes for the public docs, local
  links/anchors, Markdown tables, whitespace and final newlines. The final run
  also includes this report and the unchanged testing report.
- `git diff --check` and `git diff --cached --check`: pass. Staged scope is
  proven before each commit; changed files are only the four published Markdown
  files and the two allowed stage reports.
- Complete `dal-python/examples/009.fix_settings.py`, executed through `runpy`
  after same-process module verification: passes under normal and optimized
  Python. Both produce `PV=260.00000000000006`, `d_SCALE=80.0`, valid PV/d_ keys
  and the two expected schemas. Existing PV absolute tolerance 2.6e-10 and risk
  tolerance 1e-10 remain unchanged.
- Existing script/settings/value/API/snapshot regressions: **255 passed** in
  2.62 seconds, including all **195 F7 cases**, no skips. `python-related.xml`
  and the complete captured log retain the results.

The new docs reference the complete existing example rather than copying its
code into a second implementation. The added shell command invokes that file;
signature listings are marked `text`. No new executable Python snippet or test
was authored. The local runner only verifies identities and invokes existing
code/tests, and lives outside the repository.

### Module and source provenance

These are fresh executions using the independent S3 installed cp313 wheel,
**not a fresh S4 native build**. S4 first verifies the loaded module SHA256
against the accepted S3 report, compares all **771** source/test/build input
hashes against the current checkout, and compares the loaded `api.py` and
`__init__.py` byte-for-byte with current source. Each execution verifies the
same identities in the process that runs the example or pytest.

- Linux x86_64/WSL2, glibc 2.43; Python 3.13.9 (Anaconda interpreter).
- Loaded package: S3 task's `evidence/venv313/lib/python3.13/site-packages/dal`;
  absolute interpreter/module paths and `ldd` are retained in the logs.
- Extension: `_dal.cpython-313-x86_64-linux-gnu.so`;
  SHA256 `cd0bac5e72a36ced892d81e3135d24e182bcb8c7551d251ce2f7e80c7227ce24`.
- pybind11 **2.11.1**; native **AADET** runtime. GCC **15.2** is the S3 build
  compiler, distinct from GCC 11.2 shown in Anaconda's interpreter version.
- Example SHA256:
  `f88259a2542b10a95c03b4fe3116894f01e680be2ff131975ea8c9c9a55488d2`.
- `PYTHONPATH` was cleared and bytecode writes disabled; the active interpreter
  loaded the exact installed package, with no package/environment mutation.

Reproduction after building/installing the matching module uses these logical
commands; `run_module.py` records identity before execution. The attached JSON
contains the exact interpreter path used here:

```text
python evidence/run_module.py example
python -O evidence/run_module.py example
python evidence/run_module.py pytest dal-python/tests/test_script_settings.py dal-python/tests/test_fix_valuation.py dal-python/tests/test_script.py dal-python/tests/test_value.py dal-python/tests/test_api.py dal-python/tests/test_xccy_resettable.py dal-python/tests/test_curve_pricing.py -q --junitxml=evidence/python-related.xml
python .github/scripts/check_docs.py
git diff --cached --check
```

Run from the repository; in this task the evidence directory is its sibling.
The runner's S3 hash input comes from the authenticated evidence attachment.

## S3 archive and evidence limits

Downloaded through authenticated `multica attachment download`, verified the
two supplied hashes, and verified all **130** manifest-listed archive files.
Copied `testing.md` byte-for-byte into its authorized repository path:

- Report attachment `01a0a3ed-68c5-72d1-ac57-fc93382aed78`, SHA256
  `e4be50153b4e06011097a86d5b0f71d4ff1df9448a3aba70cd0b6bbc4c337a03`.
- Evidence attachment `01a0a3ed-72e4-7b68-afb2-6236befb2ec2`, SHA256
  `297a508e2be9a119f34783c9869f13e3b41302cde067390931807c20deeba85e`.
- F7 API note remains unchanged, SHA256
  `d158902e5fae1def90ae844ab1e70eaec0a9d62b80499a06c135f11c6f21b605`.

Accepted independent S3 results at the source input are inherited evidence:
1778/1778 native CTests including Python597, separate Python597, affected255,
fresh ASan+UBSan255 (host-Python leaks off), consumers2/2; actual pybind11 2.11.1
cp39/cp313 standalone wheels each **596 passed / 1 skipped**. The only wheel skip
is the unpublished private quote-risk fixture; all 195 F7 cases pass. S3 also
ran the five corrupted-output probes. S4 does not relabel these as its own work.
S2 CoDiPack597 remains implementer evidence from
`fd9c42d07b54506366652f6559ddf1e0fff093e8`, using pybind11 3.0.4.

No S4 full native regression, sanitizer rebuild, wheel build, cp39 run, alternate
AAD, Windows/XLL, macOS, intermediate Python, or manylinux container execution.
S3's local wheels are linux_x86_64 artifacts, not repaired manylinux releases.
The focused S4 execution and source checks are proportionate to Markdown-only
changes. No actionable implementation discrepancy was found in this scoped
documentation reconciliation; this is not the mandatory independent review.

The PR remains draft with no final closing lines or merge. After the sole final
DAL-241 comment and `in_review --no-start`, check the parent active runs and use
the dedicated rerun only if idle, verifying its returned run ID. The parent owns
S4 acceptance and promotion of the existing DAL-242 review stage.
