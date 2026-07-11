# DAL Architecture

DAL is a C++17 quantitative-finance workspace with a core numerical engine,
developer-facing adapters, and Python, Excel, and web delivery surfaces.

## Components and Dependency Direction

```text
dal-cpp (DAL::cpp)
  └─ dal-public (DAL::public)
       ├─ dal-python (_dal / dal)
       │    └─ dal-web backend
       │         └─ HTTP API ← React/Vite frontend
       └─ dal-excel (.xll, Windows)
```

The native build dependency direction is:

```text
dal-cpp <- dal-public <- {dal-python, dal-excel}
```

| Component | Responsibility |
|-----------|----------------|
| `dal-cpp/` | Math, AAD, curves, models, scripting, Monte Carlo, PDEs, random generation, storage, and concurrency |
| `dal-public/` | Convenience builders and valuation/calibration entry points over core DAL types |
| `dal-python/` | pybind11 module plus small Python convenience wrappers |
| `dal-excel/` | Excel conversion, object repository, and Machinist-generated worksheet registration |
| `dal-web/backend/` | FastAPI persistence and orchestration through one native DAL gateway |
| `dal-web/frontend/` | React/Vite portfolio, trade, model, product, and valuation UI |

`DAL::cpp` and `DAL::public` are exported as installable CMake packages.
`DAL::public` is not an ABI-isolated layer: its headers expose core handles and
concrete value types, so consumers remain coupled to compatible core headers and
libraries.

## Core Organization

The main core namespaces live under `dal-cpp/dal/`:

| Area | Contents |
|------|----------|
| `math/` | Vectors, matrices, interpolation, root finding, integration, optimization, random generation, PDE primitives |
| `math/aad/` | Native and adapter-backed reverse-mode AAD facade |
| `curve/` | Discount/forward curves, calibration instruments, single- and multi-curve solvers |
| `script/` | Product preprocessing, parsing, domain analysis, tree-walk/compiled evaluation, Monte Carlo |
| `model/` | Black-Scholes, Dupire, implied- and local-volatility machinery |
| `storage/` | Storables, archives, process-wide dates/fixings, and repository integration |
| `concurrency/` | Lazy process-wide worker pool and synchronized task queue |

Methodology belongs in `docs/methodology/`; local source comments document the
invariants needed to maintain a symbol safely.

## Runtime Ownership

DAL combines process-wide coordination state with thread-local numerical state.
Callers embedding the library need to preserve that distinction.

| State | Ownership and contract |
|-------|------------------------|
| Evaluation/accounting dates | Process-wide stores protected by the global-store mutex. Public `SetEvaluationDate` / `EvaluationDate_Set` mutates shared state. |
| Fixings | Process-wide store protected by the same global-store mutex. |
| Calendar/currency/index registration | Initialized once per process by `InitGlobalData` / Python module initialization. |
| Thread pool | Process-wide singleton, inactive until explicit start or first task. Lifecycle and queue transitions are synchronized. |
| AAD tape | One tape per executing thread (`thread_local`). Calibration guards and simulation batches rewind their thread's tape before reuse. |
| Script compiled stacks | Thread-local evaluation stacks; they are not shared between worker threads. |
| Excel object repository | Host/environment-owned repository of storable handles used between worksheet calls. |

### Thread pool policy

Loading DAL does not create worker threads. The pool records a logical thread
count and starts lazily when work is submitted; a capacity of $N$ creates
$N-1$ background workers because the calling thread can participate through
`ActiveWait`.

The default capacity is the detected hardware concurrency. Set `DAL_NUM_THREADS`
to a positive integer before loading DAL to request a smaller limit; the value is
capped at hardware concurrency. Invalid, empty, or zero values fall back to the
hardware default. Native callers may also pass a positive count on the first
`InitGlobalData` call or explicitly restart `ThreadPool_`.

### AAD tape policy

An AAD number belongs to the tape and recording frame in which it was created.
Do not transfer live AAD expressions between threads. Monte Carlo owns path,
evaluator, random-generator, and tape activity per worker. Curve calibration uses
`TapeGuard_` to rewind the current thread's tape on entry and exit, including
exception unwind.

### Web serialization policy

The Python binding releases the GIL only around the pure native Monte Carlo call.
The web service offloads that blocking call with `asyncio.to_thread`, so the event
loop and unrelated Python work remain responsive. `DalGateway` nevertheless holds
a process-wide lock across evaluation-date mutation, product/model construction,
and valuation. Web pricing is therefore serialized within one backend process.
Use multiple isolated processes when independent concurrent valuations are needed;
each process owns its own DAL globals and thread pool.

## Scripted Monte Carlo Valuation Flow

```text
C++ / Python / Excel / web caller
  -> product and model builders
  -> ValueByMonteCarlo / MonteCarlo_Value / MONTECARLO.VALUE
  -> script preprocessing and parsing
  -> model factory
  -> MCSimulation<double> or MCSimulation<AAD::Number_>
  -> path batches on the DAL thread pool
  -> tree-walk or compiled evaluator
  -> PV and optional AAD risks
```

`numPath` must be positive at the public valuation boundary. The script
tree-walk evaluator is the default; the compiled evaluator is an opt-in execution
mode with the same payoff/risk contract. AAD simulations use a separate recording
for each worker's path batch.

## Curve Calibration Flow

```text
instrument/convention builders
  -> CurveCalibrationSpec_ or public builder
  -> validation and knot construction
  -> residual map: model rate - market rate
  -> Underdetermined::Find or Underdetermined::Approximate
       -> eligible AAD analytic Jacobian
       -> finite-difference fallback
  -> calibrated curve and diagnostics
```

Exact calibration uses the weighted least-change solver and can expose the
forward Jacobian plus effective inverse Jacobian. Approximate calibration trades
fit against distance from the reference parameters. Staged multi-curve calibration
solves stages in order; joint calibration stacks declarations into one residual
system. See [yield-curve construction](methodology/yield_curve.md) and
[yield-curve Jacobian](methodology/yield_curve_jacobian.md).

## Generated Code

Machinist consumes interface/configuration markup from
`dal-cpp/config/dal.ifc` and `dal-cpp/config/dal.mgl`. The `dal_generate` target
updates both generated trees:

```text
dal-cpp/dal/auto/   core enums, storables, and readers/writers
dal-excel/auto/     worksheet registration and conversion glue
```

Regenerate both trees whenever markup changes:

```bash
cmake --build build/core-dev --target dal_generate
cmake --build build/core-dev --target dal_check_generated
```

`dal_check_generated` regenerates and fails when tracked output differs. Generated
files travel with the markup change; they are not hand-edited.

## Public Delivery Surfaces

- C++ consumers use installed `DAL::cpp` and/or `DAL::public` targets.
- Python imports `dal`; the `_dal` module initializes DAL globals and exposes
  opaque handles plus Python-friendly builders.
- Excel uses generated worksheet functions and stores object handles in its
  repository between calls.
- The web backend is native-only and imports `dal` in
  `backend/app/services/dal_gateway.py`; no other backend module imports DAL.

See the [public API guide](public-api.md) for supported entry points and the
[installation guide](installation.md) for build and package consumption.
