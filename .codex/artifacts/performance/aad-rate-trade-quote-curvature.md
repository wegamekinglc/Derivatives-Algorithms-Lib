# Rate-trade curvature scoped cost acceptance

Status: scope selected; no new timing result is claimed.

Baseline is merged #524, `b5b17a0f362daa444deb27406245583d6523cf10`.
Freeze the implementation head and both source/configuration identities before
measurement. Rebuild changed objects and selected binaries; retain hashes and
raw interleaved observations. Use the established two-round paired sampling,
warmup, checksum, single-thread/noise controls and sustained 4% old-caller gate.

## Changed paths and selected cases

- `dal-public/src/ratecurvature.cpp` and its public header: measure a complete
  new single-curve weighted six-family request, and the existing generic
  single-curve complete quote-curvature caller as a baseline/head control.
- The same adapter's fixed/free graph boundary: measure staged basis-only XCCY
  and layered joint-XCCY complete financial requests. Compare new paths with
  equivalent explicit objective capture plus the existing curvature composition;
  report their complete latency without claiming a previously existing adapter.
- `dal-cpp/dal/curve/ratecashflowpricing.cpp` and the focused internal objective
  declaration: measure one affected existing multi-component parameter-Jacobian
  caller as the core baseline/head control. Confirm dependency analysis when the
  final internal-header scope is known.

This is five small cases. The financial functional tests cover all seven trade
families and four calibration families; same-currency joint graph reconstruction
is covered by that acceptance and the core multi-component caller. Do not add a
Cartesian product of portfolio counts, curve families, diagnostics or widths.

## Exclusions and repair rule

No tape primitive, RNG, PDE, Monte Carlo, Dupire or linear-algebra implementation
changes are planned; their benchmark matrices are excluded. Native calibrators
remain unchanged and are included through complete affected financial requests.
Required exact-head CI and scheduled full monitoring remain applicable.

After repair, record which timed paths or executable identities changed before
repeating only those pairs. Add cases only for an observed failure, dependency
change or specific coverage gap, with the reason recorded first. Do not claim
excluded or identity-inapplicable evidence as a new pass.
