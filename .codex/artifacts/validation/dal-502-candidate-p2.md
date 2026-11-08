# DAL-502 required-layer failure propagation repair

The candidate product auditor now propagates every required layer's existing
mandatory-smooth, solver-residual and capacity failures to its direction's
candidate status and N6/N7 summary. Reference accuracy still gates the finest
layer only. This repairs reviewer P2 at baseline
`8c3e272e2b2e94140642dfc054fe54a4701ab599`; focused review of the repair remains
pending. Current qualification stays **9 PASS / 3 INCOMPLETE**, N3/N6/N7 stay
INCOMPLETE, PR #685 draft without auto merge, DAL-502 in progress and S4 paused.

## Reproduction, cause and minimum fix

Reviewer attachment `review_intermediate_failure.py` was inspected and run
unchanged. It reads the real `N7-1-penalty-10000000.0` capture, changes only the
in-memory Base `maxLinearBackwardError_` to `1e-8`, and calls the real
`product_case` residual predicate, `direction_case`, then `group_status`.
The original result was residual FAIL / case FAIL / direction candidate PASS /
N7 PASS. The attachment asserted failure at the direction, returning nonzero.
The same attachment is now GREEN: case, direction candidate and N7 are FAIL.

The previous direction audit required finite values, the last numerical
difference, isolation and the finest comparison, but omitted mandatory-smooth
and original residual/capacity results for other participating layers. The
group summary then trusted direction candidate statuses. Thus an unchanged
finite numerical vector could conceal an intermediate solver/capacity failure.

Fix commit `8a7df04d5a702bb694bd823e6dd30bbffd3ffc11` changes only
`bbscandidate_products.py` failure propagation, its direct regression and test
data. `case_checks` combines the existing mandatory-smooth result with all
existing hard-check statuses; `direction_checks` requires those results for
every named required fixture. `direction_case` includes that admission in
`candidate_status` and records its evidence. `direction_group` rechecks the
same named fixture guards, so a cached PASS direction cannot hide newly failed
required predicates. Empty/unavailable hard evidence cannot produce PASS.

The separate numerical `direction_status`, differences, all thresholds and
finest reference comparison are unchanged. Intermediate `candidate_error`
accuracy statuses are not added to the required-layer guard. Negative controls
retain a nonfinest error FAIL or reference INCOMPLETE while the smooth and
original hard predicates and finest comparison pass; the direction and N6
remain PASS. This explicitly protects the original finest-only contract.

## Focused RED/GREEN using actual predicates

Eight new end-to-end data tests use exact field projections of four existing
captures. The projections preserve every stored value read by `product_case`
and `audit.general`, including all scenarios, risks and budget/residual values.
Their unmodified predicate outputs exactly equal the accepted case records.
Original capture/input hashes are retained in
[test-cases.json](dal502-candidate-p2/test-cases.json). Tests change only a
fresh in-memory copy and execute the actual product → direction → group path;
they do not substitute preassigned failure booleans or call pricing code.
The fixture is committed, so these narrow tests need no runtime native binary
or uncommitted raw capture.

| Counterfactual | Cases | Before fix | After fix |
|----------------|-------|------------|-----------|
| Original penalty1e7 Base backward error=1e-8 | 1 | case FAIL, direction/N7 PASS | case/direction candidate/N7 FAIL |
| Middle N6 obstacle/dual/complementarity/iteration residual exceeds original epsilon | 4 | case FAIL, direction/N6 PASS | case/direction candidate/N6 FAIL |
| Middle N6 and N7 mandatory Vega unavailable, finite values preserved | 2 | case FAIL, direction/group PASS | case/direction candidate/group FAIL |
| Middle N6 and N7 work/payload/workspace/native capacity exceeds original limit | 8 | case FAIL, direction/group PASS | case/direction candidate/group FAIL |
| Middle N6 solver status fails, finite values preserved | 1 | case FAIL, direction/N6 PASS | case/direction candidate/N6 FAIL |

