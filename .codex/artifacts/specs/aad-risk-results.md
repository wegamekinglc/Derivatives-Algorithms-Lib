# Structured Monte Carlo risk requests and results

Status: active D04 specification. P01 resource/scaling evidence is complete; its
original-baseline production acceptance is being reconciled separately. This
increment does not close F01/F02/P02/P03 or the complete development goal.

## Source and problem

The controlling [full plan](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/de5dd8b20089223e6938dfd8100d463aa6d4a169/.codex/artifacts/plans/aad-improvement-plan.md)
requires D04 before market-quote and portfolio requests. The existing
`ValueByMonteCarlo` facade returns a dictionary with `PV` and `d_...` entries.
`SimResults_::aggregated_` is a path sum (already averaged across RQMC replicates);
`risks_` is already the mean gradient. `results_` resolves display names and can
lose information when model and script labels compare equal under `String_`.
Neither dictionary ordering nor a display label is an adequate matrix axis.

Relevant implementation: `dal-public/src/value.hpp` and `value.cpp`,
`dal-cpp/dal/script/simulation.hpp`, `preparation.hpp`, `settings.hpp`,
`dal-python/src/bindings/value.cpp`, `dal-excel/src/__value.cpp`, and their tests.

## Delivery boundary

First implement the existing default payoff with price-only or native first-order
model/script-parameter risk, stable input selection, raw results, report scaling,
immutable results and a compatibility view. Then add the public valuation entry
and Python/Excel parity. Each part needs independent failing tests before code.

F01 supplies calibrated quote axes/pullbacks later. F02 supplies output selection,
weighted objectives and blocked Jacobians later. Unsupported methods/coordinates
must fail before valuation; this increment cannot advertise them as implemented.
Scalar result projection is useful independently and must not require a new
recording, worker, tape scan or numerical solver.

## Requirements

R01. The legacy valuation overloads, default settings, keys, raw units, mean
normalization, paths/RNG and LSM behavior remain compatible. Their default path
must not allocate structured axes/provenance or construct a result handle merely
to discard it. Shared numeric projection may be factored without extra work.

R02. A structured result contains ordinary numeric values and a two-dimensional
Jacobian. Single-output risk has shape `(1, number of selected inputs)`; a
price-only result has shape `(1, 0)`. Public values and gradients are means.
Do not divide the existing mean gradient by the path count again.

R03. Axis IDs and display labels are separate. Model parameters use
`model:<ordinal>` and script constants use `constant:<ordinal>`, with zero-based
ordinals in their recorded coordinate family. The initial output ID is `payoff`.
IDs are local to a recorded axis identity, not globally unique market quote IDs.
Preserve the model type, ordered full coordinate definition and parameter
values/snapshot with the result so equal IDs cannot imply equal risk bases.

R04. Each selected input records its ID, display label, model/script origin,
original ordinal, raw coordinate value/unit and reporting scale. IDs compare
using DAL `String_` semantics. Model/script labels can coincide without merging
columns. Display labels must never drive positional numeric extraction.

R05. Omitted inputs select all coordinates for native risk. An explicit empty
input list requests no risk columns. Unknown or repeated IDs fail. Preserve the
caller-specified order. Omitted outputs select `payoff`; an explicit empty output
list or an unsupported output must fail. A future F02 extension changes available
outputs, not the omission/empty/order rules.

R06. Price-only and native-AAD execution remain explicit choices. Omitting risk
columns does not silently change fuzzy-AAD payoff semantics to hard/passive
semantics. A native request with zero extracted columns may initially execute
the existing native path; P03 will optimize only after equivalence is proved.
Price-only execution cannot accept a nonempty requested risk axis.

R07. Store raw derivatives. Reporting scale is applied once by a separate
reported-value getter, never by repeated getters or the raw compatibility view.
Factors must be finite and strictly positive, and explicit factors must match
the selected input count. Raw scale defaults to one. A result identifies its
factor/unit; no financial unit is inferred from a Python dictionary key.

