# DAL-502 bounded BBS diagnostic

**Pilot verdict: INCOMPLETE.** The 29 frozen witnesses/European controls PASS,
but three perturbed American prices failed the European terminal-value guard.
The workers stopped and 32 planned prices were not executed. No repair/retry,
additional level, favorable intermediate selection or budget extension followed.
BBS American feasibility is unestablished; it does not replace mandatory TREE.
N3/N6/N7 remain INCOMPLETE, S3 is not qualified, and S4 stays paused.

## Frozen scope and implementation

This follows coordinator decision `01a1b8ef-6fc2-7ebd-bda6-d984fc11b161`,
recorded in the active [acceptance contract](../specs/dal-497-pde-risk-acceptance.md#approved-bbs-diagnostic-pilot-dal-502)
before any new numerical execution. Baseline is the delivered direction/P2
head `d96a89e8d438eb06c69ac3a4b1884ce64b286099`. The opt-in implementation
commit is `2867fcaf4bda89b4d40cc6860e597bc14561c1bf`.

`bbs.hpp` retains CRR u/d/p and discount, inserts independent scalar European
BS at n-1, applies the intrinsic maximum there for American, and uses the
obstacle at every remaining rollback layer. It has no Richardson or product
pricing call. `reference.hpp`, pure CRR values, production, product fixtures,
Q/C/L caps and error thresholds remain unchanged. The new executable accepts
only explicit `BBS-last-step-v1` jobs. Its cache keys include method, physical
input/spot offset, n and source/configuration/executable; no CRR bridge exists.

The method's penultimate-node construction is supported by
[Chung and Shackleton (2002), pp.145,149](https://scholars.lib.ntu.edu.tw/bitstreams/7e27fcda-7bd5-4c3d-baff-5e2dcfff545e/download).
Their numerical Greek study is European; it cannot establish this American
pilot's qualification. The final condition is an approximation to the last
American interval, not an exact continuous-exercise solve.

The fixed native plan has 113 jobs: 84 N3 S=100 PUT/q=.08 CALL spot prices
at 1/.5/.25, paired 4096/4097,8192/8193,16384/16385; 12 European controls;
nine one/two-layer explicit-formula witnesses; eight expected failures.
All concrete witness inputs and limits were fixed in the contract first.
European controls disable the American obstacle. Formula witnesses check
both continuation and exercise, including deep ITM cases and two-layer
penultimate exercise that cannot be hidden by the root obstacle.

## Results and stop

| Evidence | Result |
|----------|--------|
| European controls, 12/12 | PASS; maximum normalized scalar-BS error 1.4065759860e-6, strict limit 2.5e-5 |
| One/two-layer formulas, 9/9 | PASS; maximum normalized difference 7.1054273576e-17, limit 1e-12 |
| Expected failure witnesses, 8/8 | PASS; typed errors, no published price, partial work retained |
| American pilot | 52/84 attempted: 49 prices returned, 3 errors, 32 not executed |
| Available American continuation | All 49 returned prices have root continuation and positive obstacle-hit counts |
| Available unbumped premiums | All eight available base prices have positive premium over the saved independent scalar European control |
| Final pilot self-qualification | INCOMPLETE for all six market/scale combinations |

[Witnesses](dal502-bbs-pilot/witnesses.json) retain inputs, independent scalar
values, observed values, errors, root/terminal exercise counts and failure
messages. The scalar verifier uses erf and computes d2 first, independently
of the native erfc/d1-first formula. All 25 scalar BS evaluations are counted.
The overflow failure witness retains two attempted nodes and one BS call;
invalid-n preflight witnesses perform no tree or BS calculation.

The three unexpected errors are preserved without replacement:

| Market | n | S | Error | Visited nodes / BS calls |
|--------|---|---|-------|-------------------------|
| PUT | 8192 | 99 | BBS invalid European value | 4118 / 4118 |
| PUT | 8193 | 100.5 | BBS invalid European value | 4115 / 4115 |
| CALL q=.08 | 16385 | 99 | BBS invalid European value | 8177 / 8177 |

All occur during penultimate initialization. The source guard rejects a
nonfinite or negative scalar European value, rather than publishing it.
For these bounded finite pilot inputs the formula's arguments/products are
finite; a small negative tail value caused by cancellation/underflow is the
inference from that range and the guard. The raw record does not retain the
rejected floating-point value, so its exact magnitude is **not established**.
No post-stop BS evaluation was used to infer it. A stable-tail implementation
and captured-node checks would require a further controlled decision; no
clamp, fix or numerical restart was made here.

## Per-scale evidence

[Partial proof](dal502-bbs-pilot/partial-proof.json) retains all produced raw
members and arithmetic means for all three approved pairs, with exact missing
or failed job IDs. It derives normalized Price/K, Delta and K*Gamma directly
from the independently repriced spot nodes. Missing values are never filled
from another parity, level, product, CRR cache or European value.

| Market | Spot step | Complete final three-price member stencils | Mesh / raw parity / fine-extra-half | Verdict |
|--------|-----------|--------------------------------------------|-------------------------------------|---------|
| PUT | 1 | 0/2 | unavailable | INCOMPLETE |
| PUT | .5 | 0/2 | unavailable | INCOMPLETE |
| PUT | .25 | 0/2 | unavailable | INCOMPLETE |
| CALL q=.08 | 1 | 0/2 | unavailable | INCOMPLETE |
| CALL q=.08 | .5 | 0/2 | unavailable | INCOMPLETE |
| CALL q=.08 | .25 | 0/2 | unavailable | INCOMPLETE |

All three final metric limits remain strict [2.5e-5,5e-4,5e-3]. The failures
and missing final members prevent evaluating the required joint final gate.
They do not show that the mathematical BBS method cannot converge, and
passing the lower/control calculations does not show that it can qualify.

## Resources and identity

| Resource | Observed / reserved | Limit |
|----------|---------------------|-------|
| Planned / executed / unexecuted jobs | 113 / 81 / 32 | fixed plan |
| Node reservation for full plan | 5,638,979,836 | 6,000,000,000 |
| Conservative charge for attempted jobs | 3,230,701,750 | 6,000,000,000 |
| Actual visited nodes, including partial failures | 3,028,789,320 | within attempted charge |
| BS calls, including partial failures and 25 scalar checks | 544,860 | 2,000,000 |
| Full BS reservation including 100 scalar slots | 917,672 | 2,000,000 |
| Numerical execution window through last native result | 2.957414 seconds | 600 seconds |
| Peak RSS per worker | 4,444 KiB; at most four workers | 131,072 KiB each / 524,288 total |
| Raw directory before diagnostic metadata | 319,478 bytes | 67,108,864 bytes |

Old consumed work remains **2,997,881,833,992**. Adding attempted conservative
charges gives **3,001,112,535,742**; adding the entire reserved plan instead
gives **3,003,520,813,828**, both below the explicit combined cap
**3,003,881,833,992**. Unused reservations were not reassigned to another
attempt or to pure CRR. Post-stop reporting uses saved values only and adds
zero BS/tree evaluations. All native/scalar pricing had ended before that
reporting; the 600-second deadline is not reset per phase.

[Ledger](dal502-bbs-pilot/ledger.json) distinguishes reserved from executed
jobs and retains every partial result; [identity](dal502-bbs-pilot/identity.json)
records exact sources, frozen contract, dependencies, configuration, executable
and raw-file hashes. The original CRR ledger hash still matches its committed
follow-up identity. The new executable is
`399f2b1e784f6e8d92937b5395948d7ceac49c54e3cf3a6928d74643431c5bec`.

The standalone target links only the standard C++ runtime and RapidJSON
headers, without the production core/public libraries. Fresh independent
core configuration/build records public/Python/Excel/portable OFF, and the
existing production library remains byte-identical. No example or facade
source changed. Seven planning/gate unit tests PASS, including method identity,
exact reservation, nonfinite/missing price, missing metric, raw parity and
coarse-mesh rejection and missing/incomplete witness-receipt rejection. The
post-stop receipt guard reads existing witnesses only; it does not re-evaluate
BS. The numerical producer and scalar snapshot retain their original hashes.
Initial compile/import RED checks involved no numerical
work. CCN<=8 and Semgrep Python checks pass. Codacy source check
`128609419006` on `2867fcaf` is SUCCESS with zero annotations; later-head
CI state is reported separately in the issue closeout. The metadata-only
witness-receipt guard in `f9ca996a1522e47d8db4563e1f9824c6062568b7` also
has Codacy SUCCESS (`128611671302`, zero annotations). No numerical kernel
change or pricing run followed the stop.

## Reproduction and handoff

The [tool README](../../../dal-cpp/test-support/pde-risk-qualification/README.md)
and active contract define the method and scope. Actual foreground commands:

```bash
cmake -S dal-cpp/test-support/pde-risk-qualification -B build/dal502-bbs-tools \
  -DCMAKE_BUILD_TYPE=Release -DDAL_CORE_BUILD="$PWD/build/dal502-make"
cmake --build build/dal502-bbs-tools --target dal502_bbs -j4
python dal-cpp/test-support/pde-risk-qualification/bbsplan.py \
  --output build/dal502-bbs/data --driver build/dal502-bbs-tools/dal502_bbs
xargs -n1 -P4 prlimit --as=134217728 -- \
  ./build/dal502-bbs-tools/dal502_bbs --batchfile \
  < build/dal502-bbs/data/witness.workers.txt
python dal-cpp/test-support/pde-risk-qualification/bbsverify.py witness \
  --output build/dal502-bbs/data
xargs -n1 -P4 prlimit --as=134217728 -- \
  ./build/dal502-bbs-tools/dal502_bbs --batchfile \
  < build/dal502-bbs/data/pilot.workers.txt
# Pilot xargs returned 123 after three recorded unexpected failures.
python dal-cpp/test-support/pde-risk-qualification/bbsverify.py finish \
  --output build/dal502-bbs/data
# finish records INCOMPLETE; no retry follows.
python dal-cpp/test-support/pde-risk-qualification/bbs_stopped.py \
  --output build/dal502-bbs/data
```

The attached archive includes all frozen jobs, 81 native result rows, scalar
witnesses, partial proof, executable/source/configuration snapshots and logs.
[Archive manifest](dal502-bbs-pilot/raw-archives.json) gives archive/member
hashes. Extract at the repository root. The stopped report can be recomputed
from these saved results without launching an executable or evaluating BS.
Do not rerun native batches, reset the ledger or expand this pilot.

Remaining work is the coordinator's diagnostic/method decision and focused
review of this opt-in prototype plus the prior P2 view repair. Existing
N1–N12 qualification remains **9 PASS / 0 FAIL / 3 INCOMPLETE**. DAL-502 is
in progress, PR #685 is draft without auto merge, and no other member was
triggered by this batch.