All **16** corrupted-predicate cases reproduce false aggregate PASS on the old
source and FAIL on the fix. An additional regression verifies group rejection
with the unchanged cached PASS direction, giving **17 expected RED assertion
failures** across eight tests before the fix. All **8/8** tests now pass.
The unchanged reviewer attachment also goes RED → GREEN. Two finest-only
negative controls and the four exact baseline predicate outputs pass.
[Counterfactuals](dal502-candidate-p2/counterfactuals.json) retain mutation
paths/values, input hashes, original capture/input identities, predicate
failures and complete output-payload hashes for before and after results.

Actual narrow commands, from the repository root:

```bash
python ../attachments/dal502-candidate-review/review_intermediate_failure.py
python -m unittest discover -s dal-cpp/test-support/pde-risk-qualification \
  -p test_bbscandidate_products.py
python build/dal502-candidate-p2/verify_replay.py
```

The first command exits 1 before repair and 0 after; the second has the
recorded 17 RED failures before repair and passes eight tests after repair.
All logs, the unchanged attachment and both source versions are archived.
The final command checks the recorded before/after payload hashes, cached
direction guard, finest-only controls and exact same-raw replay, with zero
pricing; it also verifies all 146 unchanged file hashes before and after.

## Same-raw applicability replay

The affected top-level `bbscandidate_products.analyze` was replayed read-only
against the same frozen plan, corrected reference receipt, 792 cached prices
and original 42 reviewed captures. The append-only
[replayed audit](dal502-candidate-p2/products-replayed.json) shows:

- **42/42** candidate comparisons and mandatory-smooth results PASS; every
  case output is exactly equal to the accepted record.
- **756/756** original residual/capacity predicates PASS, unchanged.
- **14/14** direction candidate and finest comparisons PASS. Each new
  `required_checks` receipt is PASS. Removing only this appended guard field
  leaves each direction record exactly equal to its prior record.
- Four premium/continuation records are exactly equal; candidate N3/N6/N7
  results remain PASS. These candidate results do not adopt the reference.

[Unchanged inputs](dal502-candidate-p2/unchanged-inputs.json) verifies exact
before/after SHA256 equality for **146 files**: original price batches/values,
proofs, frozen plans, identities, resource records, capture/input pairs,
native executable and production static library. No old evidence was relabeled
or overwritten. Only the candidate aggregation Python source changes; all
other previously recorded source/dependency/configuration hashes match.

[Identity](dal502-candidate-p2/identity.json) pins baseline and repair source
hashes, new test/fixture hashes, the original reviewer attachment, native
source/configuration/executable and unchanged dependency evidence. The new
product-audit source SHA256 is
`d179400adadeb6bf595180463a96b1158a8768f4f9ae0a9872e31f55168872f9`.
Native remains
`f6a72ce4dc17ac5bd62562ff33e73a21db4cac94aae0e36d5ff26a4bf5b8b518`.

## Scope, resource ledger and handoff

**New tree nodes=0; BS evaluations=0; product evaluations=0**, including
counterfactual tests and reviewer reproduction. Prior BBS actual remains
**50,457,206,664 charged nodes / 8,278,822 BS**; including the immutable CRR
cost remains **3,048,339,040,656 nodes**. Resource controls, producer, kernels,
fixtures, original prices/proofs and old FAIL/INCOMPLETE evidence are unchanged.

The original numerical/method/spot-pilot evidence and the rest of the completed
full-risk review remain applicable. No reference proof, numerical matrix,
performance benchmark, existing broad test suite or production main review was
repeated. The changed path is a Python candidate-status aggregation with no
production caller or build dependency change, so existing independent core
build and scoped performance evidence is reused. No new build is needed.
Eight new targeted tests, CCN<=8 and Semgrep zero findings/errors pass; docs
and exact-head CI snapshots accompany delivery.

The [raw archive manifest](dal502-candidate-p2/raw-archives.json) binds source,
test data, original attachment, RED/GREEN logs, counterfactual receipts and
same-raw replay evidence. Original native/product raw archives remain their
accepted sources. The only remaining blocker work is independent focused
verification of this repair and its direct effect, followed by the
coordinator's explicit reference adoption and final gates. Existing
[full-risk candidate evidence](dal-502-bbs-candidate.md) and its N1–N12 table
remain historical; no qualification promotion or merge is performed here.
