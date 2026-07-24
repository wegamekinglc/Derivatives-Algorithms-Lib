# Script Engine

This note describes the script-engine pipeline that turns a human-readable events
table into an evaluable expression tree, and the visitor passes that transform it
before simulation or valuation. The implementation lives in `dal-cpp/dal/script/`.

## Architecture

The script engine is split into two independent halves connected by a well-defined
interface:

1. **Preprocessor** (`dal-cpp/dal/script/preprocessor.hpp`) — resolves the
   "definition" half of an events table: constant variables, textual macros, and
   schedules. Produces dated event descriptions. Does not build an AST and knows
   nothing about nodes.

2. **Parser** — consumes the preprocessor's output and builds the AST (expression
   tree) of `Node_` objects. Operates independently of the preprocessor, so the two
   halves can be developed and unit-tested in isolation.

After parsing, a sequence of **visitor passes** (domain analysis, constant-condition
folding, evaluation) walks the AST to prepare and execute it.

## Parser and AST

`Parser_` (`dal-cpp/dal/script/parser.cpp`) consumes a token stream produced by
the lexer (`dal-cpp/dal/script/lexer.cpp`) and builds an expression tree of
`Node_` objects (`dal-cpp/dal/script/node.hpp`). Every node holds a vector of
child expressions in `arguments_`, so arithmetic, conditions, assignments, and
control flow all share one polymorphic hierarchy.

### Lexer

`Tokenize` (`dal-cpp/dal/script/lexer.hpp`) is the single tokenization primitive
in the engine. Both halves of the pipeline consume it: the preprocessor
(`Preprocessor_`, the definition front-end) tokenizes directive values, and the
parser (`Parser_`, the payoff back-end) tokenizes statement text. Housing it in
its own translation unit — rather than inside either consumer — keeps the two
halves decoupled and avoids a circular ownership relationship between the
front-end that resolves schedules and the back-end that builds the AST.

### Precedence Levels

The parser implements expressions as a cascade of precedence levels, each
delegating to the next tighter level:

| Level | Operator class          | Produces                                                                                                           |
|-------|-------------------------|--------------------------------------------------------------------------------------------------------------------|
| L1    | `+`, `-` (binary)       | `NodeAdd_`, `NodeSub_`                                                                                             |
| L2    | `*`, `/`                | `NodeMulti_`, `NodeDiv_`                                                                                           |
| L3    | `^` (right-assoc)       | `NodePow_`                                                                                                         |
| L4    | unary `+`, `-`          | `NodeUPlus_`, `NodeUMinus_`                                                                                        |
| Atom  | literal, variable, func | `NodeConst_`, `NodeVar_`/`NodeConstVar_`, `NodeSpot_`, `NodeLog_`, `NodeExp_`, `NodeSqrt_`, `NodeMin_`, `NodeMax_` |

Parenthesised sub-expressions re-enter at the top level through a shared
`ParseParentheses` helper. Conditions form a parallel cascade — `OR` (loosest)
binds over `AND`, which binds over comparison elements — producing `NodeOr_`,
`NodeAnd_`, and the comparison/equality nodes.

### Reserved Keywords and Variables

A fixed reserved-word set (`IF`, `THEN`, `ELSE`, `END`, `PAYS`, `AND`, `OR`,
`SPOT`, `MAX`, `MIN`, `LOG`, `SQRT`, `EXP`, `DCF`) cannot be used as variable
names. Any other alphabetic token becomes either a `NodeVar_` (looked up in the
preprocessor's constant-variable map and promoted to `NodeConstVar_` if it
resolves there). Statements are either assignments (`=`, `NodeAssign_`), pays
clauses (`PAYS`, `NodePays_`), or `IF/THEN/ELSE/END` blocks (`NodeIf_`, with
`firstElse_` indexing the else-branch within `arguments_`).

### Comparators and Smoothing Hints

