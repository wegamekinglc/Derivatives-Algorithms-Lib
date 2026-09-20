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

`Lex` (`dal-cpp/dal/script/lexer.hpp`) produces positioned `Token_` values for
the parser. Each token holds either ordinary text or an `IndexLiteral_` with
the complete raw index spelling. `SourceLocation_` carries the character offset
and one-based line and column in the expanded event text; source origins from
the preprocessor add the original one-based table row and expanded event date.
`Tokenize` projects these tokens to strings for schedule parsing and existing
callers that do not need type or position information.

The lexer preserves bracket contents, FX slashes, and EQ delivery suffixes as
one index literal. Commas and parentheses inside brackets remain name content;
the index argument ends at a comma or closing parenthesis outside the brackets.
Nested brackets, quotes, unmatched brackets, and unsupported script characters
raise errors. The shared `IndexLiteralRanges` scan also identifies the regions
that the preprocessor protects from substitution.

### Precedence Levels

The parser implements expressions as a cascade of precedence levels, each
delegating to the next tighter level:

| Level | Operator class          | Produces                                                                                                                       |
|-------|-------------------------|--------------------------------------------------------------------------------------------------------------------------------|
| L1    | `+`, `-` (binary)       | `NodeAdd_`, `NodeSub_`                                                                                                         |
| L2    | `*`, `/`                | `NodeMulti_`, `NodeDiv_`                                                                                                       |
| L3    | `^` (right-assoc)       | `NodePow_`                                                                                                                     |
| L4    | unary `+`, `-`          | `NodeUPlus_`, `NodeUMinus_`                                                                                                    |
| Atom  | literal, variable, func | `NodeConst_`, `NodeVar_`/`NodeConstVar_`, `NodeSpot_`, `NodeFix_`, `NodeLog_`, `NodeExp_`, `NodeSqrt_`, `NodeMin_`, `NodeMax_` |

Parenthesised sub-expressions re-enter at the top level through a shared
`ParseParentheses` helper. Conditions form a parallel cascade — `OR` (loosest)
binds over `AND`, which binds over comparison elements — producing `NodeOr_`,
`NodeAnd_`, and the comparison/equality nodes.

### Reserved Keywords and Variables

A fixed reserved-word set (`IF`, `THEN`, `ELSE`, `END`, `PAYS`, `AND`, `OR`,
`SPOT`, `FIX`, `MAX`, `MIN`, `LOG`, `SQRT`, `EXP`, `DCF`, `EXERCISE`) cannot be used as variable
names. Any other alphabetic token becomes either a `NodeVar_` (looked up in the
preprocessor's constant-variable map and promoted to `NodeConstVar_` if it
resolves there). Statements are either assignments (`=`, `NodeAssign_`), pays
clauses (`PAYS`, `NodePays_`), `IF/THEN/ELSE/END` blocks (`NodeIf_`, with
`firstElse_` indexing the else-branch within `arguments_`), or early-exercise
clauses (`EXERCISE <value> [IF <condition>]`, `NodeExercise_`, at most one per
event and only at the event top level).

`FIX` and `EXERCISE` are also reserved for macro and constant-variable
definitions. A conflicting definition or variable produces `ReservedIdentifier`
with source context and a request to rename it. Keyword comparisons are
case-insensitive.

### Named Fixing Syntax

`FIX(index)` and `FIX(index, date)` parse to a `NodeFix_` leaf. These are
accepted expressions:

```text
FIX(EQ[AAPL])
FIX(FX[EUR/USD])
FIX(EQ[AAPL]>3M)
FIX(EQ[AAPL]@2026-12-31, 2026-09-11)
```

The first argument must be a complete unquoted index literal. The parser passes
its raw spelling to `Index::Parse` and retains the returned index handle; the
full identity includes FX direction and any EQ delivery suffix. Index grammar
and complete-input validation belong to [index parsing](index_parsing.md).
Unknown names, malformed names, and parsers returning no index fail with script
source context.

The optional second argument must be a contiguous, valid `YYYY-MM-DD` calendar
date. In the last example, `2026-12-31` belongs to the index's delivery identity,
while `2026-09-11` is the fixing date. A schedule placeholder may supply the
date only after preprocessing has expanded it to that literal. `FIX()` is
invalid, as are quoted or runtime index arguments, runtime or arithmetic dates,
timestamps, and extra arguments. `SPOT()` remains the zero-argument model-spot
expression; it does not accept named arguments.

`NodeFix_` retains the raw `IndexLiteral_`, immutable `Index_` handle, optional
`Date_`, and `SourceLocation_`. An omitted fixing date remains unset in this
node. Parsing performs no fixing lookup or model binding.

