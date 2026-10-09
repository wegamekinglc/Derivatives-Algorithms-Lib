# P05 financial C++ API decisions

Status: active API decisions for the financial branch after #512 merged.

## Audience and initial public surface

The first entry is an explicit native C++ financial kernel over one prepared,
compiled Black–Scholes path. Subsequent MC aggregation and binding work adapts
this owning result; it does not put a tape, evaluator or borrowed adjoint in a
Python/Excel result. No existing risk mode or default strategy is renamed.

## Resumable model prerequisite

An immutable `AAD::BlackScholesStepPlan_` owns timeline/definition snapshots and
contains passive data only. Construct it from `timeline, definitions`.

- `Samples()` reports the number of model samples.
- `SimDim()` reports Gaussian increments, excluding a time-zero sample.
- `InitialLogSpot(parameters)` computes a fresh typed initial log spot.
- `Advance(sampleId, logSpot, parameters, gaussian)` emits a typed result containing
  `logSpot_` and a complete `sample_` without retaining an active prefix.

Parameters use the existing four-column order: spot, volatility, rate, dividend
yield. The Gaussian argument is the retained full vector for this path; the passive
plan computes the exact draw index and consumes no draw for time zero. Return by
value; the ordinary C++ lifetime of an active sample remains its recording's
responsibility. The detached segmented financial result never exposes this sample.

This opt-in utility is included explicitly, not by adding it to every model or
changing `Model_`'s virtual contract. Helpers share Black–Scholes numeric formulas
with existing initialization. They are implementation details rather than a second
public family of financial formulas.

## Financial adapter contract

`Script::BlackScholesSegmentedPath_` shares a
`std::shared_ptr<const BlackScholesSegmentedPreparation_>` and implements the core kernel. It exposes
`SimDim()`, `ParameterLabels()`, `Dimensions()` and
`Evaluate(parameters, gaussian, settings = {})`. The result is the core's owning
`AAD::SegmentedPathResult_`; its gradient columns correspond to the immutable
`ParameterLabels()` axis. Parameters append script constants to the four model
columns. One settings structure holds optional segment length/capacity budgets.

`PrepareBlackScholesSegmentedScript(product, valuation, snapshot = {},
contract = {}, smoothing = DEFAULT_SMOOTH)` prepares native compiled AAD explicitly.
Its opt-in preparation path resolves and compiles historical-only statements;
ordinary preparation's expired-product shortcut is retained. The resulting
`BlackScholesSegmentedPreparation_` can be moved into a shared const owner. Its
constructor is private to this factory; generic preparations cannot construct or
replace it. The wrapper owns the exact one-factor Black–Scholes preparation and
exposes only a const `Prepared()` view for independent ordinary-model evaluation.
The kernel shares the wrapper's lifetime and validates the sample definitions.
Model parameter values remain fresh request inputs. A generic `PreparedScript_`
does not establish model provenance and is not accepted by the kernel.

MC integration retains existing path numbering, random transform selection and
normalization. Results identify the explicitly selected segmented strategy and
prepared smoothing. The existing numeric calibration pullback receives the
detached model gradient; there is no nested outer activity graph.

## Errors and empty paths

Identify invalid parameter count/domains, timeline sample, Gaussian shape/value,
unsupported model or EXERCISE, inconsistent preparation/smoothing, unproved vector
shape, replay step/segment, budget admission and nested recording. Fail before
changing a caller graph whenever input admission can establish the failure.

The model step plan requires a nonempty valid timeline. A historical-only financial
kernel handles zero future samples without constructing that plan. It still returns
the complete label/gradient axis, including zero future model sensitivities and
historical constant sensitivities.

## Rejected alternatives

- Full active `BlackScholes_::Init`/`Scenario_` retained across segments: O(L) active
  prefix prevents the proposed tape bound.
- A second interpreter: duplicates financial semantics and fuzzy/vector behavior.
- Generic model resumability virtual methods now: unnecessary cross-model changes
  before the explicit first prototype is accepted.
- Automatic strategy or mode flags in all bindings now: the financial prototype
  and its workload boundaries must be accepted first.

## Typical flow

Prepare the script and freeze its Gaussian path; construct the immutable financial
kernel; pass fresh model-plus-script inputs, the path vector and explicit settings
to the segmented runner; read detached price, named gradient and capacity/replay
statistics. Repeat at a new point with a fresh recording while retaining the same
passive path when a common-path comparison is requested.

## Open questions

None requiring user input. The explicit fixed-path surface is implemented;
Monte Carlo aggregation, RNG integration and binding acceptance remain later
work within P05 and final integration.
