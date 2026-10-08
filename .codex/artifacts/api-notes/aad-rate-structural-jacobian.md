# Financial structural provider API

Audience: explicit native C++ complete curve-coordinate Jacobian callers.
Python/Excel projection belongs to the later binding acceptance boundary.

Separate header: `dal/curve/ratestructuraljacobian.hpp`. Required
trade/market/axis inputs come first; numeric payload settings come last.

```cpp
struct RateCurveParameterCoordinate_ {
    String_ componentKey_;
    size_t parameterOrdinal_;
};

RateStructuralJacobianDescriptor_ CaptureRateStructuralJacobian(
    const Vector_<RateTradeDefinition_>& trades,
    const RatePricingMarket_& market,
    const Vector_<RateCurveParameterCoordinate_>& inputAxis);

RateStructuralJacobianPlan_ PlanRateStructuralJacobian(
    const RateStructuralJacobianDescriptor_& descriptor,
    const AAD::StructuralJacobianSettings_& settings = {});

bool SameRateStructuralJacobianStructure(
    const RateStructuralJacobianDescriptor_& stored,
    const RateStructuralJacobianDescriptor_& current);
```

An unavailable descriptor owns its reason and the supplied ordered InputAxis and
OutputAxis metadata. RowSupport and PlanRateStructuralJacobian reject unavailable
proof. Inputs/Outputs retain the requested shape, including empty axes. Copies
share an immutable payload; moved-from descriptors must be reassigned before use.
Malformed coordinates throw with their component key before any AAD recording,
including later coordinates following an unsupported input. A parameter layout
that cannot be represented at the current valuation reports unavailable proof.
The caller captures current structure each request; equality compares
records, not fingerprints. Capture performs routing/proof/geometry admission and
owns the canonical records; it does not color or allocate direction/result data.
Only a cold or invalidated plan normalizes/colors/builds numeric metadata. A
reused plan compares its stored descriptor with a fresh capture, then binds and
re-records at the current numerical point. Building a second complete colored
plan merely to compare it would defeat this reuse boundary.

A later execution facade should take a request/config
structure instead of exposing more than six positional arguments.

The owning canonical descriptor remains private or read-only. Prefer a compact
opaque immutable payload if complete geometry/terms would otherwise spill large
pricing internals into this header. Keep comparison independent of numerical
curve values only when a conservative branch union proves that independence.
Changing trade scalar terms may conservatively rebuild even if the supports
would remain mathematically valid; correctness takes precedence over cache hits.

Rejected alternatives: use rowWidths as a proof; infer zeros from old risk;
key solely by names/hash/address/shape; retain mutable convention pointers;
move active Numbers across scopes; provide AUTO or forward strategies without
complete cost/capability evidence; put a cache on the ordinary pricing hot path.

Capture uses the pricing implementation's root enumeration, exact-family base
closure and request-local coupon builders. Stable traversal ordinals record
shared bases and market aliases without retaining their addresses. Complete
records include trade/convention fields, curve definitions/free-parameter
layouts, actual generated coupon and observation geometry, valuation and fixing
availability, and role-specific XCCY routes. A fixing exactly at valuation is
captured even when the historical-request list is empty. Curve parameter values
are excluded; trade/fixing scalar values may conservatively invalidate reuse.

This increment exposes dependency metadata and numeric plans. Complete financial
compressed execution and strategy selection remain separate acceptance work;
no financial execution or speedup is claimed by these entry points.
