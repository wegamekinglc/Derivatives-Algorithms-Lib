# F02 weighted Excel boundary

Verdict: stage evidence accepted at `eb2be051`, including Windows/raw,
affected scalar paired performance and component-cost measurements. Current
[publication review repairs](aad-weighted-review-repairs.md) supersede this
stage's remaining-acceptance notes. Weighted VJP does not complete F02.

## Behavior and design

Weighted requests/results reuse the immutable scalar risk storable template
with distinct type tags. Settings use the existing two-column/semicolon-list
convention. Signed and zero weights are accepted, finite checks are explicit,
and factor/budget semantics remain unchanged. Native preflight precedes history
or workers; failure preserves a previously completed output handle.

The worksheet exposes a passive output query, the weighted objective, ordered
ID/label/slot/weight/mean component rows, one-row raw/report gradients, exact
shape, selected/complete inputs and retained provenance/history/product/model.
Getters copy passive data without valuation. Empty gradients spill one blank
cell while shape retains `(1,0)`. Archive serialization remains unsupported.

Shared typed valuation, selection, numeric parsing and snapshot-table helpers
remove duplication with scalar risk. Existing scalar signatures, settings,
table order and errors are retained. The component getter allocates its five
columns directly; the raw Jacobian getter avoids an extra matrix copy.

## Evidence

Evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- `aad-weighted-excel-red-01.log`: the independent analytic test fails to
  compile because weighted handle types and entry/getters are absent.
- `aad-weighted-excel-analytic-green-01.log`: tree/compiled values `9.5`,
  components `(6,5,5)`, raw `(5,3)` and reported `(2.5,6)` pass unchanged.
- `aad-weighted-excel-refactor-green-03.log`: shared helpers retain the new
  analytic case and all four old scalar cases. The refactor initially uses an
  incorrect provenance type name; compiler logs are retained and the name is
  corrected to the existing `RiskResultProvenance_` without assertion changes.
- Edge fixture compile failures for the history vector interface and embedded
  NUL string constructor are retained in the OFF object logs. Valid native
  fixture construction fixes these; values, budgets and tolerances are unchanged.
- `aad-weighted-excel-edge-green-07.log`: ten weighted and four old scalar cases
  pass, covering readonly/detached outputs, vector exclusion, signed/zero aliases,
  permuted inputs, exact/short budgets, preflight precedence, native-empty versus
  price-only, unsupported products, frozen history/caller destruction, nonfinite
  recovery, null getters and path errors.
- `aad-weighted-excel-off-green-08.log` and
  `aad-weighted-excel-combined-green-01.log`: 25 relevant cases pass per mode,
  including the old calibration/Dupire request consumers of shared parsers.
  Combined uses lifetime diagnostics, profiling, ASan/UBSan, leak detection and
  halt-on-error. Current affected Excel producers/tests/runtime and the already
  verified current native valuation/projection/tape/recording/profiling objects
  are instrumented. Unchanged support and Google Test are cached; this is
  focused instrumentation, not a fresh whole instrumented-library claim.
- The initial focused link omits the diagnostic chunk support object; adding
  that unchanged object repairs it. Extended consumer links expose missing
  runtime helper symbols in older cached support; rebuild only the current
  runtime unit in each mode. Retain link failures `off-01.log` and
  `{off,combined}-05.log`; no producer/library full rebuild is repeated.
- `aad-weighted-excel-generated-before-01.json` and
  `aad-weighted-excel-generated-drift-01.json`: twelve generated registrations
  and 24 `.inc`/`.htm` files exist, with zero drift on another generation.
  Path/boolean/settings guards precede their generated primitive conversions.
  A formatting pass first reflows two long markup directives and causes the
  retained `generate-02.log` failure. Split those directives into short lines;
  formatting and subsequent generation pass.
- Windows-only tests exercise strict raw weight ranges, flags/path counts,
  malformed shapes/NUL and the actual generated request export. These have not
  run on this Linux host; own-head Windows CI must supply that evidence.
- `aad-weighted-excel-complexity-final-02.log`: all affected helper/binding
  functions remain at most eight under the original Codacy limit.
- `aad-weighted-excel-warnings-repair-02.json`: the scalar/weighted producers
  and both existing request consumers pass GCC 14's unchanged warning flags.
  Change the diagnostic descriptor to `const char*` for its literal-only callers
  to avoid a false dangling-reference warning; the returned immutable handle
  value and error text remain unchanged. Retain the initial warning failures.
- `aad-weighted-excel-header-green-{off,combined}-01.log`: recompile only the
  two affected existing request producers per mode, relink and pass the same
  25 cases again after the descriptor repair.
- `aad-weighted-excel-configure-proof-02.json`: standalone Excel configuration
  against each current installed public prefix includes the new producer and
  test with matching diagnostic/profiling definitions. Use the verified cached
  Google Test libraries through FindGTest; an incomplete older generated
  package and its missing mock archive are retained in the initial logs.

## Acceptance limits

No full unrelated local suite is run. Preserve existing performance filters,
oracles, tolerances, two rounds, ten alternating samples per side, minimum
reduction and failure above four percent in both rounds. Earlier performance
or binary identity cannot certify changed shared simulation source. Component
costs for 1/4/16/64 outputs remain separate measurements. Keep #483 draft until
these and own-head CI/Codacy/review pass.
