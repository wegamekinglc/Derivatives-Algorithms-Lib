# Scoped passive constants for GSR Monte Carlo

Core C++ ordinary Monte Carlo can share passive constant tape leaves for a
prepared, compiled, scalar AAD request using the native three-factor GSR model.
Set `MonteCarloSettings_::usePassiveConstants_` before preparation to enable it.
The default is `false`; default construction, input adjoints and existing Python
and Excel settings retain their current behavior. The new opt-in is exposed by
the core C++ settings only.

For a three-factor GSR model handle `data` and script data `product`:

```cpp
Dal::Script::MonteCarloSettings_ execution;
execution.enableAad_ = true;
execution.compiled_ = true;
execution.usePassiveConstants_ = true;
auto model = Dal::CreateModel<double>(data);
const auto prepared = Dal::Script::PrepareScript(product, model.get(), {}, execution);
const auto result = Dal::Script::MCSimulation<Dal::AAD::Number_>(
    prepared, data, 8192, "sobol", false, true);
```

Non-GSR models, other factor counts, tree evaluation, passive valuation and
EXERCISE/LSMC reject this setting with `UnsupportedPassiveConstants`. Default
Sobol precision without a Brownian bridge is required. The
ordinary scalar simulation objective is required; weighted/custom objectives
and block Jacobian replay are not admitted by this MC entry point. Calling
or worker tapes in vector mode reject. The setting does not implicitly fall
back to another estimator, model or execution mode.

## Recording and compatibility

Each admitted worker enables sharing within its owned `RecordingScope_`, before
creating the active model. Model parameters, script constants used as inputs
and the payoff zero still get distinct explicit independent registrations.
Implicit double construction and double assignment then share one lazily
allocated zero-argument node. Expression results retain their own nodes and
all operand edges and unused sink writes remain. `Number_` retains its 16-byte
layout when lifetime diagnostics are disabled.

`Adjoint` and native seed/read functions reject passive constants. Explicit
`RegisterIndependent`, `RecordingScope_::RegisterInput` and `PutOnTape` create
independent nodes with the usual adjoints. A passive scalar payoff gets an
active root before reverse. These semantics apply only to the enabled recording;
outside it `Number_(double)` continues to allocate an independent leaf.

The low-level `RecordingScope_::EnablePassiveConstants()` must run during input
registration and before any node is recorded. It supports native scalar/vector
expression recording and weighted/block roots, but rejects structured reverse
operators before their callbacks or event allocation. It is not a general
implicit-registration migration for calibration, solvers or public bindings.

Full resets drop the cached sink; checkpoint restore retains a prefix sink or
discards a suffix sink and clears its unused scalar/vector seeds. Optional
lifetime diagnostics keep the existing owner, epoch, slot-generation and mode
validation. Explicit close, exception unwind and failed close restore the
default constructor policy. Nested independent recordings remain unsupported.

## Memory and use limits

Enabling sharing reclaims retained excess tape blocks before input registration,
so a previous larger recording cannot hide the capacity reduction. Reclamation
uses normal tape reset and capacity-budget admission; it does not bypass
budgets. A failed reset poisons the recording through the existing recovery
path. Already minimal tapes retain their allocation.

Memory savings depend on graph size and block boundaries. Other event schedules
can save occupied payload without releasing a block. Do not multiply a
per-worker tape saving by path count or equate tape capacity with process RSS.

The production profile accepts `--passive-constants` after its positional
arguments. Its `cold` mode retains numeric admission, alternate-engine and
common-path derivative oracles. Its GSR-only `memory` mode runs ordinary
valuation and a matching reference without the alternate-engine/oracle work;
it reports `oracle_paths: 0`. Use that mode only after separate successful
numeric admission when measuring process RSS, so unrelated oracle allocations
do not set the process high-water mark.

```bash
script_mc_perf --production-profile gsr-swap-3f 8192 aad compiled cold 1 0 0 1 --passive-constants
script_mc_perf --production-profile gsr-swap-3f 8192 aad compiled memory 1 0 0 1 --passive-constants
```
