# Financial parameter Jacobian local review

Verdict: Approve local implementation; publication acceptance remains open.

## Findings

No outstanding local findings. Review covers the public interface, additive
pricing bridge, shared fixtures, complete execution tests, CI filters and active
controls. Existing pricing function bodies remain unchanged. All eleven new
production functions have cyclomatic complexity at most seven; the nine test
bodies have complexity at most seven and ten shared fixture helpers at most one.

Simultaneously selected bases and dependent coordinates use one independent slot
per physical parameter and one active curve per physical curve. Binding precedes
active arithmetic. Complete structure equality precedes compressed binding and
seeds; numeric updates re-record derivatives. Existing formula and passive
validation paths supply fixing, currency and pricing semantics.

The result owns every row/column and preserves repeated positional rows, native
units and actual PV currencies. Payload budgets cover J plus directions; mode
restoration, nested rejection, thread-local requests, finite output checks and
cleanup failures are exercised. Expired trades retain conservative provider
supports and may still require directions: numerical zeros do not prune them.

## Open questions

No blocking API choice. P04 remains open for measured strategy selection. The
small request cost evidence favors explicit dense execution; an automatic policy
needs additional scoped crossover evidence and a stated supported capability.
Python/Excel projection follows the controlling binding stage.

## Tests

Nine new and twenty-two existing affected cases pass under combined evidence
from the incremental batches, totaling 31 distinct cases. Independent acceptance
includes all 45 frozen layered matrix entries at three points, seven trade and
four curve families, native scalar/vector widths and two-step passive differences
with rebuilt base/dependent/XCCY graphs.

Eight current-source strict checks cover OFF and combined diagnostics; only the
changed test translation unit repeats its two checks after the boundary addition.
Installed-only consumption passes 1/1 with an independent deposit PV/derivative.
The eight-row cost acceptance passes, retaining 178 unchanged object hashes and
avoiding unrelated test/performance repeats. Failed intermediate assertions and
their focused repairs remain in session evidence.

## Summary and residual risk

Local functional, installation, style and affected-performance evidence supports
publication. Applicable remote runtime profiles, all current-head CI/Codacy
checks, complete external review bodies/threads and guarded tested/merged-tree
identity must pass before merge. This review does not claim those pending gates
or complete P04 acceptance.
