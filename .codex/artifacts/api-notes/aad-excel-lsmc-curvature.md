# LSMC policy curvature worksheet API

Active under the [specification](../specs/aad-excel-lsmc-curvature.md).
Use 29 functions with required arguments preceding optional defaults.

- MonteCarloSettings_Get_Configuration: all fourteen original configuration
  fields, with unset optional values blank and booleans preserved.
- BlackScholesLsmcPlan_New(name, product, optional valuation, optional simulation).
  Getters: Product, Valuation, Simulation, ParameterLabels, ScriptConstants,
  Observations, EventDates, TimeLine. Omitted simulation selects risk defaults.
- BlackScholesLsmcCurvatureResult_New(name, plan, point, paths, bumps).
  Getters: Value, Gradient, ParameterLabels, Point, Directions, Steps,
  HessianProducts, Shape, Execution, Plan, Simulation, BasePolicyCount,
  BasePolicy(result, index). The last getter returns a copied immutable
  LsmcExercisePolicy handle using a strict zero-based numeric policy ordinal.
- LsmcExercisePolicy getters: Coefficients, NormalizationMeans,
  NormalizationSigmas, BasisPowers, Configuration. There is no public factory.
  Configuration contains basis_degree, degenerate, degenerate_reason, mean,
  sigma, condition_path_count, effective_rank, solver, fallback_reason,
  validation_mse and lengths for coefficients, means, sigmas and power rows,
  plus the fixed three power columns. Optional MSE is blank when absent.

The worksheet flow creates Product, explicit valuation and an optional existing
MonteCarloSettings handle with enable_aad=true, then BLACKSCHOLESLSMCPLAN.NEW,
BUMPOVERAADREQUEST.NEW and the curvature result. Document the
actual generated names in the current-state Excel guide after generation.

Point uses a full cell matrix instead of a blank-truncating vector converter.
Raw numeric-kind guards and integer normalization precede required conversions.
BasisPowers uses three columns, including unused zero powers. Empty vectors and
matrices spill one empty text cell; shape/configuration supplies logical sizes.
Observation columns remain index, fixing_time, historical and optional value.

Exercise-policy ordinals preserve native exercise-day order. The current public
result has no individual policy event ID; expose the full event-date/time-line
plan projections separately without implying ordinal-to-event equivalence.

Frozen retains the baseline policy for all outer gradients. RetrainedBump
differences the declared native partial-plus-inner-policy-secant estimator.
Neither name claims an unbiased Gamma or general higher-order reverse AD.
