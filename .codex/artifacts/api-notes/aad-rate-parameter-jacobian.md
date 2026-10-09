# Rate financial Jacobian execution API

Active interfaces for the financial execution increment. The dependency provider is accepted in [#509](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/509).

```cpp
struct RateJacobianExecutionSettings_ {
    bool vectorAdjoints_ = false;
    size_t adjointWidth_ = 1;
    std::optional<size_t> numericPayloadBudgetBytes_;
};

struct RateTradeParameterJacobianResult_ {
    Vector_<RatePricingTradeResult_> prices_;
    Matrix_<> jacobian_;
    Vector_<RateCurveParameterCoordinate_> inputAxis_;
    Vector_<String_> outputAxis_;
    size_t reverseDirections_ = 0;
    size_t reverseSweeps_ = 0;
};

RateTradeParameterJacobianResult_ RateTradeParameterJacobian(
    const Vector_<RateTradeDefinition_>& trades,
    const RatePricingMarket_& market,
    const Vector_<RateCurveParameterCoordinate_>& inputAxis,
    const RateJacobianExecutionSettings_& settings = {});

RateTradeParameterJacobianResult_ ExecuteRateStructuralJacobian(
    const Vector_<RateTradeDefinition_>& trades,
    const RatePricingMarket_& market,
    const RateStructuralJacobianPlan_& plan,
    const RateJacobianExecutionSettings_& settings = {});
```

The first operation is explicit dense reverse; the second is explicit compressed
reverse using a fully revalidated financial plan. Both return exactly the same
price and derivative definitions. No implicit AUTO policy or new enum is needed
for this execution increment. Automatic policy is a later measured increment.

`prices_` retains every trade's current passive pricing metadata and actual PV
currency. All entries must succeed for the complete matrix result to publish;
errors throw with row/trade context. The matrix always has rows equal to trades
and columns equal to the ordered input axis, including empty dimensions.
Repeated trade identifiers remain separate positional rows. Coordinates reject
duplicate physical curve/ordinal pairs even when their component keys differ.
No currency conversion, parameter scaling or quote transformation is implicit.

Scalar mode requires width one. Vector mode accepts the existing finite native
width range, including width one. Select and restore mode outside scope entry;
caller nesting fails without altering caller tape state. Counts report actual
seed directions and `ceil(directions / width)` sweeps, with zero for no colors.
Every PV and published derivative must be finite.

The optional numeric payload cap covers only the plan's result matrix plus color
direction matrix, consistently with `StructuralJacobianSettings_`. It excludes
PV metadata, curve/plan metadata, tape nodes and retained capacities. Enforce it
before allocating those numeric matrices, including on a reused plan. Do not
materialize dense Cartesian supports before checking the full dense numeric extent
or advertise a total process-memory budget. Validate matrix index limits and byte
extent arithmetic before casting sizes or beginning active recording.

Compressed execution owns its returned axes from the stored plan only after
fresh complete equality succeeds. Changing values under proven same structure
re-records local derivatives; changing order, observations, layout or routes
rejects. An explicit stale plan error is distinct from an unsupported active
curve graph. The later selector owns verified dense fallback/rebuild policy.

Typical call sequence: capture current descriptor, plan once, execute against
the current market, then execute the same plan against fresh numeric curve
values with identical proven structure. Compare `RateTradeParameterJacobian`
when measuring full dense versus compressed request costs.

Python/Excel integration follows the original binding stage. These APIs return
owning passive values and positional axes suitable for that projection. Existing
pricing, weighted portfolio gradients and quote-risk results retain their shape
and behavior.
