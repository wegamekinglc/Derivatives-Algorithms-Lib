# European PDE worksheet API

Source: [active specification](../specs/aad-excel-european-pde-risk.md).
Audience: worksheet authors and installed XLL callers using the accepted
public financial request, without owning active AAD values.

## Surfaces

The public facade gains only inline ResolveEuropeanPdeSettings(settings),
delegating existing physical validation and quarter-node resolution. Existing
evaluator, Python factory, native algorithms and capability flags are unchanged.

Excel adds __europeanpderisk.hpp/.cpp and immutable typed aliases backed by
the existing StorableRiskValue_ template. Define value metadata beside each
value's namespace for argument-dependent lookup; do not duplicate the generic
storable or make existing risk headers depend on this new family.

Thirteen worksheet functions:

- EuropeanPdeRiskSettings_New(name, [settings])
- EuropeanPdeRiskSettings_Get_Configuration(settings)
- EuropeanPdeRiskRequest_New(name, rate, volatility, strike, [settings])
- EuropeanPdeRiskRequest_Get_Settings(request)
- EuropeanPdeRiskRequest_Get_Point(request)
- EuropeanPdeRiskResult_New(name, request)
- EuropeanPdeRiskResult_Get_Request(result)
- EuropeanPdeRiskResult_Get_Prices(result)
- EuropeanPdeRiskResult_Get_Jacobian(result)
- EuropeanPdeRiskResult_Get_Grid(result)
- EuropeanPdeRiskResult_Get_ForwardErrors(result)
- EuropeanPdeRiskResult_Get_TransposeErrors(result)
- EuropeanPdeRiskResult_Get_Execution(result)

All getters have one output, avoiding an unnecessary layout argument. Uppercase
dotted names follow generated registration. Settings_Get_Configuration returns
all ten canonical rows, with unset optional values blank. Request_Get_Point
returns rate/volatility/strike rows. Results return the actually resolved request.

## Output contract

Prices: Payoff/Price header, Call then Put. Jacobian:
Payoff/Parameter/Unit/Derivative header, payoff-major six rows preserving the
public parameter order and units. Grid: NodeIndex/Spot header, zero-based node
indices. Forward errors: Step/Call/Put; transpose errors: Step and the four
public layer/seed labels. Steps are chronological and one-based. Execution is
method plus actual_steps, numeric_payload_bytes, peak_tape_bytes,
cleanup_reserve_bytes and reverse_scratch_peak_bytes.

## Typical worksheet

Place grid_points/9 and ordinary_steps/8 in a two-column range A1:B2:

```text
D1 = EUROPEANPDERISKSETTINGS.NEW("small", A1:B2)
D2 = EUROPEANPDERISKREQUEST.NEW("request", 0.05, 0.20, 110, D1)
D3 = EUROPEANPDERISKRESULT.NEW("risk", D2)
F1 = EUROPEANPDERISKRESULT.GET.JACOBIAN(D3)
```

## Errors and compatibility

Strict raw guards retain scalar/range types before generated conversion.
Required point arguments reject blanks, text, booleans, errors and nonfinite
numbers. Typed settings require numeric/integer domains and native [0,1]
solve policies. Cell-type and individual scalar-domain errors include row,
column and key; combined physical constraints retain native validation context.
Financial point-domain/kink and
actual budget failures occur during result execution. Failed factories preserve
previous output handles. Handles reject archives rather than publishing an
incomplete serialized object.

The worksheet output ceiling reserves one header row; native bounds remain
unchanged. Native budget counters exclude worksheet spill/label/handle copies.
No automatic per-bp/per-vol-point scaling or higher-order claim is introduced.

Rejected alternatives: a dozen mesh/budget positional arguments; duplicate
financial logic; untyped dictionaries replacing immutable handles; a generic
equation callback; changing the shared storable implementation solely for this
family. No blocking API decision remains.
