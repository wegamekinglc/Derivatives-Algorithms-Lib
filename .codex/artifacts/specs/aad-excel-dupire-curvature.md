# Excel Dupire quote curvature

Status: active implementation after accepted Excel PDE PR #535.
Source: the user's native-only AAD roadmap, remaining Excel parity and the
requirement to merge each delivery before starting the next implementation.
Baseline is merge `c1068d991c1547c6b7d35bf490b49a61de574003`.

## Problem and scope

The owning C++ and Python Dupire curvature chain already recalibrates every
base/plus/minus quote point and computes common-path quote gradients. Worksheet
users cannot construct its direction request or access its owning results.
Expose that accepted financial chain through typed Excel handles. Add the common
bump request once for later rate, MC and LSMC consumers. Keep native algorithms,
public C++/Python APIs, existing risk templates and legacy functions unchanged.

This is a finite-step estimate of quote Gamma, cross-Gamma and Hessian products.
It does not enable general higher-order AD, generic worksheet callbacks, archive
serialization, EXERCISE policies or worker tape limits for Dupire curvature.

## Requirements

R01. The common request copies finite direction cells and finite positive
steps, with exactly one step per direction row. Steps accept a row or column
vector. Reject booleans, text, dates, errors, non-finite numbers and mixed blank
numeric ranges. Names and settings keys reject embedded NUL. Settings use
existing case-insensitive two-column parsing, duplicate and unknown-key checks.
Raw Windows guards run before generated coercion; actual integer cells normalize
to numbers without admitting bool or text. Errors name the function, field and
one-based worksheet row/column where applicable.

R02. Blank directions and steps mean zero direction rows. An optional
`input_count` setting supplies the logical column count for this case; absent
means zero. A typed zero-row matrix retains its existing column count. With a
nonempty direction matrix, an explicit count must match the actual columns.
The shape getter always returns `(direction_count, input_count)`, even when an
empty numeric spill is represented by one blank cell. Do not invent a zero
direction row or lose the native zero-by-N shape.

R03. Common settings also accept optional `numeric_payload_budget_bytes` and
`recording_capacity_budget_bytes`. Blank is unset; zero is an actual cap.
Reuse the exactly representable nonnegative size_t admission at most 2^53-1.
The input count is an integer in [0, 1048575]. Nonempty direction matrices fit
worksheet row/column bounds. Numeric/step shape validation is passive; native
point-specific representability and nonzero-direction checks remain in planning.

R04. The Dupire request copies existing first-order risk and bump handles.
Its getters return detached immutable handles. No request constructor runs
simulation, reads global fixings, changes caller tape/mode/seeds or retains active
numbers. Null handles reject contextually and failed assignments preserve prior
output handles.

R05. The plan delegates to `PlanDupireScriptCurvature`, preserving calibration,
direct scalar bindings, fixed grids/carry, selected first-order projection,
frozen date/history and common-path settings. It rejects mismatched quote count,
invalid base/perturbed calibration points, external direct seeds, EXERCISE and
any provided bump recording-cap request through the native contract. Check the
full quote-axis worksheet row bound before costly native planning. The plan
owns its data and exposes detached base plan, point, directions, steps, shape
and admitted payload. Getters do not repeat planning or submit workers.

R06. Execution delegates to `ValueByMonteCarloWithDupireCurvature`. Expose
detached base result, quote plan, full raw point/gradient, directions/steps,
Hessian products, logical shape and execution counters. Quote metadata uses the
existing complete calibration-plan input-axis getter through the returned quote
plan; do not duplicate its twelve-column formatting implementation. Coordinate
order is full strike-major raw decimal-vol quote order. First-order selection
and report factors never scale or shrink the raw gradient/products.

R07. M directions perform exactly 1+2M quote-gradient evaluations; M=0 performs
one base evaluation. Preserve active outer-recording rejection, caller mode and
seed recovery, success/failure/success behavior and repeated deterministic
evaluation. Getters remain usable on an active caller recording without
changing its graph. Serialization fails explicitly for every new handle type.

R08. Preserve the native combined budget
`8 * (2 + S + B + 5Q + 2MQ + M)`, where S denotes mandatory surface derivatives,
B bound constants and Q quotes. The first-order quote budget remains separate.
Both budgets exclude worksheet cells/labels/handles, detached copies, snapshots,
temporary plans, workers and allocator overhead. Do not advertise total-process
memory enforcement or an XLL/Excel-host performance measurement.

R09. Generate and commit every Windows wrapper/help pair from Machinist markup.
Every function has stable typed arguments, nonvolatile registration and help
within the repository's length contract. Never reuse an input argument name for
an output local. Public documentation describes only implemented behavior.

## Acceptance

- Establish missing-surface RED before production code. Increment through
  common request ownership, strict admission, financial execution and queries.
- Independently check the discounted bound-quote quadratic: value `1e-6*exp(-.05)`,
  quote-3 gradient `.002*exp(-.05)`, and signed products `2*exp(-.05)` and
  `-4*exp(-.05)`. Check every other raw coordinate and evaluation counter.
- Check one mixed surface/direct direction against independently recalibrated
  first-order gradients. Cover tree/compiled execution with one worker and
  small fixed paths; reuse accepted native numerical/configuration evidence.
- Exercise zero directions, selected/reported bases with full raw products,
  detached mutation, null/wrong types, NUL, strict numeric kinds, budgets,
  unsupported worker caps, archives and caller graph/seed recovery.
- Run only affected portable/strict checks locally. Real Windows CI must call
  all new exports and inspect registration contracts. Inspect actual completed
  runtime logs and installed usage; do not infer execution from build success.
- Select two affected owning financial requests, at small and normal path
  counts. Preserve calibrated two-round ten-pair sampling and raw observations.
  Native versus Excel ownership/spills are unequal contracts and informational.
  Do not repeat accepted unrelated native/Python/Excel timing rows.
- Before merge, inspect all exact-head checks, Codacy, complete review bodies
  and threads, run two final audits, merge with the accepted SHA and verify the
  merged tree. Start the rate increment only after this PR merges.

## Open questions

None. The API note fixes worksheet empty-shape and metadata access decisions.
