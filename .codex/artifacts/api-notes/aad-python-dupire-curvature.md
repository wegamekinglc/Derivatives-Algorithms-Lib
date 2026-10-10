# Python Dupire curvature surface

Audience: Python users of the existing automatic Dupire financial risk plan.
The controlling [specification](../specs/aad-python-dupire-curvature.md) preserves
the native finite-step estimator and full raw quote coordinate order.

Expose native passive types, rather than dictionaries or callbacks:

```python
directions = dal.DoubleMatrix_(1, 6, 0.0)
directions[0, 3] = 1.0
bumps = dal.BumpOverAADRequest_(directions=directions, steps=[0.0002])
request = dal.DupireScriptCurvatureRequest_(risk=risk_request, bumps=bumps)
plan = dal.DupireScriptCurvaturePlan_New(
    product, hybrid, calibration, "Z_LOCAL", request,
)
result = dal.DupireScriptCurvatureResult_New(plan)
gamma = result.hessian_products[0, 3]
```

Required constructor fields precede optional byte budgets and are keyword-only.
Factories follow the existing first-order factory argument order. All getters
return numeric/metadata copies; plans/results have no public constructors.
Execution is a copied read-only value with `method`, `quote_gradient_evaluations`,
`paths_per_evaluation` and `numeric_payload_bytes`. Result properties mirror the
C++ names in snake_case, including `base` and `input_axis`. Plan uses `base_plan`.

The generic bump request retains its native name for later rate/MC projections.
Its recording-cap field is accepted as a typed request field but rejected by the
Dupire planner, which cannot apply a caller-thread cap to parallel worker tapes.
Empty directions require an explicit zero-row matrix with the full quote width.

Reject lists/dictionaries as implicit matrices, generators as step sequences and
Python callables as active objectives. Validate native handle types before GIL
release. Existing strict conversion helpers own strings, sizes and request values;
reuse them without broad unrelated helper refactors. The adapter keeps exact
method labels and does not advertise exact Hessians or smooth native mixed mode.

No existing signature or default changes. Excel and other financial families are
deferred to their own narrow PRs. Open questions: none.
