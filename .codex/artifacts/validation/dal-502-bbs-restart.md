# DAL-502 repaired-producer BBS diagnostic restart

**Diagnostic pilot: PASS.** All 113 frozen requests completed with the repaired
producer: 29/29 controls/witnesses passed before 84/84 American prices were
executed. Both markets' three spot scales pass the required final mesh,
raw-parity and fine-to-extra-half gates. This is evidence for the approved
spot-only pilot, not approval to replace mandatory TREE or expand its scope.
N3/N6/N7 remain INCOMPLETE; S3 remains unqualified and S4 paused.

## Control and applicable identities

Baseline is `8c90d53bd7eb5630554698e346b474ce5b710e39`; kernel implementation
is `8607092c8ccd6c9c1c2df1a08387577112e9570b`. The restart decision
`01a1b947-09c0-7561-aea5-6d61b118c7de` was added to the active
[acceptance contract](../specs/dal-497-pde-risk-acceptance.md#approved-single-repaired-producer-bbs-restart-dal-502)
and committed as `dd4ba5a164df83c4d1e10ac9afa1cff7e937a7e1` before computation.
Its execution snapshot SHA256 is
`f2260c21a1bf31f1e48576861ae238cf39223e19ac72faff214efc044d5a60a5`.
The later accounting addendum imports six already executed review BS calls;
it does not alter the executed plan, kernel, limits or results.

The [identity record](dal502-bbs-restart/identity.json) binds:

- Method: `BBS-last-step-v1`; native executable SHA256
  `f6a72ce4dc17ac5bd62562ff33e73a21db4cac94aae0e36d5ff26a4bf5b8b518`.
- `bbs.hpp`: `748b35652a8fb0ef9d4a20d3c9832dab1708d55816840e8269feb5d1f8500955`;
  unchanged CRR header: `37bade3da00dc2f983ff08a5a68af4491d37a16bc75fc3a70a782430a23ad110`.
- Configuration: `4eea84d21d1e98de6afafec079ba56eeafcfc6bbbe2ad92106b3efdfa3bad9f9`;
  compile commands: `d9b4482847e7be093ff51c987fdd4f8e24d7525ef7164c7e7bb5fcfe0dd45943`.
- Exact dependency commits, standard-runtime-only link command, verifier and
  controller hashes, raw input/output hashes, and empty identity bridges.

The delivered executable/source/configuration and unchanged production static
library were verified before preparation, witness admission and final analysis.
All 113 jobs have new producer-bound identities; none uses the old producer's
49 successful values. Original failures, reservations, CRR and historical
INCOMPLETE records remain intact. The accepted repair's 4/4 tests and 41
high-precision witnesses were reused without rerunning them. No source, test,
build configuration, production capture, fixture or threshold changed here.

## Witnesses and raw reconstruction

| Evidence | Observed result |
|----------|-----------------|
| 12 European controls against independent scalar BS | PASS; maximum normalized error 1.4065759860e-6 < 2.5e-5 |
| Nine explicit one/two-layer formulas | PASS; maximum normalized difference 7.1054273576e-17 < 1e-12 |
| Eight expected failures | PASS; required errors and partial work retained, no published price |
| 84 American prices | All complete and finite; no unexpected errors |
| Twelve base American obstacle/premium checks | PASS; continuation at root, positive obstacle hits and premiums 0.3991412929 to 0.5170057255 |
| Independent reconstruction from raw native rows | PASS; 113 unique job IDs, all 36 raw three-component vectors exactly equal the verifier's vectors |

[Witnesses](dal502-bbs-restart/witnesses.json) preserve fixed inputs, scalar
values, formulas, native values and expected errors. The scalar verifier is
independent of the native BS implementation. This execution used 37 scalar
calls, including the twelve American base-premium checks. No scalar or tree
evaluation was performed during export/reconstruction/account reconciliation.
[Raw reconstruction](dal502-bbs-restart/raw-reconstruction.json) independently
forms price/K, central Delta and K*Gamma using each raw three-price stencil,
then compares the arithmetic results exactly with the admission receipt.

## Six spot self-qualification groups

Both N3 contracts have S=K=100, vol=.2, r=.05, T=365 days; PUT q=0 and
CALL q=.08. All original paired levels 4096/4097, 8192/8193 and 16384/16385
are retained. Vectors below are ordered **[Price/K, Delta, K*Gamma]**.
Mesh compares the final paired mean with the preceding paired mean. Raw
parity compares both final members. Every component is strictly below its
original quarter threshold **[2.5e-5, 5e-4, 5e-3]**. No intermediate pair is
selected as the final result.

| Market | Spot scale | Final mesh vector | Final raw parity vector | Result |
|--------|------------|-------------------|-------------------------|--------|
| PUT | 1 | [3.9779668551e-7, 6.9774969225e-7, 6.6752369854e-6] | [1.1172159441e-8, 4.2774488085e-7, 1.7058280575e-4] | PASS |
| PUT | .5 | [3.9779668551e-7, 7.5618684248e-7, 1.8918006912e-4] | [1.1172159441e-8, 4.8673678865e-7, 1.5105617383e-3] | PASS |
| PUT | .25 | [3.9779668551e-7, 8.8484335503e-7, 8.1160748024e-4] | [1.1172159441e-8, 1.3577413291e-6, 2.6736711447e-3] | PASS |
| CALL q=.08 | 1 | [4.4574867360e-7, 2.6408982778e-8, 5.3703620528e-5] | [1.2406627420e-8, 6.2446854354e-8, 1.9410029291e-4] | PASS |
| CALL q=.08 | .5 | [4.4574867360e-7, 1.0499377412e-7, 9.0384425810e-5] | [1.2406627420e-8, 2.8328738733e-7, 1.7530774169e-3] | PASS |
| CALL q=.08 | .25 | [4.4574867360e-7, 3.0703116316e-7, 2.8427660084e-4] | [1.2406627420e-8, 6.6072085581e-7, 3.1191146050e-3] | PASS |

Final paired-mean fine(.5) to extra-half(.25) vectors are
PUT [0, 2.7056681137e-5, 2.6839698286e-5] and
CALL [0, 1.0420816046e-6, 5.8335452913e-5], both strictly within the same
limits. [Execution receipt](dal502-bbs-restart/restart-result.json) retains
every raw member, paired mean and full-precision comparison at all levels.
Vega and other parameter components were outside this pilot's authorized
scope and are not claimed self-qualified.

## Actual resource account

| Account | Charged nodes | BS evaluations |
|---------|---------------|----------------|
| Prior BBS including bounded repair, immutable consumed work | 3,230,750,980 | 594,267 |
| Original restart reservation | 5,638,979,836 | 917,672 (917,572 native + 100 scalar slots) |
| Actual restart, all 113 requests | 5,638,979,836 | 917,603 (917,566 native + 37 scalar) |
| Concurrent review, imported receipts only | 0 | 6 (two native + four independent scalar) |
| Actual cumulative BBS | 8,869,730,816 | 1,511,876 |
| Cumulative BBS cap | 9,000,000,000 | 2,000,000 |

The corrected full-plan BS expectation **1,511,945 is a reservation, not actual
consumption**. [Resource account](dal502-bbs-restart/resource-account.json)
keeps prior work, original reservation, actual execution and review increment
separate. The original execution receipt's 1,511,870 subtotal predates the
six-call review increment and is preserved unchanged. Actual current-window
BS including that review is 917,609, below the 1,000,000 round cap.

Actual restart visited nodes are 5,638,062,147, below conservative charged
work. Immutable CRR consumption is 2,997,881,833,992; combined charged work is
**3,006,751,564,808 < 3,006,881,833,992**. Round charged work remains below
5.7e9. No unused balance was reassigned or spent on another attempt.

The restart's numerical execution window is **3.195091486 seconds**, below
600 seconds; maximum native worker RSS **4,396 KiB**, at most four workers
under 128 MiB address-space limits, hence within 512 MiB aggregate. Original
raw execution data occupied **365,092 bytes** before later reporting metadata,
below 64 MiB. Archive metadata records final payload sizes separately.

Review `01a1b94d-d29c-7480-97e6-8d6b4e64ab41` independently **Approved** the
method/private tail repair at fixed `8c90d53b` with no new code blocker.
[Review context](dal502-bbs-restart/review-context.json) binds the original
attachments and [complete receipt](dal502-bbs-restart/switch-check-complete.json).
The next representable x>=8 PUT/CALL inputs agree with independent 160-digit
values to relative differences 1.2583717092e-13 and 3.2223979693e-14. The
review receipt explains the initial inherited pre-exec Python RSS high-water
mark and preserves the isolated actual-image peak of 17,648 KiB; native peak
is 22,768 KiB. These six BS calls are reused, not repeated. This code approval
does not cover the newly appended restart controls/evidence; those await
direct applicability review.

## Reproduction and unchanged qualification gaps

The attached raw archive includes all inputs/native outputs, ledger, scalar
witness receipts, frozen execution contract, exact controller/export scripts,
review attachments and producer/configuration snapshots. Member and archive
hashes are in [archive manifest](dal502-bbs-restart/raw-archives.json).
Actual foreground commands, from the repository root:

```bash
python build/dal502-bbs-restart/run.py prepare
xargs -n1 -P4 prlimit --as=134217728 -- \
  ./build/dal502-bbs-repair-tools/dal502_bbs --batchfile \
  < build/dal502-bbs-restart/data/witness.workers.txt
python build/dal502-bbs-restart/run.py witness
xargs -n1 -P4 prlimit --as=134217728 -- \
  ./build/dal502-bbs-repair-tools/dal502_bbs --batchfile \
  < build/dal502-bbs-restart/data/pilot.workers.txt
python build/dal502-bbs-restart/run.py finish
python build/dal502-bbs-restart/record.py
python build/dal502-bbs-restart/reconcile.py
```

The completed ledger refuses another native restart. Export/reconciliation
can be reviewed from saved data; a new pricing execution requires a new
explicit decision. No tests were authored/repaired/run in this batch. Existing
production verification, independent core builds, repaired BBS regressions and
performance evidence are reused within their proven source/configuration
scope. This batch changed no build/example, so no new build was needed.

| Exit | Verdict | Applicable evidence or remaining gap |
|------|---------|--------------------------------------|
| N1 | PASS | Prior Q European/default-upper evidence unchanged. |
| N2 | PASS | Prior analytic/contact evidence unchanged. |
| N3 | INCOMPLETE | Mandatory Q TREE and full risk vector remain unqualified; this spot-only BBS pilot is not a replacement. |
| N4 | PASS | Prior LOG-LCP/event evidence unchanged. |
| N5 | PASS | Prior deterministic, repaired short-expiry and deep-exercise evidence unchanged. |
| N6 | INCOMPLETE | Separated American space/time directions pass; finest mandatory TREE comparisons remain unqualified. |
| N7 | INCOMPLETE | Separated American domain/penalty/spot/parameter directions pass; finest mandatory TREE comparisons remain unqualified. |
| N8 | PASS | Prior C profile with its qualified 4/2/1 reference unchanged. |
| N9 | PASS | Prior current-library direct tests/40-layer audit/strict failure unchanged. |
| N10 | PASS | Prior boundary/domain/degeneracy/upwind/stress evidence unchanged. |
| N11 | PASS | Prior sub-cell/finite-scale Gamma/spatial evidence unchanged. |
| N12 | PASS | Prior segment/parallel/LOG/selector evidence unchanged. |

The integrated S3 result remains **9 PASS / 0 FAIL / 3 INCOMPLETE**, with the
[American/P2 record](dal-502-american-directions.md) and
[original follow-up](dal-502-pde-risk-followup.md) retaining full product
residual/risk identities. No product inputs were repriced here. PR #685 stays
draft, DAL-502 in progress and S4 paused. New control/evidence review and any
full-risk/all-binding reference decision remain outstanding. Local docs and
final-head CI/Codacy snapshots are reported with delivery; no code gate is
inferred solely from the earlier head's status.
