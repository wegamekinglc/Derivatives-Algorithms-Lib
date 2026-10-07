# F03 native solve-coordinate recording

Status: active; numeric PR #495 is merged at `0ed5a908`.
Source: the approved full AAD plan, native-only backend requirement, no-regression
requirement and changed-scope performance policy.

## Problem and scope

Numeric symmetric/banded coordinates already define independent parameters and
their pullbacks. Native callers need these parameters to participate directly in
scalar producers, solve events and consumers, without constructing dense Number
matrices or manually scattering coordinate gradients.

Use the existing layouts and one numeric coordinate cache. This increment adds
three C++ recording overloads; Python/Excel bindings, sparse LU, symmetric-band
composition, diagnosed coordinate results and transpose residual report APIs
remain separate requirements.

## Requirements

1. Accept active parameters/active RHS, active parameters/passive RHS, and passive
   parameters/active RHS. Return a caller-owned Matrix of native Number outputs.
   Match numeric ordering, finite/range checks and default pivot tolerance.
2. Copy only p active coordinate bindings and active RHS bindings. Never create
   dense Number entries, including placeholders for fixed zeros. Retain active
   zero-valued parameters and paired symmetric off-diagonal contributions.
3. Capture numeric values and bindings at construction; later input-container
   changes do not alter the solve or its destinations. Own one LU and X cache.
4. Reverse each nonzero channel through the accepted numeric coordinate API;
   accumulate repeated-slot and cross-parameter/RHS/scalar aliases. Passive
   coordinates use RHS-only reverse and avoid unused gradient overflow.
5. Support scalar and vector width 1/4/8, positive/negative/zero seeds, repeated
   whole reverse, mixed ordinary/coordinate events and serial/shared solves.
6. Share existing native event admission/publication/failure and resource logic.
   Keep ordinary dense payload fields, Number/node/tape layouts and scalar
   execution free of coordinate descriptors or runtime branches.
7. Preserve checkpoints: suffix reverse stops at captured boundaries; restoring
   releases suffix caches and keeps prefixes usable. Close releases all event
   storage. Existing owner-thread, live-input and mode contracts apply.
8. Retained snapshots/bindings belong to tape accounting; caller output buffers
   and reverse scratch obey their existing scopes. Exact and one-byte-short
   tests must demonstrate admission, refunds and independent-recording recovery.
9. Reject bad counts/shapes, nonfinite values, bad tolerance, unavailable inputs,
   wrong recording/thread and numerical failures. Constructor failures publish
   no outputs. Reverse failures suppress partial adjoint reads across channels.

## Executable acceptance

- Missing optional native header RED followed by an analytic symmetric 2x2
  producer/solve/consumer GREEN with repeated negative seeds.
- Independent scalar Number Cramer formulas, three-step coordinate/RHS differences
  and explicit transpose residual identities for symmetric indefinite and
  asymmetric bands with multiple RHS; all three activity combinations/modes.
- Zero active coordinates, aliases, shared and serial solves, mutation,
  checkpoint restore, owner thread, foreign live slots and independent threads.
- Exact binding-storage reduction against passive coordinates at equal n,m;
  diagonal/narrow/full p counts. Exact tape/caller scratch limits, one-byte-short
  rejection, failure unreadability and refund/recovery.
- Affected native solve tests, formatting/complexity/docs, installed consumer,
  actual new cases in all 14 relevant CI configurations, Codacy and review.
- One changed native solve object: six ordinary boundary comparisons and two
  diagnosed-caller comparisons when executable identity is lost. Scope review
  adds that caller because it shares the changed publication/reverse helpers.
  Unchanged numeric/runtime evidence is reused.
  New coordinate cost uses three useful size/band/RHS boundaries, not a full
  portfolio/MC/PDE or benchmark parameter matrix.

## Open questions

None for this basic integration. The ledger must explicitly retain the missing
owning transpose residual diagnostics after this increment.
