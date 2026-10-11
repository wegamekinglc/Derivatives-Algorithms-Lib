# Segmented Monte Carlo worksheet API

Active decision under the [specification](../specs/aad-excel-segmented-mc.md).

| Type                                  | Factory arguments                                         | Passive projections                                                                                       |
|---------------------------------------|-----------------------------------------------------------|-----------------------------------------------------------------------------------------------------------|
| SegmentedMonteCarloSettings           | name, optional key/value table                            | Configuration with all eight settings                                                                     |
| BlackScholesMonteCarloPlan            | name, product, optional valuation, optional smoothing=.01 | Product, valuation, smoothing, parameter labels, script constants, observations                           |
| BlackScholesMonteCarloResult          | name, plan, point, paths, optional settings               | Value, gradient, labels, execution, plan, point, settings                                                 |
| BlackScholesMonteCarloCurvatureResult | name, plan, point, paths, bumps, optional settings        | Point, directions, steps, Hessian products, logical shape, execution, plan, settings, completed base mean |

Factories end in `_New`; getters use `_Get_<Field>`. Required arguments precede
optional ones; arity is at most six. No existing worksheet entry changes.
Full point input is `cell[][]`, not the generic blank-truncating vector reader.
The raw wrapper validates numerical kinds before normalizing integer cells.

The mean handle holds a small Excel value comprising the public sealed plan,
point, requested settings and completed native mean. Both public mean evaluation
and a stored curvature base can construct it. This avoids adding a public native
constructor, duplicated valuation or a variant-specific getter surface.

Configuration keys follow the Python/native names: `rsg`, `use_bb`, `first_path`,
`scramble_key`, `normal_precision`, `segment_steps`,
`checkpoint_capacity_budget_bytes`, `recording_capacity_budget_bytes`.
Scramble text is unsigned canonical decimal, without signs, leading zeros,
spaces or exponent notation. Numeric input is limited to exact `2^53-1`;
text reaches UINT64_MAX. Always spill a present scramble key as text.

Observations have index, fixing time, historical flag and optional captured
value. Existing `Product_Describe` exposes the copied contract's dates/events/
settings. Empty numeric outputs use one empty cell; shape remains authoritative.
Errors identify the function, field and physical table location where available.
Default plan smoothing is the native .01; default segmented settings remain native.

A typical worksheet flow creates Product and valuation handles, then
`BLACKSCHOLESMONTECARLOPLAN.NEW`, the common `BUMPOVERAADREQUEST.NEW`, and
`BLACKSCHOLESMONTECARLOCURVATURERESULT.NEW`. Its `GET.BASE` handle supports the
same value/gradient/metadata getters as a first-order mean result.
