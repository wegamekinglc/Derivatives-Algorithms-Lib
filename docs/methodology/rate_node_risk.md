# Rate-Trade Node Risk and Portfolio Aggregation

DAL computes rate-trade PV gradients in native curve-parameter coordinates.
`RateTradeNodeSensitivities` selects one market component;
`RateTradeNodeSensitivitiesBatch` evaluates a shared component list across trades;
`AggregateRatePortfolioNodeRisk` sums the eligible results.
The interfaces and runnable examples are in the
[public API guide](../public-api.md#c-rate-cashflow-pricing).

## Coordinates and Supported Trades

For a trade PV $V$ and a selected component's free parameter vector $x$,
the returned gradient is

$$
g_j = \frac{\partial V}{\partial x_j}.
$$

The coordinate depends on the curve representation: piecewise-constant or
piecewise-linear forwards, log discount factors, or continuously compounded
zero rates. `BuildCurveParameterLayout` owns parameter count and order;
`DescribeCurveFreeParameters` supplies the date and component labels.
Fixed anchors are not free parameters. A gradient is price per unit native
parameter, not a market-quote DV01.

| Family     | Addressable rate components                                        |
|------------|--------------------------------------------------------------------|
| Deposit    | Discount                                                           |
| FRA        | Forecast and discount                                              |
| Future     | Forecast                                                           |
| OIS / IRS  | Forecast and discount                                              |
| Basis swap | Spread forecast, reference forecast, and discount                  |
| XCCY       | Consumed domestic/foreign discount and forecast curves, plus basis |

Terms identify single-currency curves by component key. For XCCY, the
market-aware `BuildRateCashflowPlan(trade, market)` selects curves actually
consumed by the collateral and tenor configuration, then matches their object
identity to `market.curveComponents_`. An unused member of a curve block is
not a dependency. Matching a display name does not establish this identity.

All seven families require the matching terms alternative and supported curve
representations. Historical fixings supplied in the immutable snapshot remain
constants; projected future fixings can contribute curve risk. XCCY FX spot
also remains constant, so this surface does not include FX delta or volatility
risk.

## AAD and Passive Dependencies

Single-currency passive and active pricing use the same templated kernels.
The selected component is rebuilt with AAD parameters. Other curves remain
`double` curves behind the heterogeneous `CurveRef_` view; the selected
curve's base also stays passive. This prevents passive OIS daily compounding
from inflating the tape.

XCCY uses uniformly typed market/curve-block views. Consumed curves are
active-typed, but only the selected component's parameters are registered as
independent variables. Non-target parameters and FX spot stay constant.

Each sweep owns its thread's tape through `TapeGuard_`, registers inputs,
records pricing, seeds the PV adjoint, propagates, and validates the finite
gradient and its expected width. The guard rewinds on exit, including failure.
Batch execution is serial and deterministic; it does not dispatch sweeps to the
thread pool. See [AAD methodology](aad.md) for backend and tape ownership.

## Eligibility and Failure Isolation

The failure result is `eligible_ = false`, `pv_ = 0`, an empty
`gradient_`, and a stable non-empty `reason_`. Single-currency gates check:

1. Family and terms compatibility.
2. Whether the trade depends on the requested component.
3. Availability and representation of every consumed curve, in dependency order.
4. Passive trade validation.
5. AAD execution and finite-result/width validation.

The six tokens and XCCY-specific priority rules are defined in the
[public contract](../public-api.md#c-rate-cashflow-pricing). In particular, an
unclassifiable non-target XCCY dependency reports `AAD_EVALUATION_FAILED`;
an unresolved live XCCY market reports `TRADE_VALIDATION_FAILED`.
Use the passive pricing result's `error_` and missing-fixing list for detail.
An unavailable sensitivity is distinct from an eligible zero gradient.

The batch returns the Cartesian product of trades and one shared key list,
in trade-major then key order. Each cell has the single-trade result plus
instrument and component identifiers. Failed cells leave siblings available.
The sweep engine caches passive pricing per trade and curve preparation per
curve within the call.

## Aggregation and Currency

The aggregate retains one dense `Report_` per successfully prepared component,
with one `node` axis. Each node header contains its date and parameter-component
label. There is no padding across components with different node counts.
A prepared component with no eligible contribution has a zero tensor;
an unpreparable component has no tensor.

The tensor sums eligible gradients by component key. It has no currency axis.
For currency-separated node risk, group eligible batch cells by both component
key and the trade's actual PV currency before summing. Do not infer a common
denomination solely from a shared curve dependency.

PV totals use `pvByActualPvCcy_` and the
`UnconvertedByActualPvCcy` policy: single-currency trade PV uses its trade
currency; XCCY PV uses the domestic currency from covered-interest parity.
`RatePricingTradeResult_::currency_` is a market result-currency label and
does not perform conversion. Each trade with at least one eligible cell
contributes PV once. Duplicate component keys retain their cells and metadata
but contribute a gradient only once per trade and component.

The parallel metadata retains eligibility, reason, actual PV currency, and PV
for every requested cell. Component tensors are not complete risk reports by
themselves; retain this metadata to identify omitted contributions.

## Bindings and Quote-Space Risk

Python calls are keyword-only, return read-only projections, and release the
GIL for native batch work. `component_keys` must be a list. Python conversion
errors can raise before native per-cell failure handling.

Excel's batch spill contains `trade, component, reason, pv, node, value`.
The aggregate spill adds `currency`: component-node rows have a blank currency;
PV rows carry the actual currency and policy, and failure rows carry their
reason and actual currency.

For supported exact calibration domains, use
`AggregateRatePortfolioQuoteRisk` with frozen provenance to obtain
currency-separated quote sensitivities and DV01. It accumulates complete
gradients by provenance and actual PV currency before applying the retained
effective inverse and solver-scale correction. It does not transform the
already-summed component tensors. See
[quote-space DV01](yield_curve_jacobian.md#production-quote-space-dv01).

## Source and Verification

- `dal-cpp/dal/curve/ratecashflowpricing.hpp` and
  `dal-cpp/dal/curve/ratecashflowpricing.cpp`: public shapes, sweep engine,
  dependency resolution, pricing kernels, and aggregation.
- `dal-cpp/dal/curve/ratecashflowpricing_internal.hpp`: family registry,
  failure finalization, and tape guard.
- `dal-cpp/tests/curve/test_ratecashflowpricing.cpp`: central-parameter-bump
  comparisons, active/passive PV agreement, dependency and fixing failures,
  tape isolation, batch equivalence, duplicate keys, and currency metadata.
- `dal-python/tests/test_curve_pricing.py` and
  `dal-excel/tests/test_curvepricing.cpp`: binding contracts.
- `dal-cpp/benchmarks/rate_risk_perf/rate_risk_perf.cpp`: rate-pricing,
  node-risk, and quote-risk workloads in the paired regression gate.
