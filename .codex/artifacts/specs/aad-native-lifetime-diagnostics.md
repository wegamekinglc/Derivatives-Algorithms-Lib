# Native AAD active-number lifetime diagnostics

Status: active D02 implementation contract. The option and native owner/epoch/
slot-generation checks are implemented locally. Diagnostic core/public/portable
Excel, Python and installed ON/OFF consumer checks pass. Corrected default OFF
paired performance passes with retained calibration failure/confirmation evidence.
Publication CI and the final requirement audit remain acceptance work.
The checked recording head `0ee84e1` passes all 46 exact-head CI checks.
This contract controls
the next diagnostic increment in the
[full AAD plan](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/de5dd8b20089223e6938dfd8100d463aa6d4a169/.codex/artifacts/plans/aad-improvement-plan.md).
The [native-only amendment](aad-native-only.md) supersedes external backend
compatibility. The published diagnostic head `9b5febc` passed 49 checks before
removal; fresh removal-head verification is still required.

## Problem and supported boundary

Native `Number_` stores a primal and a raw node pointer. Rewind leaves allocated
storage available for reuse, so an old pointer can remain readable while naming
another node. The scope/checkpoint token protects scoped operations, but does not
validate every operand or adjoint access. A pointer-range check or thread ID
alone cannot establish that an active number still names its original node.

Add opt-in native diagnostics for active operands and adjoint access. Default
builds retain their existing number/node layouts and expression/recording hot
paths. Scoped owner/state/mode/token checks remain enabled in default builds.
Do not treat diagnostic availability as general nested-differentiation support.

DAL owns the sole supported active number and tape implementation. Diagnostic
OFF and ON are distinct ABI configurations of that native implementation.
D00 rejects legacy external-backend selection explicitly, independently of this
option. D03 reports number diagnostics and scoped lifecycle checks as separate
capabilities; scoped checks remain enabled with diagnostics OFF.

## Build and ABI contract

R01. Option `DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS` defaults to OFF. Its
enabled compile definition propagates through the public `dal_cpp` target and
installed target exports, so core, public API, consumers and bindings compile
the same layouts. A flag changing layouts cannot remain a private definition.

R02. OFF removes every diagnostic field, counter update and operand/adjoint
check by conditional compilation. No release side map, allocation registry,
per-node atomic operation, string or hash lookup is introduced. Existing required
dimension/mode/owner/state checks do not depend on the option.

R03. ON may add diagnostic metadata to numbers, nodes and the native tape. Its
runtime/storage cost is explicit opt-in diagnostic overhead. Normal performance
acceptance compares matching OFF builds; diagnostic correctness and resource
observations use matching ON builds and are not claimed as release speedups.

## Identity and validity rules

R04. Each tape lifetime has an opaque nonzero identity independent of reused
thread identifiers and reused tape addresses. Validation compares with the
calling thread's live default tape before reading an old node or context pointer.
An active number copied from a thread that has exited must be rejected safely.

R05. A full reset has a monotonically changing recording epoch. Number handles
capture that epoch; `Clear` invalidates handles before releasing node storage,
and full `Rewind` invalidates the discarded graph. Identity/epoch mismatch must
be detected before dereferencing a potentially freed node.

R06. Record each node's logical ordinal and allocation generation. Keep live-node
count and marked prefix count in diagnostic tape metadata. Restoring a suffix
sets live count to the marked count without changing prefix epoch/generations.
Reject discarded suffix handles even before their addresses are overwritten.

R07. Reusing a slot assigns a new allocation generation. A copied old number
cannot become valid merely because allocation makes its ordinal live again or
reuses its address. Check owner, epoch and live interval before checking the
current node generation. Prefix numbers and their accumulated adjoints remain valid.

R08. Capture scalar/vector mode and width for a binding. Reject incompatible
raw mode changes before selecting scalar/vector adjoint storage. Public mode
selection keeps its existing scope boundary. Diagnostics do not make raw mode
changes into supported graph conversions.

R09. Identity, epoch, count and generation counters must not silently wrap.
Exhaustion is an explicit error before destructive mutation or binding reuse.
Use narrow internal test access for exhaustion; do not add production callbacks
or public counter setters. Rejected independent registration or rebinding must
preserve the previous cached primal and handle. Partial backend/allocation
failures still require discard/reset; diagnostics must not advertise a partially
recorded graph as valid.

## Checked operations and passive values

