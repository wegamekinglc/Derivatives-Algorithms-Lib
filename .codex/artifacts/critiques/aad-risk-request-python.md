# Python request binding critique

Verdict: Proceed with caveats.

Read the [spec](../specs/aad-risk-request-python.md),
[API](../api-notes/aad-risk-request-python.md), accepted native common/automatic
contracts, complete existing scalar/common Python bindings and closest tests.

## Blocking issues

None. No native algorithm or additional estimator is proposed.

## Significant concerns

- Sharing strict parsers can alter old scalar errors/validation timing or compiled
  default costs. Preserve literal old context and defer semantic factor checks to
  native planning as before; freeze and measure old workloads before accepting.
- Property return types must own copied matrices/lists/settings. Readonly Python
  attributes alone do not prevent mutation through a borrowed C++ reference.
- All provider metadata must remain native and typed. Curves cannot supply an
  invented quote value/PV/currency; IDs cannot replace source ownership checks.
- Pre-convert optional/required values under the GIL. Only known native sealed
  model graphs and frozen callback-free calibration records permit release.
  Planning itself must not resolve global fixing data.
- Empty quote selection still executes native valuation/VJP and retains mandatory
  inputs/full numeric payload. Test this through installed C++ parity and the
  existing smoothing/oracle protocols, rather than a mocked projection.
- Do not publish another head while the previous increment's own CI is still
  being accepted. Local next-increment work can proceed without cancelling it.

## Minor notes and counter-proposals

Use copied typed binding values rather than a loosely typed pair matrix. Keep
factory names consistent with current owning risk values. Keyword-only request
fields avoid another long positional execution signature. Common result getters
reuse the native projection implementation; no additional report cache is needed.

## Author questions

None requiring user input. Common and automatic surfaces remain separately
verified increments under full F01; Excel parity follows its own worksheet design.
