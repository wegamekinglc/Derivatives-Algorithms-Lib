# F02 weighted Python boundary

Verdict: stage evidence accepted at `eb2be051`, including Excel, affected scalar
paired costs and 1/4/16/64-component measurements. Current
[publication review repairs](aad-weighted-review-repairs.md) supersede this
stage's remaining-acceptance notes.
This increment does not complete F02.

## Behavior and design

The keyword-only weighted request composes native output/input selection with
passive weights. The output query indexes a product copy and returns detached
ID/label/slot coordinates. The owning result exposes the objective mean/gradient,
component means, ordered weights/outputs, report projection and provenance.
Every container/matrix property is detached; copy/deepcopy retain passive data.

Scalar and weighted Python entries share a typed helper that copies path count,
handles, requests and settings before releasing the GIL. The numeric-list parser
is shared with existing report-factor requests, preserving their error context.
It rejects bool, enum, text, nested lists and overflow instead of silently
converting them. Native preflight checks selections, finiteness and exact payload
before history/workers. No active native number crosses the Python boundary.

## Evidence

Evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- `aad-weighted-python-red-01.log`: both evaluator modes fail because the old
  installed module has no weighted request type. Production binding follows.
- Five affected binding units are compiled from current source in each mode:
  module, scalar risk, weighted risk and both existing request-parser consumers.
  Four changed native units are rebuilt in each mode. Unchanged binding objects
  and installed support are reused; these are focused checks, not fresh whole
  library builds. Commands/results are retained in the corresponding object
  directories and link-command JSON files.
- The first OFF link uses an older calibration-only support prefix and fails
  import on missing Dupire-plan symbols; a rebuilt plan exposes further missing
  support symbols. Retain `analytic-green-01.log` and `analytic-green-02.log` as
  failures. Replace support with the accepted Dupire-request install, then overlay
  the current native objects. `analytic-green-03.log` passes both analytic cases.
  Combined mode starts from the accepted combined Dupire-request install.
- `aad-weighted-python-off-green-04.log` and
  `aad-weighted-python-combined-green-01.log`: 179 cases pass in each mode across
  weighted risk, scalar risk, calibration requests and Dupire requests. Coverage
  includes independent `9.5 / (5,3)` values, reported `(2.5,6)`, aliases/direct
  inputs/history, signed/zero weights, input order, exact budgets, native-empty
  smoothing versus explicit price only, ownership/deepcopy, preflight precedence,
  zero-weight nonfinite failure/recovery and a heartbeat during actual native
  work. These runs do not repeat the full Python suite.
- The API note freezes spot step 0.1, 4096 common paths, two evaluator modes and
  absolute/relative `1e-10` tolerances before first measurement. Independent
  passive central differences agree with the native weighted spot derivative in
  the first run; no step or tolerance is revised.
- Standalone OFF/combined CMake configuration against the isolated installed
  prefixes succeeds and includes the new source. Final whole installed-library
  and platform acceptance remains with own-head CI.
- `aad-weighted-python-complexity-01.log`: all new binding/helper functions have
  complexity at most eight. Final own-head Codacy remains required.
- GCC 14's unchanged warning flags pass for weighted/scalar risk and both request
  consumers. The extra module syntax check reports the existing empty variadic
  argument in `PYBIND11_MODULE`; the identical check against merged master's
  module reproduces it in `aad-weighted-python-warning-module-baseline-01.log`.
  No warning is suppressed or passing whole-binding warning build claimed.
  Formatting, patch integrity and 142-file documentation checks pass.

## CI repair and acceptance limits

Head `e0d1a845` has successful Codacy with zero issues/annotations, but eleven
build jobs fail. Inspected Linux OFF and MSVC logs each report exactly one failed
case: the weighted caller-snapshot test parses FIX before explicitly initializing
index parsers. CTest starts it independently, unlike the earlier grouped run.
The focused cached binary also passes in isolation before the repair, so it is
not evidence reproducing the fresh CI failure.

Commit `4b4ebc3b` calls `InitGlobalData(1)` in that test and includes the public
global header. Assertions, tolerances, filters and production code are unchanged.
The rebuilt isolated combined sanitizer test passes in
`aad-weighted-ci-isolated-green-01.log`. Linux/MSVC job logs preserve the failed
fresh-build evidence; the new CI head must confirm the repair.

New bindings have not yet been accepted by exact-head full CI/review. Keep #483
draft until Excel parity, performance and final gates pass. Earlier scalar
performance results cannot cover the changed shared simulation source.
