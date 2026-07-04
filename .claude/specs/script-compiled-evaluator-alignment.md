# Script Engine — Compiled Evaluator Alignment

Plan-only document (no implementation yet). Covers: aligning the compiled
bytecode evaluator with the tree-walk evaluator, the parity test harness that
makes that alignment regression-proof, and the path to making `compiled=true`
a safe default. This is candidate #1 in
`.claude/specs/perf-enhancement-candidates.md` (Tier 1, "use the compiled
evaluator by default"), whose risk note — "compiled and tree-walk evaluators
must produce identical numbers … Needs a parity test harness (none exists
today)" — motivates this work.

## Background

The DAL script engine (`dal-cpp/dal/script/`) prices scripted payoffs via Monte
Carlo. It has two evaluators over the same AST:

- **Tree-walk** (production default): `MCSimulation<T_>` defaults `compiled=false`;
  `product.Evaluate(path, eval)` runs a per-node virtual-dispatch visitor walk
  (`event.hpp:118-127`). For the AAD specialization the evaluator is
  `FuzzyEvaluator_<Number_>` (`simulation.hpp:239`); for `<double>` it is
  `Evaluator_<double>`.
- **Compiled** (opt-in): `product.Compile()` lowers the AST to a flat integer
  opcode stream; `EvaluateCompiled` (`compiler.hpp:362-585`) is a `switch` over
  opcodes using `thread_local static` stacks (`compiler.hpp:383/385`), zero
  per-path allocation.

Only `examples/uoc_compiled/uoc_compiled.cpp` calls `Compile()` today. The
public API (`dal-public/src/value.cpp:35,41`) hardcodes `compiled=false`, and
neither Python nor Excel exposes the flag. The compiled evaluator is
library-internal.

**Goal:** make the compiled evaluator numerically aligned with the tree-walk;
land a parity harness so the alignment cannot silently regress; then flip
`compiled=true` to the default where it is safe, capturing the evaluator-
throughput win already measured by `dal-cpp/benchmarks/script_mc_perf`.

### Dispatch matrix

| Template | `compiled=false` | `compiled=true` |
|---|---|---|
| `<double>` | `Evaluator_<double>` (`simulation.hpp:140`) — hard-step | `EvalState_<double>` + `EvaluateCompiled` — hard-step |
| `<Number_>` (AAD) | `FuzzyEvaluator_<Number_>` (`simulation.hpp:239`) — smoothed | `EvalState_<Number_>` + `EvaluateCompiled` — **no fuzzy** |

The `<double>` path is hard-step on both evaluators; the `<Number_>` path is
the fuzzy-vs-no-fuzzy asymmetry.

## Defect register (severity-ranked)

A read-only review of `simulation.hpp`, `event.hpp`, `compiler.hpp`,
`evaluator.hpp`, `fuzzy.hpp`, `node.hpp`, and the script test tree surfaced the
following. File:line anchors are project-relative. Rows are ordered by
severity; IDs are stable (rows #11-#13 were added in review and slotted by
severity).

| # | Defect | Path | Severity | Anchor |
|---|---|---|---|---|
| 1 | **`IfElse` true-branch `bStack` underflow.** `EvalCompiled` resets the `thread_local` `bStack` on entry; the `IfElse` case runs the true branch via a recursive `EvalCompiled` which wipes the parent's `bStack`, then the parent does `bStack.Pop()` — underflowing `sp_` (unchecked in `StaticStack_::Pop`). A later condition `Push` then writes into the call frame's locals. Every `IfElse` whose condition is **true** underflows. Suspected until the parity test confirms at runtime. | both | 🔴 memory-safety / correctness | `compiler.hpp:521-530`, `:383-386` |
| 2 | **`&&`/`\|\|` lose short-circuit.** `Compiler_` emits both RHS substreams unconditionally; tree-walk short-circuits. Side-effecting RHS (div0, `log(neg)`) executes in compiled → throw/NaN where tree-walk skips; AAD records dead RHS adjoints. | both | 🔴 correctness | `compiler.hpp:252-262`, cases `:543/549` |
| 3 | **Silent AAD smoothing drop.** `MCSimulation<Number_>` `compiled=true` ignores `eps`/`maxNestedIfs`, runs un-smoothed → zero/nonsense greeks through conditionals, no diagnostic. Smoking gun: `uoc_compiled.cpp:120-122` passes `eps=0.01` that is thrown on the floor. | `<Number_>` | 🔴 silent wrongness | `simulation.hpp:231-237` |
| 11 | **Const variables (`NodeConstVar_`) dead in compiled path — const-var greeks silently zero.** Tree-walk `Visit(NodeConstVar_)` reads `constVariables_[index_]` (`evaluator.hpp:255-258`), which on the AAD path is put on tape (`simulation.hpp:47-48`) so const-var adjoints flow. The compiler instead bakes `constVal_` into `constStream_` as a plain double (`compiler.hpp:308-312`) and the `ConstVar` opcode falls through to `Const` (`compiler.hpp:485-489`) — `EvalCompiled` never reads `state.constVariables_`. Worse, `NodeConstVar_` is born `isConst_=true`, so `VisitBinary` const-folds whole sub-expressions containing const vars away. Consequences: `accumulateConstVarRisks(evalState.ConstVarVals())` (`simulation.hpp:237`) reads adjoints of variables never used → **const-var greeks always exactly zero** (the plan's own example prints `dP/dB`/`dP/dK` as silent zeros in the compiled AAD run); the mutable `EvalState_::ConstVarVals()` seam (`compiler.hpp:98-103`) is a no-op in compiled mode where the identical seam on `Evaluator_` is live. Distinct root cause from #3: Phase 5a masks the AAD consequence but 5b cannot ship without making `ConstVar` a real state read. | both (silent-zero greeks on `<Number_>`) | 🔴 silent wrongness | `compiler.hpp:308-312,485-489`, `evaluator.hpp:255-258`, `simulation.hpp:47-48,237` |
| 4 | **`NodeNot_` / `visitNot` typo.** `fuzzy.hpp:210` lower-case `v` does not override CRTP `Visit`; falls through to base which Pops an empty `bStack_`. Any `!=` in a fuzzy-evaluated product breaks. Examples dodge it by using `>`/`>=`. | `<Number_>` fuzzy | 🟠 latent | `fuzzy.hpp:210`, `nodebase.hpp:64` |
| 6 | **UB if `Compile()` skipped.** `MCSimulation` does not call `Compile()`; if a caller passes `compiled=true` without it, `EvaluateCompiled` loops `events_.size()` times indexing the **empty** `nodeStreams_[i]` (`event.hpp:129-141`) — out-of-bounds UB in release, no assertion. | both | 🟠 footgun | `simulation.hpp`, `event.hpp:129-141`, `event.cpp:127-155` |
| 5 | **`SupEqual` const-fold** uses `x > -EPSILON` where runtime uses `x >= 0` — a tiny-negative const folds to `True` vs `False`. | both | 🟡 edge | `compiler.hpp:246-248` |
| 12 | **Const-folded conditions vs fuzzy.** `Compiler_::VisitCondition` folds constant conditions to hard `True`/`False` (`compiler.hpp:227-248`); `FuzzyEvaluator_` would push `CSpr/BFly(const, eps)` — a *fractional* dt when `\|const\| < eps/2` — while the compiled stream has already crisped it. Latent until Phase 5b (compiled fuzzy). | `<Number_>` fuzzy | 🟡 latent (5b blocker) | `compiler.hpp:227-248`, `fuzzy.hpp:174-207` |
| 13 | **`Compile()` mutates the shared AST.** `Compile()` runs `ConstProcess()` first (`event.cpp:127-129`), setting `isConst_`/`constVal_` across the same AST the tree-walk evaluator uses. Benign today (tree-walk ignores those flags), but "build once, run both evaluators" has an ordering dependency, and Phase 4's "call `Compile()` in `MCSimulation`" hits an API wrinkle: `MCSimulation` takes `const ScriptProduct_&` while `Compile()` is non-const. | both | 🟡 harness / API | `event.cpp:127-129`, `simulation.hpp:61-68` |
| 7 | **No fuzzy opcode.** `Smooth = 31` is declared in the enum but never emitted and has no `case`. Structural cause of #3. | `<Number_>` | (root of #3) | `compiler.hpp:142`, switch ends `:582` |
| 8 | **Dead `dataStream`.** Declared, stored, threaded, recursively forwarded, never read. | — | 🟢 cleanup | `compiler.hpp:157,165,367,526`, `event.hpp:51,138`, `event.cpp:153` |
| 9 | **`MG_NodeType_enum` generated but unused.** The hand-written `enum NodeType_` (`compiler.hpp:110-151`) is what runs; the Machinist output drifts silently. | — | 🟢 drift | `compiler.hpp:30-73,110-151`, `dal-cpp/dal/auto/MG_NodeType_enum.*` |
| 10 | Minor: scratch `T_ x,y,z,t` (`compiler.hpp:379`) adds AAD tape noise; public API hardcodes `compiled=false`, Python/Excel don't expose the flag. **Correction:** `NodeCollect_` is *not* vestigial — `ConstCondProcessor_` produces it when collapsing always-true/false conditions (`constcondprocessor.hpp:63,73`; pinned by `tests/script/test_constcondprocessor.cpp`), and `PreProcess` runs `ConstCondProcess()` whenever `fuzzy \|\| !skip_domain` (`event.cpp:86-90`) — including the `<double>` compiled config `PreProcess(false, false)`. Compiled products routinely contain `NodeCollect_` at `Compile()` time; both `Compiler_` and `Evaluator_` handle it only via the *accidental* `ConstVisitor_` catch-all traversal (`visitor.hpp:52-59`). Must not be pruned; pin with a parity test instead. | — | 🟢 | `compiler.hpp:379`, `dal-public/src/value.cpp:35,41`, `constcondprocessor.hpp:63,73` |

## Alignment plan (phased, TDD-ordered)

Each phase lands tests first (red), then turns them green. Nothing is gated on
a later phase.

**Phase 0 — Land the parity harness red** at
`dal-cpp/tests/script/test_compile_parity.cpp` (see "Parity test-suite design"
below). On landing it confirms the `<double>` failures (#1, #2, #5), the
`<Number_>` conditional failures (#3, #4), and the const-var risk failure
(#11). That red set is the worklist.

**Phase 1 — Fix `<double>` correctness bugs** (unblocks any default flip):
- **#1 IfElse underflow**: the recursive `EvalCompiled` must not clobber the
  parent frame. Preferred fix: extract an `EvalCompiledRange(first, last)`
  helper that skips the `Reset()` prologue and dispatch the true branch through
  it. Verify with the consecutive-both-true `IfElse` test.
- **#2 short-circuit**: add jump opcodes so the RHS substream is skipped when
  the LHS decides the boolean, mirroring the tree-walk (`evaluator.hpp:187-201`).
  Alternative (document both-evaluate + forbid side-effecting RHS) is less
  aligned; short-circuit is preferred.
- **#5 SupEqual const-fold**: `x > -EPSILON` → `x >= 0.0` to match the runtime
  predicate.
- **#11 ConstVar state read**: make `ConstVar` a real opcode that reads
  `state.constVariables_[idx]` instead of falling through to `Const`, and stop
  `Compiler_` const-folding through `NodeConstVar_` (its value is mutable via
  `ConstVarVals()` and must stay live on the AAD tape). This restores the
  compiled `EvalState_::ConstVarVals()` seam and un-zeros const-var greeks.

**Phase 2 — Fix `NodeNot_` (#4)** in `fuzzy.hpp:210` (rename `visitNot` →
`Visit`, operate on `fuzzyStack_`), with the `!=` fuzzy regression test.

**Phase 3 — Guards against silent failure:**
- **#6**: THROW if `EvaluateCompiled` runs on an un-compiled product (empty
  `nodeStreams_`), or if the `compiled=true` branch of `MCSimulation` is entered
  without a prior `Compile()`.
- **#3**: THROW in `MCSimulation<Number_>` (`simulation.hpp:231`) when
  `compiled=true` AND the product needs fuzzy (`eps > 0` or any `CompNode_` with
  `isDiscrete_/eps_` set, or any `NodeIf_` with non-trivial `affectedVars_`).

**Phase 4 — Flip the `<double>` default to `compiled=true`** in `simulation.hpp`
(now safe: Phase 1 gives `<double>` byte-parity modulo FMA; Phase 3 prevents
misuse). Call `product.Compile()` once in the `<double>` path (idempotent). Two
explicit work items from #13:
- **API wrinkle:** `MCSimulation` takes `const ScriptProduct_&` while
  `Compile()` is non-const — either make `Compile()` const with `mutable`-free
  lazy streams (streams populated into a separate compiled artifact), have
  `Compile()` return a compiled artifact passed alongside the product, or
  require callers to pre-compile and keep the Phase 3 THROW as the guard.
  Decide before flipping the default.
- **AST mutation ordering:** `Compile()` runs `ConstProcess()` which mutates
  the shared AST (`isConst_`/`constVal_`); pin with the
  `TestParity_TreeWalkUnchangedAfterCompile` test so tree-walk results are
  provably identical before and after `Compile()`.
Gate on the full gcc/clang/msvc × AAD-backend matrix. Leave `<Number_>` default
`false`.

**Phase 5 — AAD path safety** (recommend 5a now, 5b deferred):
- **5a:** force `FuzzyEvaluator_<Number_>` on the AAD path regardless of the
  `compiled` flag — the flag only selects evaluators on `<double>`. Removes the
  silent-wrongness footgun (#3) at the source; AAD stays correct. Cost: no AAD
  compiled speedup yet (acceptable — AAD/bumping is the smaller workload).
- **5b (deferred):** implement compiled fuzzy — new opcodes `CSpr`/`BFly`
  (mirroring `fuzzy.hpp:15-51`), `FuzzyAnd`/`FuzzyOr`/`FuzzyNot`, and a
  dt-blend `FuzzyIf` over `affectedVars_` (eval-true → `SaveAffected` →
  eval-false → `BlendAffected(dt)`). Land last, behind Phase 0's red tests, only
  if profiling justifies. Wire the dead `Smooth=31` to a real opcode or replace
  it. **5b blockers from the register:** #11 must be fixed first (compiled
  fuzzy cannot ship with const vars baked into `constStream_`), and #12 must be
  resolved — `Compiler_::VisitCondition`'s hard `True`/`False` const-fold
  diverges from `FuzzyEvaluator_`'s fractional `CSpr/BFly(const, eps)` when
  `|const| < eps/2`; either suppress the fold on the fuzzy path or fold to the
  fuzzy dt.

**Phase 6 — Cleanup + public API:** delete dead `dataStream` (#8); remove or
regenerate-and-use `MG_NodeType_enum` (#9). **Do not prune `NodeCollect_`** —
per corrected #10 it is produced by `ConstCondProcessor_` and reaches
`Compile()` under `PreProcess(false, false)`; instead give `Compiler_` and
`Evaluator_` explicit `Visit(NodeCollect_)` overloads (visit-children) so the
handling is deliberate rather than the catch-all accident, and pin with
`TestParity_ConstCondProcessed_Collect`. Expose
`compiled` through `dal-public/src/value.{hpp,cpp}` and the Python/Excel
bindings **only after 5b** lands (otherwise there is nothing for consumers to
opt into).

## Parity test-suite design ("un-breakable")

**Harness** — `dal-cpp/tests/script/test_compile_parity.cpp`, suite
`ScriptCompiledParityTest`, `TEST(...)` (no fixtures), `ASSERT_*` (fail fast).
Templated helper `<class T_>`: build the product once; seed Sobol identically;
run `MCSimulation<T_>(..., compiled=false)` and `MCSimulation<T_>(..., compiled=true)`
on the **same seed and path count**; compare **per-path payoffs first**
(strictest), then aggregated PV/greeks.

Tolerance: `ASSERT_NEAR(compiled, treeWalk, 1e-8)` — **not** `ASSERT_DOUBLE_EQ`.
The two evaluators are different arithmetic paths; gcc-13/14 FMA contraction
diverges by an ULP even when algebraically identical (the gcc-FMA tolerance
gotcha). `1e-8` is the floor; gate on the gcc CI matrix, not a local clang
build.

**Preprocess-config axis.** Several defects are config-dependent, so the
harness must run the parity set under each config actually used in the wild:
`PreProcess(false, false)` (the `<double>` compiled config — domain +
const-cond processing, produces `NodeCollect_`) and `PreProcess(true, true)`
(the AAD config). Each named test states which config(s) it runs under; the
conditional-surface tests run under both.

**Compile-ordering rule.** Because `Compile()` mutates the shared AST (#13),
the harness always runs the tree-walk evaluation both *before* and *after*
`Compile()` on at least one test (`TestParity_TreeWalkUnchangedAfterCompile`)
and otherwise fixes the order tree-walk-first.

**Opcode-coverage assertion.** The named tests alone don't guarantee the
const-variant opcodes (`AddConst`, `ConstSub`, `SubConst`, `DivConst`,
`ConstDiv`, `PowConst`, `ConstPow`, `Max2Const`, `Min2Const`, `AssignConst`,
`PaysConst`, `UMinus`, `Exp`, …) are ever emitted. Add a harness check that the
union of compiled `NodeStream()`s across the suite covers every reachable
opcode, so a future opcode cannot ship untested.

**Per-defect regression cases** (each pins a register row — these are the
"un-breakable" anchors):

| Test | Pins | Today |
|---|---|---|
| `TestParity_IfElse_ConsecutiveBothTrue` | #1 underflow | 🔴 |
| `TestParity_IfElse_NestedInTrueBranch` | #1 recursion depth × shared `thread_local` stacks | 🔴 |
| `TestParity_And_OrShortCircuit_SideEffectingRHS` | #2 short-circuit (RHS `log(neg)`) | 🔴 |
| `TestParity_SupEqual_ConstFold_TinyNegative` | #5 const-fold edge | 🔴 |
| `TestParity_Number_NotEqual_Fuzzy` | #4 `visitNot` typo | 🔴 |
| `TestParity_CompileNotCalled_GuardsThrow` | #3 / #6 guards — assert the *guard* only; today's behavior is OOB UB, never observe pre-fix behavior (ASAN CI would flag it) | 🔴 |
| `TestParity_Number_ConstVarRisks` | #11 — macro const vars (STRIKE/BARRIER style), assert compiled AAD const-var adjoints match tree-walk (zero today) | 🔴 |
| `TestParity_ConstVarVals_MutationSeam` | #11 — mutate `ConstVarVals()` post-build on both paths (or explicitly document the seam as compile-time-frozen) | 🔴 |
| `TestParity_ConstCondProcessed_Collect` | #10 — `PreProcess(false, false)` collapses an always-true `if` into `NodeCollect_`; run both evaluators | 🔴 |
| `TestParity_TreeWalkUnchangedAfterCompile` | #13 — tree-walk PV identical before vs after `Compile()` | ✅ (guard) |
| `TestParity_PastEvents_InitSeeding` | init parity — evaluation date mid-schedule, `PastEvaluate()` seeds both `Evaluator_::Init` and `EvalState_::Init` | ✅ (guard) |
| `TestParity_Number_ConstCondition_WithinEps` | #12 — constant condition with `\|expr\| < eps/2` (5b gate) | 🔴 at 5b |
| `TestParity_VanillaCall` | arithmetic-only baseline | ✅ |
| `TestParity_Barrier_Sup` / `_Digital_Equal` / `_NestedIf` / `_MultiEvent` | conditional surface (`<double>` green after Phase 1; `<Number_>` red until 5b) | mixed |
| `TestGolden_FixedBarrier_PV_Risks` | drift catcher (fixed PV + spot/vega/barrier greeks at 1e-6) | ✅ once aligned |

**Property/fuzz layer** — `test_compile_parity_fuzz.cpp`: a seeded, deterministic
random-product generator (fixed-seed PRNG object — no `volatile`/`mutable`,
both banned here) emits random operator trees, random `if`/`IfElse` structure,
random `:eps` values, random schedules, **random const variables (macro
events) and random past dates** (without these the fuzz layer structurally
cannot hit the #11 const-var or past-event surfaces); each generated product
runs through both evaluators on the same seed; assert parity at `1e-8`; ~500
products/run; print the seed on failure. This covers combinations no human
authored — the actual "un-breakable" guarantee.

**CI gating** — register the parity tests into `dal_cpp_tests` so the full
**gcc-13/14, clang-18/19, msvc × aadet/adept/codipack/xad** matrix runs them on
every PR (the project gate). gcc is the bar for FMA tolerance.

## Open decisions (resolve before implementation)

1. **Phase 1 IfElse fix shape** — `EvalCompiledRange` helper (preferred, fixes
   root cause) vs. save-and-pop-before-recursion. Confirmable empirically once
   the red test exists.
2. **AAD path** — Phase 5a (force `FuzzyEvaluator_`, defer compiled-fuzzy —
   recommended, low risk) vs. push straight to Phase 5b (implement compiled
   fuzzy; larger project, only path to an AAD-compiled speedup).
3. **Short-circuit (#2)** — real short-circuit via jump opcodes (recommended)
   vs. document both-evaluate and forbid/guard side-effecting RHS.
4. **`Compile()` const-API shape (#13, Phase 4)** — const `Compile()` producing
   a separate compiled artifact vs. caller-must-pre-compile with the Phase 3
   THROW as the guard.

The most urgent single action is **#1 (IfElse underflow)** — memory-safety,
independent of everything else.