R08. Known model units must come from typed model definitions. For arbitrary
script constants/payoffs or an unsupported physical-unit descriptor, report
the coordinate's native numeric unit and explicitly mark the physical unit as
unknown. Do not invent currency, bp, vol-point or market-quote meaning.
Typed descriptors can be extended without changing raw values or stable IDs.

R09. The immutable result exposes raw values/Jacobian, axes, reported Jacobian,
method and provenance. Getters do no preparation, calibration or MC work.
They expose no active numbers, recordings, checkpoints or tape ownership.
Old handles remain valid after later successful or failed requests.

R10. Legacy projection returns `PV` and raw `d_<display label>` for the initial
single-output result. Reject an unrepresentable collision under `String_`
semantics rather than silently overwriting a column. The structured result
itself remains readable with both distinct IDs and columns.

R11. Result payload budget covers values and numeric Jacobian storage, with
overflow-safe byte arithmetic. Validate known extents/budget before allocating
that storage and, in the public valuation entry, before submitting path work.
State this scope explicitly; it is not a total-process, tape or worker budget.
F02 introduces separately enforced tape/workspace budgets. Unsupported budget
types cannot be accepted with a false enforcement claim.

R12. Validate path count, method/coordinate capability, duplicate/unknown IDs,
scales and statically determinable budget before expensive preparation/history
resolution or worker submission. Low-cost parsing/model-coordinate inspection
is allowed. Snapshot caller settings before callbacks or released Python GIL.

R13. Publish a complete result only after every requested value and derivative
is finite and every dimension/axis matches. Failures identify the operation,
output and input where possible. A subsequent valid request recovers using the
existing task-drain/recording contracts; no partial handle is returned.
Caller-provided execution metadata must also have matching product date/event
extents, a path count consistent with normalization, positive replicate counts
and finite historical values when supplied. Structural checks do not certify
that arbitrary conversion metadata describes an actual valuation.

R14. Provenance identifies the native engine, estimator/method, fixed calibration
boundary, evaluation date, input snapshots/axes, paths per RQMC replicate and
replicate count, RNG/BB, smoothing, compiled/tree execution and LSM policy.
`RetrainedBump` is a mixed AAD/policy-secant method and cannot be labelled wholly
analytic AAD. No uncomputed standard error is fabricated.

R15. C++, Python and Excel have the same axes, shape, raw/report scaling and
error semantics. Python optional choices are keyword arguments; one-row
matrices remain two-dimensional. Excel settings/results are immutable handles;
getters extract existing data. Add public enums via Machinist and commit generated
outputs only with their markup. No result archive format is promised in this increment.

## Executable acceptance

- Projection reference: path sum 120 over ten paths yields PV 12; an existing
  mean gradient 7 stays 7. Scale 0.01 yields reported 0.07 on every repeated getter
  and raw legacy `d_...` remains 7.
- Deliberately equal model/script display labels produce distinct IDs/columns;
  legacy projection rejects their key collision. Case variants exercise DAL comparison.
- Request omission, explicit empty lists, reordered subsets, repeated/unknown IDs,
  invalid path counts, zero/negative/NaN/infinite scales and dimensional mismatch.
- Budget exact boundary, one-byte-short rejection and overflow-safe extent arithmetic;
  price-only zero-column shape is preserved.
- Nonfinite PV/risk, wrong result lengths, bad axis mapping and unsupported capability
  fail without a successful result. A-success/B-failure/C-success preserves A and recovers C.
- Fixed-path structured/legacy tree and compiled BS and representative Hybrid/GSR/LSM
  agree at rel/abs 1e-10; retain existing stronger LSM bitwise contracts where applicable.
- All relevant core/public/Python/portable Excel tests, generated output, consumers,
  docs and exact publication-head CI pass. Preserve the existing nine-target gate.
  Old short/production requests retain the unchanged paired 4% policy; new metadata
  and selected-result extraction have separately labelled cost measurements.

## Remaining scope

Public factory/getter names and passive snapshot representation are resolved in
the API note and implemented. Exact publication-head CI is the remaining D04
acceptance gate. Future market axes, multi-output strategies, tape budgets and
second-order methods require their own controlling designs and mathematical tests.
