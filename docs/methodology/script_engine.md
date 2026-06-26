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

## Preprocessing Pipeline

The `Preprocessor_` class in `dal-cpp/dal/script/preprocessor.hpp` resolves a raw
`(Cell_, String_)` events table into `PreprocessedEvents_`: a map of constant
variables (`String_ -> double`) and dated event descriptions (`Date_ -> String_`).

The class is built for extension: `Process` orchestrates a fixed pipeline while
every meaningful decision delegates to a protected virtual method, so a derived
preprocessor can recognise new directive kinds or new placeholders without
re-implementing the orchestration.

| Virtual method                 | Role                                                              |
|--------------------------------|-------------------------------------------------------------------|
| `IsSchedule(desc)`             | True when a non-date directive description denotes a schedule     |
| `IsConstVariable(value)`       | True when a non-date directive value defines a constant variable  |
| `ExpandMacros(stmt, macros)`   | Replace every registered macro name in a statement with its body  |
| `ExpandSchedulePlaceholders(...)` | Replace `PeriodBegin` / `PeriodEnd` placeholders for one period |

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

Two primitive smooth transitions are used. For a comparison `expr > 0` (or `>=`),
the **continuous spread** function transitions from $0$ to $1$ across a band of
width $\varepsilon$:

$$
\operatorname{CSpr}(x;\; \varepsilon) =
\begin{cases}
0       & x < -\varepsilon/2,\\
1       & x > +\varepsilon/2,\\
(x + \varepsilon/2)/\varepsilon & \text{otherwise}.
\end{cases}
$$

For an equality `expr = 0`, the **butterfly** function is a triangular pulse
centred at $0$:

$$
\operatorname{BFly}(x;\; \varepsilon) =
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
\operatorname{CSpr}(x;\; lb, rb) =
\begin{cases}
0          & x < lb,\\
1          & x > rb,\\
(x - lb)/(rb - lb) & \text{otherwise},
\end{cases}
\qquad
\operatorname{BFly}(x;\; lb, rb) =
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

## See Also

- [Automatic Adjoint Differentiation](aad.md) — the reverse-mode machinery that
  fuzzy evaluation feeds, enabling pathwise Greeks through discontinuous payoffs.
- `dal-cpp/examples/script.cpp` — runnable example of the full pipeline: events
  table parsing, preprocessing, domain analysis, condition folding, and evaluation.
