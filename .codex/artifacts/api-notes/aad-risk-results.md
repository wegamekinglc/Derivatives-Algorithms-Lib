# D04 scalar risk-result boundary

Status: scalar D04 accepted at exact `e3720f9f` across converter, public entry
and Python/Excel surfaces; this decision constrains active Stage B extensions.
Controlling [requirements](../specs/aad-risk-results.md) remain authoritative.

## Current and first proposed surface

Existing `Script::SimResults_` and public `ValueByMonteCarlo` retain their API and
execution path. Introduce a focused core header `dal-cpp/dal/script/riskresults.hpp`
for passive structured result projection. It does not include or expose tape,
recording, worker or active-number state.

`RiskRequest_` specifies an optional ordered input-ID list, an optional ordered
output-ID list, optional per-input reporting factors, and an optional numeric
result-payload budget. An unset budget is unlimited; zero is a valid budget and
cannot fit the mandatory one-double payoff. Explicit empty inputs preserve a
zero-column matrix. Unsupported output selection fails; only `payoff` exists in
this increment. Execution-method selection belongs to the subsequent public
request planner rather than pretending the projection routine executes AD.

`RiskCoordinate_` contains a stable ID, display label, coordinate-family name,
ordinal, native numeric value, native unit, optional physical-unit descriptor
and report factor. Supported coordinate families are `model` and `constant`;
the ID must equal `family + ':' + ordinal`. Model coordinates precede constants
in the complete source axis and each family has contiguous zero-based ordinals.
Selected results can reorder them. Duplicate display labels remain legal.

`RiskResultProvenance_` contains a method label, engine label, normalization,
fixed-calibration boundary, optional resolved evaluation date and an immutable
numeric snapshot of the complete input axis. Future public planning fills the
simulation/product/history fields; the initial projection routine preserves
provided provenance rather than manufacturing missing facts.

`RiskResult_` owns ordinary numeric values, a raw `(1, n)` Jacobian, selected axes
and provenance. It is created only by a validating factory and has const getters.
Returned references are const; `ReportedJacobian()` returns a new matrix whose
columns are multiplied by their factors exactly once. `LegacyValues()` returns
raw `PV`/`d_...` and rejects display-key collisions under DAL string comparison.
The result is safe to copy/read across threads. No serialization is introduced.

The factory `ProjectMonteCarloRiskResult(source, paths, completeAxis, request,
provenance)` receives the existing simulation result and complete axis. It
validates the complete source dimensions/names, requested IDs/factors, method
metadata, finite payoff/requested risks and overflow-safe numeric payload size,
then normalizes the payoff sum once and selects mean gradients by ordinal.
Provided execution metadata must have consistent path/product extents and finite
supplied history, even though a converter cannot certify its execution claims.
This is a conversion boundary, not a second Monte Carlo valuation.
`paths` is a signed integer consistent with the existing public entry; zero and
negative counts fail. `RiskResultPayloadBytes(outputs, inputs)` performs the
overflow-safe numeric extent calculation needed by both conversion and the
future pre-execution request planner. Selected column counts must also fit the
existing matrix's signed dimension type. Reporting multiplication overflow
fails at conversion, before a successful result is published.

## Rationale and alternatives

Separate IDs prevent a model `spot` and a script `spot` from overwriting each
other. Ordinals have meaning only with the retained complete axis/snapshot;
there is no claim that `model:0` identifies a globally stable market quote.
The later public planner resolves IDs and dimensions before submitting workers.

Do not wrap the legacy fast path in a structured handle: allocating metadata
and matrices there would add cost to every old short request. Do not select risk
by `SimResults_::operator[]`: its label map cannot represent equal display names.
Do not infer units from string fragments; unknown physical units remain explicit.

The budget covers the returned values and one raw numeric matrix. It excludes
metadata, caller-owned source storage, worker/tape memory and subsequently
requested copies. A later tape/workspace budget needs independent enforcement.
This narrower named budget avoids a false total-memory promise.

## Example and errors

For a complete axis `model:0 = spot`, `model:1 = vol`, `constant:0 = strike`,
requesting `constant:0, model:1` yields columns in that order. A payoff sum 120
over ten paths becomes 12; risks already averaged by the simulation are copied
unchanged. A factor 0.01 changes a raw vol gradient 7 into reported 0.07 while
the raw getter and compatibility view remain 7.

Error messages identify `InvalidRiskRequest`, `InvalidRiskResult`,
`RiskResultBudgetExceeded` or `LegacyRiskCollision` with the offending ID/field.
Empty output selection, unknown/repeated IDs, axis/name mismatch, nonfinite
values, invalid factors and insufficient payload budget never publish a result.

## Public execution decision

The passive boundary passed nine focused tests and the standard OFF build passed
2,341 CTest cases. The next increment exposes
`ValueByMonteCarloWithRisk(product, modelData, paths, request = {}, valuation = {},
simulation = DefaultRiskMonteCarloSettings())`. The six arguments stay grouped
by their existing configuration structs. The new default helper enables native
AAD; the existing `MonteCarloSettings_` default and old entry remain unchanged.
An explicitly supplied simulation with `enableAad_ = false` chooses price-only
execution. This reuses the existing authoritative mode instead of maintaining
two potentially conflicting mode selectors. Price-only omission selects no
inputs; an explicit nonempty list fails. An explicit empty native list still
runs native AAD, preserving the smoothed primal estimator.

`PlanScalarRiskRequest(axis, request, enableAad)` validates and normalizes selection,
factors and numeric budget before date capture, history resolution, compilation
or task submission. A low-cost model construction and unpartitioned script
indexing supply the full coordinate axis. Preparation subsequently verifies the
same axis before execution. Selection never uses display-name lookup.

The result's optional execution snapshot contains actual paths per replicate,
the resolved simulation settings, product dates/text/settings, resolved today
policy, fixing-source kind and each observation's canonical index/time and
frozen historical value when present. A passive JSON copy of the model data
retains fixed model inputs as well as differentiated coordinates. Public entries
fill these from the sealed preparation; the standalone converter continues to
accept caller-supplied metadata without claiming it certifies execution.
Native LSM `RetrainedBump` is labelled `NativeAADWithRetrainedPolicySecant`;
ordinary native execution is `NativeAAD`, passive execution `PriceOnly`, and
fully expired execution `Expired`. No standard error is fabricated.

Known BS units use typed model ordinals: spot units remain physically unknown,
vol is decimal volatility per square-root year, and rate/dividend are continuous
decimal rates per year. Correlated BS uses its typed interleaved coordinate
layout. Other model families initially retain `model-coordinate` with unknown
physical units; script constants retain `script-number`. No label-based unit
guessing occurs.

Python exposes keyword-only `RiskRequest_`, read-only result/coordinate objects
and `MonteCarlo_ValueWithRisk` with keyword request/valuation/simulation choices.
Matrices retain `(1, n)` shape, including `(1, 0)`. Every getter returns a copy or
read-only scalar. Excel exposes `RiskRequest_New`, `MonteCarlo_ValueWithRisk` and
`RiskResult_Get_...` over immutable wrappers. Zero-column Jacobians use an
explicit empty cell for Excel while a shape getter retains the exact extent.
The C++ and Python numeric matrix remains zero-column. There is no result archive
format: Excel wrapper serialization must fail explicitly, not write an empty
archive that looks usable.

Market axes, weighted outputs, method enums, worker reuse and tape/workspace
budgets remain subsequent controlled increments.
