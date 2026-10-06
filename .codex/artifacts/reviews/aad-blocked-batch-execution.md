# Native blocked prepared-script batch execution

Verdict: Approve the internal batch increment; whole #484 remains incomplete.

## Findings and design

No unresolved finding in the exercised batch behavior. The shared native batch
loop now delegates seeding and input harvesting to its collector. Scalar and
weighted policies retain scalar seed one and their original per-batch gradient
normalization. The block policy materializes/seeds distinct output lanes and
harvests raw channel sums in requested input order. It reuses model/path/RNG,
historical initialization, suffix propagation and single prefix reversal.

`EvaluateAADBlockBatch` selects vector mode before recording, creates root
capacity before recording, and restores mode afterward. Fresh results include
padded rows which stay zero. Output collection does not discount script slots again.
Finite values and raw gradients identify the output/input in failure messages.
Unsupported expired/exercise/passive preparation fails before scratch attachment.

The caller can execute tasks while holding coordinator result reservations.
`BufferCapacityScope_::ForWorker` explicitly shares that same budget in this
case; normal nested scopes still fail. Attachments close in stack order and
restore the coordinator. Cross-budget nesting and out-of-order close fail before
releasing any reservation. Fixed admission includes both current and historical
evaluators, collector and scalar bookkeeping, with dynamic arrays admitted at
their allocation boundaries.

## Evidence

- `aad-blocked-simulation-red-01.log`: absent batch interface compiler RED.
- `aad-blocked-simulation-expired-red-03.log`: a zero-byte budget incorrectly
  masks unsupported expired preparation with a 7120-byte scratch error. The
  unchanged unsupported-identity test passes after the early product check.
- `aad-blocked-simulation-green-04.log`: four cases cover historical prefix
  aliases/direct inputs/constants, widths 1/2/4 and padding; reversed 1/4/16/64
  output selections versus independent scalar batches; absolute path offset 37;
  requested input order; coordinator/worker scratch, fixed-bound rejection and
  recovery; and early unsupported preparation.
- `aad-blocked-coherent-off-build-01.json` and corresponding test log: the
  freshly rebuilt matching core/public shared libraries pass all 20 selected
  blocked, scalar-risk and weighted-risk cases, including GSR/hybrid/LSM and
  four-worker legacy behavior. No cached production objects are mixed into this
  final OFF integration run.
- Caller-worker scope compiler RED and `aad-blocked-buffer-build-07.json`:
  seven buffer cases pass in OFF, combined ASan/UBSan and combined TSan after
  rebuilding the two affected primitive units. This is primitive instrumentation,
  not a full sanitized block-driver acceptance claim.
- Six canonical GCC 14 OFF/combined syntax checks pass. New functions meet
  complexity eight. All six sanitizer selections include the new batch cases
  without dropping previous selections.

## Remaining acceptance and limitations

This is a single-batch primitive over already sealed preparation, not the public
owning Jacobian operation. Internal batch slots may contain partial sums after
runtime failure; the request producer must discard the failed attempt and drain
all tasks before returning. Request-level fixed/result/cached-tape admission,
initial tape reservations, guaranteed failure-cleanup headroom and diagnostics
remain required. Full one/four-worker blocked requests, weighted `J^T w`, frozen
finite differences, passive empty-column handling and public C++/Python/Excel
results remain open.

Changed legacy allocation and collector paths require the frozen existing-entry
performance acceptance. The 20 numerical regressions do not establish a speed
verdict. Complete current-head CI/Codacy/reviews and guarded merge follow the
finished public delivery.
