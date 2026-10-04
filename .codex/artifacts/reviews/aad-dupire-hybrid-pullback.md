# F01 public Hybrid pullback review

Verdict: Approve for the published Hybrid increment. No unresolved local API
correctness findings; exact `c5c922ef` passes all 35 CI checks. Full F01
integration and the later [Python increment](aad-dupire-python-bindings.md)
remain independently open.

## Findings

The read-first review inspected the complete new public header/source/tests,
installed consumer, changed sanitizer filters, methodology and changelog, plus
the controlling specification/API note and numerical-oracle critique. The
retained malformed-JSON test first fails because the parser error omits its
input field, then passes after adding scoped `modelSnapshotJson` context to
model restoration. This is the production RED/GREEN correction in this increment.

The adapter derives offsets from typed components in actual runtime name order,
verifies the retained Hybrid model against the complete model-coordinate axis,
and requires every surface coordinate in the selected risk axis. It validates
ordered grids, extents and values independently of names, extracts raw mean PV
adjoints without report-factor conversion, and performs the core pullback after
numeric MC reduction. It does not invoke preparation, history lookup, another
simulation or worker calibration. Results own passive valuation/quote payloads.

Unsorted components and reverse selected-column ordering are explicit tests.
Boundary checks include missing zero-risk coordinates, changed unrelated
unselected model inputs, labels/units, grids/values, wrong model/component type,
missing/malformed JSON, unsupported/price-only execution, mismatched direct
quotes, original model mutation/destruction and outer-recording recovery.

The [aggregation oracle review](../critiques/aad-dupire-aggregation.md) preserves
the first failed strict distinct-seed comparison and an unchanged-published-core
reproduction. Two exactly equal trade seeds retain the original 1e-10 sum
assertion; both original distinct trades also pass independent full portfolio
recalibration under the controlling predeclared steps/tolerances. No production
calibration or reverse arithmetic changes. This is an oracle repair, not a
production gradient fix or proof of exact commutation for general floating seeds.

## Open questions and remaining scope

- Complete OFF/combined CTest passes 2,382/2,395. The OFF run includes all
  34 examples, including the existing billion-path European MC case.
- [Public costs](../performance/aad-dupire-hybrid-entry-cost.md) retain 1,280
  independent processes after all builds/tests finish, with unchanged numeric
  references and 1,040 input hashes. Existing default gate identities and P01's
  inconclusive production verdict remain separate from this capability cost.
- Public `c5c922ef` passes its own 35 exact-head checks. This accepts the public
  increment separately from the preceding core and subsequent bindings.
- Python/Excel factory/getters, common curve pullback adaptation and complete
  F01 acceptance remain required. No full F01 checkbox closes here.
- The mixed policy-secant source remains explicitly labeled. Mapping that
  model-coordinate secant does not establish a full quote-level retraining
  estimator; that requires its separately specified validation.

## Tests

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- Missing public API RED build: `aad-dupire-hybrid-api-red-build-01.log`.
- Malformed model snapshot RED/GREEN:
  `aad-dupire-hybrid-malformed-red-01.log` and
  `aad-dupire-hybrid-malformed-green-02.log`.
- OFF focused suite: 18 passes; `aad-dupire-hybrid-portfolio-off-01.log`.
  The 216 node and 105 quote/portfolio rows all pass; original 300 rows remain
  identical in `aad-dupire-hybrid-portfolio-oracle-identity-01.json`.
- Complete combined lifetime/profiling CTest: 2,395 passes;
  `aad-dupire-hybrid-full-combined-ctest-01.log`.
- Complete OFF CTest: 2,382 passes, including Python and all 34 examples;
  `aad-dupire-hybrid-full-off-ctest-01.log`.
- Fully instrumented ASan/UBSan: 28 public passes;
  `aad-dupire-hybrid-sanitized-green-01.log`. All 321 rows are identical to OFF
  in `aad-dupire-hybrid-sanitized-oracle-identity-01.json`.
  Leak detection is disabled; no leak claim.
- Two installed consumers pass each OFF/combined package:
  `aad-dupire-hybrid-{off,combined}-consumer-green-01.log`.
- Changed source/tests pass the unchanged CCN-eight threshold (57 functions,
  zero warnings); `aad-dupire-hybrid-complexity-03.log`.
  Changed C++ formatting passes; `aad-dupire-hybrid-format-02.log`.
- Final methodology integrity passes 92 files;
  `aad-dupire-hybrid-docs-final-01.log`.
- Generated verification passes with zero content drift across 497 files;
  `aad-dupire-hybrid-generated-final-01.log` and
  `aad-dupire-hybrid-generated-identity-01.json`. Identical generated timestamps
  are restored after checking; no compiled source content changes.

## Summary

The numeric Hybrid-to-quote chain, compatibility checks and passive wrapper
have focused, independent mathematical, complete OFF/combined, diagnostic,
installed-package, cost and documentation evidence. Publication CI acceptance
and the remaining F01 integration stay required. The full AAD plan remains active.
