# Monte Carlo Simulation and Evaluation

The public C++ call `ValueByMonteCarlo` prepares a script, builds a model,
generates future paths, evaluates payments, and returns a `PV` with optional
`d_` parameter sensitivities. The core driver is `MCSimulation<T_>` in
`dal-cpp/dal/script/simulation.hpp`. A product containing `EXERCISE` is sent to
the separate [LSM driver](lsm.md); the ordinary driver does no regression.

## A C++ Valuation

The full runnable public-facade pattern is in the
[C++ API guide](../public-api.md#scripted-monte-carlo). At core level, the
[American put example](../../dal-cpp/examples/american_put_mc/american_put_mc.cpp)
uses this call for both a terminal European payment and early exercise:

```cpp
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>

Dal::Script::MonteCarloSettings_ simulation;
simulation.rsg_ = "sobol";
simulation.compiled_ = true;
simulation.lsmcTrainingPaths_ = 4096; // used only when the product has EXERCISE
const auto result = Dal::Script::MCSimulation<double>(
    product, modelData, 32768, Dal::Script::ScriptValuationSettings_(), simulation);
const double pv = result.aggregated_ / 32768.0;
```

Here `product` is `ScriptProductData_` and `modelData` is a BS or Dupire model
handle. `32768` counts pricing paths. For exercise products the separate
training block has 4096 paths; see [path partitioning](lsm.md). The public
`ValueByMonteCarlo` overload takes the analogous settings through its typed
valuation and simulation arguments.

## Preparation and Cashflows

The script preprocessor expands definitions and schedules, parses the AST, and
resolves constant variables. Model-aware preparation partitions expired and
future events, captures the valuation date and fixing snapshot, resolves each
`FIX` to historical or model data, and constructs a future sample timeline.
The model is allocated and initialized against that timeline and the retained
observation definitions. A future `PAYS` adds a cashflow to a receiver; its
contribution is divided by the event's own numeraire. Historical `PAYS` are
expired. The detailed binding and today-fixing rules are in
[script preparation](../methodology/script_engine.md#historical-fixing-preparation).

The factory constructs Black-Scholes, Dupire local-volatility,
[correlated equity Black-Scholes](correlated-bs.md), and
[hybrid](hybrid-model.md) models from model data. Script preparation and public
valuation currently accept only the first two; multi-asset script binding is
planned for the next stage. The model interface exposes whether its numeraire
is deterministic. The future observation plan keeps exact event-to-sample
mapping rather than assuming one model sample per script event. A model path
with a non-finite or non-positive numeraire is rejected as `InvalidModelPath`.

## Random Draws and Paths

`MonteCarloSettings_` defaults to `rsg_ = "sobol"`, no Brownian bridge, no
AAD, smoothing width `0.01`, and tree evaluation. The ordinary driver also
accepts `mrg32` and `irn`. `CreateRNG` sizes the generator to the model's
simulation dimension; a zero-dimensional model validates the method name but
does not construct a generator or bridge. `useBb_` wraps draws in a Brownian
bridge, changing the order in which normal variates drive time increments.
Multi-factor models that declare bridge support bridge each factor's time
series separately; see the [hybrid model](hybrid-model.md#brownian-bridge-and-risks).
For direction numbers, normal transforms, path seeking, and the pseudo-random
generators, see [sampling](sampling.md).

## Batches, Compiled Scripts, and AAD

Ordinary paths use batches of size `min(8192, ceil(nPaths / nThreads))`.
Worker-owned RNG, Gaussian vector, scenario, and evaluator are reused across
batches in one valuation. Value-only `double` evaluation can walk the AST or
execute a compiled per-event opcode stream. `compiled_` defaults to tree mode;
compiled mode is an opt-in execution choice and must preserve the same
cashflow and error semantics. Preparation compiles once before dispatch.
The detailed opcode and observation contracts are in
[script evaluation](../methodology/script_engine.md#tree-walk-and-compiled-evaluation).

With `enableAad_ = true`, `MCSimulation<AAD::Number_>` creates an active model
and tape on each worker. Model parameters and script constants are registered
once per batch; each path rewinds to a tape mark, evaluates fuzzy future
conditions, and back-propagates the path payoff. Batch adjoints are reduced
into the risk vector. The [AAD guide](../methodology/aad.md#pathwise-adjoints-in-monte-carlo)
explains tape ownership and smoothing. For `EXERCISE`, the AAD meaning depends
on the [frozen or retrained policy mode](lsm.md#early-exercise-aad).

## Interpreting a Result

An ordinary Monte Carlo PV is the average over pricing paths. Core
`MCSimulation` returns the sum in `aggregated_`, so divide it by `nPaths`;
the public `ValueByMonteCarlo` facade performs that division and returns the
`PV` key. AAD risks are
pathwise derivatives of the smoothed estimator where future script conditions
are fuzzy. Tree and compiled paths share the same model and preparation
contract. For exercise products, regression samples are excluded from pricing;
for ordinary products there is no training block. `ExplainScriptValuation`
describes preparation without simulating paths, while
`ExplainScriptSimulation` runs the double LSM valuation to report exercise
statistics. See [diagnostics](../methodology/script_engine.md#product-archive-and-diagnostics).
