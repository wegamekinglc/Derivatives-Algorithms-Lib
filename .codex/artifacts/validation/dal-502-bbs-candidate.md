# DAL-502 full-risk BBS candidate evidence

**Candidate numerical evidence: PASS.** All twelve existing TREE actual bump
bindings self-qualify the complete six-component vector at every actual scale.
All 42 affected owning captures, four N3 premium/continuation controls and
fourteen separated American directions pass the candidate comparisons.
Mandatory TREE has **not** been replaced: current S3 stays **9 PASS / 3
INCOMPLETE**, N3/N6/N7 remain INCOMPLETE pending explicit adoption and independent
review of this controller/control/evidence. S4 stays paused, PR #685 draft,
without auto merge, and DAL-502 in progress.

## Frozen execution and scope

Decision `01a1b95b-ac6f-7cd2-8896-f5662f183e8b` is recorded in the active
[acceptance contract](../specs/dal-497-pde-risk-acceptance.md#approved-bbs-full-risk-candidate-stage-dal-502).
Baseline is `9af285a8971d34d7d74bf04288f60a527b10ec0a`. The exact
[plan](dal502-bbs-candidate/plan.json), controller and controls were committed
as `09640b0a` before any new native calculation. The runner verifies the plan
and contract against Git, reconstructs the frozen input union, checks the
permitted reuse receipts and validates both round and cumulative reservations.

The 132 physical inputs at paired 4096/4097, 8192/8193, 16384/16385 require
792 prices. Exactly **84** American spot rows from the repaired producer's
complete restart were reused, after matching physical input, n, method,
source/configuration/executable, original ID, axis/offset use, raw-file hash
and payload hash. No original CRR, old BBS producer or identity bridge supplies
a value. All **708** new prices completed once, with no error or missing row.
Their ten deterministic shards each stay below the unchanged native batch
6e9-node / 2e6-BS limits. At most four run concurrently, under 128 MiB address
space per worker; an unexpected failure would stop further launches and retain
in-flight results. No numerical process remains active.

The producer remains SHA256
`f6a72ce4dc17ac5bd62562ff33e73a21db4cac94aae0e36d5ff26a4bf5b8b518`;
`bbs.hpp` stays `748b35652a8fb0ef9d4a20d3c9832dab1708d55816840e8269feb5d1f8500955`.
[Identity](dal502-bbs-candidate/identity.json) binds exact dependencies,
configuration, compile commands, native and audit sources, raw files and all
42 capture/input pairs. Production library SHA256 remains
`7317377c982fd132c20bc8f74634feeb0ad422aa8d1d9dd424deb41077daa125`.
Production, numerical kernels, fixtures, existing references and build
configuration are unchanged. The existing 29 same-producer witnesses, 4/4
repair regressions and 41 high-precision proofs are reused without evaluation.
Review `01a1b95e-d573-76e5-a35b-f6d3f8726f7a` Approved the preceding spot-only
restart at fixed 9af285a8 and added **zero** nodes/BS. It does not cover this
full-risk stage. Its earlier six-call increment is already included in prior
consumption and is not charged twice.

## Actual-scale audit correction and regression

The first verifier used `physical(request)` when reconstructing derivatives;
that intentionally drops numerical bump controls. Eight nondefault bindings
therefore inherited default scales in the first analysis, although the frozen
price plan and all 792 physical prices were correct. This was a test-tool
defect; no price, kernel, threshold or product result was repaired.

The focused regression
`test_all_actual_binding_scales_survive_physical_input_normalization` failed
on 4/2/1 versus 1/.5/.25 before the fix. Audit commit
`55071227410ac2f242ab2f5158c1a2c9843ff11d` restores each binding's frozen
coarse/fine/extra-half controls and verifies its binding ID before estimating
risks. The test now passes; all seven candidate admission tests pass. They
also reject missing Vega, nonfinite/truncated vectors, threshold equality,
changed cached identity/payload/offset, excessive reservations, coarse Vega
mesh and individual raw parity failures, and an error PASS against an
unqualified reference.

The immutable [first analysis](dal502-bbs-candidate/first-analysis.json) is
preserved **as superseded, not qualification evidence**. The
[correction record](dal502-bbs-candidate/analysis-correction.json) identifies
all eight affected bindings and before/after proof hashes. The corrected
[qualified result](dal502-bbs-candidate/result-qualified.json) uses the same
native raw rows without evaluation. A separate
[final-source replay](dal502-bbs-candidate/final-source-replay.json) at the
fixed audit commit reproduces all twelve full proofs and the product audit
exactly, adding zero tree/BS/product calculations. Existing historical
FAIL/INCOMPLETE records and original raw data remain unchanged.

## Full-vector self-qualification

Component order is [Price/K, Delta, K*Gamma, Vega/K, Rate[0]/K,
DividendYield[0]/K]. At all three actual scales, final mesh and both raw final
parities, plus final fine-to-extra-half, must strictly satisfy
**[2.5e-5, 5e-4, 5e-3, 7.5e-4, 7.5e-4, 7.5e-4]**. No intermediate level
or parity is selected. The ratio below is the largest observed component
difference divided by its quarter limit over every required gate; it must
be strictly less than one. Vol/carry columns specify coarse values, each
followed by its half and quarter for fine/extra-half.

| Binding prefix | Market / S | Spot coarse/fine/extra | Vol coarse | Carry coarse | Maximum quarter ratio | Result |
|----------------|------------|-----------------------|------------|--------------|-----------------------|--------|
| b1166a77cf | PUT / 100 | 1/.5/.25 | .001 | .0001 | .534734229 | PASS |
| 277665fdba | PUT / 120 | 1/.5/.25 | .001 | .0001 | .183264472 | PASS |
| 325c81055c | CALL q=.08 / 100 | 1/.5/.25 | .001 | .0001 | .623822921 | PASS |
| e2a7ca0f1d | CALL q=.08 / 120 | 1/.5/.25 | .001 | .0001 | .263669771 | PASS |
| ba5f3ed104 | PUT / 100 | 4/2/1 | .001 | .0001 | .866147866 | PASS |
| 6b0637d9a4 | PUT / 100 | 2/1/.5 | .001 | .0001 | .302112348 | PASS |
| 52bbede0ef | PUT / 100 | 1/.5/.25 | .0005 | .00005 | .534734229 | PASS |
| c087fe5f6f | PUT / 100 | 1/.5/.25 | .00025 | .000025 | .534734229 | PASS |
| cb29dc3178 | CALL q=.08 / 100 | 4/2/1 | .001 | .0001 | .201938842 | PASS |
| f51dd42ace | CALL q=.08 / 100 | 2/1/.5 | .001 | .0001 | .350615483 | PASS |
| b60c5b84eb | CALL q=.08 / 100 | 1/.5/.25 | .0005 | .00005 | .623822921 | PASS |
| 1a799dc2b7 | CALL q=.08 / 100 | 1/.5/.25 | .00025 | .000025 | .623822921 | PASS |

The qualified result retains all **216 raw six-component member vectors**,
108 paired-mean vectors, per-scale final mesh/parity differences and every
fine-to-extra-half difference. [Binding summary](dal502-bbs-candidate/binding-summary.json)
maps complete IDs to all product aliases and actual scales. All 792 original
prices and producer-bound keys are delivered in the raw archive.

## Read-only affected product audit

The [candidate product audit](dal502-bbs-candidate/products-candidate.json)
reads the already reviewed current owning captures directly, verifying the
resolved source/input paths and hashes before and after reading. It checks
the frozen fixture input, exact five-risk order, actual observed coarse/fine
bumps, COMPLETE/AVAILABLE and scenario SUCCESS, finite six-component values,
and the original residual/capacity predicates. All **42/42** mandatory smooth
and hard-error comparisons, and **756/756** residual/capacity checks PASS.
Maximum product hard-limit ratio is **.3100466313** at N6-1-space-401.
No unqualified binding can publish an error PASS.

| N3 market | Max hard-limit ratio | Positive product premium | Continuation / candidate error |
|-----------|----------------------|--------------------------|--------------------------------|
| PUT S=100 | .0412829819 | .5164737310 | PASS / PASS |
| PUT S=120 | .0163107847 | .0749755902 | PASS / PASS |
| CALL q=.08 S=100 | .0411346026 | .3987305672 | PASS / PASS |
| CALL q=.08 S=120 | .0500442366 | 2.0996986351 | PASS / PASS |

The independent European premium values come from the original accepted
scalar receipt, verified against its prior archive manifest and matching
captured price/input. They were **not** reevaluated. Both final BBS base
members also retain continuation, obstacle hits and positive premium for each
market. All actual product error vectors are retained in the JSON record.

All **14/14** separated direction results remain PASS with byte-identical
current inputs and the same source-bound isolation evidence. Their required
finest candidate comparisons now PASS. Largest finest error/hard-limit ratios:

| Direction | PUT | CALL q=.08 |
|-----------|-----|------------|
| N6 space | .0412829819 | .0411346026 |
| N6 time | .0598986729 | .0606617452 |
| N7 domain | .0412829819 | .0411346026 |
| N7 penalty | .0412829818 | .0411346026 |
| N7 spot, grid801 | .0950713921 | .0905856971 |
| N7 spot, grid1601 | .0412829819 | .0411346026 |
| N7 parameter | .0412829819 | .0411346026 |

Candidate N3/N6/N7 evidence is PASS. This is separate from the current
mandatory-reference qualification and does not rewrite the
[American/P2 record](dal-502-american-directions.md) or its INCOMPLETE results.

## Resources, commands and remaining acceptance

| Account | Charged nodes | BS evaluations |
|---------|---------------|----------------|
| Prior BBS actual, including original six review calls | 8,869,730,816 | 1,511,876 |
| New-stage reservation | 41,587,475,848 | 6,766,946 |
| New-stage actual, 708/708 | 41,587,475,848 | 6,766,946 |
| Cumulative BBS actual | 50,457,206,664 | 8,278,822 |
| Cumulative BBS hard cap | 51,000,000,000 | 9,000,000 |

New actual visited work is **41,580,708,194 nodes**, with no unknown partial
attempts. Stage actual remains below 42e9 nodes / 7e6 BS. Original CRR work is
still **2,997,881,833,992**; combined actual is
**3,048,339,040,656 < 3,048,881,833,992**. There are no new scalar, witness,
product or review evaluations. Old actual, reserved and new actual work are
separate in [resources](dal502-bbs-candidate/resources.json).

Native window through the last returned price is **23.183981657 seconds**;
the original first receipt including initial analysis is **23.202475309
seconds**, both below 600. Largest worker RSS is **26,908 KiB**, at most four
workers (aggregate bound 107,632 KiB), within 128/512 MiB. Final execution raw
is **1,775,800 bytes**, below 64 MiB. Packaging adds no numerical work and
records payload sizes separately in [archive manifest](dal502-bbs-candidate/raw-archives.json).

Actual commands (each numerical command executed only once):

```bash
python dal-cpp/test-support/pde-risk-qualification/bbscandidate.py \
  --plan .codex/artifacts/validation/dal502-bbs-candidate/plan.json \
  --driver build/dal502-bbs-repair-tools/dal502_bbs
# Commit controls, exact plan and controller as 09640b0a before pricing.
python dal-cpp/test-support/pde-risk-qualification/bbscandidate_run.py \
  --plan .codex/artifacts/validation/dal502-bbs-candidate/plan.json \
  --output build/dal502-bbs-candidate/data \
  --driver build/dal502-bbs-repair-tools/dal502_bbs
# After focused RED/GREEN, audit saved native rows only; no pricing.
python dal-cpp/test-support/pde-risk-qualification/bbscandidate_run.py \
  --audit-only --plan .codex/artifacts/validation/dal502-bbs-candidate/plan.json \
  --output build/dal502-bbs-candidate/data \
  --driver build/dal502-bbs-repair-tools/dal502_bbs
python dal-cpp/test-support/pde-risk-qualification/bbscandidate_products.py \
  --plan .codex/artifacts/validation/dal502-bbs-candidate/plan.json \
  --output build/dal502-bbs-candidate/data
python -m unittest discover -s dal-cpp/test-support/pde-risk-qualification \
  -p test_bbscandidate.py
```

The archive includes raw batches, all 792 rows, exact plan, execution trace,
both analysis receipts, RED/GREEN logs, frozen execution source and final audit
source, producer snapshots and member hashes. Existing product raw archives
remain their original source; no product input was repriced. Seven new tests,
CCN<=8 and Semgrep zero findings/errors pass. Existing tests/builds/performance
are reused: only Python reference planning/auditing changed, with no production
caller or build/example change. Local docs and final-head CI snapshots accompany
delivery; final CI is not inferred from the earlier head.

| Exit | Current qualification | New candidate evidence / applicable prior scope |
|------|-----------------------|-------------------------------------------------|
| N1 | PASS | Prior unchanged European/default-upper evidence. |
| N2 | PASS | Prior unchanged analytic/contact evidence. |
| N3 | INCOMPLETE | Four full-risk candidate comparisons and premiums PASS; adoption/review pending. |
| N4 | PASS | Prior unchanged LOG-LCP/event evidence. |
| N5 | PASS | Prior unchanged deterministic/short-expiry/deep-exercise evidence. |
| N6 | INCOMPLETE | Four American directions and finest candidate comparisons PASS; adoption/review pending. |
| N7 | INCOMPLETE | Ten American directions and finest candidate comparisons PASS; adoption/review pending. |
| N8 | PASS | Prior unchanged C-profile evidence; its existing TREE binding is included in the twelve-binding union. |
| N9 | PASS | Prior unchanged current-library direct tests/40-layer audit/strict failure. |
| N10 | PASS | Prior unchanged boundary/domain/degeneracy/upwind/stress evidence. |
| N11 | PASS | Prior unchanged Gamma/spatial evidence. |
| N12 | PASS | Prior unchanged segment/parallel/LOG/selector evidence. |

There is no remaining numerical gap within this explicitly bounded candidate
scope. The required next decisions are independent review of the new planner,
corrected verifier/control/evidence and explicit coordinator reference adoption,
plus final-head gates. No additional levels, attempts, fixture expansion,
mandatory substitution, member dispatch or merge was performed.
