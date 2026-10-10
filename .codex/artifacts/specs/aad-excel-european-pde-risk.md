# Excel European PDE risk

## Source and problem

Merged #534 (`1ee4b6c19f74b99aeea9ac66d815b550edc3829d`) accepts the
owning C++/Python fixed-grid European financial program. The remaining Excel
phase needs typed worksheet access to the same program and passive results.
The accepted native algorithms, first-derivative interpretation and separate
payload/tape budgets remain the numerical contract.

## Goals and exclusions

Add immutable settings, request and result handles; strict raw worksheet
admission; labeled passive result spills; generated registration/help; and
actual Windows export execution. Reuse the public evaluator and existing
generic risk-value storable template. No new solver, native recording API,
callbacks, interpolation, Delta/Gamma, curvature or other Excel risk family
is part of this increment.

## Requirements

1. Constructors are `EuropeanPdeRiskSettings_New(name, [settings])`,
   `EuropeanPdeRiskRequest_New(name, rate, volatility, strike, [settings])`
   and `EuropeanPdeRiskResult_New(name, request)`. Optional settings use
   native defaults. Required arguments precede defaults; no long positional
   mesh/budget signature is introduced.
2. The settings input is a two-column key/value range without a header.
   Canonical keys are grid_points, ordinary_steps, upper, spot_index, expiry,
   dividend_yield, forward_backward_error_limit,
   transpose_backward_error_limit, numeric_payload_budget_bytes and
   recording_capacity_budget_bytes. Use existing case-insensitive settings
   semantics and reject unknown or duplicate keys, malformed rows and NUL.
3. Numeric cells must contain finite numbers; reject bool, text, dates and
   Excel errors rather than coercing them. Integer fields must be exactly
   integral and representable. Budgets use the existing nonnegative size_t
   admission with the worksheet ceiling of 2^53-1. Explicit zero is a limit.
   Blank optional index/budget cells mean unset; blank required numeric
   settings fail. A wholly blank settings range uses defaults.
4. Add the small public inline `ResolveEuropeanPdeSettings` projection of
   existing native physical-settings validation. Excel calls this public
   facade, preserving `dal-cpp <- dal-public <- dal-excel`. Settings
   construction validates without allocating a grid or recording. Keep an
   omitted index unset in the settings handle; native evaluation returns
   its resolved snapshot. Existing C++/Python numerical callers are unchanged.
5. Reserve one header row within the existing Excel output ceiling of
   1,048,576 rows: grid_points <= 1,048,575 and ordinary_steps <= 1,048,573.
   Enforce these Excel-specific bounds before execution/allocation. Do not
   narrow native C++/Python bounds or silently coarsen a request.
6. Requests copy settings, budgets and three finite parameters. Construction
   is passive and does not price, capture a date or modify a tape. Financial
   point-domain/kink and budget admission run in the public evaluator when
   creating a result. A failed factory must preserve its previous output
   handle; caller graph/mode preservation remains the public contract.
7. Results own the complete public result. Getters perform no recording,
   pricing, history access or worker submission. Returning request/settings
   handles or cell matrices must detach copies from original inputs and from
   previous getter outputs. Reject null/wrong handles and unsupported archives
   with existing contextual risk errors.
8. Settings and point getters return canonical two-column key/value data.
   Request_Get_Settings and Result_Get_Request return copied handles.
   Result_Get_Prices returns a header and Call/Put rows; Get_Jacobian returns
   six labeled payoff/parameter/unit/derivative rows. Get_Grid returns labeled
   zero-based node indices and physical spots.
9. Get_ForwardErrors returns a header plus one chronological row per actual
   step, columns Step/Call/Put. Get_TransposeErrors returns Step and the four
   exact public layer/seed labels. Step ordinals are one-based; node indices
   are zero-based. Get_Execution returns method and all five execution counters.
   Derivatives retain raw decimal rate/volatility and strike units.
10. Numeric-payload and recording limits retain #534 semantics. The native
    payload is `8*(17+N+6*(ordinary_steps+2))`; worksheet cell/label/handle
    overhead and additional getter copies are outside that budget. It is
    neither an Excel heap quota nor a universal tape-capacity estimate.
11. Normalize supported xltypeInt/one-cell inputs before generated
    conversions; reject nonnumeric required parameters before conversion can
    erase their type. Generate all wrapper/help outputs with Machinist, and
    preserve uppercase dotted names, exact argument names/types, nonvolatile
    registration, complete help and resolvable XLL exports.

## Executable acceptance

- First RED is a portable typed-handle test requiring the missing Excel
  header/functions and accepted 9-node/8-interval prices. Add subsequent RED
  cases for admission, copied spills and missing registrations as needed.
- Verify both prices and all six risks against accepted financial constants;
  check every diagnostic and resource field, not just a single price. Add one
  nondefault/negative-rate contract to verify settings/point propagation.
- Cover defaults, optional blank/zero/exact/short budgets, invalid numeric
  types/limits/rows/keys, worksheet extent boundaries without executing huge
  meshes, null/wrong handles, atomic failure/recovery and detached ownership.
- Exercise passive constructors/getters beside a caller graph, then reject
  result execution without graph/seed/mode loss. Test late header inclusion to
  keep generic-storable metadata lookup independent of include order.
- Run strict OFF/combined syntax, fresh scoped portable tests and generated
  consistency checks. Rebuild only dependencies actually affected; preserve
  accepted numerical evidence only with source/configuration/binary proof.
- Windows tests must inspect all 13 registration contracts and call actual
  exports for valid numeric inputs, type/range failures and owning spills.
  Required exact-head CI and installed consumers remain mandatory.
- Preselect the two affected full financial requests at 9/8 and 61/120 for
  native versus typed-Excel boundary cost. Keep >=25ms loops, two rounds of
  ten alternating pairs, complete observations and unequal-contract disclosure.
  No full benchmark or parameter matrix.
- Complete local review, published docs/changelog, all remote review/CI/Codacy
  repair, actual runtime capture, two final audits and guarded merge before
  the next Excel family.

## Open questions

None blocking. Existing repository settings, handle, error and output conventions
resolve the routine interface choices. This is the first Excel increment;
Dupire/rate/MC/LSMC and final integration remain separate deliveries.
