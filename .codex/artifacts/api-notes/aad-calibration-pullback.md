# F01 common calibration API decision

Status: active decision. Native opt-in curve record capture is accepted.
The shared C++ API is accepted with all 35 exact `12b3d7d` CI checks.
Python capture/common factories are accepted with full local verification,
incremental performance checks and all 35 exact `fb1a116b` CI checks. Excel is
accepted with portable/Windows correctness, generated integrity, changed-wrapper
performance and all 35 exact `a0801dec` CI checks. Full F01 request integration
and production performance remain open.
The [specification](../specs/aad-calibration-pullback.md) controls correctness,
identity, failure, resources and acceptance.

## Audiences and current surfaces

C++ callers, Python users and Excel worksheets need to move passive model/node
adjoints into the correct market quote space and audit the direct/calibration
split. Existing Dupire callers use a checked native VJP. Curve callers build
immutable RateQuoteRiskProvenance_ and aggregate trades through its retained
effective inverse. Keep those entry points and numerical contracts.

The common operation uses a shared owning result rather than merely renaming
two functions returning unrelated results. Its typed source retains the existing
domain axes and provenance, so no coordinate is reconstructed from display text.

## Source capture

Append `bool retainCalibrationRecord_ = false` to
`RateQuoteRiskProvenanceConfig_`. Add
`const std::string& RateQuoteRiskProvenance_::CalibrationRecord() const`.
Retain the complete existing canonical state bytes when requested. Preserve
fingerprint and inverse bytes and all factory signatures. Python adds the
corresponding strict Boolean config property. Excel's native provenance
factories add an optional capture flag last, with old worksheet prefixes and
test-export signatures retained. Its generic-joint dispatcher exclusion stays.

The common curve constructor rejects a provenance without captured content.
The caller may rebuild provenance from an already retained result with capture
enabled; this is record construction, not another calibration or inverse.

## C++ boundary

Add `dal-public/src/calibrationrisk.hpp` and its implementation. Expose:

```cpp
class CalibrationPullback_;
class CalibrationQuoteRisk_;
struct CalibrationParameterSeedTag_;
struct CalibrationDirectQuoteSeedTag_;
template <class T_> class CalibrationAdjoints_;
using CalibrationParameterAdjoints_ = CalibrationAdjoints_<CalibrationParameterSeedTag_>;
using CalibrationDirectQuoteAdjoints_ = CalibrationAdjoints_<CalibrationDirectQuoteSeedTag_>;

CalibrationPullback_ NewCalibrationPullback(
    const DupireCalibrationSnapshot_& calibration);
CalibrationPullback_ NewCalibrationPullback(
    const RateQuoteRiskProvenance_& calibration);

CalibrationParameterAdjoints_ NewCalibrationParameterAdjoints(
    const CalibrationPullback_& calibration, const Matrix_<>& adjoints);
CalibrationDirectQuoteAdjoints_ NewCalibrationDirectQuoteAdjoints(
    const CalibrationPullback_& calibration, const Matrix_<>& adjoints);

CalibrationQuoteRisk_ PullbackCalibration(
    const CalibrationPullback_& calibration,
    const CalibrationParameterAdjoints_& parameters,
    const std::optional<CalibrationDirectQuoteAdjoints_>& direct = {});
```

Both seed roles use one shared implementation and aliases/tags.
Each owns the immutable source and matrix values. Required arguments come first.
No callback supplied by the caller or per-node polymorphic interface is needed.

`CalibrationPullback_` exposes its typed source, domain, parameter/quote matrix
dimensions and method/unit/boundary. Store the typed source as a closed variant
of the existing Dupire snapshot and curve provenance under immutable ownership.
`CalibrationQuoteRisk_` exposes `Calibration()`, `CalibrationAdjoints()`,
`DirectAdjoints()`, `TotalAdjoints()`, `Method()`, `Unit()` and `Boundary()`.
All returned references are const; binding numeric copies are detached.

Dupire matrices keep their existing native orientation. Curves use a single
column in global parameter or quote ordinal order. Domain axes remain available
on the typed source: Dupire inputs/surface, or complete RateQuoteRiskAxis_ and
RateQuoteRiskState_. Do not flatten duplicate quote names into identity keys.
Curve method/unit/boundary are `RetainedCurveEffectiveInverse`,
`DECIMAL_QUOTE`, `FrozenCalibrationEffectiveInverse`. Dupire retains its accepted
labels. A common result is not automatically an exact continuous-calibration
derivative or a complete market Greek.

