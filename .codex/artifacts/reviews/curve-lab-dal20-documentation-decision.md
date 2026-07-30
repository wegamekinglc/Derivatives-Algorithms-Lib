# Curve Lab DAL-20 Documentation Decision

## Baseline

- Integration branch: `feature/DAL-16-curve-lab`
- PR: `#265`
- Complete implementation head reviewed:
  `c6d83afffd1a8dd461feae2c3d13da8ea93cdb1f`
- Previously documented head:
  `61fd87e1a6e145260d1a6a11e45a8397f8ba0d55`
- Independent final-head verification: DAL-23, passed at the complete
  implementation head
- Product disposition: DAL-17, `Proceed with caveats`

The final implementation delta wires the stateless server canonicalization
response into the explicitly selected workspace instrument. Equivalent
`4/PERCENT` and `0.04/DECIMAL` authoring now persist the same canonical
financial identity, fingerprint, build/risk quote axes, and replay result.
Display convention and scale remain presentation-only state. DAL-18 final
review remains the downstream gate.

## Published current-state documentation

`docs/curve-lab.md` and `dal-web/README.md` now state the complete production
contract:

- the quote endpoint accepts an exact string lexeme, returns a canonical
  `raw_quote`, and does not itself persist a draft;
- the browser applies a successful response only to the explicitly selected
  instrument;
- input convention and lexeme plus display convention and scale are not part
  of the persisted draft or fingerprint;
- equivalent percent and decimal authoring has one persisted financial
  identity, quote/risk axes, and replay; and
- changes to the selected target, family, or draft invalidate an in-flight
  response rather than redirecting it to new workspace state.

No installed C++ or Python public surface, methodology document, discovery
entry, or generated contract changed in this delta. `docs/public-api.md`,
`docs/README.md`, and `CLAUDE.md` therefore require no update.

## CHANGELOG decision

No `CHANGELOG.md` change is required. Its July 2026 entries already record the
significant seven-family Curve Lab workflow, exact quote/risk axes, and
replayable sensitivity matrices. The final wiring is correctness closure
inside that recorded capability; it introduces no breaking public API, new
methodology or numerical algorithm, separate significant capability,
significant methodology shift, public removal, or deprecation. Adding another
entry would duplicate the existing capability narrative.

## Historical and control evidence

The following remain under `.codex/artifacts/` and are not presented as the
current user contract:

- product specification `curve-lab-dal-web-v0.5.md`;
- Revision 8 design package and technical design;
- technical-design critique, whose historical `## Verdict: Revise` remains
  unchanged;
- Revision 8 implementation plan; and
- DAL-23/DAL-19 repair plans.

The additive `curve-lab-dal17-final-disposition.md` records the later
`Proceed with caveats` product decision and its durable source mapping. It does
not rewrite the critique or serve as merge authorization.

At the reviewed head it also corrects the earlier production-wiring
overstatement: the quote API/client existed before the workspace applied its
response, while the current page passes the selected target token and callback
through `Curves.tsx`, `CurveLabQuoteAuthoring.tsx`, and
`CurveLabWorkspace.tsx`. The preserved technical-design critique still has its
historical `## Verdict: Revise` and remains byte-for-byte unchanged.

## Verification and review evidence

Source reconciliation at `c6d83aff` confirmed that the canonical response is
validated against the expected target token and family, then atomically
replaces only that row's `raw_quote`. Display state is not passed to the
workspace. Focused integration coverage verifies selected-row application,
failure immutability, idempotence, family changes, same-family target changes,
and display-only preferences. The browser scenario verifies persisted document
and fingerprint equality, identical build/risk axes and native payload hash,
and replay equality for percent and decimal forms.

The full guide was also rechecked against the earlier repair set: all seven
family selections reconstruct legal drafts; admitted build, import, and risk
IDs remain available for same-job polling recovery; and non-finite numeric
input returns stable `422 REQUEST_VALIDATION_FAILED` responses before draft or
audit persistence. Those existing descriptions remain accurate.

DAL-23 independently verified the complete implementation head and reported:

- 1,209 passing native CTest cases;
- 400 passing non-native backend tests, with 2 native tests deselected, plus
  the native binding integration test;
- 66 passing frontend unit tests and a successful production build;
- 10 passing real-API Playwright scenarios;
- clean generated-file and OpenAPI drift checks;
- all 58 GitHub checks passing.

DAL-20 locally reran the documentation checker across 40 Markdown files, all
18 checker unit tests, and the two focused canonical-quote frontend test files
with 11 tests; all passed.

DAL-17 issue state, decision metadata, and final comment remain consistent with
the durable `Proceed with caveats` disposition. That disposition does not
claim review readiness or close historical caveats without production wiring;
the current source and tests provide the wiring evidence, while DAL-18 retains
final-review authority.
