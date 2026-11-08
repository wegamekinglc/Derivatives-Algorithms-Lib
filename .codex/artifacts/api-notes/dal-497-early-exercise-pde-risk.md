# DAL-497 owning PDE risk API decision

Status: active, frozen for schema version 1. Core C++ fixed-plan repricing is
implemented; public-facade/Python/Excel risk projections and full independent
numerical qualification remain pending. Audience: core/public C++ implementers, Python and
Excel binding authors, independent numerical testers and risk consumers.
The [R1–R11 contract](../specs/dal-497-early-exercise-pde-risk.md) owns semantics;
the [acceptance contract](../specs/dal-497-pde-risk-acceptance.md) owns fixed
fixtures and thresholds. This decision owns names, types, field order and
three-end projections. Core declarations are implemented; the other projections
remain proposed. The [published guide](../../../docs/methodology/pde/early-exercise.md#owning-core-risk-request)
owns current calling and availability guidance.

## Design baseline and decision

At master `4e9eb5eb7cae770826e6f4e0a05aeb9bf4c4c44e`,
`PDE::EvaluateEarlyExercisePde` and its public aliases provide owning prices
and diagnostics. Python `EarlyExercisePdeResult_New(request)` and Excel
`EARLYEXERCISEPDERESULT.NEW` project that passive contract. No early-exercise
risk result exists. `EvaluateEuropeanPdeRisk` has an incompatible AAD/tape
lifecycle and a different parameter axis; keep it separate.

Choose a new owning request/result with named records, shared numeric plan,
central full repricing, finite-scale Gamma, and per-record unavailability.
Preserve all existing price signatures, tables, defaults, status/method strings
and failure behavior. New APIs are additive; C++ consumers rebuild normally.
No ABI, pickle, Excel archive or cross-schema fingerprint promise is added.

Rejected alternatives: an anonymous mixed-order Jacobian obscures units;
micro-bump interpolation gives spurious Gamma; independent price planning
moves the numerical function; cached exercise masks omit exercise-policy
changes; all-or-nothing throws discard usable prices/risks; NaN/zero absence
is ambiguous; one-sided fallback changes the derivative; European AAD reuse
would mutate caller tape. Native partials lack the provenance needed for quote
DV01. Active-set/implicit AAD remains a later design.

## Settings and request

Core declarations are in `dal-cpp/dal/math/pde/earlyexercisepderisk.hpp`.
Numerical types live in `Dal::PDE`. Machinist enum classes live in `Dal` like
the existing price enums; never handwritten enum classes. Public header
proposed `dal-public/src/earlyexercisepderisk.hpp` aliases every numerical type
and all three validation/evaluation functions below into `Dal`. Existing price
types are used directly, not wrapped in a second incompatible definition.

```cpp
struct EarlyExercisePdeRiskSettings_ {
    bool delta_ = true;
    bool gamma_ = true;
    bool vega_ = true;
    bool carryRisk_ = true;
    std::optional<double> spotBump_;
    double volatilityBump_ = 1e-3;
    Vector_<double> rateBumps_, dividendYieldBumps_;
    double deltaStabilityTolerance_ = 2e-3;
    double gammaStabilityTolerance_ = 2e-2;
    double vegaStabilityTolerance_ = 3e-3;
    double rateStabilityTolerance_ = 3e-3;
    double dividendYieldStabilityTolerance_ = 3e-3;
    size_t maxScenarios_ = 129;
    size_t maxWorkUnits_ = 100000000;
    size_t numericPayloadBudgetBytes_ = 67108864;
    size_t workspaceBudgetBytes_ = 268435456;
};
struct EarlyExercisePdeRiskRequest_ {
    EarlyExercisePdeRequest_ valuation_;
    EarlyExercisePdeRiskSettings_ risk_;
};
void ValidateEarlyExercisePdeRiskSettings(const EarlyExercisePdeRiskSettings_&);
void ValidateEarlyExercisePdeRiskRequest(const EarlyExercisePdeRiskRequest_&);
[[nodiscard]] EarlyExercisePdeRiskResult_
EvaluateEarlyExercisePdeRisk(const EarlyExercisePdeRiskRequest_&);
```

Field/keyword mapping is camelCase_ → snake_case without the trailing underscore.
Settings constructor arguments are all keyword-only in Python, in declaration
order above. Defaults are exactly those shown: spot None, carry vectors empty.
`EarlyExercisePdeRiskRequest_(valuation, *, risk=None)` copies an existing
price request and optional typed settings; None creates defaults. Evaluator
factory is `EarlyExercisePdeRiskResult_New(request)` with one required request.
Result/diagnostic records have no public Python constructor.

Local settings validation: flags must be actual bool; bump fields follow R3;
stability tolerances are finite and strictly positive; caps are unsigned
size_t, with zero meaningful as specified in R10. Python rejects bool/enums as
numbers, non-int count values, NaN/Inf, out-of-range integers, malformed
sequences and unknown keywords. Typed arguments must be their precise DAL
type. Strings/bytes are not numeric sequences. Use zero-based bracket paths
such as `risk.rate_bumps[1]`; Excel adds one-based cell coordinates.
Keyword/type errors include `InvalidEarlyExercisePdeRiskRequest` as well as
the argument name. Matching pybind conversion errors may need explicit input
helpers to retain that prefix; default pybind messages alone do not satisfy it.

## Frozen enum alternatives

Generate these Machinist enums in exactly the order below, with `switchable`.
Python exports their Value_ classes under the names with trailing underscore;
output properties/tables return canonical strings. No integer coercion or
new aliases. This fixes strings/order, not portable binary enum ordinals.

```text
EarlyExercisePdeRiskStatus:
  COMPLETE, PARTIAL, BASE_FAILED, WORK_LIMIT
EarlyExercisePdeRiskAvailability:
  AVAILABLE, AVAILABLE_EXERCISE_REGION,
  UNAVAILABLE_BASE_FAILED, UNAVAILABLE_WORK_LIMIT,
  UNAVAILABLE_SCENARIO_FAILED, UNAVAILABLE_DEGENERATE_DIFFUSION,
  UNAVAILABLE_DOMAIN_BOUNDARY, UNAVAILABLE_NEAR_EXERCISE_BOUNDARY,
  UNAVAILABLE_EXERCISE_SWITCH, UNAVAILABLE_NONFINITE,
  UNAVAILABLE_STEP_UNSTABLE
EarlyExercisePdeRiskScenarioStatus:
  PLANNED, SUCCESS, NUMERICAL_FAILURE,
  NOT_RUN_WORK_LIMIT, NOT_RUN_BASE_FAILED
EarlyExercisePdeRiskWorkLimitSource:
  NONE, SCENARIO, GLOBAL
EarlyExercisePdeRiskMembership:
  NOT_EXERCISABLE, CONTINUATION, EXERCISE, BOUNDARY
EarlyExercisePdeRiskBumpOrigin:
  DEFAULT, EXPLICIT
```

`reason_` is empty for available records and contains explanatory text for
unavailable ones. Flags use booleans, not undocumented bitmasks. Method, basis,
interpretation and interpolation policy are version-1 fixed strings rather
than additional enum factories.

## Frozen owning records

In these declarations `Opt<T>` means `std::optional<T>` and `Vec<T>` means
`Vector_<T>`; they are documentation shorthand, not new public aliases.
Every member below is exposed readonly in Python with the stated mechanical
snake_case spelling. Nested values/vectors/optional records detach on access.
An absent optional maps to None in Python and a blank cell in Excel; no -1 or
zero sentinel. Dates use Date_ in Python/C++, valid integer serials in Excel.

```cpp
struct EarlyExercisePdeRiskStencilEntry_ {
    size_t scenarioIndex_;
    double weight_;
};
struct EarlyExercisePdeRiskRecord_ {
    size_t riskIndex_;
    String_ riskId_, parameterId_;
    int derivativeOrder_;
    Opt<size_t> segmentIndex_;
    Opt<Date_> startDate_, endDate_;
    double parameterValue_;
    String_ unit_;
    Opt<double> value_;
    EarlyExercisePdeRiskAvailability_ status_;
    String_ reason_, method_, interpretation_;
    double fineBump_, coarseBump_;
    EarlyExercisePdeRiskBumpOrigin_ bumpOrigin_;
    Opt<double> fineEstimate_, coarseEstimate_, stepDifference_;
    Opt<double> normalizedStepDifference_;
    double stabilityTolerance_;
    Vec<EarlyExercisePdeRiskStencilEntry_> fineStencil_, coarseStencil_;
};
struct EarlyExercisePdeRiskPlanStep_ {
    int sourceTimeIndex_, timeIndex_;
    double dt_, theta_;
    int carryIndex_;
    bool damped_, projection_;
};
struct EarlyExercisePdeRiskPlan_ {
    int schemaVersion_;                       // 1
    String_ planId_;
    Date_ evaluationDate_, maturityDate_;
    double upper_;
    int gridPoints_;
    double gridSpacing_;
    Vec<double> grid_;
    Vec<EarlyExercisePdeTimeNode_> timeGrid_;
    Vec<EarlyExercisePdeRiskPlanStep_> steps_;
    double epsilon_, theta_;
    int effectiveRannacherIntervals_;
    double penaltyFactor_;
    int maxIterationsPerStep_;
    double linearBackwardErrorLimit_;
    String_ interpolationPolicy_;             // LINEAR_SPOT
    EarlyExercisePdeRiskSettings_ resolvedRisk_;
    EarlyExercisePdeRiskBumpOrigin_ spotBumpOrigin_, volatilityBumpOrigin_;
    Vec<EarlyExercisePdeRiskBumpOrigin_> rateBumpOrigins_, dividendYieldBumpOrigins_;
    size_t plannedScenarios_;
    int basePlannedSteps_, plannedSteps_;
    size_t plannedNumericPayloadBytes_, plannedWorkspaceBytes_;
    size_t plannedPeakNativeBytes_;
};
struct EarlyExercisePdeRiskScenario_ {
    size_t scenarioIndex_;
    String_ scenarioId_, planId_;
    Opt<String_> parameterId_;
    Opt<size_t> segmentIndex_;
    Opt<double> parameterValue_, offset_;
    EarlyExercisePdeRiskScenarioStatus_ status_;
    Opt<EarlyExercisePdeStatus_> solverStatus_;
    EarlyExercisePdeRiskWorkLimitSource_ workLimitSource_;
    bool priceAvailable_;
    Opt<double> price_;
    double epsilon_;
    int plannedSteps_, actualSteps_, completedSteps_;
    size_t totalSolves_, workUnits_;
    size_t numericPayloadBytes_, peakWorkspaceBytes_;
    Opt<double> maxIterationChange_, maxLinearBackwardError_;
    Opt<double> maxObstacleViolation_, maxDualViolation_, maxComplementarityResidual_;
    Opt<EarlyExercisePdeFailure_> failure_;
    Opt<EarlyExercisePdeRiskMembership_> evaluationMembership_;
    Opt<bool> evaluationBoundaryIntersectsSpotStencil_;
    Opt<bool> futureContactChanged_, upwindChanged_;
};
struct EarlyExercisePdeRiskExecution_ {
    size_t plannedScenarios_, attemptedScenarios_, successfulScenarios_;
    size_t failedScenarios_, notRunScenarios_;
    size_t totalSolves_, workUnits_;
    size_t plannedNumericPayloadBytes_, plannedWorkspaceBytes_;
    size_t plannedPeakNativeBytes_;
    size_t numericPayloadBytes_, peakWorkspaceBytes_, peakNativeBytes_;
    Opt<size_t> workLimitScenarioIndex_;
    EarlyExercisePdeRiskWorkLimitSource_ workLimitSource_;
};
```

`resolvedRisk_` replaces unset spot and empty carry vectors by actual bumps and
retains all switches, tolerances and caps. Bump origin vectors always have m
entries, including disabled axes. C++ cannot detect explicitly assigning the
default scalar: for volatility use DEFAULT when the numeric value equals 1e-3,
EXPLICIT otherwise,
in all three ends. Empty vs populated carry vectors and optional spot retain
unambiguous origin. This rule makes cross-end equality independent of call syntax.

`steps_` are ordered by increasing target time; each source index is target+1.
`projection_` means a Bermudan projection after rollback, not American exercise.
`basePlannedSteps_` is the old base single-request count, not an extra solve.
The plan digest serializes the fields from schemaVersion through
interpolationPolicy, skipping planId. Serialize time-node members in existing
declaration order (timeYears, optional calendarDate, exercise/carry/maturity
flags); serialize step fields in declaration order. No struct padding bytes.

For each scenario, `parameterValue_` is the perturbed parameter, `offset_` its
signed difference from original input. Base has those and parameterId/segment
absent. epsilon and plannedSteps are populated even for not-run scenarios;
work/capacity/execution counts are zero there, solverStatus and observed
diagnostics absent. Failed scenarios have no price; failure_ preserves the
existing solver record. Nonfinite observed scalars map to absence and a
NONFINITE status/failure; never export NaN. Evaluation/contact/upwind fields
are absent on incomplete scenarios; successful base change flags are false.
Spot-boundary intersection is absent if neither spot risk was requested;
otherwise compute against the complete coarse interval for every successful
scenario. Future contact change compares all nonterminal levels after time 0
with base, using its own computed masks; upwind change compares row choices.

Risk bumps and tolerance are present even for unavailable rows; estimates and
differences exist only if all needed prices and arithmetic are finite.
`normalizedStepDifference_` is the absolute normalized difference used in R8.
No stencil → no estimates. Failure/nonsmoothness can leave diagnostics present
while `value_` stays absent. available implies finite value and both estimates.

## Result surface

```cpp
class EarlyExercisePdeRiskResult_ {
public:
    int SchemaVersion() const;
    EarlyExercisePdeRiskStatus_ Status() const;
    bool IsComplete() const;
    bool PriceAvailable() const;
    double Price() const;
    double RiskValue(const String_& riskId) const;
    const EarlyExercisePdeRiskRequest_& Request() const;
    const EarlyExercisePdeResult_& BaseResult() const;
    const EarlyExercisePdeRiskPlan_& Plan() const;
    const Vector_<EarlyExercisePdeRiskRecord_>& Risks() const;
    const Vector_<EarlyExercisePdeRiskScenario_>& Scenarios() const;
    const EarlyExercisePdeRiskExecution_& Execution() const;
    String_ Method() const;
    String_ RiskBasis() const;
    bool AadUsed() const;
};
```

All are pure const nodiscard getters; result construction is evaluator-only.
Python properties are `schema_version, status, is_complete, price_available,
price, request, base_result, plan, risks, scenarios, execution, method,
risk_basis, aad_used`, plus `risk_value(risk_id)`. BaseResult is present even
when failed; its Price/Values getters keep their existing error behavior.
Request is the original risk input; BaseResult.Request is the resolved
valuation used by the shared-plan rollback. Do not confuse the two.

## Excel functions and strict inputs

Functions use owning `StorableRiskValue_` types named
`StorableEarlyExercisePdeRiskSettings_`, `StorableEarlyExercisePdeRiskRequest_`
and `StorableEarlyExercisePdeRiskResult_`. Runtime value-type labels drop
`Storable` and trailing `_`. Workbook names are uppercase with underscores
changed to dots, e.g. `EARLYEXERCISEPDERISKRESULT.GET.RISKS`; no DA prefix.
Native C++ stubs use the usual final output pointer, omitted below.

```text
EarlyExercisePdeRiskSettings_New(name, settings=blank, carry_bumps=blank)
EarlyExercisePdeRiskSettings_Get_Configuration(settings)
EarlyExercisePdeRiskSettings_Get_CarryBumps(settings)
EarlyExercisePdeRiskRequest_New(name, valuation, risk=blank)
EarlyExercisePdeRiskRequest_Get_Valuation(request)
EarlyExercisePdeRiskRequest_Get_Settings(request)
EarlyExercisePdeRiskResult_New(name, request)
EarlyExercisePdeRiskResult_Get_Request(result)
EarlyExercisePdeRiskResult_Get_BaseResult(result)
EarlyExercisePdeRiskResult_Get_Summary(result)
EarlyExercisePdeRiskResult_Get_Plan(result)
EarlyExercisePdeRiskResult_Get_PlanGrid(result)
EarlyExercisePdeRiskResult_Get_PlanTimeGrid(result)
EarlyExercisePdeRiskResult_Get_PlanSteps(result)
EarlyExercisePdeRiskResult_Get_ResolvedSettings(result)
EarlyExercisePdeRiskResult_Get_BumpOrigins(result)
EarlyExercisePdeRiskResult_Get_Risks(result)
EarlyExercisePdeRiskResult_Get_Stencils(result)
EarlyExercisePdeRiskResult_Get_Scenarios(result)
EarlyExercisePdeRiskResult_Get_Execution(result)
EarlyExercisePdeRiskResult_Get_Price(result)
EarlyExercisePdeRiskResult_Get_RiskValue(result, risk_id)
```

Valuation is an existing price request handle. Settings is a two-column
headerless table using scalar settings keys in declaration order; vectors
`rate_bumps`/`dividend_yield_bumps` cannot appear there. `spot_bump` blank means
unset. Other supplied scalar values cannot be blank. Both empty optional
ranges and omitted ranges use defaults. Names reject embedded NUL. Keys and
IDs use exact canonical ASCII case; unknown/duplicate keys are invalid.
Only native Boolean cells set flags; numbers/text do not coerce to Boolean.
Numerics exclude bool, dates and text; counts require integral nonnegative
values within size_t and Excel's exact integer range (≤2^53-1). Date serials
follow the existing Date_ bounds. Reject errors/unsupported cells, half-empty
rows, header rows in input and partially populated bump rows.

`carry_bumps` is headerless m×2 (rate, yield). Entirely blank means both vectors
empty. An entirely blank column independently defaults that family; every cell
of a nonblank column must be positive numeric. A blank inside a nonblank
column is invalid; this is distinct from a wholly default column. Row count
must equal m when attached to a valuation. The independent settings constructor
checks column completeness; request construction checks m. This projects the
C++/Python independent empty-vector semantics without inventing explicit
default bumps. Get_CarryBumps returns zero data rows for two empty vectors;
otherwise max(lengths) rows with default-family cells blank. Its two value
columns (without header/index) round-trip directly as input.

Handle getters return detached values with anonymous names as existing price
getters do. Never replace an existing output handle until local construction
succeeds. Numerical failure still creates an inspectable result; malformed
input/preflight failure does not. No archive serialization.

## Excel table schema and order

All tables include one header row. Empty record sequences return the header
only. Scalar configuration/plan/execution tables use `Field,Value`, except
Configuration uses `Key,Value`. Enumerated values and fixed tags are text;
counts/indices are numeric; booleans stay Boolean. Optional means blank.
Each row/column order below is normative. Getter reads do zero native solves.

Configuration rows are scalar settings in declaration order, snake_case;
Get_CarryBumps columns are `SegmentIndex,RateBump,DividendYieldBump`.
ResolvedSettings returns a risk settings handle with filled bumps; its
Configuration/CarryBumps getters expose them. Plan also exposes origin flags.

```text
Summary Field rows:
  SchemaVersion,Status,PriceAvailable,RiskComplete,Price,PriceUnit,Method,
  RiskBasis,AadUsed,PlanId
  PriceUnit = price per unit underlying

Plan Field rows:
  SchemaVersion,PlanId,EvaluationDate,MaturityDate,Upper,GridPoints,GridSpacing,
  Epsilon,Theta,EffectiveRannacherIntervals,PenaltyFactor,MaxIterationsPerStep,
  LinearBackwardErrorLimit,InterpolationPolicy,SpotBumpOrigin,
  VolatilityBumpOrigin,PlannedScenarios,BasePlannedSteps,PlannedSteps,
  PlannedNumericPayloadBytes,PlannedWorkspaceBytes,PlannedPeakNativeBytes

PlanGrid columns:
  NodeIndex,Spot
PlanTimeGrid columns:
  TimeIndex,TimeYears,CalendarDate,IsExerciseDate,IsCarryKnot,IsMaturity
PlanSteps columns:
  SourceTimeIndex,TimeIndex,Dt,Theta,CarryIndex,Damped,Projection

Risks columns:
  RiskIndex,RiskId,ParameterId,DerivativeOrder,SegmentIndex,StartDate,EndDate,
  ParameterValue,Unit,Value,Status,Reason,Method,Interpretation,FineBump,
  CoarseBump,BumpOrigin,FineEstimate,CoarseEstimate,StepDifference,
  NormalizedStepDifference,StabilityTolerance

Stencils columns:
  RiskIndex,RiskId,Scale,EntryIndex,ScenarioIndex,Weight
  risk order, COARSE before FINE, entry order as R7

Scenarios columns:
  ScenarioIndex,ScenarioId,PlanId,ParameterId,SegmentIndex,ParameterValue,Offset,
  Status,SolverStatus,WorkLimitSource,PriceAvailable,Price,Epsilon,PlannedSteps,
  ActualSteps,CompletedSteps,TotalSolves,WorkUnits,NumericPayloadBytes,
  PeakWorkspaceBytes,MaxIterationChange,MaxLinearBackwardError,
  MaxObstacleViolation,MaxDualViolation,MaxComplementarityResidual,
  FailureStatus,FailureTimeIndex,FailureIteration,FailureCalendarDate,
  FailureNode,FailureResidual,FailureThreshold,FailureWorkUnits,FailureReason,
  EvaluationMembership,EvaluationBoundaryIntersectsSpotStencil,
  FutureContactChanged,UpwindChanged

Execution Field rows:
  PlannedScenarios,AttemptedScenarios,SuccessfulScenarios,FailedScenarios,
  NotRunScenarios,TotalSolves,WorkUnits,PlannedNumericPayloadBytes,
  PlannedWorkspaceBytes,PlannedPeakNativeBytes,NumericPayloadBytes,
  PeakWorkspaceBytes,PeakNativeBytes,WorkLimitScenarioIndex,WorkLimitSource
```

The price failure record is flattened in its existing member order with the
Failure prefix. Plan carry origins are read from the risk rows when requested;
`EarlyExercisePdeRiskResult_Get_BumpOrigins(result)` also exposes disabled axes, columns
`ParameterId,Origin`, in Spot, Volatility, then Rate[j], DividendYield[j] order.
This getter is part of schema version 1 and always has 2+2m data rows.
No other nested fields are silently dropped by a projection.

## Typical use and error handling

Given an existing valid price_request, the happy path is short:

```python
request = dal.EarlyExercisePdeRiskRequest_(price_request)
result = dal.EarlyExercisePdeRiskResult_New(request)
if result.price_available:
    print(result.price)
for risk in result.risks:
    print(risk.risk_id, risk.value, risk.unit, risk.status)
```

```cpp
const auto result = Dal::PDE::EvaluateEarlyExercisePdeRisk({priceRequest, {}});
if (result.PriceAvailable())
    std::cout << result.Price() << '\n';
for (const auto& risk : result.Risks())
    std::cout << risk.riskId_ << ' ' << risk.status_.String() << '\n';
```

Scalar `risk_value("Vega")` either gives a qualified finite estimate or raises
`EarlyExercisePdeRiskValueUnavailable` with riskId/status/reason. Unrequested
or misspelled IDs raise `EarlyExercisePdeRiskUnknownRiskId`. Price getter on
failed base raises `EarlyExercisePdeRiskPriceUnavailable`. Input errors use
`InvalidEarlyExercisePdeRiskRequest` with offending field/value/constraint;
preflight errors use `EarlyExercisePdeRiskBudgetExceeded` with budget field,
required and limit. Partial results always preserve readable records and failures.

## Open questions and release constraints

No blocking public-interface choice remains for S1. All method/availability,
default, schema, unit, budget and acceptance choices above are frozen. Windows
XLL availability and independent reference convergence are later evidence
dependencies, not permission to weaken the schema or claim acceptance.
Any failed smooth qualification or unavoidable old-price incompatibility must
return to DAL-497 with concrete evidence for an explicit decision. Keep the
current published price documentation until the implemented surfaces exist.
