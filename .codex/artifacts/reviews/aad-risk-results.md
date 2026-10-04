# Structured scalar risk-result review

Verdict: Approve the scalar D04 boundary at exact `e3720f9f`.
The complete core/public/Python/Excel surface is implemented; this review does
not treat a passive converter alone as completed D04.

## Findings

No unresolved correctness finding after the following review fixes.

- Exact `d2916e9c` CI exposes an overstrong Python parallel equality fixture:
  two MSVC configurations and the Windows/ARM-macOS wheels fail the same new
  PV assertion, with the Windows gate consequently failing. A local four-worker
  run reproduces the failure. The shared simulation sums by worker, so ordinary
  scheduling changes rounding even between two legacy requests. A 64-pair
  control per evaluator sees legacy/self and legacy/structured discrepancies
  of at most 7.11e-15, all within the predeclared rel/abs 1e-10 contract.
  Preserve every original exact assertion and all 257 paths in an isolated
  one-worker process, and add four parallel tree/compiled 257/2057-path cases
  with that numeric contract and exact within-result getter checks. No production
  arithmetic, scheduler or tolerance contract changes. The focused suite passes
  20 cases; the complete four-worker Python suite passes 813. All 35 exact
  corrective `e3720f9f` checks pass, including Windows/macOS verification.

- Caller-provided execution snapshots now validate product date/event extents,
  path count consistency, positive replicate counts and finite supplied history.
  The missing extent check first fails its new test, then passes. This prevents
  the Excel product getter from indexing an incomplete event vector. Arbitrary
  conversion metadata still cannot certify actual execution.
- Machinist optional reported/complete booleans now explicitly default to false.
  A compiler probe over the actual generated declaration/call first fails on
  optional<bool>-to-bool conversion, then compiles and preserves omission,
  false and true. Regeneration changes only the two corresponding inc/HTML pairs.
- Excel path failures now identify InvalidPathCount, function and n_paths before
  other work. The new test first fails, then the implementation shares the
  existing error wrapper. Existing typed entries retain their message prefixes.
- Two new functions exceeded CCN eight. They were split without changing the
  repository threshold; the complete inspected production surface now passes.

## Correctness and compatibility

Numeric extraction uses validated ordinal IDs, not the legacy label map. Mean
gradients are not divided again, native empty inputs preserve the fuzzy estimator,
reported matrices are detached copies, and raw compatibility keys reject display
collisions. Numeric budget checks precede preparation/history/workers. Settings,
request and model/product handles are copied before callbacks or released GIL.

Actual public provenance retains passive model/product/history and execution
settings, including LSM replicate counts and the mixed retrained-policy method.
Python read-only properties return copies; Excel getters do no valuation and
wrapper serialization fails explicitly. Old simulation/LSM hot loops are unchanged.
Shared parser/validation helpers avoid duplicating their contracts; model-type
validation retains namespace-scope initialization.

## Tests

Retained RED/GREEN evidence covers missing interfaces, the real callback mutation
defect, malformed Excel factor context, supplied snapshot geometry, generated
boolean types and path-count context. Before the final review fixes, nine core
projection tests, seven public tests, sixteen Python cases and three portable
Excel cases pass. The additional boundary tests bring core/Excel counts to ten
and four; their final focused reruns are recorded with the publication checks.

LSM tree/compiled Frozen/RetrainedBump with RQMC retains bitwise legacy equality.
Hybrid/GSR mappings, zero-column shape, early rejection and result-A/failure-B/
result-C independence pass.

The initial complete OFF build passes 2,351 non-benchmark CTest cases, including
809 Python tests and all 34 examples. Combined diagnostics passes 2,364 CTest
cases. Both installed consumers pass in OFF and combined configurations.
The installed analytic check uses its declared 1e-10 numeric contract and adds
strict legacy bitwise parity; production was not changed for floating-point
roundoff. Python 3.9.25 syntax passes for 109 files.

Final review-fix OFF/combined rebuilds pass. OFF focused checks pass 25 cases;
combined checks pass 89 and legacy Excel contracts pass 28 per configuration.
Fully instrumented shared-library ASan/UBSan passes 77 core and 10 public cases
with the CI leak-detection setting disabled. Both installed consumers and the
independent date-capture consumer pass after the fixes. Regenerated-source
integrity, generated boolean compiler probes and the unchanged CCN-eight gate
pass. Documentation integrity passes 90 Markdown files.

The [new-entry cost](../performance/aad-risk-entry-cost.md) records 640 successful
process samples and strict single-worker bitwise parity. The new structured
entry's short-request metadata cost is about four microseconds; existing APIs
do not construct that metadata. All nine final OFF gate binaries are unchanged.
P01 production performance remains inconclusive. Exact corrective `e3720f9f`
passes all 35 Linux/MSVC/wheel checks. This accepts the scalar D04 implementation,
not the subsequent Dupire implementation or the complete development goal.

## Open questions and scope

Payload budget covers values/raw Jacobian only, excluding snapshots, source
storage, workers/tapes and getter copies. Native zero-column requests currently
still execute native risk to preserve semantics. Demand-driven extraction and
workspace/tape budgets belong to F02/P02/P03.

P01 remains inconclusive on the shared WSL2 host. D04 does not close Stage A,
F01/F02, structured operators, second-order risk or the complete implementation
goal. The next F01 boundary is controlled by its frozen-calibration specification,
API decision and critique.
