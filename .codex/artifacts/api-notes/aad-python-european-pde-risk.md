# European PDE owning interface

Source: [active specification](../specs/aad-python-european-pde-risk.md).
Audience: public C++ and Python financial callers; the following Excel phase
reuses this result without exposing native active numbers.

## Surfaces

- Core `dal/math/aad/europeantheta.hpp`: EuropeanThetaSettings_,
  EuropeanThetaRecording_, RecordEuropeanOptions and shared validation/grid
  construction. Existing `AAD::Example` aliases preserve the repository example.
- Public `dal-public/src/europeanpderisk.hpp`:
  EuropeanPdeSettings_ (alias), EuropeanPdeRiskRequest_ (point/settings/budgets),
  EuropeanPdeRiskExecution_, EuropeanPdeRiskResult_ and
  EvaluateEuropeanPdeRisk(request).
- Python immutable keyword-only `EuropeanPdeSettings_` and
  `EuropeanPdeRiskResult_New(rate, volatility, strike, *, settings=None,
  numeric_payload_budget_bytes=None, recording_capacity_budget_bytes=None)`.
  Factory naming matches other owning risk results.

Settings keywords: grid_points=61, ordinary_steps=120, upper=400.0,
spot_index=None, expiry=1.0, dividend_yield=0.02,
forward_backward_error_limit=1e-12, transpose_backward_error_limit=1e-12.
An omitted index resolves the quarter-grid node; an explicit index is an
interior node. Result settings expose the resolved index.

Result properties: point, settings, grid, spot, prices, jacobian, payoff_labels,
parameter_labels, parameter_units, method, forward_backward_errors,
transpose_backward_errors, transpose_error_labels and execution.
Arrays/matrices/settings are detached copies; execution reports actual steps,
numeric_payload_bytes, peak_tape_bytes, cleanup_reserve_bytes and
reverse_scratch_peak_bytes. Preserve each optional caller budget.

## Example

```python
import dal

risk = dal.EuropeanPdeRiskResult_New(0.05, 0.20, 110.0)
call_price, put_price = risk.prices
call_rho = risk.jacobian[0, 0]
call_vega_per_point = 0.01 * risk.jacobian[0, 1]
```

## Choices and errors

A closed request avoids callbacks and Python tape ownership. Returning both
payoffs in one two-channel reverse shares all retained time-step factorizations.
The request form prevents a long C++ positional signature; Python keeps the
three required parameters first and optional settings/budgets keyword-only.
No interpolation or moving-grid derivative is implied by the spot node.

Errors identify EuropeanPdeRisk and the failing field/constraint. Invalid
Python types raise TypeError; checked numeric/native failures raise the
existing DAL RuntimeError mapping. A numeric payload limit is a retained-data
limit; the recording limit uses actual block/event charging, with separately
reported cleanup reserve. Neither is a full process-memory quota.

Rejected alternatives: duplicate the example chain inside dal-public, install
the test-support directory, expose a generic equation trampoline, or present
continuum Greeks as exact derivatives of this finite-domain discretization.
These add maintenance or change the accepted contract without closing the
specific financial boundary.

Compatibility: additive public/Python entry points, preserved example aliases,
unchanged native solve algorithms/default capability flags. No open API question.