Typical Dupire usage:

```cpp
const auto boundary = NewCalibrationPullback(calibrated);
const auto nodes = ExtractDupireParameterAdjoints(valuation, calibrated, "equity");
const auto parameters = NewCalibrationParameterAdjoints(boundary, nodes.adjoints_);
const auto risk = PullbackCalibration(boundary, parameters);
const Matrix_<>& quotes = risk.TotalAdjoints();
```

For curves, set the capture flag before existing provenance construction, then
pass the node gradient as an M-by-one matrix. Calibration, pricing and currency
grouping remain explicit. This boundary does not rerun either operation or
convert currencies. Portfolio seeds can be summed in matching coordinates
before a single pullback; distinct sources must remain separate.

## Python

Expose `CalibrationPullback_New(calibration)`,
`CalibrationParameterAdjoints_New(calibration, adjoints)`,
`CalibrationDirectQuoteAdjoints_New(calibration, adjoints)` and
`PullbackCalibration(calibration, parameter_adjoints, *, direct=None)`.
Return read-only source/seed/result objects. Source projection returns the
owning registered Dupire snapshot or curve provenance, not a serialized label.
Matrices are explicitly two-dimensional; curve inputs are M-by-one columns.
Accept owning `DoubleMatrix_` values or rectangular lists/tuples. Raw sequence
cells reject bool/enums, coercion and non-finite values; both routes reject
wrong shapes, non-finite stored doubles and wrong domain objects. The old generic
matrix constructor's prior normalization remains unchanged. Required source cannot be None.

Projection retains the GIL. Callback-free native mapping may release it with
owning arguments retained; it never invokes a Python IVS callback because
Dupire has already frozen that input. Return detached numeric matrices. Common
values are readonly and support copy/deepcopy, but reject pickle/native archives.
Keep the original two-argument config overload first; a separate keyword-only
capture overload accepts actual Python bool only.

## Excel

Use immutable native-value wrappers and these factories:

- `CalibrationPullback_New(name, calibration)` takes a Dupire calibration
  handle or a native captured RateQuoteRiskProvenance handle.
- `CalibrationParameterAdjoints_New(name, calibration, adjoints)` and
  `CalibrationDirectQuoteAdjoints_New(name, calibration, adjoints)` take the
  common boundary and a numeric matrix in its canonical layout.
- `CalibrationQuoteRisk_New(name, calibration, parameters, direct = blank)`
  performs the common operation.

Getters expose boundary source/provenance, copied seed matrices and
`CalibrationQuoteRisk_Get_Adjoints(result, contribution = blank)`,
`CalibrationQuoteRisk_Get_Calibration(result)` and
`CalibrationQuoteRisk_Get_Provenance(result)`. Blank contribution means total,
as for the accepted Dupire getter; validate other selectors explicitly.
Provenance reports domain, method, units, boundary and canonical source identity.
Do not publish the full potentially large canonical record in a spill unless
explicitly requested. The source getter retains typed access to its axes/state.

Reject null/wrong-type and non-native legacy provenance. Support local integer
normalization in numeric ranges and original Excel text/NUL checks. Validate
the optional capture flag before coercion. Keep settings error row/column
context, output-handle preservation and explicit archive rejection. Windows
native declarations precede SDK input headers. Regenerate every markup/inc/HTML
pair and compile actual Windows registration and DLL imports.

## Alternatives and compatibility

Reject a shared name alone: it misses the required common result structure.
Reject label-based flattening and fingerprint-only equality: they lose source
identity. Reject eager canonical record storage: it adds default-path cost.
Reject a new inverse/transposed solver: it changes the current calibration map
and belongs to F03. Reject adapting by repricing synthetic trades: the boundary
accepts existing adjoints and must remain passive.

Keep legacy Dupire and rate aggregation entry points. Record capture changes
storage only; it is not a new solve setting. Native config aggregate prefixes
remain supported, with no binary-layout or structured-binding-arity promise.
The shared mapping helper must preserve legacy arithmetic/allocation/error
behavior and pass unchanged performance gates. New-entry overhead is measured
and reported independently. Full market-coordinate request/budget integration
and all three binding acceptances remain open until actually implemented.
