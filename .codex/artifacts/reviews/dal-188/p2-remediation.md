# DAL-188 P2 Threshold Contract Remediation

Status: author correction delivered for independent dal-reviewer reacceptance.
The existing Request Changes verdict remains pending that review.

## Scope and Ground Truth

The sole P2 finding concerns the still-effective threshold-review contract in
the retired `.codex/artifacts/plans/quote-space-dv01-implementation-plan.md`
at master `0870bc08c97fc5c4fc3db3758aaf0d9e5ffe6d8d`, lines 462–482.
The surrounding frozen-baseline definitions and per-bucket evidence requirements
are also retained by this migration. The source excerpt is included in the
attached evidence bundle; Git preserves the original plan.

The follow-up starts from PR #343 head
`f5641e45143ca34262c170dba9bfa9cd29e4c9a0` on
`fix/DAL-188-documentation-contracts`. Fresh remote-reference reads confirm
both master and the PR branch still match those commits before editing.

The complete target methodology and the independent review were read. Existing
fixed thresholds and per-bucket evidence in
`dal-cpp/tests/curve/test_quoteriskprovenance.cpp` agree with the methodology.
The missing governance rules come from the approved plan, not from executable
threshold-update logic: thresholds do not adapt automatically.

## Preserved Contract

The durable destination is
[Threshold baseline review and changes](../../../../docs/methodology/yield_curve_jacobian.md#threshold-baseline-review-and-changes).
Each original requirement is preserved:

- Frozen worst baselines `7e-7`, `1.8e-5`, and `1.3e-4`, the
  `T = Round125Up(5 * B)` derivation, upward `{1, 2, 5} * 10^k` rounding,
  and derivative/DV01 scaling remain explicit. Wider fixtures do not relax
  the existing `N > 10` band automatically.
- All seven review triggers remain: residual scaling/tolerance,
  parameterization/interpolation, supported success domains, oracle bump,
  axis-width bands, required fixtures adding `N > 16`, and any valid
  observation exceeding `T / 5`. The warning requires review even while the
  observation still passes the existing threshold.
- A change preserves commit/fixture/axis/platform/compiler/AAD-backend evidence
  and every raw bucket row, including width, gross PV scale, API/oracle values,
  errors, and threshold, identified by domain/mode/currency/bucket.
- The only update is `Round125Up(5 * max(B_old, B_new))`, using an approved new
  worst baseline; it cannot reduce the approved baseline through replacement.
- A new API-note/design revision and critic approval remain mandatory.
  Results above the current threshold fail before that review. Runtime and
  implementation-PR threshold self-learning remain forbidden.

The old baseline commit and historical validation observations remain in the
attached original excerpt. The current methodology retains the conservative
baseline values rather than substituting the tighter historical example result.

## Changed Files

- `docs/methodology/yield_curve_jacobian.md`: restore the continuing governance
  contract beside its existing numerical acceptance rules, using current-state
  language. Existing threshold values, oracle, and acceptance inequalities
  are unchanged.
- `.codex/artifacts/reviews/dal-188/README.md`: identify its inventory and test
  results as the initial delivery and link this follow-up so old hashes and
  counts are not mistaken for the corrected head.
- `.codex/artifacts/reviews/dal-188/p2-remediation.md`: record the finding,
  requirement mapping, focused checks, and reacceptance boundary.

## Validation and Handoff

`check_affected_docs.py` in the attached evidence invokes the repository's
`check_docs.py` link, table, whitespace, stale-command, math-macro, and referenced
path checks for the three changed Markdown files. It also checks incoming
Markdown links to those files and verifies final newlines. The exact affected
paths and results are recorded in `p2-validation.json`.

Run the attached helper from the checkout root:

```bash
python3 .codex/artifacts/reviews/dal-188/check_affected_docs.py
git diff --check
git diff --cached --check
```

These focused checks passed. No full documentation audit, checker unit suite,
roster refresh, C++ build, numerical test, or performance suite was rerun for
this bounded prose migration. No agent contract or Multica configuration changed.
No methodology file was added, renamed, or removed, and no threshold or algorithm
changed; index and CHANGELOG updates are unnecessary.

The PR remains #343 and is not merged. Return to dal-orchestrator for dal-reviewer
to recheck the sole finding and its direct impact. DAL-188 remains `in_progress`.
There are no user questions or execution blockers. The prior head's passing CI
is historical evidence; any new-head CI snapshot is supplied separately with
the handoff, without waiting for CI or claiming independent acceptance.