`ParseCondElem` lowers every comparison to a subtraction wrapped in the
appropriate condition node (`NodeEqual_`, `NodeSup_`, `NodeSupEqual_`), with
`!=`, `<`, `<=` rewritten in terms of `=`, `>`, `>=`. An optional `;eps` or
`:eps` suffix on a comparison sets the node's `eps_` field, which the fuzzy
evaluator consumes as the smoothing width for that condition (see
[Fuzzy Evaluator](#fuzzy-evaluator)).

### Boolean Operator Semantics

`AND` and `OR` are **eager**: both operands are always evaluated, in all
evaluators - the exact tree-walk (`Evaluator_`), the fuzzy tree-walk
(`FuzzyEvaluator_`, whose probability combinators $a \cdot b$ and
$a + b - a \cdot b$ are inherently two-sided), and the compiled stream (which
emits both operand sub-streams before the combinator opcode). Scripts must not
rely on short-circuit evaluation. Conditions in this grammar are pure (no
side effects); eager evaluation may produce IEEE NaN or infinities in a
discarded operand, but each script comparison still resolves to a boolean that
`AND`/`OR` combines normally. The eager contract exists so that all three
evaluators share one semantics.

### Day-Count Functions

`DCF(basis, start, end)` is folded to a literal at parse time: the parser
extracts the basis code and the two date strings, constructs a `DayBasis_`, and
emits a `NodeConst_` carrying the computed year fraction. This means a
`DCF(...)` call cannot contain a nested expression — its arguments must be
literal tokens.

## Events and Schedules

A script product is a sequence of dated events. `ScriptProduct_`
(`dal-cpp/dal/script/event.hpp`) splits the preprocessed events into **past**
events (dates before the evaluation date, evaluated once with the known fixings)
and **future** events (dates on or after the evaluation date, evaluated per
simulated path). Both halves share the same AST representation
(`Event_ = Vector_<Statement_>`).

### From Events to a Timeline

`PreProcess` builds the simulation timeline from the future event dates: each
event date is converted to a year fraction from the evaluation date and paired
with an `AAD::SampleDef_` that requests the numeraire, a forward maturity at the
event time, and a discount factor at the event time. The model consumes this
`defLine_` to allocate the per-event scenario structure that evaluators read
when they walk the AST.

### Schedule Expansion

Schedule expansion itself lives in the preprocessor
([Preprocessing Pipeline](#preprocessing-pipeline)): the `ExpandSchedulePlaceholders`
virtual replaces `PeriodBegin` / `PeriodEnd` placeholders for each period of a
recurring schedule, producing one dated event description per period. The parser
and event layer see only the expanded, dated descriptions — they know nothing
about schedules.

### Variable Indexing and the Payoff Slot

After parsing, `IndexVariables` runs a `VarIndexer_` pass to assign every named
variable a stable integer slot in the evaluator's variable vector. The product
also records the slot of the variable named in its `payoff_` field
(`payoffIdx_`, defaulting to the last variable); simulation harvests that slot
as the path value.

## Preprocessing Pipeline

The `Preprocessor_` class in `dal-cpp/dal/script/preprocessor.hpp` resolves a raw
`(Cell_, String_)` events table into `PreprocessedEvents_`: a map of constant
variables (`String_ -> double`) and dated event descriptions (`Date_ -> String_`).

The class is built for extension: `Process` orchestrates a fixed pipeline while
every meaningful decision delegates to a protected virtual method, so a derived
preprocessor can recognise new directive kinds or new placeholders without
re-implementing the orchestration.

| Virtual method                    | Role                                                             |
|-----------------------------------|------------------------------------------------------------------|
| `IsSchedule(desc)`                | True when a non-date directive description denotes a schedule    |
| `IsConstVariable(value)`          | True when a non-date directive value defines a constant variable |
| `ExpandMacros(stmt, macros)`      | Replace every registered macro name in a statement with its body |
| `ExpandSchedulePlaceholders(...)` | Replace `PeriodBegin` / `PeriodEnd` placeholders for one period  |

The default implementation recognises simple textual macros and period-based
schedule expansion; a derived preprocessor can override any of these to support
domain-specific directive syntax without touching the orchestration logic.

## Domain Processor

`DomainProcessor_` in `dal-cpp/dal/script/visitor/domainproc.hpp` determines the
domains (value ranges) of all script variables and expressions. Its purposes are:

1. Set the `alwaysTrue_` / `alwaysFalse_` flags on condition nodes (`NodeEqual_`,
   `NodeSup_`, `NodeSupEqual_`, `NodeNot_`, `NodeAnd_`, `NodeOr_`) and `NodeIf_`
   by evaluating the condition's domain.
2. When fuzzy processing is enabled, additionally set the `isDiscrete_` flag and
   left/right interpolation bounds on equality and comparison nodes for the fuzzy
   evaluator.

**Domain stack.** The processor maintains a stack of `Domain_` objects that
represent the runtime value range of each sub-expression. All variable domains
start as the singleton $\{0\}$. As the walker descends through arithmetic
operations, domains are combined (pushed/popped) on the stack. A boolean condition
stack (`DomainCondProp_` values: `AlwaysTrue`, `AlwaysFalse`, `TrueOrFalse`)
tracks the outcome of evaluated conditions.

**Condition detection.** For `NodeEqual_` (expr $= 0$), if the domain of the
sub-expression `expr` cannot be zero, the condition is `AlwaysFalse`; if its
only possible value is $0$, it is `AlwaysTrue`. For `NodeSup_` (expr $> 0$),
the same reasoning applies with the sign of the domain.

**If-node analysis.** `NodeIf_` evaluation is the most important pass. The
processor evaluates the condition, then:

- **Always true:** sets `alwaysTrue_ = true` and visits only the if-true statements.
- **Always false:** sets `alwaysFalse_ = true` and visits only the else statements.
- **Otherwise (domain cannot decide):** records variable domains before and after
  both the if-true and else branches, then merges them via `Domain_::AddDomain`.
  This merged domain is the post-if variable range and feeds downstream analysis.

This analysis makes the constant-condition processor's job possible: once
`alwaysTrue_`/`alwaysFalse_` flags are set, the next pass can collapse dead
branches.

**Fuzzy interpolation bounds.** When fuzzy mode is active, the processor also
computes the `lb_` (left bound) and `rb_` (right bound) fields on equal and
comparison nodes. For a discrete sub-expression whose domain has no sign-change,
these capture the nearest subdomain boundaries on either side of zero; for a
continuous expression with a zero-crossing, the bounds come from the `eps_`
smoothing parameter.

## Constant Condition Processor

`ConstCondProcessor_` in `dal-cpp/dal/script/visitor/constcondprocessor.hpp`
mutates the AST by collapsing always-true and always-false condition and if
nodes into their concrete outcomes:

- An `alwaysTrue_` boolean condition node is replaced by a `NodeTrue_` leaf.
- An `alwaysFalse_` boolean condition node is replaced by a `NodeFalse_` leaf.
- An `alwaysTrue_` if-node is replaced by a `NodeCollect_` containing only its
  if-true statements.
- An `alwaysFalse_` if-node is replaced by a `NodeCollect_` containing only its
  else statements.

`DomainProcessor_` must run first so the `alwaysTrue_`/`alwaysFalse_` flags are
properly set. Because this visitor *mutates* the tree structure (replacing nodes
in place), it must be invoked from the root via `ProcessFromTop(std::unique_ptr<Node_>& top)`,
which passes a reference to the owning `unique_ptr` so the replacement is safe.

## Fuzzy Evaluator

`FuzzyEvaluator_<T>` in `dal-cpp/dal/script/visitor/fuzzy.hpp` is a templated
AST evaluator that smooths discontinuous payoff functions for pathwise AAD (see
[Automatic Adjoint Differentiation](aad.md)). Indicator functions
$\mathbb{1}_{S>K}$ are replaced by smooth approximations over a small spread,
trading a small controlled bias for a finite, low-variance derivative.

### Smoothing Functions

Two primitive smooth transitions are used; both kernels live in
`dal-cpp/dal/script/visitor/smoothing.hpp`, shared by the fuzzy tree-walk and
the compiled fuzzy opcodes. For a comparison `expr > 0` (or `>=`),
the **continuous spread** function transitions from $0$ to $1$ across a band of
width $\varepsilon$:

$$
\mathrm{CSpr}(x;\; \varepsilon) =
\begin{cases}
0       & x < -\varepsilon/2,\\
1       & x > +\varepsilon/2,\\
(x + \varepsilon/2)/\varepsilon & \text{otherwise}.
\end{cases}
$$

For an equality `expr = 0`, the **butterfly** function is a triangular pulse
centred at $0$:

$$
\mathrm{BFly}(x;\; \varepsilon) =
\begin{cases}
0 & |x| \ge \varepsilon/2,\\
(\varepsilon/2 - |x|)/(\varepsilon/2) & \text{otherwise}.
\end{cases}
$$

When the condition node carries explicit domain-derived left and right bounds
(`lb_`, `rb_`) — set by `DomainProcessor_` for discrete sub-expressions — the
two-argument overloads use the subdomain endpoints instead of a symmetric
$\pm\varepsilon/2$ band:

$$
\mathrm{CSpr}(x;\; lb, rb) =
\begin{cases}
0          & x < lb,\\
1          & x > rb,\\
(x - lb)/(rb - lb) & \text{otherwise},
\end{cases}
\qquad
\mathrm{BFly}(x;\; lb, rb) =
\begin{cases}
0            & x < lb \text{ or } x > rb,\\
1 - x/lb & lb \le x < 0,\\
1 - x/rb & 0 \le x \le rb.
\end{cases}
$$

### Nested If Evaluation

The fuzzy evaluator handles conditionally-assigned variables via a **split
evaluation** with probability-style fuzzy logic. When an if-condition has a
fractional degree of truth $\delta$ (neither absolutely true nor absolutely
false), the evaluator:

1. Records the current values of all affected variables (`varStore0_`).
2. Evaluates the if-true branch with the current variable state and records the
   post-true values (`varStore1_`).
3. Resets the variables to their pre-if state.
4. Evaluates the else branch (if present).
5. Blends the two outcomes: `variables_[idx] = δ * varStore1_[idx] + (1 - δ) * variables_[idx]`.

The nested-if level counter (`nestedIfLvl_`) tracks nesting depth, and variables
are stored in two-level arrays preallocated for performance (`varStore0_[level][var]`,
`varStore1_[level][var]`).

### Combinators

Boolean combinators use a probability-style fuzzy logic:

- **And:** `dt(lhs) * dt(rhs)` — the joint truth is the product of the component truths.
- **Or:** `dt(lhs) + dt(rhs) - dt(lhs) * dt(rhs)` — probability of the union.
- **Not:** `1 - dt(condition)` — the complementary truth.

### Default Smoothing Factor

Each fuzzy evaluator carries a `defEps_` default smoothing factor. Individual
condition nodes can override it via their `eps_` field; a negative `eps_` on
the node means "use the evaluator's default". This lets the caller set a global
smoothing width while allowing exceptional conditions (e.g., a known
discontinuous barrier) to carry a tighter or looser spread.

## Pipeline Ordering

The visitors must run in a specific order:

1. **`DomainProcessor_`** — computes variable/expression domains and sets
   `alwaysTrue_`/`alwaysFalse_` flags (and `isDiscrete_` bounds if fuzzy).
2. **`ConstCondProcessor_`** — collapses always-true/false nodes into concrete
   forms, pruning dead branches.
3. **Evaluation** — `Evaluator_<T>` (exact) or `FuzzyEvaluator_<T>` (smoothed)
   walks the simplified tree to produce numeric results.

The shared AST nodes and visitor base classes live in
`dal-cpp/dal/script/visitor/`; the preprocessor lives in `dal-cpp/dal/script/`
and is deliberately separate.

## Simulation and Evaluation

`MCSimulation<T_>` (`dal-cpp/dal/script/simulation.hpp`) drives Monte Carlo
valuation of a `ScriptProduct_`. It has two instantiations: `T_ = double`
(value-only) and `T_ = AAD::Number_` (pathwise-adjoint, see
[Automatic Adjoint Differentiation](aad.md)).

### RNG and Brownian Bridge

`CreateRNG` selects the underlying generator from a method string — `sobol`
(Sobol low-discrepancy), `mrg32` (MRG32k32a pseudo-random), or `irn` (industrial
pseudo-random) — sized to the model's simulation dimension. When the Brownian
bridge flag is set, the generator is wrapped in a `BrownianBridge_` so the draw
order reconstructs the path from coarse to fine maturities rather than in
chronological order; this often reduces variance for path-dependent payoffs.

### Batching and Thread Pool

Paths are divided with an effective batch size
`min(8192, ceil(nPaths / nThreads))` and submitted to
`ThreadPool_::GetInstance()`. Batch sizes, offsets, and counts use `size_t`, and
the planner rejects a zero thread count without performing division. This keeps
multiple useful tasks for small simulations while capping large batches at
`8192`. Each thread owns its own RNG, Gaussian vector, scenario
(`Scenario_<T_>`), and evaluator state, so the per-path work is lock-free. An
AAD task activates its thread-local tape and constructs its active model on that
same thread; active numbers are never copied from the coordinator's tape into a
worker tape. Compiled operand stacks are members of the task-owned `EvalState_`,
and recursive compiled evaluation reuses those stacks without leaving active
numbers registered beyond the state lifetime. A task group owns each future as
soon as submission succeeds and drains all accepted tasks during normal or
exceptional unwinding, including when a later submission is rejected. It first
waits until every accepted future is ready and then consumes every future, so a
task failure is rethrown to the valuation caller only after no task can still
reference local simulation state. Thread-local results are summed at the end.
This is the parallel structure described in
[Pathwise Adjoints in Monte Carlo](aad.md#pathwise-adjoints-in-monte-carlo).

### Value-Only vs AAD Evaluation

The `double` instantiation walks the AST with an `Evaluator_<double>` (or, in
compiled mode, an `EvalState_<double>` over the pre-compiled node/const
streams) and accumulates the payoff slot across paths. The
`AAD::Number_` instantiation additionally:

1. Activates the tape and registers model parameters and constant variables on
   it (`InitModel4ParallelAAD`), then marks.
2. Per path: rewinds to the mark, generates the path, evaluates the AST with a
   `FuzzyEvaluator_<AAD::Number_>` (or compiled `EvalState_<AAD::Number_>`),
   seeds the payoff adjoint to $1$, and propagates back to the mark.
3. After the batch: propagates from mark to start, harvests per-parameter
   adjoints, and divides by `nPaths`.

The result is a `SimResults_` carrying the aggregated payoff and a risk vector
labelled by model parameter and constant variable.

### Tree-Walk and Compiled Evaluation

The script engine has two execution modes. The tree-walk mode evaluates the
preprocessed AST directly with `Evaluator_<double>` for value-only paths and
`FuzzyEvaluator_<AAD::Number_>` for AAD paths. The compiled mode first lowers
the same AST into flat per-event streams and then evaluates those streams with
`EvalState_<T>`.

`compiled` is a performance flag, not a pricing model flag. The default is
`compiled = false` for both `MCSimulation<double>` and
`MCSimulation<AAD::Number_>`, preserving the historical tree-walk behavior.
Callers can pass `compiled = true` to opt into the stream evaluator. Both modes
are required to produce the same payoff and risk numbers, aside from normal
floating-point association noise.

`ScriptProduct_::Compile(fuzzy)` is `const`, but it requires that
`PreProcess(...)` has already run. Preprocessing indexes variables, seeds past
event values, folds domain-provable constant conditions, and finalizes constant
metadata. `Compile` returns a separate `ScriptCompiled_` artifact instead of
storing streams on the product, so the AST remains reusable by tree-walk
evaluation after compilation. `MCSimulation` compiles once per valuation on the
main thread before dispatching path batches; a missing `PreProcess` is therefore
reported as a normal exception rather than being hidden inside worker tasks.

The compiled artifact stores one integer opcode stream and one constant stream
per future event. The integer stream contains opcodes plus operands such as
variable indexes, const-stream indexes, and branch jump positions. This removes
per-node virtual dispatch in the inner path loop and replaces it with a tight
switch over the opcode stream. `NodeType_` is intentionally hand-written:
opcodes are both non-type template parameters in the compiler visitor and
serialized integers in the node stream, which a generated Machinist wrapper enum
does not provide.

In value mode (`fuzzy = false`), compiled evaluation mirrors
`Evaluator_<T>`:

- arithmetic and payoff opcodes use the same formulas as tree-walk nodes;
- comparisons push hard boolean values;
- `If` and `IfElse` take only the active branch;
- `AND` and `OR` remain eager, matching the documented boolean semantics;
- `SupEqual` uses the same `x >= 0` comparison for constant folding and runtime
  evaluation.

In AAD mode (`fuzzy = true`), compiled evaluation mirrors
`FuzzyEvaluator_<T>`:

- comparisons emit `FuzzyEqual`, `FuzzyComp`, or their discrete-bound variants;
- the smoothing kernels are shared with tree-walk (`BFly` for equality,
  `CSpr` for inequalities);
- a negative per-node `eps_` falls back to the evaluator default `defEps_`;
- `AND`, `OR`, and `NOT` use the same probability-style fuzzy combinators;
- `If` emits `FuzzyIf`, which stores the affected variable list, evaluates the
  hard branch when the degree of truth is within `EPSILON` of 0 or 1, and
  otherwise evaluates both branches and blends the affected variables by the
  fuzzy degree of truth.

Constant variables stay live in both modes. They are represented by the
`ConstVar` opcode and read from evaluator state, rather than being baked into
the const stream. This keeps `EvalState_::ConstVarVals()` mutable in the same
way as tree-walk evaluators and allows AAD const-variable risks to be recorded
on the tape.

Regression coverage lives in `dal-cpp/tests/script/test_compile_parity.cpp`.
It checks per-path double parity, Monte Carlo aggregate parity, AAD PV/risk
parity, const-variable mutation, preprocessing guards, fuzzy condition
behavior, and opcode reachability. The benchmark target
`dal-cpp/benchmarks/script_mc_perf` compares `compiled=false` and
`compiled=true` runs across simple and schedule-heavy products for both
`double` and `AAD::Number_`; it times the Monte Carlo path loop rather than the
parser front-end.

## Visitor Machinery

Every AST pass — indexing, IF-flattening, domain analysis, constant-condition
folding, evaluation, fuzzy evaluation, compilation, debugging — is a visitor.
The machinery in `dal-cpp/dal/script/visitor.hpp` and
`dal-cpp/dal/script/visitorlist.hpp` makes adding a new pass or a new node type
cheap.

### Visitor Base

`Visitor_<V_>` and `ConstVisitor_<V_>` provide two helpers each: `VisitNode`
dispatches a node to the concrete visitor `V_` via `node.Accept(...)`, and
`VisitArguments` walks the node's `arguments_` children. They also provide a
default `Visit(Node_&)`/`Visit(const N_&)` that simply visits the children — so
a concrete visitor only declares `Visit(...)` overloads for the node types it
actually cares about, and the rest fall through to traversal.

### The Visitable Sugar

`VisitableBase_<V1, V2, ...>` declares, for the abstract `Node_`, one pure
virtual `Accept(Vi&)` per visitor on the list. `Visitable_<Node_, NodeTag, V1, V2, ...>`
generates, for a concrete node, the matching overrides that each call
`v.Visit(*this)`. The generated code is exactly what one would write by hand
(`virtual void Accept(Vi& v) override { v.Visit(*this); }` for each visitor),
so the meta-programming is purely sugar — the run-time behaviour is a plain
double-dispatch table. Adding a node type means inheriting `Visitable_<...>`
with the full visitor list; adding a visitor means adding it to the list once.

### Visit-Trait Dispatch

The dispatch layer in `dal-cpp/dal/script/visitorlist.hpp` resolves, entirely at
compile time, three questions that the visitor framework needs answered without
run-time cost:

- `IsVisitorConst<V_>()` — whether `V_` is a read-only (`ConstVisitor_`-derived)
  pass or a mutating (`Visitor_`-derived) pass. It is a `Pack_<...>::Includes`
  check against the const-visitor list, so the answer is a `constexpr bool`.
- `HasConstVisit_<V_>::ForNodeType<N_>()` — whether `V_` declares
  `void Visit(const N_&)`. Uses SFINAE on the member-function pointer type.
- `HasNonConstVisit_<V_>::ForNodeType<N_>()` — the mutating dual of the above,
  probing for `void Visit(N_&)`.

Because all three are constant expressions, the dispatch in `visitor.hpp` can
select the correct `Accept` / `Visit` path with no virtual overhead beyond the
double-dispatch table itself, and const-correctness (next subsection) is decided
during template instantiation rather than at run time.

### Const-Correctness

`ConstVisitor_<V_>` is used by passes that only read the tree (evaluation,
debugging); `Visitor_<V_>` is used by passes that mutate it
(`ConstCondProcessor_`, `DomainProcessor_`). The const visitor enforces its
contract at compile time via `HasConstVisit_` / `HasNonConstVisit_`: a non-const
`Visit` overload on a const visitor does not satisfy the const base signature, so
attempts to mutate through a const visitor fail to build rather than corrupt the
tree.

## Examples

The script program feeds an events table to `ScriptProduct_`, which runs the
parser and AST construction in its constructor, and then prints the debug walk
of the processed tree. See [`dal-cpp/examples/script/`](../../dal-cpp/examples/script/)
for a runnable version; its events table and product construction are:

```cpp
// from dal-cpp/examples/script/script.cpp
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;
using namespace Dal::Script;

Dal::RegisterAll_::Init();
Global::Dates_::SetEvaluationDate(Date_(2022, 9, 25));

Vector_<Cell_> eventDates;
Vector_<String_> events;

// Constant variables resolve in the preprocessor and become NodeConstVar_ leaves
eventDates.emplace_back("BARRIER");
events.emplace_back("150.00");
eventDates.emplace_back("STRIKE");
events.emplace_back("120.00");

// Schedule directive: the preprocessor expands PeriodBegin/PeriodEnd per period
eventDates.emplace_back(
    "START: 2022-09-25\n"
    "END: 2025-09-25\n"
    "FREQ: 1W");
events.emplace_back("IF spot() > BARRIER:0.1 THEN alive = 0 END");

// Final payoff: an IF/END block followed by a PAYS clause on the same date
eventDates.emplace_back(Date_(2025, 9, 25));
events.emplace_back(
    "IF spot() > BARRIER:0.1 THEN alive = 0 END "
    "uoc pays alive * MAX(spot() - STRIKE, 0.0)");

// The constructor runs the preprocessor and parser; the returned product holds
// the dated AST that DomainProcessor_, ConstCondProcessor_, and the evaluator
// then walk
ScriptProduct_ product(eventDates, events);
```

The `BARRIER:0.1` suffix on each comparison sets the node's `eps_` field, which
the fuzzy evaluator consumes as the smoothing width for that condition. Running
`product.Debug(out)` after the constructor walks the AST and writes the dated,
variable-indexed event listing that the visitor passes operate on; downstream
valuation calls `IndexVariables` and `PreProcess` before evaluation or
`Compile`. The Monte Carlo driver is the free function `MCSimulation<T_>` in
`dal-cpp/dal/script/simulation.hpp`, templated on `double` for value-only runs
and on `AAD::Number_` for pathwise-adjoint runs.

## See Also

- [Automatic Adjoint Differentiation](aad.md) — the reverse-mode machinery that
  fuzzy evaluation feeds, enabling pathwise Greeks through discontinuous payoffs.
- [`dal-cpp/examples/script/`](../../dal-cpp/examples/script/) — runnable example
  of the full pipeline: events table parsing, preprocessing, domain analysis,
  condition folding, and evaluation.