`FIX` supports parsing, AST inspection, immutable
[historical preparation](#historical-fixing-preparation),
[public C++ valuation](#public-c-settings), and core
[double/tree valuation](#core-doubletree-fixing-valuation) and
[AAD/tree valuation](#core-aadtree-fixing-valuation), both also available in
compiled mode with model-aware preparation. Raw `ScriptProduct_` objects
containing any `FIX` still raise `PreparationRequired`
from `PreProcess`, `Compile`, `PastEvaluate`, `Evaluate`, and `MCSimulation`,
including when the node is in a dead branch. Domain and compiler visitors
require a prepared observation ID and plan to process `NodeFix_`.
Prepared tree and compiled evaluation share hard historical replay, exact
double or fuzzy AAD future semantics, and dependency-aware optimization.

### SPOT() Compatibility and the FIX Boundary

`SPOT()` is the retained zero-argument compatibility form for the model spot
at the event date; `FIX(index[, date])` is the named-observation form,
expressing both historical and future fixings. `SPOT` takes no
arguments — within script text, an observation's named identity is spelled
only as the unquoted literal inside `FIX`; the product `defaultIndex_`
supplies a default-bound `SPOT()` identity outside script text — and `FIX()`
is invalid. The forms differ in where an observation takes its identity and
what each fixing-date relation requires. For `SPOT()`, the fixing date is
always the event date:

| Form                   | Identity                 | `F < D`                                    | `F = D`                                                     | `F > D`                                       |
|------------------------|--------------------------|--------------------------------------------|-------------------------------------------------------------|-----------------------------------------------|
| `FIX(index[, date])`   | Unquoted literal         | Midnight history; `MissingFixing` on a gap | Model, or history under `REQUIREHISTORICAL`                 | Model, bound to the script's EQ index by name              |
| Unbound `SPOT()`       | Model spot at event date | `UnboundHistoricalSpot`                    | Model, or `UnboundHistoricalSpot` under `REQUIREHISTORICAL` | Legacy model path; no binding                 |
| Default-bound `SPOT()` | Product `defaultIndex_`  | History shared with matching `FIX`         | As for `FIX`, shared with matching `FIX`                    | Model; same script index as `FIX`            |

The compatibility rules are:

- New scripts name observations with `FIX`. Existing future-only `SPOT()`
  scripts need no change and keep their entry points, defaults, and
  tree/compiled and AAD paths.
- A product `defaultIndex_` only names `SPOT()`; it does not change `FIX`
  literals, bind a model, or supply market data.
- Mixing a future-only unbound `SPOT()` with `FIX` fails
  with `MissingDefaultIndex`; these checks include dead branches.
- Missing required history is always an error, never a model or placeholder
  value, whichever form the observation uses.
- An omitted `FIX` date resolves to the event date during preparation;
  `F > E` fails with `LookAheadObservation`, including in a dead branch.

Valuation-level detail lives under
[The Script Model Index and Legacy SPOT](#the-script-model-index-and-legacy-spot).

### Comparators and Smoothing Hints

`ParseCondElem` lowers every comparison to a subtraction wrapped in the
appropriate condition node (`NodeEqual_`, `NodeSup_`, `NodeSupEqual_`), with
`!=`, `<`, `<=` rewritten in terms of `=`, `>`, `>=`. An optional `;eps` or
`:eps` suffix on a comparison sets the node's `eps_` field, which the fuzzy
evaluator consumes as the smoothing width for that condition (see
[Fuzzy Evaluator](#fuzzy-evaluator)). The same suffix also feeds the
`EXERCISE` decision width: a conditioned statement's decision degree uses the
**first** comparison of its condition — falling back to `simulation.smooth_`
when that comparison carries no suffix — while every comparison keeps its own
width for the condition degree itself.

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

A script product is a sequence of dated events. Parsing `ScriptProduct_`
(`dal-cpp/dal/script/event.hpp`) retains every event in `Events()` and
`EventDates()` without reading the global evaluation date. `ParsedEventDates()`
retains the complete date list after partitioning too.

`PartitionEvents(D)` separates **past** events (dates before `D`) from
**future** events (dates on or after `D`). Both halves share the same AST
representation (`Event_ = Vector_<Statement_>`). Partitioning is a one-time
operation; appending events afterwards or partitioning again fails. For legacy
scripts, `PreProcess` captures the global date if the product has not already
been partitioned. It uses that same date for its timeline and rejects a second
preprocessing call. `PrepareScript` resolves an explicit valuation date or
captures the global date once before parsing a fresh product.

### From Events to a Timeline

For legacy scripts, `PreProcess` builds the simulation timeline from the future
event dates: each is converted to a year fraction from the evaluation date and paired
with an `AAD::SampleDef_` that requests the numeraire, a forward maturity at the
event time, and a discount factor at the event time. The model consumes this
`defLine_` to allocate the per-event scenario structure that evaluators read
when they walk the AST. Model-aware FIX preparation instead builds the
[union of event and observation dates](#retained-observations-and-payment-dates).

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

## Public C++ Settings

Include `dal-public/src/script.hpp` for product construction and description,
and `dal-public/src/value.hpp` for valuation and explanation. The public
`Dal::ScriptProductSettings_`, `Dal::ScriptValuationSettings_`, and
`Dal::MonteCarloSettings_` names expose the core types defined in
`dal-cpp/dal/script/settings.hpp` under `Dal::Script`. `TodayFixingPolicy_`
and `MarketFixingSnapshot_` belong to `Dal`.

The supported call shapes are:

| Entry                        | Arguments and defaults                                                                                       |
|------------------------------|--------------------------------------------------------------------------------------------------------------|
| `NewScriptProduct`           | `name, dates, events` or `name, dates, events, contract`                                                     |
| Legacy `ValueByMonteCarlo`   | `product, modelData, numPath, rsg="sobol", useBb=false, enableAad=false, smooth=0.01, compiled=std::nullopt` |
| Settings `ValueByMonteCarlo` | `product, modelData, numPath, valuation, simulation=MonteCarloSettings_()`                                   |
| `DescribeScriptProduct`      | `product`                                                                                                    |
| `ExplainScriptValuation`     | `product, modelData, valuation=ScriptValuationSettings_()`                                                   |

The product settings overload requires its fourth argument. Three-argument
valuation selects the legacy overload, which maps its optional arguments to
settings and forwards through the same preparation as the settings overload.
All original valid three-to-eight-argument calls retain their interpretation.
Use an explicitly typed `ScriptValuationSettings_` for the fourth argument;
`ValueByMonteCarlo(product, model, numPath, {})` is ambiguous.

### Fields and Defaults

| Settings type              | Field                | Default                             | Contract                                                                                                           |
|----------------------------|----------------------|-------------------------------------|--------------------------------------------------------------------------------------------------------------------|
| `ScriptProductSettings_`   | `defaultIndex_`      | Empty string                        | Gives legacy `SPOT()` its index identity; a nonempty value must parse completely.                                  |
| `ScriptValuationSettings_` | `todayFixingPolicy_` | `TodayFixingPolicy_::Value_::MODEL` | The other valid value is `TodayFixingPolicy_::Value_::REQUIREHISTORICAL`.                                          |
| `ScriptValuationSettings_` | `evaluationDate_`    | `std::nullopt`                      | Capture the global date once if omitted; an explicit `Date_` must be valid.                                        |
| `ScriptValuationSettings_` | `fixings_`           | Null handle                         | Capture required global history for this call; a non-null snapshot is authoritative, even when empty.              |
| `MonteCarloSettings_`      | `rsg_`               | `"sobol"`                           | `sobol`, `mrg32`, or `irn`, using DAL's case-insensitive comparison.                                               |
| `MonteCarloSettings_`      | `useBb_`             | `false`                             | Enable Brownian bridge; a zero-dimensional model constructs neither RNG nor bridge.                                |
| `MonteCarloSettings_`      | `enableAad_`         | `false`                             | Enable parameter risks, hard historical replay, and fuzzy future evaluation.                                       |
| `MonteCarloSettings_`      | `smooth_`            | `0.01`                              | Finite and strictly positive, including when AAD is disabled or the product is expired.                            |
| `MonteCarloSettings_`      | `compiled_`          | `std::nullopt`                      | Unset means `false` (tree); `true` selects compiled execution.                                                     |
| `MonteCarloSettings_`      | `lsmcBasisDegree_`   | `3`                                 | Integer 1..8: polynomial degree of the LSMC regression basis for `EXERCISE` valuation.                             |

The valuation settings initialize the policy to `MODEL`; assigning a separately
default-constructed, unset `TodayFixingPolicy_` is invalid. There is no
use-history-if-available policy. The product default supplies identity only;
it neither binds a model nor changes model spot/volatility inputs. Explicit
`FIX(index)` obtains its identity from the script and needs no product default.

`NewScriptProduct` checks equal date/event lengths and copies the input table
and contract settings. It performs no parsing or market access. Full script
and index validation occurs in description or preparation; pricing additionally
requires dated events and a syntactic `PAYS`. Product and model handles must
be non-null, the model must be BS or Dupire, and `numPath` must be a positive
`int`. C++ cannot detect fractional values already converted to `int` before
entry, and these typed settings have no string-key dictionary interface.

Errors retain a stable identifier, the offending field/value and its constraint,
with the throwing function in DAL exception context. Examples include
`InvalidPathCount` (`numPath`), `InvalidSetting` with `InvalidSmoothing`
(`simulation.smooth_`) or `InvalidLsmcBasisDegree`
(`simulation.lsmcBasisDegree_`), `InvalidTodayFixingPolicy`
(`valuation.todayFixingPolicy_`), `InvalidIndex`, and `MultipleModelIndices`.
Observation failures additionally identify original/canonical index names,
exact fixing timestamp, event, source row and expanded position, statement,
and node. `MissingFixing` reports the selected source and the exact-history
requirement. Exceptions are not converted to a successful zero price.

### Copies, Dates, and Concurrency

Valuation and simulation settings are copied at entry. An explicit
`evaluationDate_` causes zero global evaluation-date reads or writes; otherwise
the entry captures the global date once and carries it through preparation and
simulation. Neither path changes the caller's settings or global date.
`ValueByMonteCarlo` and `ExplainScriptValuation` still hold the native
valuation/mutation barrier, so these public calls serialize within a process
even with explicit dates. See [runtime ownership](../architecture.md#evaluation-date-synchronization).

Snapshot construction copies the input map; `Handle_<MarketFixingSnapshot_>`
shares const data. A null handle selects `GlobalSnapshot`. A handle to
`new MarketFixingSnapshot_()` selects an empty `ExplicitSnapshot`: required
history fails rather than falling back to the global store. No historical
requests means no global history capture. Global sequences are copied
sequentially, without an atomic cross-sequence market snapshot; callers must
exclude concurrent fixing writes during capture. Callers must also prevent
concurrent mutation of input tables, settings, handles, or model data while
an operation reads them. Entry copies do not make unsynchronized writes safe.

Each valuation creates fresh preparation. Its worker reads use only the sealed
plan and scenario; historical AAD state is rebuilt on the worker's own recording.
The returned map contains `PV` and, with AAD, `d_<model parameter>` and
`d_<script constant parameter>` only. PV is a path mean; risks are already
normalized. There are no fixing-risk, preparation, or diagnostic result keys.

The executable [settings example](../../dal-public/examples/script_settings.cpp)
uses an explicit date and snapshot, historical `SCALE * FIX(EQ[DAL196_TEST])`,
and compiled AAD. With history 80, SCALE 2 and zero rates, it checks `PV=160`
and `d_SCALE=80`, then prints Describe and Explain. The
[signature consumer](../../dal-public/test-consumer/script.cpp) exercises old
three-to-eight-argument calls and typed settings.

## Historical Fixing Preparation

`PrepareScript(data, settings = {}, snapshot = {})` in
`dal-cpp/dal/script/preparation.hpp` accepts `ScriptProductData_`, core
`ScriptValuationSettings_`, and an optional `Handle_<MarketFixingSnapshot_>`.
All script preparation types are in `Dal::Script`; `TodayFixingPolicy_` and
the snapshot type are in `Dal`. Each call resolves `settings.evaluationDate_`
(capturing the global date once only when omitted), reparses the original
product data, collects observations before any
condition folding, and returns a `PreparedScript_`. The result exposes const
access to its product, date, settings, and `ObservationPlan_`.

The optional core snapshot tail argument and `settings.fixings_` are normalized
to one source. A non-null tail fills an omitted setting; if both are non-null
they must refer to the same object. Different handles raise `InvalidSetting`.
The optional model-aware core contract tail uses the product's stored settings
when its default is empty. Nonempty defaults on both inputs must parse to the
same canonical identity; the stored product spelling is preserved.

### Dates and Structural Validation

Let `D` be the captured evaluation date, `E` the expanded event date, and `F`
the fixing date. An omitted FIX date resolves to `E` during preparation;
the AST's optional date remains unchanged. Every fixing key uses exact midnight
`DateTime_(F, 0.0)`: a quote at another time on the same date cannot satisfy it.

- `F < D` requires history.
- `F = D` is a model request under the default `MODEL` policy. Setting
  `settings.todayFixingPolicy_` to
  `TodayFixingPolicy_::Value_::REQUIREHISTORICAL` requires history instead.
- `F > D` remains a model request, regardless of snapshot contents.
- `F > E` raises `LookAheadObservation`, including in a dead branch or a
  wholly expired product.

A model request has no historical value slot. The overload without a model
leaves it unresolved; model-aware preparation binds it to a scenario output.
Missing required history raises an error without model fallback.
Historical requests accept the built-in `Index::Equity_` and `Index::Fx_`
implementations only. EQ delivery identities remain distinct. IR, composite,
third-party indices, and subclasses are not admitted as historical adapters.

Preparation requires at least one dated event and a syntactic `PAYS` statement.
Empty, definitions-only, and assignment-only products raise
`InvalidScriptStructure`. When any event is on or after `D`, collection and
historical resolution cover all syntax branches, including constant-false
branches and the right-hand sides of past `PAYS` statements. A missing fixing
therefore cannot disappear through branch pruning. If every event is before
`D`, structural and observation validation still run, but all historical I/O
is skipped and `KnownValues()` is empty.

### Identity, Snapshot Reads, and Virtual Fixings

`ObservationPlan_::Requests()` deduplicates FIX uses by `Index_::Name()` and
exact timestamp, using DAL's case-insensitive name comparison. Different
dates, EQ delivery identities, and the two FX directions remain distinct
logical requests. Each request retains every source use and its
event/statement/node identifiers; `historyValueId_` identifies its resolved
entry in `KnownValues()` when history was read.

Without an explicit snapshot, `SnapshotGlobalFixings` receives only historical
requests. It copies each required global `History(name)` sequence at most once
per preparation, even when several dates or logical requests depend on that
sequence. FX dependencies include both direct and reverse sequence names. No
historical requests means no snapshot construction or history-source access.
These sequential sequence captures do not provide an atomic joint market snapshot.
Concurrent fixing writes during capture are unsupported and must be excluded
by the caller.

An explicit snapshot is authoritative, including when empty: preparation never
fills gaps from global history. `SnapshotFixingEnvironment` projects only the
required names and exact timestamps from the snapshot's raw `Values()` into
`Fixings_` records in a dedicated, non-null `FixingsAccess_` environment. Extra
records, including future timestamps, do not become observation values. The
bridge does not turn a reciprocal lookup from `snapshot.Find()` into a
synthetic direct FX quote.

Each unique historical request calls the retained index's virtual `Fixing`
exactly once against that environment. `Fx_::Fixing` keeps direct-first lookup
and reverse fallback: a reverse quote of `0.8` supplies a direct value of
`1.25`. Direct and reverse quotes must be positive and, when both are supplied
at a timestamp, their product must differ from one by at most `1e-10`.
Snapshot quotes and final values must be finite; ordinary EQ fixings may be
zero or negative. FX may perform two in-memory lookups inside its one virtual
call; this is separate from the count of global sequence reads.

Lookup errors report `MissingFixing`; invalid final values report
`InvalidFixing`, and failure while capturing a global snapshot reports
`InvalidFixingSnapshot`. Historical resolution failures include the canonical
request, fixing timestamp, and source use. Preparation publishes no partial
product or plan on failure and submits no workers.

The finished plan owns passive `double` values and retains no fixing
environment. Repeating preparation captures current global history in a new
plan without changing an earlier plan. Supplying the same explicit snapshot
can deliberately preserve the earlier market. Historical values themselves
carry no fixing risk. Model-aware preparation also replays past events into
double state; AAD simulation separately rebuilds parameter-dependent state on
each worker recording, as described under [historical AAD state](#historical-state-and-recording-lifetime).

### Preparation Without a Model

`PrepareScript(data, settings, snapshot)` only prepares history. Its nonexpired
result raises `UnsupportedExecutionMode` on simulation, even if every FIX
already has a historical value. Use model-aware preparation for valuation:
future payments still require model numeraires. A structurally valid wholly
expired result supports the [zero-value path](#expired-and-today-only-products).

## Core Double/Tree Fixing Valuation

The core overload in `dal-cpp/dal/script/simulation.hpp` is
`MCSimulation<double>(data, modelData, nPaths, settings, simulation, snapshot, contract)`.
It accepts `ScriptProductData_`, a `Handle_<ModelData_>`, and a positive path
count. The required `settings` argument is `ScriptValuationSettings_`;
the remaining simulation, snapshot, and contract arguments are optional.
`MonteCarloSettings_` defaults to Sobol, no Brownian
bridge, no AAD, smoothing width `0.01`, and an unset compiled flag (tree).
Set `simulation.compiled_ = true` to use compiled exact-double execution.
`SimResults_::aggregated_` is the sum of path payoffs; divide by `nPaths` for PV.

This entry creates the model and calls
`PrepareScript(data, modelPointer, settings, simulation, snapshot, contract)`.
The model-aware preparation overload requires both settings arguments and a
non-null `AAD::Model_<double>*`. It validates all observation requests and
model capabilities, builds the sample plan, and calls model `Allocate`/`Init`
before resolving any history. History and past script state are ready before
workers start. Low-level callers must retain the returned `PreparedScript_`
while using that initialized model: the model may reference its sample
definitions. Moving the prepared object preserves that storage.

### The Script Model Index and Legacy SPOT

Black-Scholes and Dupire are single-asset models. A model-sourced `FIX`,
including today under `MODEL`, is managed purely by its index name: the
script's single model-observed ordinary EQ supplies the identity, so
`pay PAYS FIX(EQ[AAPL])` binds the model's spot output to `EQ[AAPL]` with no
settings of any kind. The named index declares which equity the caller's
model inputs describe; the library cannot verify that the caller supplied the
intended equity's market data.

An unsupported model-observed index, or a second distinct future index
(`MultipleModelIndices`), fails before history access or worker submission.
Future FX, IR, composite, EQ delivery (`>` or `@`), and multi-asset
outputs are unsupported. Historical EQ/FX observations need no model index;
several historical equities, delivery identities, and FX directions can coexist
with one future ordinary EQ. Historical inverse-FX lookup does not imply a
future FX model or a reciprocal projection of model spot.

Zero-argument `SPOT()` retains the existing future-only legacy tree, compiled,
and AAD paths and defaults. In model-aware preparation,
`ScriptProductSettings_::defaultIndex_` gives SPOT an explicit named identity
at its event date. Matching SPOT and FIX uses share one request and the same
history value or model cell. A default index names SPOT; it does not change
`FIX` literals or supply market data. Without a default, historical
SPOT raises `UnboundHistoricalSpot`; mixing future-only SPOT with FIX raises
`MissingDefaultIndex`. These checks include dead
branches. SPOT takes no arguments, and `FIX()` is invalid.

### Retained Observations and Payment Dates

Model-aware preparation sorts and deduplicates all event dates on or after
`D` together with all model-sourced fixing dates. Historical fixings add no
simulation points. Sample times remain `(date - D) / DAYS_PER_YEAR`.
`ObservationPlan_` records a `historyValueId_` or a model
`(sampleId_, outputId_)` for each request, plus a separate `EventToSample()`
mapping for payments. Sample definitions request only the needed index outputs
and event numeraires; the adapters support samples with no forward arrays.

Tree and compiled evaluators read those addresses without parsing names or
accessing an index, environment, or fixing store on a path. The entire scenario
remains available until that path finishes evaluation. A FIX observed on a day
with no event is therefore retained for every later use: if its value is 120
and payment-day spot is 999, later references still read 120. Two reads of the
same request use the same cell. Generating a full path first does not relax
`F <= E`; collection checks lookahead in all branches before valuation.

`PAYS` divides its right-hand side by the numeraire at the payment event, not
at the fixing sample. A known fixing of 80 paid at time `T` under a constant
rate `r` contributes `80 * exp(-r*T)`. Even with no future FIX, future events
retain their timeline and numeraire requests. There is no separate payment
calendar, settlement lag, or currency conversion.

Model-aware exact and fuzzy preparation retain future branches in the AST and
compiled streams, skipping domain analysis and constant-condition pruning.
Dependency-aware constant arithmetic can still be folded during compilation.
Past events replay once into the initial double variable state. Past `PAYS`
evaluates and consumes its right-hand side without adding settled cash to
payoff; historical assignments still affect future events. Each path starts
from that initial state. Statements on the same date
keep input/expansion order, and that day's model values are available when its
events execute.

### Expired and Today-Only Products

If all events precede `D`, preparation still validates structure, names,
lookahead, and ordinary settings. Simulation validates path count, RNG name,
and model construction, then returns zero value and zero labelled risks. It
does no history capture, past replay, model `Allocate`/`GeneratePath`, or worker
submission, and needs no unused model-output capability. Empty tables,
definitions-only tables, and products without a syntactic `PAYS` are errors,
not expired products. Unbound historical SPOT remains an error.

Today-only products have a valid `t=0` sample. When model dimension is zero,
`CreateRNG` validates the method name and returns without constructing an RNG
or Brownian bridge; paths use no random draws. Sobol, MRG32, and IRN names
remain valid with either bridge setting. Today's FIX still follows the chosen
`MODEL` or `REQUIREHISTORICAL` policy.

Deterministic model inputs and initialization must be finite and within the
adapter's domain before history is read. Non-finite generated spots,
observations, or payoffs and nonpositive/non-finite numeraires cause path
errors. The task group drains all accepted tasks before returning a worker
failure; preparation does not claim to predict every random numerical failure.

### Unsupported Execution and Public Surfaces

Model-aware preparation supports named double exact and AAD fuzzy valuation
in tree and compiled modes, including all-historical FIX with future payments
and default-bound SPOT. Set `simulation.compiled_ = true` before preparation
to select compiled execution; unset or false selects tree execution. The
simulation type, compiled selection, and AAD smoothing must match preparation.
History-only preparation remains nonexecutable for nonexpired products. Legacy
AAD through a raw `ScriptProduct_` rejects nonexpired products containing any
past events, even when they use no FIX; use the
model-aware prepared path for historical AAD replay. The expired zero-risk
return does not imply support for an otherwise rejected execution mode. Raw
unprepared FIX continues to raise `PreparationRequired`.

The [public C++ settings overload](#public-c-settings) exposes this preparation
for tree/compiled price and AAD valuation. Product archive v2 persists the
default index, and [Describe and Explain](#product-archive-and-diagnostics)
provide separate contract and valuation JSON.

Python exposes the same preparation through
`MonteCarlo_ValueWithSettings(product, modelData, num_path, *, valuation=None, simulation=None)`
and the three native settings types. `Product_New(events_dates, events, *, settings=None)`
accepts a product default; valuation settings supply an explicit date, today's
policy, and an immutable fixing snapshot.
Legacy product calls and three-to-eight-argument `MonteCarlo_Value` remain
available. Both Value entries validate integer paths and use fresh preparation.
The [Python settings reference](../../dal-python/README.md#script-settings-and-copies)
covers exact policy strings, property copies, conversion errors and GIL ownership.

Python `Product_Describe` and `ScriptValuation_Explain` return dictionaries;
their low-level bindings return the unchanged C++ JSON strings. Describe is
pure contract inspection; Explain always uses independent default exact/tree
price preparation, even after compiled/AAD valuation. Neither diagnostic is a
Python product archive. `ScriptSimulation_Explain` runs the full double
valuation and returns the `dal.script-simulation/1` dictionary described
[below](#simulation-diagnostic-full-valuation-with-exercise-statistics). The complete
[Python FIX example](../../dal-python/examples/012.fix_settings.py) checks a
historical SCALE payment plus a retained future observation, with an explicit
date/snapshot, compiled AAD, `PV=260` and `d_SCALE=80`; the
[early-exercise example](../../dal-python/examples/013.exercise_bermudan.py)
prices the Bermudan and weekly-exercise puts.

Excel exposes the same preparation through `MONTECARLO.VALUEWITHSETTINGS`
and immutable product, valuation, and simulation settings handles. Two-column
settings ranges supply a default for legacy SPOT, an explicit date,
today policy, snapshot, and compiled/AAD options.
Omitted handles use defaults; an explicit empty snapshot never falls back to
global history. `PRODUCT.DESCRIBE` and `SCRIPTVALUATION.EXPLAIN` project the
native schemas as headerless JSON text columns, concatenated without separators.
Each Value/Explain call prepares afresh; the functions are nonvolatile, so global
state changes require explicit recalculation. `SCRIPTSIMULATION.EXPLAIN` returns
the `dal.script-simulation/1` chunks the same way. The seven-input `MONTECARLO.VALUE`
retains default valuation settings and has no compiled argument. See the
[Excel FIX guide](../excel-script-settings.md) for exact worksheet inputs,
date/time rules, and the executable workbook.

## Early-Exercise Valuation (LSMC)

Products containing `EXERCISE` divert from the plain double driver to the LSMC
driver (`MCLsmcSimulation` in `dal-cpp/dal/script/lsmc.cpp`), in both tree and
compiled execution. Valuation runs in three phases over a fixed,
thread-count-independent batch layout: batches of `min(8192, nPaths)` paths
indexed by batch order, with per-batch contributions reduced in batch-index
sequence, so PV, the frozen coefficients, and every exercise rate are bitwise
invariant across thread counts.

- **Phase A** generates the paths and evaluates the script forward;
  `EXERCISE` is a no-op forward. Each batch records, per path, the payments of
  every `PAYS` event and, per exercise date, the regressor observation, the
  exercise value, and the condition indicator (hard 0/1 in double mode).
- **Phase B** walks the events backward. The holding value is
  $H_k = p_k + D_{k,k+1} W_{k+1}$ — the day's `PAYS` enter the hold side — and
  on each exercise date the driver regresses $H_k$ on the condition-true path
  subset over the z-normalized monomial basis $z=(x-\hat\mu)/\hat\sigma$ of
  degree `simulation.lsmcBasisDegree_` (default 3). The normal equations carry
  an explicit relative ridge $A + \lambda\,\mathrm{diag}(A)$, $\lambda=10^{-12}$,
  and a day degenerates to the constant basis — flagging
  `ConditionPathsBelowMin`, `SigmaFloor`, or `IllConditioned` — when the
  condition-true path count is below $10(d{+}1)$, the $\hat\sigma$ floor
  engages, or the estimated condition number exceeds $10^{12}$. A path
  exercises when its condition holds and $h_k > C_k(z_k)$ strictly; exercise
  replaces the day's and all later payments.
- **Phase C** replays the frozen policy: the same Sobol generator state is
  reconstructed (`SkipTo`), paths are regenerated, and each path is priced by
  its first winning exercise decision. Exercise dates are scanned in order;
  the earliest date whose condition holds with $h_k > C_k(z_k)$ under the
  frozen coefficients pays $h_k$ discounted at that date's numeraire on top
  of the payments accumulated before it. A path that never exercises keeps
  its full `PAYS` value — zero for `EXERCISE`-only products — with every
  payment discounted at its own event date. PV is the mean over paths.

The regressor is the product's single model-sourced future observation (the
same index binding as `FIX`); an unbound `SPOT()` regressor keeps a null
`regressor_index` in diagnostics. Preparation allows only `rsg = "sobol"`
for exercise products (`UnsupportedRsgForExercise`): Phase C consumes normal
paths, and only Sobol's `SkipTo` reconstructs them exactly — see
[Random and path generation](random.md#path-seeking).

Peak memory is roughly `nPaths × nPaysEvents × 8B` for the stored payments
plus `nPaths × nExerciseDates × 3 × 8B` for the per-date rows (regressor,
exercise value, and pre-exercise payoff snapshot, allocated on every exercise
date), one byte per path on conditional dates, and an `nPaths × 8B` backward
working vector. Budget examples with 52 payment events and 12 exercise dates:

| Paths | Payments | Triples | Peak   |
|-------|----------|---------|--------|
| 2^16  | 27 MB    | 19 MB   | 46 MB  |
| 2^18  | 109 MB   | 76 MB   | 185 MB |
| 2^20  | 436 MB   | 302 MB  | 738 MB |

Reduce the path count or event count to stay inside a budget.

AAD valuation of exercise products uses the fuzzy driver described in the
next section; the per-exercise-date statistics above are observable through
the [simulation diagnostic](#simulation-diagnostic-full-valuation-with-exercise-statistics),
and the acceptance suite anchors both engines against a test-only Bermudan
PDE pricer (`dal-cpp/test-support/bermudan_pde.hpp`; the library PDE itself
stays European-only). The runnable
[`dal-cpp/examples/american_put_mc/`](../../dal-cpp/examples/american_put_mc/)
prices the European-limit, two-date Bermudan, and weekly-exercise puts and
prints the diagnostic.

## Core AAD/Tree Fixing Valuation

`MCSimulation<AAD::Number_>(data, modelData, nPaths, settings, simulation, snapshot, contract)`
uses the same model-aware observation plan, date policies, the script's EQ model index,
and payment numeraires as the double entry. The data overload enables AAD
automatically; `simulation.smooth_` selects the default future-condition width
and `simulation.compiled_` selects tree or compiled execution. Each call
prepares a fresh product and historical plan from the supplied inputs.
Historical EQ/FX values remain passive, while model observations and script
constant variables participate in differentiation.

To simulate a retained `PreparedScript_`, prepare it with a double model and
`simulation.enableAad_ = true`, then call
`MCSimulation<AAD::Number_>(prepared, modelData, nPaths, rsg, useBb, compiled, maxNestedIfs, eps)`.
For a nonexpired product, the requested type and effective `compiled` flag
must match preparation, and `eps` must equal its `smooth_`; a mismatch raises
`UnsupportedExecutionMode`. In the AAD simulation driver the diagnostic is
`UnsupportedExecutionMode: AAD mode, smoothing, or compiled/tree mode differs from preparation`.
The outer prepared overload checks the scalar type first and reports
`UnsupportedExecutionMode: evaluation mode differs from preparation` for that
mismatch. An omitted `compiled` argument resolves to tree; it does not inherit
the prepared selection. The prepared product supplies the required IF
nesting depth. Retaining it retains its captured date, script parameters, and
sealed history; prepare again when those inputs change. Each simulation still
constructs fresh active models and historical seeds.

A fuzzy-double reference uses the low-level evaluators on a product prepared
with `enableAad_ = true`: `BuildFuzzyEvaluator<double>` plus `Evaluate`, or
`BuildEvalState<double>` plus `Compile(true).Evaluate` when compiled preparation
was selected. Use the prepared smoothing width in either case. This does not
make `MCSimulation<double>` a fuzzy pricing entry; it rejects AAD-enabled settings.

### Early-Exercise AAD

Products containing `EXERCISE` divert to the fuzzy LSMC driver
(`MCLsmcAadSimulation` in `dal-cpp/dal/script/lsmc.cpp`), in both tree and
compiled execution. The forward storage and backward regression phases run
exactly as the double driver, so the continuation coefficients `C_k` are the
thread-count independent hard-decision artifact. Each replay worker then
regenerates its batch on its own tape, records the per-event payments and the
per-date exercise values and fuzzy condition degrees, and prices the path with
the recursive blend

$$V_k = d_k h_k + (1 - d_k)(p_k + D_{k,k+1} V_{k+1}),\qquad d_k = \mathrm{CSpr}(h_k - C_k(z_k), \varepsilon)\cdot c_k,$$

where $c_k$ is the fuzzy condition degree (1 when unconditional), the discount
ratios come from the path's own numeraires, and $\varepsilon$ is the exercise
statement's smoothing width resolved against `simulation.smooth_`. The blend
is carried in event-date units, so the recursion ends with the explicit
division by the first event's numeraire. As $\varepsilon \to 0$ the decision
degrees degenerate to hard indicators and the fuzzy path value converges to
the hard-mode payoff.

The regression coefficients enter the replay as passive tape constants, so the
harvested adjoint is the exact gradient of the *frozen-policy* price
functional. By the envelope theorem the difference to the total derivative —
the missing $\partial C/\partial\theta$ term — is second order in the policy
suboptimality error. Bump tests that regenerate the policy (production
behavior) therefore carry this envelope remainder on top of their Monte Carlo
error: on the two-date benchmark it measures around 1% for spot/rate, below 1%
for dividend, and around 2-3% for volatility, the most policy-sensitive
parameter because the continuation regression itself is vol-dependent; where
the policy is exactly optimal (single exercise date, worthless continuation)
the adjoint matches analytic Greeks to better than 0.2%. Seeded coefficient
regeneration is the recorded fallback for tightening this band and stays
outside the current version. Adjoint accumulation and reduction follow the
same thread-count independent batch layout and batch-index ordering as the
double driver, so PV and every `d_<param>` are bitwise invariant across
thread counts in AAD mode as well, and historical fixings replay into the
seed exactly as for non-exercise products.

### Historical State and Recording Lifetime

Preparation resolves historical I/O before any worker starts and retains only
passive values and the historical program. Its double replay provides the
initial value state, but converting that state to active numbers would lose
the dependence of `x = SCALE * FIX(index, date)` on `SCALE`.

Each AAD batch therefore activates its worker's tape and constructs its own
model, constant variables, evaluator, and registered zero input. After rewind,
input registration, and `NewRecording`, it initializes the model and replays
past events with `PastEvaluator_<AAD::Number_>` or the prepared hard historical
bytecode using the sealed doubles.
`SetHistoricalSeed` retains the resulting typed variable state before `Mark`.
Every path rewinds to that mark and restores the seed's live node references;
it does not restore parameter-dependent variables from `variablesInit_` doubles.
Historical replay reads no index, fixing store, or environment on the worker.
The next batch rebuilds the seed even when it runs on the same thread; active
numbers and seeds never cross threads or recordings.

After future evaluation, `AAD::PayoffRoot` in `dal-cpp/dal/math/aad/aad.hpp`
reuses the native payoff node only when the post-mark range is nonempty and
the payoff is its current terminal node. Otherwise it records
`payoff + activeZero`, using the registered zero input to create a path-local
root. Adept, XAD, and CoDiPack always use this addition. Seeding the resulting
root and propagating to the mark accumulates contributions without overwriting
a historical seed's adjoint. The fallback also handles a pre-mark payoff,
a passive constant, or an otherwise empty post-mark recording, ensuring a
valid reverse range.
After all paths in a batch, propagation from mark to start carries the
accumulated seed risk into script and model inputs. Each batch's risks are
divided by the total simulation path count once; summing worker results does
not normalize them again. `SimResults_::risks_` therefore contains mean-price
sensitivities, while `aggregated_` remains the unnormalized payoff sum.

For example, let the evaluation date be 2026-09-12, the 2026-09-11 fixing of
`EQ[DAL196_TEST]` be 80, and `SCALE` be 2. A past event
`x = SCALE * FIX(EQ[DAL196_TEST], 2026-09-11)` followed by
`pay PAYS x` on 2026-09-22 gives, for `T = 10 / DAYS_PER_YEAR`,
`PV = 160 * exp(-r*T)`, `d_SCALE = 80 * exp(-r*T)`, and
`d_rate = -T * PV`. Spot and volatility risks are zero, and no fixing risk key
is emitted. If the selected payoff slot directly retains `x` without a future
payment, its value is 160 and its SCALE sensitivity is 80, independent of path
count; no payment discount is introduced implicitly.

### Hard Historical and Fuzzy Future Conditions

Events before the evaluation date use hard comparisons in both double and AAD
replay, including at equality and when a comparison carries a smoothing hint.
The selected branch's arithmetic remains differentiable; there is no risk
through the hard branch decision itself. Past `PAYS` consumes its right-hand
side without adding settled cash. Repricing with changed script parameters
requires fresh preparation and reselects the historical branch.

Events on or after the evaluation date use exact comparisons in double mode
and the existing fuzzy kernels in AAD mode. A future condition on an already
known historical fixing can still have fractional truth and parameter risk
inside its smoothing band. Prepared fuzzy evaluation keeps parsed continuous
comparison kernels and their default or explicit runtime widths. Preparation
skips domain analysis and condition pruning, preserving arithmetic and
fractional branch weights across nested IFs and later events. Tree and compiled
evaluation use the same kernel. Exact-double and fuzzy-AAD PVs
can consequently differ near future discontinuities; compare the AAD primal
with `FuzzyEvaluator_<double>` using the same widths and paths. Historical
switching points do not have a smooth derivative across branches.

For a future condition `FIX(EQ[DAL196_TEST], 2026-09-11) > K:0.2`, a known
fixing of 80 and `K = 79.95` give weight 0.75. A true-branch amount
`SCALE * 80` with `SCALE = 2` and zero otherwise has value 120 before discount,
SCALE sensitivity 60, and K sensitivity -800. Folding the fixing into a passive
value must preserve this fractional weight and the live K/SCALE dependencies.
Finite differences must reprepare and use the same smoothing width and paths.

## Preprocessing Pipeline

The `Preprocessor_` class in `dal-cpp/dal/script/preprocessor.hpp` resolves a raw
`(Cell_, String_)` events table into `PreprocessedEvents_`: a map of constant
variables (`String_ -> double`) and dated event descriptions (`Date_ -> String_`).
It also records source origins for each event description. Statements sharing
an event date are concatenated in input/expansion order while retaining their
individual table rows for parser diagnostics.

The class is built for extension: `Process` orchestrates a fixed pipeline while
every meaningful decision delegates to a protected virtual method, so a derived
preprocessor can recognise new directive kinds or new placeholders without
re-implementing the orchestration.

| Virtual method                    | Role                                                             |
|-----------------------------------|------------------------------------------------------------------|
| `IsSchedule(desc)`                | True when a non-date directive description denotes a schedule    |
| `IsConstVariable(value)`          | True when a non-date directive value defines a constant variable |
| `ExpandMacros(stmt, macros)`      | Replace macro names outside complete index literals              |
| `ExpandSchedulePlaceholders(...)` | Replace period placeholders outside complete index literals      |

The default implementation recognises simple textual macros and period-based
schedule expansion; a derived preprocessor can override any of these to support
domain-specific directive syntax without touching the orchestration logic.

Macro and schedule-placeholder substitutions protect complete index literals,
including delivery suffixes. For `FIX(EQ[PeriodBegin], PeriodBegin)`, only the
second argument expands to a date. A macro may expand into a complete index
literal; each subsequent substitution rescans the result so the new literal is
protected too. Outside these regions, macros retain their case-insensitive
regular-expression replacement in map order, followed by `PeriodBegin` and
`PeriodEnd` expansion for schedules.

## Domain Processor

Neither exact nor fuzzy model-aware preparation runs `DomainProcessor_` or
`ConstCondProcessor_`. The tolerance-based `Domain_` operations cannot prove
exact floating-point branch decisions: adjacent values may compare equal in
the domain model, and a small nonzero factor may collapse a parameter's range
to zero. Domain interval inversion can also treat a finite signed tiny
nonzero divisor as zero and reject valid fuzzy arithmetic. Model-aware
preparation therefore retains parsed future arithmetic and branches, including
those depending only on known fixings, and preserves continuous fuzzy kernels
with default or explicit epsilon. This policy also applies to model-aware
preparation of default-bound SPOT. The execution cost of retained branches is
unmeasured.

The domain passes below apply to legacy `ScriptProduct_::PreProcess` when
domain processing is enabled. Model-aware preparation does not invoke them.

`DomainProcessor_` in `dal-cpp/dal/script/visitor/domainproc.hpp` determines the
domains (value ranges) of all script variables and expressions. Its purposes are:

1. Set the `alwaysTrue_` / `alwaysFalse_` flags on condition nodes (`NodeEqual_`,
   `NodeSup_`, `NodeSupEqual_`, `NodeNot_`, `NodeAnd_`, `NodeOr_`) and `NodeIf_`
   by evaluating the condition's domain.
2. When fuzzy processing is enabled, additionally set the `isDiscrete_` flag and
   left/right interpolation bounds on equality and comparison nodes for the fuzzy
   evaluator.

**Domain stack.** The processor maintains a stack of `Domain_` objects that
represent the runtime value range of each sub-expression. The legacy constructor
starts variable domains at the singleton $\{0\}$; script constant variables
and unbound model SPOT expressions have unrestricted real domains.
A parameter's current numeric value is not its domain. Model-aware historical
dependencies instead feed constant metadata without constructing domains.
As the walker descends through arithmetic operations, domains are combined
(pushed/popped) on the stack. A boolean condition stack (`DomainCondProp_`
values: `AlwaysTrue`, `AlwaysFalse`, `TrueOrFalse`)
tracks the outcome of evaluated conditions.

**Legacy domain condition flags.** For `NodeEqual_` (expr $= 0$), if the domain of the
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

Where domain-derived condition flags are used, this analysis makes the
constant-condition processor's job possible: once
`alwaysTrue_`/`alwaysFalse_` flags are set, the next pass can collapse dead
branches.

**Fuzzy interpolation bounds.** Legacy singleton domains retain the runtime
kernel with the node's `eps_` or the evaluator default. For other legacy fuzzy
expressions, domain analysis may set `isDiscrete_` and the `lb_`/`rb_` endpoints around zero;
continuous conditions use the runtime smoothing width.

## Constant Condition Processor

`ConstCondProcessor_` in `dal-cpp/dal/script/visitor/constcondprocessor.hpp`
mutates the AST by collapsing always-true and always-false condition and if
nodes into their concrete outcomes. Both exact and fuzzy model-aware
preparation skip this visitor; legacy preprocessing invokes it after enabled
domain processing:

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
Conditions containing eager `AND` or `OR`, and their enclosing IF nodes, are
retained so folding cannot remove the required operand evaluation. Model-aware
preparation collects and resolves history across all syntax branches before
constant metadata or compilation; every required fixing in a nonexpired
product must be supplied even if its branch will not execute.

Constant arithmetic metadata belongs to the separate `ConstProcessor_` pass,
which runs in both exact and
fuzzy model-aware preparation. It marks literal/history-only expressions using
ordinary double operations, keeps script parameters live, and treats variables
assigned inside an IF as nonconstant. The exact compiler may emit a constant
boolean for a constant expression using `== 0`, `> 0`, or `>= 0`; it still
emits the IF and both supplied branches. This does not use tolerance-domain
truth flags or remove branches from the prepared AST.

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
(`lb_`, `rb_`) — set by legacy `DomainProcessor_` for discrete sub-expressions — the
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

Model-aware preparation fixes exact/fuzzy mode, default epsilon, and compiled
selection before optimization. Its order is:

1. Parse and collect every syntactic observation, partition events, index
   variables, and validate settings.
2. Validate model capabilities, build the observation/payment sample plan, and
   allocate and initialize the model. Resolve and seal required history before
   any branch pruning. Wholly expired products take the separate zero path.
3. Replay hard history once into double initial state. `ConstProcessor_`
   separately tracks historical constants and parameter dependencies, ignoring
   settled `PAYS` accumulation, to seed future constant metadata.
4. In both exact and fuzzy modes, retain parsed future arithmetic and branches
   and skip `DomainProcessor_` and `ConstCondProcessor_`. Parsed fuzzy
   comparisons keep continuous kernels with their default or explicit widths.
   Compute IF nesting and affected-variable metadata on the final control flow.
5. Finalize future constant metadata from the historical dependency state.
   Literal/history-only arithmetic may fold; `SCALE * H` remains active when
   SCALE is a script parameter, even though both current values are known.
6. If compiled execution was requested, build hard historical and exact/fuzzy
   future streams. Compilation errors occur before worker submission.

For legacy scripts without named observations, after indexing and past replay,
the enabled domain passes run in this order:

1. **`DomainProcessor_`** — computes variable/expression domains and sets
   `alwaysTrue_`/`alwaysFalse_` flags (and `isDiscrete_` bounds if fuzzy).
2. **`ConstCondProcessor_`** — collapses always-true/false nodes into concrete
   forms, pruning dead branches.
3. **Final metadata** — rebuild IF metadata after folding and finalize constants.
4. **Evaluation** — `Evaluator_<T>` (exact) or `FuzzyEvaluator_<T>` (smoothed)
   walks the simplified tree to produce numeric results.

The shared AST nodes and visitor base classes live in
`dal-cpp/dal/script/visitor/`; the preprocessor lives in `dal-cpp/dal/script/`
and is deliberately separate.

## Simulation and Evaluation

`MCSimulation<T_>` (`dal-cpp/dal/script/simulation.hpp`) drives Monte Carlo
valuation of a `ScriptProduct_`. It has two instantiations: `T_ = double`
(value-only) and `T_ = AAD::Number_` (pathwise-adjoint, see
[Automatic Adjoint Differentiation](aad.md)). Named observations use the
[core double/tree entry](#core-doubletree-fixing-valuation) or the
[core AAD/tree entry](#core-aadtree-fixing-valuation), with compiled execution
available for both. Nonexpired AAD through a raw `ScriptProduct_` requires no
past events; model-aware preparation supplies historical replay in both modes.

### RNG and Brownian Bridge

`CreateRNG` selects the underlying generator from a method string — `sobol`
(Sobol low-discrepancy), `mrg32` (MRG32k32a pseudo-random), or `irn` (industrial
pseudo-random) — sized to the model's simulation dimension. When the Brownian
bridge flag is set, the generator is wrapped in a `BrownianBridge_` so the draw
order reconstructs the path from coarse to fine maturities rather than in
chronological order; this often reduces variance for path-dependent payoffs.
For a zero-dimensional model, the name is still validated but neither an RNG
nor a bridge is constructed.

### Batching and Thread Pool

Paths are divided with an effective batch size
`min(8192, ceil(nPaths / nThreads))` and submitted to
`ThreadPool_::GetInstance()`. Batch sizes, offsets, and counts use `size_t`, and
the planner rejects a zero thread count without performing division. This keeps
multiple useful tasks for small simulations while capping large batches at
`8192`. Each thread owns its own RNG, Gaussian vector, scenario
(`Scenario_<T_>`), and evaluator state, so the per-path work is lock-free. For
value-only simulation, each worker constructs its buffers on first use and
reuses them for subsequent batches in the same request. The caller's state is
prepared before task submission, preserving input validation even for an empty
native simulation. All states are destroyed after the request's tasks drain;
they are never reused across valuation requests. An
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

1. Activates the worker's tape and registers model parameters, constant
   variables, and a zero input (`InitModel4ParallelAAD`). It starts a recording,
   initializes the model, rebuilds any prepared historical seed, then marks.
2. Per path: rewinds to the mark, generates the path, restores initial state
   (the typed seed for prepared AAD), and evaluates the AST with a
   `FuzzyEvaluator_<AAD::Number_>` (or compiled `EvalState_<AAD::Number_>`).
   It creates or reuses a path-local payoff root via `AAD::PayoffRoot`, as
   described under [recording lifetime](#historical-state-and-recording-lifetime),
   seeds its adjoint to $1$, and propagates back to the mark.
3. After the batch: propagates from mark to start, harvests per-parameter
   adjoints, and divides by total `nPaths` once. Worker reduction sums these
   normalized contributions without a second division.

The result is a `SimResults_` carrying the aggregated payoff and a risk vector
labelled by model parameter and constant variable.

### Tree-Walk and Compiled Evaluation

Legacy and model-aware prepared scripts have two execution modes. The tree-walk
mode evaluates the preprocessed AST directly with `Evaluator_<double>` for value-only paths and
`FuzzyEvaluator_<AAD::Number_>` for AAD paths. The compiled mode first lowers
the same AST into flat per-event streams and then evaluates those streams with
`EvalState_<T>`.

`compiled` selects the evaluator implementation. It does not change prefetch,
model capabilities, today policy, error semantics, or cashflow rules. The default is
`compiled = false` for both `MCSimulation<double>` and
`MCSimulation<AAD::Number_>`, preserving the historical tree-walk behavior.
Callers can pass `compiled = true` to opt into the stream evaluator. Both modes
are required to produce the same payoff and risk numbers, aside from normal
floating-point association noise.

For the raw legacy path, `ScriptProduct_::Compile(fuzzy)` is `const`, but it
requires that `PreProcess(...)` has already run. Preprocessing indexes variables, seeds past
event values, folds domain-provable constant conditions, and finalizes constant
metadata. `Compile` returns a separate `ScriptCompiled_` artifact instead of
storing streams on the product, so the AST remains reusable by tree-walk
evaluation after compilation. `MCSimulation` compiles once per valuation on the
main thread before dispatching path batches; a missing `PreProcess` is therefore
reported as a normal exception rather than being hidden inside worker tasks.

Artifacts built by the compiler without an observation plan or historical
mode use the legacy instruction dispatcher and address scenario samples by
event index. Prepared artifacts use the dispatcher that supports
`LoadObservation` and `Discard`. Directly constructed opcode streams also use
that dispatcher: constructing a stream does not certify it as legacy, and an
observation load without a plan still raises `PreparationRequired`.

For named observations, use `PreparedScript_::Compile(fuzzy)` after requesting
compiled model-aware preparation. It returns the already built future artifact
and rejects a different fuzzy mode. The prepared object also retains a hard
historical artifact for typed AAD replay. Both streams share const ownership of
the same sealed `ObservationPlan_`; a copied compiled artifact retains that
plan even after the prepared object is destroyed.

`LoadObservation` carries an observation ID and calls `ObservationPlan_::Read`
to select a known value or a retained scenario output. Future event numeraires
use `EventToSample()` rather than assuming event `i` means scenario `i`.
Historical bytecode always uses hard comparisons and emits `Discard` for
`PAYS`, consuming the expression without accumulating settled cash. The
historical stream needs no model sample. Constant arithmetic may be inlined,
but live parameter inputs and their historical-state dependencies are retained.

Products with `EXERCISE` compile into a recording variant of the prepared
stream: `PAYS` lowers to `LsmcPays`/`LsmcPaysConst` and `EXERCISE` to
`LsmcExercise`, appended opcodes that live in the prepared dispatch tail. The
LSMC driver installs a per-thread `LsmcSinks_` recorder into `EvalState_`
before evaluation; the recording opcodes append the raw payment per `PAYS`
event and the per-exercise-date regressor, exercise value, and condition
indicator to the driver's storage rows, while the script-state arithmetic
mirrors the plain pay opcodes statement for statement. Fuzzy (AAD) preparation
lowers the same statements to `LsmcFuzzyPays`/`LsmcFuzzyPaysConst`/
`LsmcFuzzyExercise`, which record into the driver's typed `LsmcFuzzySinks_`
rows instead — conditions are degrees on the double stack there, so
`LsmcFuzzyExercise` pops its condition from the double stack while the hard
opcode reads the boolean stack. Recording opcodes are dispatched only by the
driver's evaluation chain — including the branches of `If`/`IfElse`/`FuzzyIf`
statements — and the plain compiled dispatcher rejects them as unknown
opcodes.

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
  model-aware preparation uses the continuous forms, while legacy domain
  preprocessing can select discrete bounds;
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
behavior, opcode reachability, and early-exercise LSMC parity between the
tree-walk and compiled recording engines (named cases plus the randomized
generator in `dal-cpp/tests/script/test_compile_parity_fuzz.cpp`). The benchmark target
`dal-cpp/benchmarks/script_mc_perf` compares `compiled=false` and
`compiled=true` runs across simple and schedule-heavy products for both
`double` and `AAD::Number_`; it times the Monte Carlo path loop rather than the
parser front-end.

Named coverage in `dal-cpp/tests/script/test_exact_folding.cpp`,
`dal-cpp/tests/script/test_fuzzy_arithmetic.cpp`,
`dal-cpp/tests/script/test_compiled_observations.cpp`,
`dal-cpp/tests/script/test_past_replay.cpp`, and
`dal-cpp/tests/script/test_observation_simulation.cpp` also checks independent
analytic path values and risks, retained fixing samples, typed batch lifetimes,
smooth-point finite differences, strict prefetch, and preparation failures.
Exact regressions cover adjacent floating-point comparisons, signed small
factors, and nested future state across events with independent price/risk
oracles. Fuzzy arithmetic regressions cover finite signed tiny literal, known
and computed divisors, live parameter denominators, and nested fractional
state across events with default and explicit smoothing widths. Analytic
values and risks and unoptimized fuzzy-double central differences provide
references independent of shared preparation optimizations.
The isolated `dal-cpp/test-support/test_script_observation_allocations.cpp`
fixture measures C++ allocation requests during repeated native-double exact
and fuzzy tree/compiled evaluation after state/scenario construction. It does
so over 8193 evaluations, including the first, with ordinary and aligned
allocation positive controls. Both fixture cases are registered with CTest.
Double evaluator state restores its existing passive initial-value vector;
active evaluator state retains a separate typed historical seed. The probe
does not measure AAD tape allocations or arbitrary `malloc` calls.
`ObservationPlan_::Read` uses fixed indexed
access; this complexity property is established by source inspection rather
than a dynamic count of every load. These checks are not runtime benchmarks.

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

This product observes the model spot through the legacy zero-argument
`SPOT()` form, which remains compatible unchanged for future-only observations
like this one; the [named form](#spot-compatibility-and-the-fix-boundary) is
`FIX(index)`. The `BARRIER:0.1` suffix on each comparison sets the node's
`eps_` field, which the fuzzy evaluator consumes as the smoothing width for
that condition. Running
`product.Debug(out)` after the constructor walks the AST and writes the dated,
parsed event listing; this example leaves variable indices unresolved.
Downstream valuation calls `PreProcess`, which partitions and indexes the
product, before evaluation or `Compile`. The Monte Carlo driver is the free
function `MCSimulation<T_>` in
`dal-cpp/dal/script/simulation.hpp`, templated on `double` for value-only runs
and on `AAD::Number_` for pathwise-adjoint runs.

## Product Archive and Diagnostics

Product archives, contract descriptions, and valuation explanations are
independently versioned formats:

| Format                   | Entry or archive type                                   | Meaning                                                                 |
|--------------------------|---------------------------------------------------------|-------------------------------------------------------------------------|
| Archive v1 reader        | `ScriptProductData_v1`                                  | Read name/dates/events with an empty default index.                     |
| Archive v2 reader/writer | `ScriptProductData_v2`                                  | Persist the contract with optional `default_index`.                     |
| Contract JSON /2         | `DescribeScriptProduct(product)`                        | Describe all parsed syntax without market or valuation-date access.     |
| Valuation JSON /1        | `ExplainScriptValuation(product, modelData, valuation)` | Prepare once with default price settings and report the resulting plan. |
| Legacy debug JSON /1     | `DebugScriptProductJson(product)`                       | Date-partitioned legacy AST; reject FIX and nonempty defaults.          |

### Contract Archive

The default writer emits `ScriptProductData_v2`. Its fields are `name`,
`dates`, `events`, and optional `default_index`, alongside the archive type
tag. Omitted or empty `default_index` means an unbound product. The v1 reader
remains available; reading v1 and writing it again produces v2. No public v1
export is provided.

Archive roundtrips preserve the original product name, event cells, unexpanded
script text, default-index spelling, and unquoted FIX literals, including FX
direction and EQ delivery identity. They do not rewrite script text to
canonical names. The archive contains no valuation date, fixing values or
sources, model index, slots, sample/observation plan, prepared AST, bytecode,
or AAD seed. Explaining or repricing a product does not change its archive.

The v1 reader does not imply that an old binary can read v2 or execute FIX
text. Source-call compatibility also provides no ABI guarantee: rebuild
consumers and bindings against matching core/public headers and libraries.

### Describe: Pure Contract Syntax

`DescribeScriptProduct` emits `dal.script-product/2`. It parses a fresh copy
of the original table, indexes variables, and validates index identities and
the date constraint `F <= E`. It reads no historical data or global evaluation
date, constructs no model, and submits no workers. It retains all dated events
and syntax branches without a `phase` or `evaluation_date` field. A successful
description does not establish historical availability, model support, or
pricing validity: empty, definitions-only, and no-PAYS products can be
described, but cannot be valued or explained.

The JSON includes:

- `name`, `default_index` with `original` and `canonical`, and `input_rows`
  containing the original `row`, `date_or_definition`, and `text`.
- `variables`, `constants`, and `payoff_index` (null when no payoff variable
  exists: EXERCISE-only products pass the payoff gate — `EXERCISE` is a payoff —
  but have no `PAYS` receiver, so the sentinel path keeps the null key).
- `events` with `event_id`, `date`, preprocessing `origins`, and AST
  `statements`; node IDs `n0`, `n1`, … follow preorder over all events.
- Observation leaves with `kind:"fix"` / `type:"Fix"` or
  `kind:"spot"` / `type:"Spot"`, `index_original`, `index_canonical`,
  `fixing_date_mode` (`Explicit` or `EventDate`), `fixing_date_literal`,
  resolved `fixing_date`, exact-midnight `fixing_time`, and `source`.

An omitted FIX date has a null `fixing_date_literal` and resolves to the event
date for display; the AST retains the omission. Bound SPOT uses the product
default identity; unbound SPOT has null index fields. An empty product default
has `original:""` and `canonical:null`. JSON strings use standard quotes and
escaping even though index literals inside the script have no quotes.

`source` carries `row`, `offset`, `line`, `column`, and `event_date`. Rows refer
to the original input table; offsets and line/column positions refer to the
expanded event text. Rows, lines, and columns are one-based; offsets and
event/statement IDs are zero-based. FIX positions identify the index literal;
SPOT positions identify the function token. Event `origins` connect expanded
statements to their input rows and offsets. Identity and source fields come
from parser/preprocessor metadata, including macro and schedule expansion.

### Explain: One Valuation Preparation

`ExplainScriptValuation` emits `dal.script-valuation/1` using the same input,
date, model, and historical preparation as pricing. Its optional valuation
settings default exactly as in [public C++ settings](#fields-and-defaults).
Simulation settings are fixed to default price semantics: Sobol, no bridge,
no AAD, smoothing `0.01`, and tree execution. There is no path-count or
simulation-settings argument. Explain can construct, allocate, and initialize
a model, resolve history, and replay hard past state; it generates no paths,
starts no workers, and creates no active AAD recording.

The JSON reports `evaluation_date`, `today_fixing` (`Model` or
`RequireHistorical`), `source_kind` (`GlobalSnapshot` or `ExplicitSnapshot`),
effective `simulation`, `all_expired`, `observation_mode`, and the model's
`spot` index in `model_bindings`. The source kind describes the selected historical input,
even if no history is needed; each request separately has source `Historical`
or `Model`.

`requests` retain plan order and zero-based `request_id`. Every request has its
canonical identity, exact timestamp, resolution, and all `uses`, including
original index spelling, fixing-date mode/literal, observation type, source,
event/statement IDs, and the matching Describe node ID. A historical request's
`history_value_id` selects its final virtual-index fixing value; it need not
equal the request ID. Its `model_slot` is null. A model request has
`model_slot:{sample_id,output_id}`, with null history ID and value; Explain
does not invent a simulated fixing.

`sample_dates`, `timeline`, `sample_definitions`, `event_to_sample`,
`live_events`, and `numeraire_requests` come from the actual prepared plan.
`live_events` maps all-event IDs to future-event indices before indexing
`event_to_sample`. Payment numeraires use payment-event samples, independently
of observation samples. These are requests, not generated numeraire values.
Legacy unbound SPOT can have no named requests and uses `observation_mode:Legacy`.
Wholly expired requests have `resolution:SkippedExpired` and null values/slots.
They perform no historical I/O; sample and numeraire arrays are empty. Missing live history
throws a preparation error rather than producing a successful null value.

Every Explain and Value call prepares independently; Explain supplies no
implicit cache or reusable public prepared handle. To compare the same market,
pass the same explicit date and snapshot and preserve product/model inputs.
With global history, an update between calls can change Value after Explain.
Explain describes default price preparation, not AAD branch behavior or a
caller-selected compiled plan. It neither returns a price nor adds diagnostic
keys to Value's numeric map.

### Simulation Diagnostic: Full Valuation with Exercise Statistics

`ExplainScriptSimulation(product, modelData, numPath, valuation, simulation)`
emits `dal.script-simulation/1`. Unlike the valuation Explain it explicitly
runs a full three-phase double valuation — path generation, worker
parallelism, and the exercise regressions — so its cost is the cost of a
simulation, and that cost is part of its contract. The optional valuation and
simulation settings default exactly as in
[public C++ settings](#fields-and-defaults); `simulation.compiled_` selects the
tree-walk or compiled engine for the run, while `simulation.enable_aad_` is
rejected with `UnsupportedExecutionMode` (the diagnostic runs the double
valuation path only).

The JSON reports `evaluation_date`, the effective `simulation` echo
(`rsg`, `use_bb`, `enable_aad`, `smooth`, `compiled`, `lsmc_basis_degree`),
the explicit `n_paths`, and `exercise_events`. Products without `EXERCISE`
return an empty `exercise_events` array, never an omitted key. Each exercise
event carries `event_id`, `date`, the effective `basis_degree`
(`0` marks the degenerate constant basis), `regressor_index` (the canonical
index name of the model-sourced regressor — the same join space as Explain's
`requests[].index_canonical`; null when the product has no model index, such
as an unbound `SPOT()` regressor), `num_cond_true_paths`, `num_coefficients`
with the frozen `coefficients` on the z-normalized monomial basis, the
`degenerate` flag with its PascalCase `degenerate_reason`
(`ConditionPathsBelowMin`, `SigmaFloor`, or `IllConditioned`), and the
path-set `exercise_rate`. A degenerate day reports the constant fit, so its
coefficients remain meaningful; a truly low exercise rate is distinguishable
from a degenerate regression by the flag.

Python exposes the same diagnostic as
`ScriptSimulation_Explain(product, modelData, num_path, *, valuation=None, simulation=None)`
and Excel as `SCRIPTSIMULATION.EXPLAIN(product, modelData, n_paths, [valuation],
[simulation])`, both returning the identical JSON contract (as a dictionary and
as concatenated text chunks respectively). The runnable
[`dal-cpp/examples/american_put_mc/`](../../dal-cpp/examples/american_put_mc/)
prices the European-limit, two-date Bermudan, and weekly-exercise puts off the
same engine and prints the diagnostic.

## Product Debug Outputs

For supported inputs, the legacy text, JSON /1, and tree wrappers capture the
global evaluation date `D` once before
parsing a fresh private product copy, then explicitly partitions it. Events
before `D` are past; events on or after `D` are future. Repeating a dump after
changing the evaluation date uses a new copy and the new date. Dumping does
not call `PreProcess`, fold branches, read fixings, set up a model, or submit
workers.

`ScriptProduct_::Debug` writes the legacy dump: any indexed variable table followed by
each future event's statements as indented s-expressions with labels like
`MAX(`, `VAR[x,-1,0.000000]`, `IF[FIRSTELSE=-1]`.

Two further dumps render the same AST for other consumers. All three are
produced from the debug IR that `Debugger_` builds. JSON and tree dumps include
both event containers, tagged with their phase. The lower-level core dump
methods render the product's existing containers; direct core callers use
`PartitionEvents(D)` when they need date-based phases. Public wrappers perform
that partitioning automatically on their private copies.

- **JSON** — `ScriptProduct_::DebugJson` writes a compact, deterministic
  document with schema `dal.script-product/1`, meant for machine consumption
  (the `Product_DebugJson` binding, and web front ends). The document carries
  the variable and constant tables plus the payoff slot whenever the product
  has been through `IndexVariables`; every AST node becomes an object with a
  unique pre-order `id` and a stable snake_case `kind` (`add`, `sub`, `mul`,
  `div`, `pow`, `log`, `exp`, `sqrt`, `max`, `min`, `neg`, `uplus`, `eq0`,
  `gt0`, `ge0`, `and`, `or`, `not`, `assign`, `pays`, `if`, `spot`, `const`,
  `var`, `const_var`, `true`, `false`, `collect`). Structured kinds get
  structured fields — `condition`/`then`/`else` for `if`, `target`/`value`
  for `assign` and `pays`, `mode`/`eps` (continuous) or `mode`/`lb`/`rb`
  (discrete) for comparisons — and everything else uses `children`. Numbers
  use the shortest decimal form that round-trips; a continuous comparison
  reports the smoothing width in `eps`, where `-1` means the script did not
  set one (the default smoothing factor applies). Products containing any
  `FIX` raise `DebugSchemaUnsupported` before writing any JSON, including FIX
  in past events or dead branches, because schema `/1` cannot represent the
  complete named observation. The error directs callers to
  `DescribeScriptProduct` and `dal.script-product/2`.
- **Tree** — `ScriptProduct_::DebugTree(ost, ascii, width)` writes a
  human-friendly rendering: statements collapse to inline math while they fit
  the width budget (`width` caps the line width, default 125), for example
  `call ⇐ max(spot() − STRIKE, 0)`, with comparisons folded back from
  `GTZERO(SUB(a, b))` to `a > b` and smoothing shown as `⟨ε=0.1⟩` or
  `⟨[lb, rb]⟩`. Anything wider expands into box-drawing branches; `if`
  branches are marked `▶` (then) and `▷` (else). `ascii = true` switches
  every symbol to a pure-ASCII set for constrained consoles — Unicode output
  needs a UTF-8 terminal (on Windows, Windows Terminal or `chcp 65001`).

For `NodeFix_`, the legacy text and ASCII/Unicode tree dumps preserve the raw
index and optional fixing date as `FIX(index)` or `FIX(index, date)`, including
inside nested expressions. The tree keeps the complete leaf even when it
exceeds the requested width. These human-readable dumps inspect the contract;
they do not establish that the product can be valued.

`DebugScriptProduct` renders only the private copy's live events and skips
variable indexing, preserving unresolved variable indices and omitting the
variable-table header. It raises `empty script product description` when no
live event remains. `DebugScriptProductJson` and `DebugScriptProductTree` run
`IndexVariables` on their partitioned copies, so indices, variable/constant
tables, and the payoff slot are resolved while both phases and raw branches
remain inspectable. Public JSON keeps schema `/1` and rejects any FIX or
nonempty product default index, even an unused default on an empty product,
with `DebugSchemaUnsupported` and a Describe /2 migration hint. Archive v1
and debug /1 are independent versions. The Python
surface is `Product_Debug(product)`, `Product_DebugJson(product)`, and
`Product_DebugTree(product, ascii=False, width=125)`; Excel's `PRODUCT.DEBUG`
uses the legacy text wrapper.

## See Also

- [Automatic Adjoint Differentiation](aad.md) — the reverse-mode machinery that
  fuzzy evaluation feeds, enabling pathwise Greeks through discontinuous payoffs.
- [Public C++ settings example](../../dal-public/examples/script_settings.cpp)
  — historical fixing, explicit date/snapshot, compiled AAD, Describe, and Explain.
- [Python FIX settings](../../dal-python/README.md#historical-and-future-fix)
  — keyword-only settings, midnight snapshots, copies, diagnostics, and a complete example.
- [Excel FIX settings](../excel-script-settings.md)
  — immutable handles, two-column ranges, JSON chunks, and the runnable workbook.
- [`dal-cpp/examples/script/`](../../dal-cpp/examples/script/) — runnable example
  of the full pipeline: events table parsing, preprocessing, domain analysis,
  condition folding, and evaluation.
- [`dal-cpp/examples/script_tree/`](../../dal-cpp/examples/script_tree/) and
  [`dal-python/examples/008.script_tree.py`](../../dal-python/examples/008.script_tree.py)
  — runnable demos of the Unicode/ASCII tree dump at several width budgets.