R10. Validate operands when an expression is materialized or assigned to a
native active number, and validate a number before reading or setting its
adjoint. Validate all operands before allocating the result node or changing an
existing destination's primal/handle. A rejected stale assignment leaves the
destination and a previously valid graph usable. Commit expression assignments
only after successful diagnostic allocation/materialization; counter exhaustion
must not replace a cached primal while leaving its previous node bound.

R11. Expression validation follows the existing compile-time operand tree.
It must cover both sides of binary operations and nested unary operations,
including a saved expression evaluated after suffix restoration. It does not
dereference destroyed C++ objects to diagnose them; ASan/UBSan remain necessary.

R12. Cached primal extraction through `Value` remains passive and does not read
node storage. Explicit double assignment, `RegisterIndependent`, and `PutOnTape`
may bind a cached value as a fresh independent on the current tape. This does
not preserve a derivative link to the discarded graph. Ordinary copying preserves
the original handle; copying alone cannot repair stale activity.

R13. Default numbers without a node retain their cached primal behavior. An
active operand or adjoint access requiring a node reports the missing binding.
Do not manufacture a hidden leaf during reverse or change expression-template
operand counts to bypass this error.

R14. Errors identify the checked operation and the violated ownership, epoch,
slot, generation or mode constraint. Include opaque context/recording/slot
coordinates where useful; business layers may add parameter/output labels.
Construct messages only after failure. No diagnostic should dereference another
thread's expired thread-local context for its message.

## Required tests and acceptance

Use plain `TEST` and independent mathematical references. Each behavioral
increment first establishes a failing case, then the implementation and refactor.

| ID  | Observable contract                                                                                                                               |
|-----|---------------------------------------------------------------------------------------------------------------------------------------------------|
| A01 | A smooth scalar graph has unchanged primal/gradient with diagnostics ON and OFF.                                                                  |
| A02 | An old full-rewind handle rejects operand and adjoint use; a fresh registered input yields its analytic gradient.                                 |
| A03 | Clear followed by new allocations rejects old handles without reading freed node memory under ASan/UBSan.                                         |
| A04 | Restore rejects a discarded suffix before reuse, while the valid prefix still accumulates three independent suffix contributions.                 |
| A05 | Reusing the same node address does not make a copied old handle valid; the new handle produces the correct derivative.                            |
| A06 | A number from a live foreign thread or an exited thread rejects active use before old node/context access.                                        |
| A07 | Fresh registration/explicit passive rebinding works after reset and correctly starts a new independent derivative chain.                          |
| A08 | Saved expressions, compound assignment and container/model copies obey the same operand rules; stale assignment preserves its destination.        |
| A09 | Native scalar/vector mode mismatch is diagnosed before storage access; supported mode/width cases keep their mathematical results.                |
| A10 | Counter exhaustion rejects before mutation and never revalidates old handles.                                                                     |
| A11 | Empty graphs, full final blocks, retained capacity and prefix/slot boundaries are tested without unsafe iterator traversal.                       |
| A12 | Default OFF layouts/instrumentation match the existing build contract; required scoped checks remain active.                                      |
| A13 | Native OFF/ON core/public/portable bindings and installed consumers pass with matching exported ABI; legacy selection fails explicitly under D00. |
| A14 | Fresh unchanged nine-target and affected production pairing pass for default OFF builds; exact-head CI is inspected.                              |

Keep active diagnostic tests conditional on the native diagnostic build and
exercise that configuration explicitly. Tests excluded from default OFF builds
are not evidence that diagnostics have been implemented or verified.

## Implementation order and review boundaries

1. Establish the option/export contract and default layout evidence.
2. Add tape identity/full-reset epochs and safe owner/epoch-first access checks.
3. Add ordinal/live-prefix/generation rules and same-address reuse regressions.
4. Validate expression operands before mutation; cover assignments and saved trees.
5. Cover modes, exhaustion, thread exit, sanitizers, complete diagnostic builds
   and matching default performance/CI acceptance.

The production cursor audit finds allocation through `RecordNode` and reset/mark
through the tape functions only. The allocation-boundary test now uses `RecordNode`
in ON builds. Direct mutation of exposed internal block lists is unsupported;
it must not be used to discard or allocate a diagnostic graph. The option cannot
protect arbitrary bypasses of these storage APIs. Do not claim that this
first option detects arbitrary dangling C++ references or unsupported independently
activated/nested tapes. The unsafe full-final-
block traversal in `BlockList_::Size` is a separate bounded-count repair to verify
before using size information in new diagnostic or production measurement code.
