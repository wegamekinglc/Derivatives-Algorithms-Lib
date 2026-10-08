# Native checked coordinate linear solves

Status: local implementation accepted after numeric #499 merge; remote gates
remain open. Existing dense checked accuracy/report semantics are the
compatibility contract. This does not accept implicit-root or PDE risk.

## Required behavior

Expose three native overloads with packed parameters/RHS both active, only
parameters active, or only RHS active. Reuse CheckedLinearSolveResult_ and the
existing collection wrappers. Captured layout, values, policy and factorization
are owning; Number bindings refer to validated slots in the recording. Caller
mutation/destruction after capture cannot change primal values or reverse.

Use one CheckedCoordinateLinearSolve_ physical cache and O(p) parameter
bindings. Never expand active parameters into n-squared Number bindings, retain
a second coordinate numeric cache, form a dense matrix gradient, or factor
again during reverse. Symmetric off-diagonal risk sums both physical entries;
zero-valued in-layout parameters remain differentiable. Omitted out-of-band
entries stay passive. Passive-parameter reverse omits unused parameter work and
overflow, while a requested paired overflow rejects the graph.

Forward diagnostics preserve the physical system's condition estimate and
per-RHS componentwise backward errors. Forward/transpose limits are inclusive,
finite [0,1] caller policies; no risk clipping, automatic regularization or
relaxation of default pivot checks. Large finite risks remain valid with an
explicit legal pivot tolerance. Physical factorization/residual work remains
dense; compressed parameters alone do not provide sparse/PDE complexity.

## Invocation reports and ownership

Coordinate and dense checked events share one collector. Each successful
full/suffix/prefix invocation owns a unique invocation ID, captured mode/width
and the executed events' errors. An event's m-by-width report covers actual RHS
seed columns and actual channels, including zeros. Lookup rejects unexecuted,
discarded or foreign events. Restoring a checkpoint does not reuse event IDs.
Copies survive collector destruction, source mutation and graph closure.

Prepare the report before entering event-owned scratch accounting. The caller
owns diagnostics/output/report containers; tape ownership covers retained
cache/bindings and scratch. Parent ceilings overlap report and scratch peaks.
On allocation, nonfinite seed, accuracy or accumulation failure, publish no
partial new report collection, invalidate the native graph and refund temporary
allocations. Historical successful reports remain unchanged. Ordinary reverse
still enforces the captured policy. Unchecked ordinary semantics stay compatible.

## Acceptance

- First analytic RED then minimal GREEN before importing edge cases; never
  weaken numerical/resource assertions to pass.
- Independent symmetric and nonsymmetric pivoting Cramer/native expressions,
  three finite-difference steps, zero-valued coordinates, slot aliases, ordinary
  producers/consumers and multiple RHS.
- All three activities in scalar and vector 1/4/8 modes, zero channels,
  repeated reverse, mixed dense/coordinate serial events and restore/windows.
- Inclusive rational rounding boundary 2^-55 and nextafter-below rejection;
  finite large risks, omitted unused risk, requested paired overflow and invalid
  capture/seeds with graph failure and valid later recording recovery.
- Exact O(p) binding deltas, bounded per-channel scratch, exact caller/report
  capacity and one-byte-short rejection/refund. No hidden n-squared adjoints.
- All 25 accepted dense checked cases affected by the payload refactor; numeric
  evidence reused only after source/archive identity proof. If a shared helper
  changes, verify and measure its actual ordinary/coordinate callers as well.
- Strict units/direct public header in OFF/combined diagnostics ON, CCN <=8,
  formatting/current-state docs and installed CMake consumer.
- Frozen scoped old-caller performance and separate optional costs; exact-head
  CI/Codacy/review, actual new cases in 14 configurations, repeated final audits
  and SHA-guarded merge before the following production increment.

Local evidence: eighteen new and twenty-five dense cases pass; inline-repair
boundaries and eight ordinary/packed callers pass. Both affected units pass
strict OFF/combined ON compilation; installed consumer passes 1/1. The initial
dense cached regression is retained and fixed; all eighteen actual helper-caller
rows pass the unchanged gate, with four separate optional cost rows. See the
[review](../reviews/aad-native-coordinate-solve-accuracy.md) and
[performance report](../performance/aad-native-coordinate-solve-accuracy.md).
