# DAL-200 F3 implementation handoff

## Opcode decomposition and bounded tree experiments, 2026-09-13

This is the current DAL-216 implementation handoff. The older sections below are
retained historical evidence. This attempt addresses the concrete interpreter
size/complexity findings locally; two tree optimizations failed to demonstrate
a repeatable improvement and remain unpublished. This is an implementation
attempt awaiting independent acceptance, not F3 completion.

### Source, authority and scope

- Starting published SHA/tree: `e3a92c1df6a6d87294916e5e777eaafbbe332ff8` /
  `22295a412d86a1912ce9f9bdb42f30b6f17ddd21`.
- Explicit F2 comparison baseline: `ec8b0072fbf70dab814a543edc625c8e0bf77efa`.
- Tested production SHA/tree: `c0ac441ea851d96d70b694c3c7f4e5aae7996f1c` /
  `e3ba5b4a1c0e735646b555064b8360897548b9bf`.
- Publication adds only this implementation report. The attachment preface,
  publication identity JSON and final issue comment identify that complete
  successor SHA/tree without claiming it is a newly tested production change.
- Existing draft PR: <https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369>;
  publish branch `feature/dal-200-eq-observation-slots`. F2, every earlier
  correctness repair, and DAL-218's `fb071974877ebf12c1aa7c5faf252b5798e23691`
  documentation successor remain ancestors.

The current DAL-216 description, parent fixed F3 semantics/matrix, original
Codacy annotations and retained DAL-222/E3 evidence control this attempt.
Repository role/style/test/performance/publication guidance, relevant API note,
independent critique/testing record, script/AAD methodology, interpreter/tree
source and nearby opcode/parity/fuzz tests were read before implementation.
No new spec/API/critic stage or public-design deviation was needed.

Published production scope is only `dal-cpp/dal/script/visitor/compiler.hpp`.
The report is the only other repository change. Experiments touched
`dal-cpp/dal/script/visitor/evaluator.hpp` in an isolated worktree; it is restored
byte-for-byte. No sample/evaluator member layout, virtual interface, public
signature/default/error string, opcode value, model/RNG/batch/thread behavior,
dependency, generated source, benchmark inventory/workload/threshold, build/CI
policy or published docs/CHANGELOG changes.

### Implementation and static RED/GREEN

The interpreter keeps one outer call across all event streams per path. Small
forced-inline internal helpers separate arithmetic, load/store, hard control,
unary and fuzzy operations and return the next event-local instruction position.
The event driver resets both operand stacks once for each top-level event;
recursive branches still call the unchanged single-event `EvalCompiled` entry
with `reset=false`. Variable initialization, persistent path state, branch
bounds, fuzzy blends and default epsilon behavior are preserved. Min/max share
the same comparator/update helper without changing equality or NaN comparisons.

Original exact-head check `103711581879` reports 306 lines (limit 200) and CCN69
(limit 8). The authenticated GitHub annotations are retained. Lizard **1.23.0**
reproduces **306 NLOC / CCN69** on that original driver, with 310 physical lines.
The final event driver is **13 lines / CCN5**; all new helpers are at most
**45 physical lines / CCN8**. The whole changed header passes the requested
limits. This is local metric evidence, not a claim that successor Codacy ran.

```text
lizard -C 8 -L 200 -w experiments/original-compiler.hpp
  RED: exit 1, EvalCompiledEvents 306 NLOC / CCN69
lizard -C 8 -L 200 -w perf-candidate/dal-cpp/dal/script/visitor/compiler.hpp
  GREEN: exit 0, no findings at those limits
python3 experiments/run.py build candidate
python3 experiments/run.py test candidate
  initial candidate: exit 0, 425/425 semantic controls
```

This is a behavior-preserving optimization/refactor, so the authorized static
and unchanged performance failures serve as RED. No artificial functional
failure, new assertion mirroring implementation or weakened test was introduced.
All arithmetic/opcode, model validation, finite-output/payoff, owned snapshot,
custom virtual generation, task draining, live-parameter-before-history,
retained FIX/payment/no-lookahead and lifetime controls remain unchanged.

### Causal experiments and retained negatives

`hypotheses.md` records the prediction, mechanism, invariants and decision rule
before each edit. `diagnostic-summary.md/json` lists all **37 comparisons / 1,480
fresh processes**, including every same-case control. Every process exited 0
and passed the original workload validation. Each pair uses two rounds of ten
alternating samples, reduced by each round's minimum. Isolated and unchanged
suite-prefix histories are distinct; neither replaces the full gate.

- **E1, compiler decomposition:** isolated versus independently rebuilt current
  **+3.6755/+3.7859%**, versus F2 **-6.8544/-8.4952%**. Prefix versus current
  **+1.2578/+7.4616%**, versus F2 **-8.8339/-14.0652%**. It preserves an
  F2-relative compiled gain, with a measurable dispatch cost versus current.
  Prefix F2 A/A **+10.6088/-2.9098%** is retained; current A/A is
  **-0.5311/+0.9512%**, candidate A/A **+3.6172/-0.8726%**. No unchanged
  noisy full gate was rerun to obtain a preferred outcome.
- **E2, plan-pointer-first SPOT predicate:** all 425 controls pass. Isolated
  versus E1 **+1.3873/-1.2697%**, prefix **-3.7890/+2.6560%**. No repeatable
  effect beyond controls; rejected and retained as `e2-tree.patch`.
- **E3, one checked push for both SPOT branches:** original predicate order;
  all 425 controls pass. Isolated versus current **+0.5812/+0.1150%**, prefix
  **+0.3005/+0.6284%**. Prefix versus F2 fails **+8.2099/+5.3098%**, with its
  own A/A **-0.7703/-2.9853%**. Sub-percent differences versus E1 are smaller
  than controls; rejected and retained as `e3-tree.patch`.

The tree regression is history-sensitive: a fresh isolated F2/current pair is
**-0.4106/-0.2699%**, while the rejected E3 prefix pair reproduces a failure.
That contrast does not waive the earlier full gate. The final source is E1
alone. The prior selected-worker-state, owned-snapshot and core-specialization
negative experiments were read and not repeated.

### Fresh correctness and inventory correction

- Canonical `NUM_CORES=12 DAL_NUM_THREADS=4 bash ./build_linux.sh`: exit 0,
  **1,693/1,693** CTest tests. Canonical compiler is GCC **15.2.0**.
- Final focused unique coverage: **426 native, 421 Adept, 420 CoDiPack**.
  Native is the main 425 plus the one distinct clone/live-risk control;
  the alternative-backend selections explicitly include all three direct
  model tests in one filter. Existing 1e-8 IRN shared-path oracles are unchanged.
- Native Clang **21.1.8** address+undefined sanitizer with
  `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
  `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`: **426/426**, exit 0.
  All **296 built translation units** have both sanitizer compile flags;
  the test link and library instrumentation symbols are verified. Adept and
  CoDiPack have Release coverage, not new sanitizer clearance.
- Independently built F2 and final Python packages: **402/402 each**, exit 0,
  including the optional native quote-risk test fixture.
- `dal_check_generated`, `check_docs.py`, changed-line clang-format **22.1.5**
  (zero replacements), whitespace, ancestor and scope checks pass.

**Correction to the preceding report:** 428/423/422/428 was the sum of main and
supplemental executions, not the unique union. `*Domain*` already matched
`ModelTest.TestConstructorParameterDomains` and
`ModelTest.TestAadLiveParameterDomains`; only
`ModelTest.TestAadValidLiveParametersAndCloneRisks` was additional. Reconciliation
of the actual predecessor RUN records gives **426/421/420/426 unique tests**.
`predecessor-inventory-correction.json` preserves counts and overlaps. Coverage
is retained; no test is removed or claimed as a new case twice.

Exact commands, filters, exits and raw logs are in `commands.jsonl`,
`run.py`, `verify_final.py`, and the verification logs. The final benchmark
build main filter and the one additional native control have separate logs.

### Complete final-source performance gates

`python3 experiments/full_gates.py` collected every nested result once using
**10 samples per round, two rounds and +4%**. Its driver continues after failed
gates so that all controls are retained; the nested exits below are authoritative.

- **Python F2-to-final: exit 1, 89/90.** Compiled barrier improves
  **-13.2564/-13.2849%**, 25.228538→21.884134 ms and
  26.086587→22.620999 ms. Tree barrier fails **+4.1893/+8.9971%**,
  34.855458→36.315643 ms and 33.771036→36.809436 ms.
- **F2 complete A/A: exit 1, 89/90.** The unrelated
  `mc.vanilla.double.compiled` fails **+4.07/+4.88%**. Barrier compiled
  controls are **-2.5158/-0.3509%**, tree **-0.1668/+1.0432%**.
- **Final complete A/A: exit 0, 90/90.** Barrier compiled
  **-1.5083/+1.8573%**, tree **-0.5093/-0.8197%**.
- **Nine-target native gate: exit 0, 63/63.** Sobol precise-opt-in/fast
  ratio is **8.42x**, below its unchanged 10x cap.
- Informational `script_mc_perf`: exit 0; it is not a tenth native gate target.

The tree failure remains a failed acceptance gate. The unrelated F2 A/A
failure does not waive it, and the smaller same-case barrier controls support
the much larger compiled improvement. The evidence retains **10,800 full
Python observations**, **360 native gate process outputs**, every one of the
90 Python cases with its own controls in `full-gate-ledger.md/json`, and every
native case in the unchanged gate report. Earlier failures remain visible.


All timing builds are separate Release/static/native-AADET worktrees/build
roots with GCC/G++ **14.3.0**, CMake **4.2.3**, CPython **3.13.9**, benchmarks
explicitly ON and the same Python interpreter. Actual compile databases retain
the existing core `-ffp-contract=fast -march=native`, public `value.cpp`
`-O3 -DNDEBUG -std=c++17 -fPIC` and Python binding LTO differences identically
on both sides. There are no compiler-policy changes.

Timing preserves the actual Python product/model construction, preprocessing,
simulation and result-conversion boundary, 100,000 paths for the barrier case,
four DAL threads, affinity 0/2/4/6, correctness invocation and two warmups,
unchanged full workloads, one thread in numerical helper libraries and fixed
Python hash seed. No build/test overlaps timing. WSL2/i9-13900HX remains a noisy
host; software timings and the retained predecessor instruction profiles do not
establish native hardware cycle/cache/stall attribution.

`final-identity` and `verification-identities` retain immutable source/submodule,
actual compile command/object, library/test/package and tool/environment
identities. The compact archive contains raw text/JSON, patches and source
snapshots; it deliberately omits large binary packages. Rebuild instructions
and exact revisions remain explicit.

### Remaining work and delivery limits

No tree performance repair earned publication. The next concrete tree
experiment is to isolate the named observation read and checked push in an
internal helper, inspect whether the compiler shrinks the legacy virtual thunk
without a layout change, then use the same controlled timing protocol. It was
not executed in this bounded attempt. Lack of PMU was not the stopping reason;
the attempted source changes failed their publication criteria.

Earlier full Python failures (including e3a92c1d 89/90, tree
+7.5199/+5.6684%), controls and rejected experiments remain historical evidence.
Original GCC TLS sanitizer failures and their no-DAL/no-Adept attribution are
not erased by current Clang results. No Windows/XAD, Python-sanitizer or
alternative-backend-sanitizer acceptance is claimed.

PR #369 stays draft and unmerged; DAL-216 is delivered in review for the
attempt/report. Parent coordinates existing DAL-217 independent verification,
DAL-218 successor documentation applicability and mandatory DAL-219 review.
There is no F3/#357 closure or F4 activation. Published documentation is
byte-identical to the starting head; successor applicability remains the
documentation specialist's decision. Online successor checks are captured once
after push and reported with their actual pending/failing status.


## Compiled interpreter performance repair attempt, 2026-09-13

This is the current DAL-216 handoff. The following sections retain earlier
repairs and their original limitations. The current candidate substantially
improves the compiled-barrier case locally; **complete F3 performance acceptance
remains open** because the fresh Python gate still fails tree-barrier pricing.

### Source and implementation

- Start: `fb071974877ebf12c1aa7c5faf252b5798e23691`, preserving DAL-218's
  nine-line `docs/methodology/aad.md` successor and every preceding correctness fix.
- F2 baseline: `ec8b0072fbf70dab814a543edc625c8e0bf77efa`.
- First tested candidate: `76dcdb043b5d5e2677b288074f15ff93bd61f261`, tree
  `a9207fddd4238dc6f1f3b69ade55853354e3f1b5`. Isolated and prefix comparisons
  used these source bytes, initially recorded as the start SHA plus the E2 patch.
- Final tested code: `6d7ae4f05a0975cf5b7b05ebb7e6635a982a7180`, tree
  `280a9f3c78ec09490c9eb0f49e65781080d5ca00`. This successor adds exactly one
  required clang-format line break, with identical tokens. Final canonical,
  backend/sanitizer, Python and complete performance evidence is reconciled to it.
- Publication adds this report only. The final issue comment and attached
  publication identity record the complete published successor SHA/tree.

The production change is confined to `dal-cpp/dal/script/event.hpp` and
`dal-cpp/dal/script/visitor/compiler.hpp`. `ScriptCompiled_::Evaluate` passes
the event streams to one internal interpreter call per path. Previously it
entered the large interpreter separately for every event; DAL-222 measured
5.4 million such entries for the 100,000-path compiled-barrier workload.

An internal `CompiledEventView_` lets the multi-event driver and the existing
single-event `EvalCompiled` API share one switch. Each top-level event still
resets both operand stacks, recursive branches preserve them, jump positions
remain event-local, and variables initialize once per path. The 289-line
opcode body is identical after indentation normalization (SHA256
`70d39014d659c405ec6b7a19816f8e5558066b65e21acbe4b7ca618fcba8221a`). Most of
the diff is indentation. Lizard reports complexity 69 before and after, with
complexity 1 for the compatibility wrapper.

No public signature/default/error message, opcode, sample layout, path count,
RNG sequence, thread/batch policy, dependency, binding, generated source,
benchmark workload/threshold, or published documentation/CHANGELOG changes.
All model/path/payoff checks, owned snapshots, custom virtual generators,
task draining, historical/FIX/payment semantics, named-mode rejections and
the AAD/preprocessor/exception lifetime repairs remain in place. The only
other published file is this active implementation report.

### Hypotheses, RED and candidate evidence

The issue explicitly uses the unchanged performance case as RED for this
behavior-preserving work; no artificial functional failure or relaxed oracle
was introduced. Fresh F2-to-current compiled-barrier timing reproduces
**+6.2499/+5.7951%**, exceeding the unchanged +4% rule in both rounds.

Each experiment had a written prediction, isolated source edit, invariant list
and decision criterion before execution; `experiments/hypotheses.md` retains
the ledger and `experiments/commands.jsonl` the build/test commands and exits.

| Experiment/comparison                                     | Round 1   | Round 2   | Outcome  |
|-----------------------------------------------------------|-----------|-----------|----------|
| E1 selected evaluator state, current to candidate         | +2.9560%  | +4.1255%  | Reject   |
| E1 selected evaluator state, F2 to candidate              | +7.5777%  | +9.4642%  | Reject   |
| E2 event-loop interpreter, current to candidate, isolated | -14.2017% | -17.5122% | Continue |
| E2 event-loop interpreter, F2 to candidate, isolated      | -10.3358% | -10.8531% | Continue |
| E2 current to candidate, unchanged suite prefix           | -15.4198% | -17.1235% | Continue |
| E2 F2 to candidate, unchanged suite prefix                | -16.2723% | -13.8955% | Continue |

E1 replaces the two worker evaluator members with one variant in an isolated
`simulation.hpp`; its 425 functional controls pass, but timing gets worse.
It remains an unpublished patch and is excluded from E2. Reducing worker
storage alone did not repair this case. The earlier owned-snapshot and
explicit-specialization experiments were read and were not repeated.

All 600 initial diagnostic processes pass the original workload validation.
Each comparison/control uses two rounds of ten fresh processes per side,
alternating order and reducing to each round's minimum. Prefix mode replays
the ten preceding full-size cases with the unchanged harness. Current and
E2 same-case controls are smaller than the improvement. F2 prefix A/A has a
retained one-round +6.0398% excursion (+2.7867% in round two); this WSL host
is noisy, and no precise hardware cache/stall attribution is claimed.

### Correctness and final-source verification

- Fresh canonical `NUM_CORES=12 DAL_NUM_THREADS=4 bash ./build_linux.sh`:
  **1,693/1,693**, repeated successfully after the formatting successor.
- Main focused selection: native **425/425**, Adept **420/420**, CoDiPack
  **419/419**, including compiled opcode/parity/fuzz, F3/legacy/IRN, model
  snapshots/invalid outputs, submission/path-failure draining, AAD tape and
  exception controls. Existing numerical tolerances remain unchanged.
- Fresh Clang address+undefined with `detect_leaks=1:halt_on_error=1` and
  `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`: **425/425** in that
  selection. All 296 built translation units have both sanitizer compile
  flags; library symbols confirm instrumentation. This is native-backend
  sanitizer coverage; Adept and CoDiPack have Release coverage in this run.
- Three additional direct model-domain/valid-mutation/clone-risk controls
  are recorded separately because their names are outside the main filter.
  They pass **3/3** on native, Adept, CoDiPack and the combined sanitizer
  build, covering all eight prior live-parameter repair tests. The union of
  the two focused selections is **428 native, 423 Adept, 422 CoDiPack and
  428 sanitizer tests**, all passing. The canonical full run also includes them.
- Both isolated Python builds pass **402/402**, including the unchanged
  benchmark workload tests. The optional native quote-risk fixture was built.
- Generated-source, repository documentation, changed-line clang-format,
  whitespace and source-scope checks pass. Generated files and submodule
  pointers are unchanged. The first formatting check identified one line
  break; the final check reports zero replacements in both changed files.

Exact filters, outputs, object/source/library hashes, compile databases and
CMake caches are attached. The initial name-based selections on F2/current
were 341/408 tests; their 17 compiler controls were subsequently run separately.
These inventories are explicit and are not represented as the independently
reported DAL-217 inventory merely because some counts coincide.

### Complete performance gates

All performance builds are isolated Release/static/native-AADET builds with
GCC/G++ 14.3.0, CMake 4.2.3 and CPython 3.13.9. Every configured translation
unit's actual flags are retained and comparable across revisions. Core uses
`-ffp-contract=fast -march=native`; public `value.cpp` uses `-O3 -DNDEBUG
-std=c++17 -fPIC` without those core flags; Python binding units use LTO.
This existing difference was preserved. Benchmark and Python options are ON.

Measurements retain the actual Python construction/preprocessing/simulation/
result-conversion boundary, 100,000 paths for the failing case, four DAL
threads, affinity 0/2/4/6 and the original correctness invocation plus two
warmups. Numerical helper libraries use one thread and Python hash seed is
fixed. No timing loop overlaps a build or test. WSL2/i9-13900HX hardware
counters were not used; software timings are not hardware stall measurements.

Final-source commands are supplied by `experiments/full_gates.py`; it invokes
the unchanged repository scripts with **10 samples, two rounds, +4%** and
uses their unchanged collection/evaluation functions for complete A/A controls.

- **Python paired gate: exit 1, 89/90.** Compiled barrier improves
  **-15.0785/-16.1249%** versus F2: 28.252112 to 23.992119 ms and 27.375592
  to 22.961305 ms. Barrier comparison Greeks at 65,536 paths improve
  **-11.2684/-16.1243%**.
- Remaining failure: `mc.barrier.double.tree`, **+7.5199/+5.6684%**:
  37.253277 to 40.054682 ms and 35.556332 to 37.571823 ms. This failed
  full gate remains failed regardless of the follow-up attribution experiment.
- **F2 full A/A: exit 0, 90/90.** Compiled barrier +0.8155/-1.2616%; tree
  barrier -0.1426/+1.1676%.
- **Candidate full A/A: exit 1, 89/90.** The failing control is unrelated
  `provenance.generic.n10`, +5.7591/+5.4533%, retained as host instability.
  Compiled barrier +3.5548/-3.3340%; tree barrier +1.8643/+2.1779%. An
  unrelated control cannot waive the tree failure or invalidate the much
  larger compiled improvement by itself.
- **Nine-target native gate: exit 0, 63/63** with the unchanged policy.
  Informational `script_mc_perf` also exits 0 and is not a tenth gate target.

`experiments/full-gate-ledger.md` lists every one of the 90 Python cases with
both rounds and its own controls; the native gate's original summary lists
all 63 cases. Raw reports retain 10,800 full-suite Python observations and
360 native process outputs, plus the focused diagnostic evidence.

The remaining tree case received one bounded, source-unchanged comparison of
F2/current/candidate with its original suite prefix (E3). Unmodified current
already regresses **+7.2982/+6.7962%** versus F2. Candidate versus current
is **-4.1131/-6.2820%**; current A/A is -1.8911/+1.6585% and candidate A/A
+2.4502/+0.7019%. All 160 processes pass validation. This supports an inherited
F3 tree cost rather than a new candidate regression in this experiment; it does
not establish a tree-code repair or waive the failed complete gate. There are
**760 total new diagnostic processes** across E1/E2/E3; no unchanged full gate
was retried. The next concrete performance work is independent verification
of this successor and focused attribution of the remaining legacy tree-SPOT
observation dispatch, routed by the parent because `evaluator.hpp` is outside
this repair's production scope.

### Delivery limits and continuation

This is a measured compiled-path repair candidate with an honestly unresolved
full performance gate, not F3 acceptance. PR #369 stays draft. Parent routes
the successor through DAL-217 independent correctness/performance checks,
DAL-218's documentation decision and mandatory DAL-219 re-review. No merge,
F3/#357 closure, F4 activation or independent-review waiver is authorized here.

Earlier CI c59ae919 remains 63/63 native and 89/90 Python, with compiled
barrier +4.4929/+4.5110%; earlier local full Python remains 86/90
failed/inconclusive. DAL-222 profiles and DAL-217's 1f2cefd4 correctness
results are inherited evidence, not new candidate tests. Original GCC TLS
sanitizer failures remain recorded; current Clang success does not clear them.
No new XAD, Windows/XLL or Python-sanitizer acceptance is claimed.

The archive includes rejected E1 and original E2 patches, all raw evidence,
source/compile/object identities and final build-tree packages/binaries.
The pre-format E2 module was rebuilt for the formatting successor; its original
hash/source/raw timings are retained, while the packaged E2 binary is the final
one used by the complete gates. Publication identities are recorded separately
so a report-only successor is not confused with a new tested implementation.

---


## Live-parameter validation repair, 2026-09-13

This section records the preceding DAL-216 handoff. All following repair sections are
historical evidence. The current correction restores the approved model-domain
contract before historical I/O; it does not resolve the independent performance
gate or accept F3.

Starting head/tree: `c59ae9198daa944414458692cdf0a2a66ad36feb` /
`bda0567046b8ab34376319ef9e35a43a31a33154`.
Tested production/test commit: `ef3ac954e4fd0e7ca305e5c111feda0a4c1adde5`, tree
`4541a502fcbb8eb02d0ee957ebca588d794e69c4`.
Publication adds only this implementation report. The final DAL-216 comment and
attached published identity record the complete delivered SHA/tree. The existing
draft PR is <https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369>;
the publish branch remains `feature/dal-200-eq-observation-slots`.

### Authority, scope and implementation

The active DAL-216 assignment, parent DAL-200 fixed F3 design/matrix and continuing
publication authorization, DAL-219 current review and unchanged reproduction
driver, existing API/implementation notes, script-engine/AAD methodology, and
repository implementation/style/test/Git guidance control this repair. No new
spec/API/critic stage is required by the handoff. The dedicated checkout and live
PR were clean and matched the starting identity. F2 ancestry is retained; no
unrelated recovery work was applied or discarded.

Exactly six repository files change from the starting head:

- `dal-cpp/dal/model/blackscholes.hpp`: a private `ValidateParameters()` shares
  the existing constructor checks with the beginning of `Init()`.
- `dal-cpp/dal/model/dupire.hpp`: private scalar and volatility validators share
  existing checks between construction and `Init()`. Keeping them separate
  preserves constructor ordering around immutable grid checks.
- `dal-cpp/dal/math/aad/tape.hpp`: only the explanatory comment above
  `EnsureGradientCapacity()` is shortened; no AAD executable code changes.
- `dal-cpp/tests/script/test_observation_simulation.cpp`: five new tests and
  private helpers; every existing test and assertion remains unchanged.
- `dal-cpp/tests/model/test_live_parameters.cpp`: three new constructor,
  mutable AAD parameter and clone-risk tests.
- `.codex/artifacts/reviews/dal-200/implementation.md`: this handoff.

Current finite positive spot, finite nonnegative BS/local volatility, and finite
rate/dividend/repo domains are checked on every model initialization. Existing
`InvalidModelParameter` text, public signatures, parameter pointers, clone
behavior, defaults and supported modes are preserved. No data member or virtual
surface changes. The existing `Allocate`/`Init` calls already precede
`ResolveHistory`, so preparation/simulation do not need edits. There is no new
per-path parameter scan. Immutable grids are still validated at construction.
Finite negative rates/carry and zero volatility remain valid. Finite input that
later generates a non-finite random path retains the checked-path diagnostic.

The existing all-expired return still precedes allocation/initialization and
history. Directly supplied mutated models do not force unused setup on that
path. Owned BS snapshots, derived/custom generators, payoff validation, task
draining and previous AAD/preprocessor/exception lifetime repairs remain intact.
There is no design deviation or expansion beyond the assigned scope.

### Regression coverage

The original reviewer fixture is retained: D=2026-09-12, H=2026-09-11 midnight,
H=80, explicit `spot -> EQ[DAL196_TEST]`, payment H+current FIX. A direct negative
BS spot test checks the full intended spot diagnostic from `PrepareScript`.
The two adapter tables additionally execute **168** rejection cases: **60 BS**
and **108 Dupire**, crossing today/future payment dates with global/explicit
history. They cover spot zero/negative, negative BS volatility, each of four
local-volatility ordinates, and NaN/+Inf/-Inf in every writable parameter.
Each catches `InvalidModelParameter` specifically and asserts zero global
history reads, zero virtual fixing reads and zero submitted workers. Mutation
through cloned models also exercises the existing parameter-pointer rebinding.

Eight deterministic price controls use both adapters, today/future payments,
zero volatility and zero/negative finite rate/carry. The today oracle is
H80+spot124=204 at N=1; the future oracle is
`80*exp(-r*T)+124*exp(-q*T)`. Both adapters also retain the all-expired no-history,
no-allocation, no-initialization and no-worker behavior despite a mutated invalid
spot. Existing expired path/risk tests remain in the compatibility run.

Model tests check 30 invalid constructor combinations and valid zero-volatility,
negative-rate controls. They check **84** mutable AAD domain cases at t=0/t=1,
with no requested numeraire so incidental step/output checks cannot substitute
for the input-domain check. A separate valid nonzero-volatility mutation/clone
test checks independent parameter addresses and values, deterministic price,
delta, rate cancellation, dividend/repo risk and parallel vega on both adapters.
All eight new tests execute on native, Adept and CoDiPack. Existing MC tolerance
**1e-8** and all prior RNG/bridge/thread/path combinations are unchanged.

### RED, GREEN and final commands

Commands below run from the repository root unless stated otherwise. The raw
archive includes `run.py`, JSON argument/working-directory/return-code records
and complete stdout/stderr logs. Command names beginning `red-dupire-build`
include a corrected test-only compile error: `Date_` has no stream insertion,
so the trace now prints relative days. The earlier `red-dupire.log` accidentally
ran the stale one-test binary after that failed build and is **not** Dupire RED
evidence. `red-dupire-current.log` is the correctly rebuilt three-test execution.

```bash
cmake --preset=Release-linux -S . -B build/red -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/red --target dal_cpp_tests -j12
DAL_NUM_THREADS=4 build/red/dal-cpp/dal_cpp_tests --gtest_filter=ScriptObservationSimulationTest.TestInvalidLiveBlackScholesSpotBeforeHistory --gtest_fail_if_no_test_selected
DAL_NUM_THREADS=4 build/red/dal-cpp/dal_cpp_tests '--gtest_filter=ScriptObservationSimulationTest.TestInvalidLive*' --gtest_fail_if_no_test_selected
```

- **BS RED:** one new test against unchanged production, exit **1**, no expected
  `InvalidModelParameter` exception. After sharing BS validation, the same test
  passes **1/1**, exit **0**.
- **Dupire RED:** after adding the tables but before modifying Dupire, the rebuilt
  three-test selection passes the two BS tests and fails Dupire's negative spot
  setup case, exit **1**. After sharing Dupire validation, **3/3**, exit **0**.
- **Controls/refactor:** the expanded model/observation/legacy-AAD selection
  passes **100/100**. Two test-only helper extractions then reduce new test
  complexity without removing assertions; final verification below includes
  their rebuilt sources.

The unchanged reviewer driver SHA256 is
`05e42a7392ac4f0f1ce95bf530b70a976de377994e144e5324bf15b3f4b8497d`.
It was read before executing. From the repository root it was compiled with:

```bash
g++-15 -std=c++17 -O2 -I dal-cpp ../evidence/dal219/evidence/reviewer/mutated_model_probe.cpp build/red/dal-cpp/libdal_cpp.a -pthread -o ../evidence/live-parameters/probe-red
```

The GREEN compile uses identical arguments with output `probe-green` after the
library rebuild. From the workspace root, each executable is run separately
with `DAL_NUM_THREADS=4` and arguments `bs-spot`, `bs-vol`, `dupire-spot`,
`dupire-vol`, `positive`. RED exit codes are **1,1,2,1,0**: invalid models reach
history, with BS spot returning -43; the positive control returns 204. GREEN
exit codes are **0,0,0,0,0**: all four invalid cases report the appropriate
`InvalidModelParameter` with zero fixing reads, and the control retains 204
and one read. Repository assertions establish the specific error independently
of this probe's broad catch branch.

```bash
NUM_CORES=12 DAL_NUM_THREADS=4 bash ./build_linux.sh
cmake --preset=Release-linux -S . -B build/adept-live -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DDAL_USE_ADEPT_AAD=ON -DDAL_CPP_BUILD_EXAMPLES=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/adept-live --target dal_cpp_tests -j8
cmake --preset=Release-linux -S . -B build/codi-live -DDAL_USE_CODIPACK_AAD=ON -DDAL_CPP_BUILD_EXAMPLES=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/codi-live --target dal_cpp_tests -j8
```

The same focused filter runs with `DAL_NUM_THREADS=4` and
`--gtest_fail_if_no_test_selected` on each core binary:

```text
*Script*:*Domain*:*IFProcessor*:*Fuzzy*:*BlackScholes*:*Dupire*:*Observation*:*ModelBinding*:ModelTest.*:SimulationTest.*:PastEvaluatorTest.*:AADTapeTest.*:ExceptionTest.*:ThreadPoolTest.*
```

- Fresh canonical GNU C++ **15.2.0** native Release build: **1,693/1,693 CTest**,
  exit **0**, core/public/portable-Excel tests and normal examples. The canonical
  benchmark exclusion remains unchanged. There was no root `test_output.txt`;
  raw output is kept separately. After the test helper refactor the canonical
  script rebuilds the affected tests and again passes **1,693/1,693**, exit **0**.
- Final native focused run: **425/425**, 19 suites, exit **0**.
- Fresh Clang **21.1.8** Adept Release core build, followed by final affected-test
  rebuild: **420/420** focused tests, 19 suites, exit **0**.
- Fresh GNU C++ **15.2.0** CoDiPack Release core build, followed by final
  affected-test rebuild: **419/419** focused tests, 19 suites, exit **0**.
- Backend inventories and compile commands are attached; count differences
  reflect existing conditional tests, not a narrowed filter. Pinned dependency
  commits remain unchanged. All configurations use C++17, static DAL,
  native-architecture tuning off and no sanitizer instrumentation.

```bash
cmake --build build/Release-linux --target dal_check_generated -j8
python3 .github/scripts/check_docs.py
git clang-format --diff c59ae919 -- dal-cpp/dal/model/blackscholes.hpp dal-cpp/dal/model/dupire.hpp dal-cpp/dal/math/aad/tape.hpp dal-cpp/tests/script/test_observation_simulation.cpp
clang-format --dry-run --Werror dal-cpp/tests/model/test_live_parameters.cpp
git diff --check
```

All pass: generated files written **0**, docs **64** checked, no formatting or
whitespace drift. Lizard records new private validators at complexity **3/3/4**;
all new test/helper functions are at most 8. The existing Dupire constructor's
complexity is reduced to 15; no unrelated immutable-grid refactor is claimed.
Evidence captures final source, library and executable SHA256 hashes, source
tree/commit, dependency identities, test inventories, CMake caches and compile
commands. Final publication changes only this report from the tested commit.

### Documentation handoff and residual limits

The full explanation removed from the Adept source comment is preserved here
for DAL-218: Adept grows its gradient array inside `initialize_gradients()` while
`register_gradient()` can continue assigning indices up to `max_gradient_`.
A later recording window can therefore reference indices beyond allocated
storage. Growth must preserve accumulated adjoints and initialize the added tail
to zero. The source keeps a one-line rationale. Published methodology and
CHANGELOG decisions remain the doc writer's responsibility; neither is edited.

This repair has no new sanitizer run, Python build, Windows XLL execution, full
alternative-backend suite or XAD execution claim. The prior independent GCC TLS
sanitizer reduction still fails without DAL/Adept while matching Clang passes;
all original failed runs remain failures. The previous Clang sanitizer evidence
is historical, not a sanitizer clearance for this successor.

Performance remains independently unresolved: CI run **34742964925**, artifact
**10313730794**, passes nine C++ targets but Python **89/90**; compiled-barrier
**+4.4929/+4.5110%** exceeds the unchanged **+4%** threshold despite same-case A/A
controls within 1%. The local full gate **86/90** remains failed/inconclusive.
DAL-222 established no safe minimum repair. The required retained-package
isolated/suite-prefix experiment needs native Linux cycle/cache/branch events;
the available WSL daemon has no CPU event source. No repeat performance run,
threshold/inventory change, suppression or performance closure is claimed.

The PR remains draft. Current-head required checks, baseline reconciliation and
acceptance remain parent-owned. A single post-push CI snapshot is attached at
delivery; it is not a merge gate clearance. Parent must reconcile the successor
and route DAL-217 independent retesting, DAL-218 documentation/CHANGELOG decision,
then DAL-219 independent re-review. No merge, closing lines or F4 dispatch.

## Exception-stack ownership repair, 2026-09-13

The bounded DAL-216 utility repair is delivered for independent acceptance.
Ordinary exception formatting now releases its per-thread diagnostic storage
at thread exit even when there was no NOTE/NOTICE guard to pop. Exact message
formatting, context order, scope cleanup, thread isolation and explicit
push/pop behavior are preserved. All sections after this repair are historical
records and do not establish acceptance of this successor.

Starting published SHA: `254af2bad4bdc3b3af9d0e65b340de45b010c270`, tree
`5b8f6d8fd468ba73074ad5cd6f669d5a6e22d1e1`.
Tested production/test SHA: `d7d3e692115d91a7d8b9597b8e4f078d50d472e5`, tree
`4ddc0208a304cf865f9ad0a9c4a899e2ac0cfc09`.
Publication adds only this report to that tested commit. The final DAL-216
comment and attached publication identity record the exact delivered SHA.
Existing PR: <https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369>;
publish branch: `feature/dal-200-eq-observation-slots`, without force-push.

### Authority, design and changed files

The current DAL-216 exception-lifetime assignment supersedes the earlier
constant-condition assignment. The complete parent F3 design/matrix,
independent DAL-217 diagnostic critique and attachments, existing API/implementation
notes, script-engine methodology and repository role/style/test/publication
references were read before editing. The parent explicitly skips repeated
spec/API/critic roles because this repair preserves their contract. No business
question or new public design was needed.

The clean dedicated checkout and live PR initially matched the starting SHA.
Both published repairs, performance work and F2 ancestry remain intact:
`15ae4fd086f6f51dea135e30fdf2f3864915b53d`,
`63c134c096f2f3d2fd9865cbe5c52cfd5bb8e2dd`, and
`ec8b0072fbf70dab814a543edc625c8e0bf77efa`.
Historical unpublished recovery work was neither applied nor discarded.

Exactly three repository files change relative to the starting head:

- `dal-cpp/dal/utilities/exceptions.cpp`: replace the raw thread-local owner
  with `std::unique_ptr<Vector_<XStackInfo_>>`. Retain lazy allocation and
  reset when a pop empties the stack. Its thread-local destructor handles the
  previously unpaired allocation. Include grouping and the private helper
  parameter spelling follow repository style.
- `dal-cpp/tests/utilities/test_exceptions.cpp`: four focused regressions;
  all eight existing tests and assertions remain intact.
- `.codex/artifacts/reviews/dal-200/implementation.md`: this handoff.

Keeping the existing allocation/reset path avoids changing when an emptied
stack releases its capacity. Message construction and XStackInfo_ payload
formatting are untouched. No public header, signature, caller, simulation,
AAD adapter, dependency, generated source or build policy changed. There is
no design deviation, error-surface redesign or new execution-mode support.

The new tests cover an ordinary throw/catch followed by worker join; exact
outer/NOTICE/inner order; normal scope exit and exception unwinding; a bare
thread while both parent and another worker hold distinct live contexts; and
empty pop, manual push/pop and subsequent scoped reuse. Promise synchronization
keeps the isolation contexts overlapping. Assertions run after all workers join.
Expected complete messages explicitly preserve the existing GNU and MSVC
format branches; only the GNU branch was executed here.

### RED and GREEN evidence

Fresh native Clang ASan built unchanged production for the independent
one-thread probe. It exited **1** with **24 bytes in one allocation** at
XTheStack after the worker joined. No simulation or AAD backend was involved.
The newly added TestThrowOnJoinedThread was then compiled against that same
identified library before the production fix: its assertion passed, but
LeakSanitizer reported the same 24-byte leak and process exit **1**.

After the minimum ownership fix, a separate fresh native ASan build passed the
added test and exited **0**. Context/reuse tests and a small shared throwing
helper were then added while green. The final twelve utility tests were also
linked against the frozen, unchanged pre-repair library: **12/12 assertions
passed**, but **96 bytes in four allocations** leaked and the process exited
**1**. That control compiles current test sources only; it does not rebuild or
misidentify repaired production as the baseline. The original library hash is
checked against its earlier manifest in frozen-library-attribution.json.

With final repaired source, **12/12 utility tests pass and exit 0**. The same
standalone probe prints `exception caught=1; worker joined` and exits **0**.
Every sanitizer execution retains
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`. Assertion success is reported
separately from process-level sanitizer success throughout the raw logs.
F2 attribution remains the independent tester's established result; no
unnecessary F2 rebuild was performed in this turn.

### Full verification and actual instrumentation

- Canonical fresh `bash ./build_linux.sh`: GNU C++ **15.2.0**, native AADET,
  static Release, core/public/portable-Excel tests and normal examples;
  **1,685/1,685 CTest pass**, exit **0**, CTest 8.98 seconds. There was no
  existing root test_output.txt. The complete fresh log is attached.
- Focused native utility/script/domain/model/F3/AAD regressions:
  **401/401**, 19 suites, exit **0**. Shared-path MC assertions remain **1e-8**.
- Fresh Clang **21.1.8** Adept AddressSanitizer: the independent tester's
  exact unchanged **129-case inventory**, filter `*`, passes and exits **0**.
  Inventory comparison verifies identical names and order. It includes all
  eight constant-condition cases, gradient growth (3 to 2083), nested fuzzy,
  simulation errors, BS/Dupire and F3 observation cases.
- The same Adept binary's isolated
  `ScriptObservationSimulationTest.TestDeadBranchPathAndPayoffFailuresDrainEveryBatch`
  passes **1/1**, process exit **0**, with leak detection enabled.
- Native and Adept sanitizer audits verify all **130 DAL** and **four
  Google Test/Mock** objects were actually built with address instrumentation
  and contain ASan symbols. Adept additionally verifies **16 Adept** objects.
  Final native utility runner: **two** instrumented translation units;
  standalone probe: **one**; exact Adept selection: **nine**. Both builds use
  C++17, Debug `-O1 -g`, static DAL and frame pointers; native-architecture
  tuning is off. Compile/link commands, source/library/runner SHA256 identities
  and object audits are retained. Objects in unused static libraries need not
  execute; compiler/system libraries were not rebuilt.
- `dal_check_generated` passes and writes **zero files**; `git diff --check`,
  staged scope checks and `git clang-format --diff` pass.

The intermediate native build was fresh after the production change. Final
include grouping and private parameter spelling rebuilt exceptions.cpp in
that native build; final runner sources were freshly compiled. The final
probe and utility runner link the same final native library. The Adept and
canonical Release builds start from empty build directories at the tested
commit. runner-instrumentation.json, final-source-identity.json and the build
manifests distinguish intermediate, final and frozen-baseline identities.

### Reproduction commands and raw evidence

Commands below run from the workspace root; the attached `leak/` directory
contains the inspected/adapted diagnostic driver, original standalone probe,
command records, full logs and identity audits. `--reuse-library` means a
frozen pre-repair library control, explicitly skipping configure/build.

```bash
python3 evidence/leak/verify_sanitizer.py Derivatives-Algorithms-Lib evidence/leak/red-probe --probe evidence/leak/exception_thread_probe.cpp --files test_main.cpp
python3 evidence/leak/verify_sanitizer.py Derivatives-Algorithms-Lib evidence/leak/red-added --build evidence/leak/red-probe/build --filter ExceptionTest.TestThrowOnJoinedThread --files utilities/test_exceptions.cpp test_main.cpp
python3 evidence/leak/verify_sanitizer.py Derivatives-Algorithms-Lib evidence/leak/green-added --filter ExceptionTest.TestThrowOnJoinedThread --files utilities/test_exceptions.cpp test_main.cpp
python3 evidence/leak/verify_sanitizer.py Derivatives-Algorithms-Lib evidence/leak/red-final-utilities --reuse-library --build evidence/leak/red-probe/build --filter 'ExceptionTest.*' --files utilities/test_exceptions.cpp test_main.cpp
python3 evidence/leak/verify_sanitizer.py Derivatives-Algorithms-Lib evidence/leak/green-native --build evidence/leak/green-added/build --filter 'ExceptionTest.*' --files utilities/test_exceptions.cpp test_main.cpp
python3 evidence/leak/verify_sanitizer.py Derivatives-Algorithms-Lib evidence/leak/green-probe --build evidence/leak/green-added/build --probe evidence/leak/exception_thread_probe.cpp --files test_main.cpp
python3 evidence/leak/verify_sanitizer.py Derivatives-Algorithms-Lib evidence/leak/green-adept --backend adept --filter '*' --files math/aad/test_tape.cpp script/test_compile_parity.cpp script/visitor/test_fuzzy.cpp script/test_constcondprocessor.cpp script/test_simulation.cpp script/test_observation_simulation.cpp model/test_blackscholes.cpp model/test_dupire.cpp test_main.cpp
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 DAL_NUM_THREADS=4 python3 evidence/leak/run.py adept-narrow-draining . evidence/leak/green-adept/focused-tests --gtest_filter=ScriptObservationSimulationTest.TestDeadBranchPathAndPayoffFailuresDrainEveryBatch --gtest_fail_if_no_test_selected
NUM_CORES=8 DAL_NUM_THREADS=4 python3 evidence/leak/run.py native-canonical Derivatives-Algorithms-Lib bash -c 'bash ./build_linux.sh > test_output.txt 2>&1'
DAL_NUM_THREADS=4 python3 evidence/leak/run.py native-focused Derivatives-Algorithms-Lib build/Release-linux/dal-cpp/dal_cpp_tests '--gtest_filter=*Script*:*Domain*:*IFProcessor*:*Fuzzy*:*BlackScholes*:*Dupire*:*Observation*:*ModelBinding*:SimulationTest.*:PastEvaluatorTest.*:AADTapeTest.*:ExceptionTest.*:ThreadPoolTest.*' --gtest_fail_if_no_test_selected
python3 evidence/leak/run.py generated-check Derivatives-Algorithms-Lib cmake --build build/Release-linux --target dal_check_generated -j 8
python3 evidence/leak/audit_evidence.py
```

The first commit attempt found no author identity in this fresh checkout.
Repository-local identity was set to the existing implementer commit identity;
no global Git setting changed. Intermittent read-only Multica requests were
retried successfully. No verification failure was hidden or test weakened.

### Limits and continuation

This repair clears the reproduced leak in the measured processes. It is not
complete F3 acceptance or a blanket memory-safety claim. DAL-217 owns
independent successor testing, the unchanged historical Python performance
**2/90 failures** and the separate Adept first-tape UBSan disposition.
No performance loops or UBSan run were undertaken in this utility repair.
Full alternative-backend Release suites, Windows/XLL, Python and XAD-backend
execution were not repeated; earlier three-backend passes remain attributed
tester evidence at the starting SHA.

The duplicate-date thread `PRRT_kwDOBtahP86h0kcB` was already resolved by the
parent on independent 32.5/40/25 oracles and source confirmation. That finding
is not reopened and no compiled restriction is introduced. DAL-218's
documentation/CHANGELOG decision and mandatory DAL-219 successor review remain
pending. The prior review at 9bc4f000 is not successor approval. The PR must
remain draft; no merge, F3 closing lines, F4 activation or downstream role
dispatch belongs to this handoff. The final comment records one post-push CI
snapshot and the parent active-run/rerun continuation.

---

## Constant-condition lifetime repair, 2026-09-13

The bounded DAL-216 repair is complete and ready for independent successor
acceptance. Constant-false IF preprocessing now retains the ELSE range before
replacing the owning IF node. Nested selected statements remain in order and
are recursively folded. All older sections below are historical evidence;
their acceptance statements do not approve this successor.

Starting published SHA: `a21ff62b5a50f809c392e8701cb4352bc98f700f`, tree
`c7297e29cd591d0ea96a461ecb980b918da746eb`.
Tested repair SHA: `15ae4fd086f6f51dea135e30fdf2f3864915b53d`, tree
`374ebf6d6c31d3c3006b205b50d8d62e530d8303`.
The publication adds only this report to that repair; the final DAL-216 comment
records the delivered report-commit SHA. Existing draft PR:
<https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369>, publish
branch `feature/dal-200-eq-observation-slots`.

### Authority, cause and scope

The current DAL-216 description, parent DAL-200 F3 contract, independent DAL-217
diagnostic/report, repository implementer/style/test/publication references,
and script-engine methodology control this repair. The dedicated clean checkout
matched the live PR head and retained F2 ancestor
`ec8b0072fbf70dab814a543edc625c8e0bf77efa` and published Adept repair
`63c134c096f2f3d2fd9865cbe5c52cfd5bb8e2dd`. Historical unpublished work and
recovery patches were not modified or applied.

In the original `ConstCondProcessor_::Visit(NodeIf_&)`, replacing `*current_`
destroys the IF still referenced by `node`, then the false branch calls
`node.HasElse()` and reads `node.firstElse_`. Both ELSE-present and ELSE-absent
scripts therefore read freed storage. The minimum correction captures one
range start while the node is alive: the ELSE index when present, otherwise
the original argument count. After moving the arguments and replacing the
owner, the existing ordered move loop uses that saved value and visits the
retained collection. No subsequent access uses the destroyed node.

Changed files:

- `dal-cpp/dal/script/visitor/constcondprocessor.hpp`: the lifetime correction
  and formatting of the touched conditional.
- `dal-cpp/tests/script/test_constcondprocessor.cpp`: two nested regressions
  and the evaluator include, with existing six assertions preserved.
- `.codex/artifacts/reviews/dal-200/implementation.md`: this current handoff.

The ELSE test discards multi-statement true branches, retains a nested false
ELSE, and evaluates ordered decimal accumulation to exactly **12345**. The
no-ELSE test folds a nested false branch inside a retained true branch, discards
another top-level false block, and evaluates to exactly **1234**. Collection
checks establish recursive folding, while evaluated results detect statement
loss, reordering, and execution of discarded statements. No nearby integration
file or other production surface needed editing. There is no design deviation.

### RED, GREEN and full verification

Fresh unchanged-source RED used the independent tester's inspected
`reproduce_constcond.py` driver. Its six isolated executions produced **4 passes
and 2 ASan heap-use-after-free failures**, both exit 1:
`ScriptTest.TestConstCondAlwaysFalseIfReplacedByElse` and
`ScriptTest.TestConstCondAlwaysFalseNoElseEmptyCollection`. The failing logs
identify the invalid read in `NodeIf_::HasElse()` after owner replacement.
The added ELSE regression was then compiled and run before production edits:
the same ASan failure, exit 1. The no-ELSE regression was added next and also
failed before the fix. These are observed RED results, independent of the
misnamed historical `green-constcond.log`. F2 was not rerun in this repair turn;
its separate identical failure remains the independent tester's evidence.

Fresh repaired-source GREEN rebuilt the DAL library and runner in a new
directory: **6/6 existing cases pass**, and the combined filter passes **8/8**,
including both added cases. After formatting, affected production objects and
the runner were rebuilt and the same 6/6 and 8/8 results confirmed against the
final code bytes. The native address-only configuration is Clang **21.1.8**,
C++17, AADET, Debug `-O1 -g`, static DAL, `-fsanitize=address`, frame pointers,
and `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`. Audits verify all **130 DAL
production translation units** and Google Test were actually built with those
flags. The two narrow runner translation units use the same instrumentation.
No sanitizer suppression was supplied; system/compiler runtime libraries were
not rebuilt.

Final-code compatibility verification:

- **366/366** native script/model cases: preprocessing, constant/domain/IF
  processing, tree/compiled/fuzzy parity, F3 preparation and observations,
  simulation, and BS/Dupire behavior.
- Canonical `build_linux.sh`, GNU C++ **15.2.0**, Release/native AADET:
  **100% tests passed, 0 tests failed out of 1681**. This is fresh discovered
  CTest output, including core, public and portable Excel targets, with the
  script's unchanged benchmark exclusion.
- Clang **21.1.8**, Adept, Debug `-O1 -g`, address-only sanitizer:
  **43/43** focused tape/compiled parity/fuzzy/constant-condition cases pass.
  `AADTapeTest.TestGradientCapacityGrowsAfterSeeding` retains its exact
  accumulated-adjoint assertions **3 then 2083**. The formerly failing
  `ScriptCompiledParityTest.TestParity_Number_NestedIf_Fuzzy` also passes
  **10/10** additional repetitions. All 130 DAL, 16 Adept and four Google
  Test/Mock translation units were instrumented; the five selected test/runner
  translation units were compiled from the CMake database with the same flags.
- `dal_check_generated`, full changed-test clang-format, touched-header-range
  clang-format, and Git whitespace/scope checks pass. No generated files drifted.

The first canonical configure stopped because the fresh checkout lacked the
pinned XAD source used by an existing example target. Initializing that submodule
allowed the unchanged build workflow to pass. The first auxiliary Adept link
also stopped because the selected build targets omitted Google Mock/main static
libraries referenced by its CMake link line; building those existing targets
resolved it. Both setup logs are retained. Neither failure led to a production,
dependency-pin, test, or build-policy change.

### Reproduction and evidence

Commands run from the workspace unless noted. The attached drivers and each
output directory's `commands.json` preserve expanded configure/build/compile/
link/test commands and exit codes.

```bash
python3 evidence/dal217/dal-217-constcond-diagnostic/reproduce_constcond.py Derivatives-Algorithms-Lib evidence/build-red-asan evidence/red-existing
# Before the fix, compile the updated test source with the RED link command,
# changing only the runner output, and run each new filter separately:
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 evidence/red-added-else/constcond-asan --gtest_filter=ScriptTest.TestConstCondNestedFalseElsePreservesStatementOrder --gtest_fail_if_no_test_selected
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 evidence/red-added-noelse/constcond-asan --gtest_filter=ScriptTest.TestConstCondNestedFalseNoElsePreservesStatementOrder --gtest_fail_if_no_test_selected
python3 evidence/dal217/dal-217-constcond-diagnostic/reproduce_constcond.py Derivatives-Algorithms-Lib evidence/build-green-asan evidence/green-existing
python3 evidence/dal217/dal-217-constcond-diagnostic/reproduce_constcond.py Derivatives-Algorithms-Lib evidence/build-green-asan evidence/green-final
python3 evidence/repair/run_checks.py green-added
# From Derivatives-Algorithms-Lib, with no stale test_output.txt:
NUM_CORES=8 bash ./build_linux.sh > test_output.txt 2>&1
# Back in the workspace:
python3 evidence/repair/run_checks.py native-focused
python3 evidence/repair/run_checks.py adept-asan
python3 evidence/repair/audit_instrumentation.py evidence/build-red-asan evidence/red-existing
python3 evidence/repair/audit_instrumentation.py evidence/build-green-asan evidence/green-final
python3 evidence/repair/audit_instrumentation.py evidence/build-adept-asan evidence/adept-asan
```

Final native ASan runner SHA256:
`993b0e5e60ba063e15ba47326bbc1800250e62ee1f4a0f08a212dafce62f9243`.
Final Adept ASan runner SHA256:
`a8a60b7a7865e21993e9a9ebd342c1dd361e4b270fac243f2e55f8e6fdeee7b7`.
The raw attachment includes library/source/object hashes, actual-object
instrumentation audits, compile databases, CMake caches, source patch, RED/GREEN
logs, the canonical full log, and the drivers. Source and binary identities
distinguish the pre-fix, pre-format GREEN and final GREEN results.

### Remaining limits and serial handoff

This repair changes no numerical path generation, finite-output diagnostics,
frozen history, observation/payment addressing, duplicate-date handling,
normalized shared-path MC tolerance **1e-8**, or supported execution mode.
No new named AAD/compiled/fuzzy capability, public/Python/Excel API, dependency
pin, documentation/CHANGELOG, benchmark inventory, or threshold is introduced.

Independent DAL-217 full successor acceptance is still required, including
the integrated Adept repair. This turn does not claim full Adept CTest, UBSan,
CoDiPack, XAD backend, Windows, or Python runtime acceptance. The native build
uses XAD only for its existing comparison example.
The prior Python performance gate remains **88/90, two failures**, and
duplicate-date thread `PRRT_kwDOBtahP86h0kcB` remains unresolved. No expensive
performance gate was repeated or inferred from noisy controls. DAL-218's
documentation decision, DAL-219's independent successor review, and required
current-head CI remain parent-owned gates. The PR stays draft with no F3
closing lines or merge. DAL-216 goes to `in_review` for this bounded repair,
followed by the authorized parent active-run/rerun check.

---

## Adept gradient-array repair, 2026-09-13

Repair commit: `63c134c0` on `feature/dal-200-eq-observation-slots`, directly on top of
the recorded head `4c346c04`. It repairs the current-head Linux Adept abort listed as
open in the summary above: all five Adept legs passed 1,669/1,670 CTest cases and then
aborted `ScriptCompiledParityTest.TestParity_Number_NestedIf_Fuzzy` with a glibc
`sysmalloc` allocator assertion.

Root cause, reproduced with a CI-identical static Release build (intermittent, roughly
5% per process at four threads) and pinpointed with an AddressSanitizer build: Adept's
`Stack` grows its gradient array only inside `initialize_gradients()`, while
`register_gradient()` keeps handing out indices up to the all-time high-water mark
`max_gradient_`. DAL's custom `Tape_::compute_adjoint` (`dal-cpp/dal/math/aad/tape.hpp`)
froze the array at a worker thread's first seed and never resized it. A later recording
window with more simultaneously live variables — here a branchier fuzzy path in the
compiled parity evaluation — then referenced gradient slots past the allocation. ASan
reports the heap-buffer-overflow at `tape.hpp:119`, zero bytes after the 151-double
array allocated at the first seed. The condition is latent on the base revision as well:
the F2 tree reproduces the identical overrun, so this is not introduced by DAL-200; the
restructured generation loop changed per-path registration patterns enough for CI to
observe it.

The repair is confined to the Adept backend facade: `Tape_::EnsureGradientCapacity()`
grows the gradient array on demand, preserving accumulated adjoints and zeroing the new
tail, and runs before every reverse sweep and every adjoint seed/read. Numerics are
unchanged when capacity suffices, so the non-Adept backends and the native benchmark
gate are unaffected by construction. The vendored Adept sources are not modified.

Verification on the repair commit:

- New backend-neutral regression `AADTapeTest.TestGradientCapacityGrowsAfterSeeding`:
  deterministic heap corruption (`malloc(): invalid size`) on the unrepaired base,
  exact adjoint accumulation (3.0 then 2083.0) after the repair.
- CI-identical Adept static Release build, gcc-14: full CTest **1,671/1,671 pass**; the
  previously aborting parity test ran 40 times at four threads with zero failures.
- AddressSanitizer build: 169 focused tape/parity/observation/simulation/model tests
  pass with zero sanitizer reports; the same build caught the overrun on its first run
  before the repair.
- gcc-15 Adept and native/xad/codipack focused parity/tape suites pass;
  `dal_check_generated`, changed-range clang-format and whitespace checks pass.

A separate artifact was observed only in address+undefined sanitizer builds: UBSan
reports a null `adept::_stack_current_thread` load at the first tape touch. It
reproduces identically on the F2 base, never occurs in Release or address-only builds,
and is unrelated to this PR. The Python performance acceptance above remains open and
is unchanged by this repair.

---

## Correctness and performance remediation, 2026-09-13

Incoming head: `9bc4f000554945199b8edda73b9a90b2508c65be`.
Tested production successor: `fe12a75041f6f3a9db042847a4bb76b6f952a3d1`.
F2 baseline: `ec8b0072fbf70dab814a543edc625c8e0bf77efa`.
The verification successor and final DAL-216 comment identify the delivered PR head; it adds a finite-extremes test and this report without changing production code.
Draft PR: <https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369>.

### Duplicate-date result and justified deviation

The reported prepared legacy `t,t,T` event/sample mismatch does **not** reproduce
on the incoming production tree. The new three-date regression passed before
production edits; no behavioral RED or mapping repair is claimed for that report.
`AppendEvent` in `dal-cpp/dal/script/preprocessor.cpp` coalesces rows
in a date-keyed map and concatenates their statements in input order.
`ScriptProduct_::ParseEvents` parses each map entry once.
`ScriptProductData_::Product()` in `dal-cpp/dal/script/event.hpp` constructs a fresh
product, and `PreparedScriptBuilder_` always obtains the product through it.
Consequently `t,t,T` produces two event streams and two samples, with an identity
`EventToSample`; all-same-date produces one, and distinct dates produce three.
Repeated raw `ParseEvents` calls are a different route, excluded after preparation;
a duplicate raw model timeline also fails allocation before workers. No new
compiled restriction or nonidentity mapping API is introduced.

`TestLegacyPreparedDuplicateDates`, `TestLegacyPreparedSameDate`, and
`TestLegacyPreparedDistinctDates` exercise both tree and compiled settings through
preparation, direct evaluation of an exactly sized path, and 257 simulated paths.
Controlled sample spots are 10/20/40 and numeraires 2/4/8. Ordered statements set
`x=SPOT()`, then `x=2*x+SPOT()`, paying at each event and finally paying `x+SPOT()`.
The respective exact per-path values are 32.5, 40, and 25. Tests assert event,
compiled-stream and sample counts, identity mapping, and reject all history reads.
The independent tester must confirm this source-based disposition before the
parent resolves `PRRT_kwDOBtahP86h0kcB`.

### Production repair and scope

The full path validation traversal was measurable overhead. Exact Black-Scholes
double simulation now uses `BlackScholes_::CheckedPaths_`, an owned worker-local
snapshot of spot/log-spot, drifts, standard deviations, numeraires and scenario
storage. It initializes and validates fixed numeraires once, chooses observation
filling once for each generated path, and checks every emitted spot. Observations
are copies of that checked spot. Its public path access is const; it retains no
caller-owned model/definition storage. Worker reuse removes the redundant generic
scenario and avoids allocating snapshots for each batch. Construction rejects
unallocated, inconsistent and derived models.

The shared generation loop retains Gaussian draw order and Black-Scholes arithmetic.
For AAD, `GeneratePathAndValidate` checks actual emitted spots and numeraires during
generation, including numeraires not requested by a definition. It performs no
AAD caching across rewind marks. Exact type selection occurs outside the path
loop. Derived/custom generators still execute their virtual override once and
receive full validation of the resulting shape, including appended samples and
observations. Dupire uses that generic route.

Validation completes generation before diagnosing any invalid sample. The cold
ordered scan retains the first offending field, existing `InvalidModelPath`
messages, and `ScriptError_`. Negative finite spots, positive subnormal numeraires,
zero spot from finite underflow, NaN/Inf rejection, intermediate overflow followed
by later recovery, pre-evaluation failure, payoff checks and accepted-task draining
are covered. No correctness check is removed or made debug-only. The numerical
snapshot is double-only, so the AAD mark/rewind/propagation lifetime remains intact.
The shared helper computes initial log-spot before copying today's sample instead
of after; both consume the same parameter node and the native AAD regressions pass.
Other AAD backends require the independent tester's successor verification.

Changed files are `dal-cpp/dal/model/base.hpp`,
`dal-cpp/dal/model/blackscholes.hpp`, `dal-cpp/dal/script/simulation.hpp`,
`dal-cpp/tests/model/test_blackscholes.cpp`,
`dal-cpp/tests/script/test_observation_simulation.cpp`,
`dal-cpp/tests/script/test_simulation.cpp`, and this report.
Independent tests/tolerances, compiled bytecode, published documentation/CHANGELOG,
public/Python/Excel surfaces, dependencies, generated files, CI policy, RNG defaults
and benchmark inventory remain unchanged. Named compiled/AAD/fuzzy execution stays
rejected; prior retained-fixing and frozen-history coverage remains green.

### RED, GREEN and correctness verification

- Checked generation RED: building the new focused model regression failed because
  `GeneratePathAndValidate` did not exist (`red-checked-generation.log`). Minimum
  implementation GREEN passed the focused test (`green-checked-generation.log`).
- Owned snapshot RED: `cmake --build build/Release-linux --target dal_cpp_tests -j8`
  exited 2 because `CheckedPaths_` did not exist (`red-owned-paths.log`). GREEN
  `--gtest_filter=ModelTest.TestBlackScholesOwned*` passed the initial snapshot test.
- Allocation guard RED: the new
  `ModelTest.TestBlackScholesOwnedPathsRequireConsistentAllocation` exited 139 on
  an unallocated model (`red-owned-allocation.log`). After adding the guard, the
  same command exited 0; expanded/shrunk definition storage also throws.
- Final Release build exits 0. Focused script/preparation/observation/model,
  legacy double/AAD, tree/compiled and task-draining suites pass **357/357**.
  Full CTest passes **1678/1678** (core, public and portable Excel contracts).
- GCC ASan+UBSan build and run pass **75/75** model, simulation and observation
  cases, including all three duplicate-date regressions. The three test translation
  units and relevant inline generation/evaluation/simulation code are instrumented;
  the linked static DAL library and GoogleTest are the regular Release build.
  This is focused memory-safety evidence, not a fully instrumented library claim.
- Fresh isolated Python tests pass **402/402** on F2 and **402/402** on the final
  candidate. The current head benchmark smoke tests also pass **109/109** on F2.
- `dal_check_generated` writes zero files and passes. Changed-range clang-format,
  whitespace and submodule-scope checks pass. Lizard 1.23 reports new/changed
  helpers at complexity 1–6, `MCDoubleSimulation` at 8; the existing AAD entry point
  remains 13. No suppressions or policy changes.

Final native commands (from the repository root):

```bash
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF
cmake --build build/Release-linux -j8
DAL_NUM_THREADS=4 build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationSimulationTest.*:ScriptFixingPreparationTest.*:ScriptObservationTest.*:ScriptTest.*:SimulationTest.*:ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*:PastEvaluatorTest.*:ModelTest.*'
DAL_NUM_THREADS=4 ctest --test-dir build/Release-linux --output-on-failure -j8
cmake --build build/Release-linux --target dal_check_generated -j8
```

Sanitizer command:

```bash
g++ -std=c++17 -O1 -g -DNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer -I dal-cpp -isystem dal-cpp/externals/googletest/googletest/include dal-cpp/tests/script/test_observation_simulation.cpp dal-cpp/tests/script/test_simulation.cpp dal-cpp/tests/model/test_blackscholes.cpp dal-cpp/tests/test_main.cpp build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -lpthread -o ../remediation-evidence/final-paths-asan
DAL_NUM_THREADS=4 ASAN_OPTIONS=detect_leaks=0 ../remediation-evidence/final-paths-asan --gtest_filter='ScriptObservationSimulationTest.*:SimulationTest.*:ModelTest.*'
```

### Performance attribution and reproducibility

The CI synthetic merge `e635be54e5c3cad432295a18c6767d2588ad8bd8` has F2 and the
incoming head as parents. GitHub's commit API verifies its tree
`ad91a695a9a79e7a157d240a874f527e5e74d6f4` equals the incoming head tree; the retained
CI data therefore measures that production content. It fails **13/90 total cases**,
all among the **16 MC cases**, not 90 MC cases. Barrier comparison Greeks use
repeated compiled double bumps. All 74 non-MC CI cases pass. Retained CI A/A
vanilla-tree instability does not explain every paired MC regression.

Local builds use isolated F2, incoming and candidate worktrees; GCC/G++ 14,
Release, native architecture ON, native AADET, external AAD backends OFF,
CPython 3.13 and identical interpreter/configuration, DAL_NUM_THREADS=4.
The host is WSL2 on an Intel Core i9-13900HX and is a noisy environment; timing
controls and both positive and negative excursions must be retained. Correctness
builds use GCC 15.2. Final manifests record full compiler/Python/CMake versions,
SHA/blob/SHA256 identities, pinned submodules and every native module identity.
The existing public/core compile-flag difference is preserved on both sides.
No installed or remotely archived binaries were executed.

Initial local reproduction fails 5/90: barrier comparison Greeks 16384
+7.33/+7.91%, Greeks 65536 +10.72/+9.80%, barrier compiled double +22.52/+15.42%,
barrier tree double +15.04/+9.85%, vanilla compiled double +8.39/+9.78%.
A fast validity accumulator and separated diagnostics alone did not consistently
improve MC performance. A **diagnostic-only** omission of validation in the isolated
candidate (never the published tree) improved compiled barrier by 7.99/10.38%
against incoming code, establishing traversal cost; vanilla effects were mixed.
The patch is retained with its unsafe diagnostic label, not proposed as a fix.

Fusing checks alone left four full-gate failures. Moving the checked double method
into a core explicit specialization did not remove the barrier regression and was
abandoned. Owned snapshots improved the controlled compiled-barrier comparison to
+3.98/−0.17% versus F2 and vanilla to −12.35/−11.33%. The first full owned-snapshot
comparison passed all long barrier gates but failed short vanilla price 16384
(+12.06/+9.71%). The final version reuses the snapshot in worker state and removes
the redundant scenario allocation. These are distinct recorded source changes,
not blind reruns of unchanged binaries. All diagnostic patches, raw results and
failed setup/build attempts are retained. One early comparison stopped at zero
cases because compiler cache spellings differed; both were normalized to the same
G++ 14 compiler before further measurement.

Every acceptance run uses the unchanged current complete inventory, ten samples per
side, two alternating rounds, best-of-ten minima and the strict +4% rule in both
rounds. Diagnostic subsets are labeled and never substituted for final coverage.
`final-verification-commands.json` records exact executed commands and exit codes.
`final-source-identity.json` and the gate manifests identify the clean production
SHA `fe12a75041f6f3a9db042847a4bb76b6f952a3d1` and actual build/module hashes.
Raw logs, reports, controls and source patches are delivered in the attached
remediation archive. Runtime-local paths in the logs are provenance, not links to
delivered files.

The later bitwise finite-value reduction was also rejected: its isolated
compiled-barrier comparison regressed +9.74/+8.55% versus F2. The first subset
launch overlapped a finishing native test build and is explicitly excluded in
`bit-reduction-overlap-note.md`; the isolated result, not that confounded run,
is used for the decision. No bitwise or floating-sum validation is published.

The owned-generator core specialization was also abandoned: its isolated
compiled-barrier deltas (+10.24/+3.73%) did not demonstrate a repeatable improvement.
The saved disassembly confirms native FMA generation, but this is not sufficient
performance evidence to justify changing the linked implementation. The measured
`fe12a750` source was restored; all nine native executable hashes still match the
successful native gate. The final additional finite-exponent regression passes on
that unchanged production code, including ordinary 1/4/16 spots and valid spots
near the maximum finite double. No behavioral RED is claimed for that extra
positive coverage or for the discarded performance-only ablations.

### Final measured outcome: performance acceptance remains open

The final complete F2-to-`fe12a750` Python gate exits **1**: **88/90 pass**.
These two workloads still fail the unchanged rule:

| Remaining workload                 | Round 1 | Round 2 |
|------------------------------------|---------|---------|
| comparison.mc_barrier_greeks_65536 | +4.35%  | +5.91%  |
| mc.barrier.double.tree             | +4.17%  | +4.70%  |

The other 14 MC cases and all 74 non-MC cases pass. Compiled barrier double is
−0.39/−3.47%; barrier Greeks 16384 is +1.86/+2.06%. These results improve on the
reproduced incoming hot-path failures, but they do **not** meet complete Python
performance acceptance. No green or resolved-performance claim is made.

Final same-binary Python A/A passes 90/90, but remains noisy in both directions:
barrier Greeks 65536 is −5.10/+1.21%; barrier double/tree is +2.47/+0.57%; barrier
Greeks 16384 is +9.81/+1.76%; short vanilla price is +25.43/+3.45%. The baseline A/A
fails vanilla compiled double at +4.06/+4.16%, and has other one-round excursions.
These controls limit precision and prohibit crediting the approximately 50%
vanilla-tree speedup to this repair. They do not erase the two failed paired cases.

The unchanged nine-target native gate exits **0**, with no failures at 10 samples
per side × 2 rounds and +4%. The informational `script_mc_perf` run also exits 0;
it is not counted as a tenth native gate target.

The implementer delivers the validated source repair and unresolved performance
measurements for the existing independent tester, documentation reconciliation
where needed, and independent reviewer. Repeat the complete comparison and controls
on a second controlled host at the successor production blobs; determine whether
the two residual cases are stable before accepting this requirement or routing
further production repair. The draft PR and duplicate-date review thread remain
open. No F3 acceptance, thread resolution, required-CI success, merge or deferred
F4 capability is claimed.

---

## Codacy remediation handoff, 2026-09-13

Before SHA: `5d578729386be2038552b73395ab14eef9e206e2`.
After production SHA: `061db30b28618b9f2806970791d4d4309aecc574`.
The following report-only commit records this verification without changing the
tested production tree. DAL-216's final comment and attached publication record
identify the exact delivered PR head. The branch remains
`feature/dal-200-eq-observation-slots`, draft PR
<https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369>.

GitHub check run `103626628207` reported exactly three new cyclomatic-complexity
findings, at limit 8. The installed Lizard 1.23.0 reproduces all three original
counts. After the cohesive helper extractions:

- `dal-cpp/dal/model/base.hpp`: `Model_::ValidateTimeline` decreases **9 → 6**.
  Private `ValidateSampleOutputs` is **4**. Each sample's time check still
  precedes its output-count, parsed-index capability, and common-index checks,
  before advancing to the next sample. The same canonical name is carried
  between samples; all predicates and diagnostic messages are retained.
- `dal-cpp/dal/script/preparation.cpp`: `PreparedScriptBuilder_::ModelPlan`
  decreases **9 → 6**. Private `BindModelObservations` is **4**. Binding and
  request validation still precede construction of the sorted union timeline,
  payment-numeraire addresses, and retained fixing slots. Model Allocate/Init
  still precede history capture. The plan and its storage keep their ownership.
- `dal-cpp/dal/script/simulation.hpp`: `MCDoubleSimulation` decreases
  **10 → 7**. Internal `Detail::EvaluateDoubleBatch` is **4**. It reuses the
  existing worker state, seeks to the same first path, and keeps draw,
  GeneratePath, path validation, evaluation, payoff validation and summation
  in their original order. Capturing the existing `PathBatch_` by value replaces
  captures of its two fields. Task-group declaration, lifetime and draining,
  caller-side validation, expired fast return, zero-dimensional RNG behavior,
  tree/compiled selection and historical replay are preserved.

No behavioral RED is claimed for this pure refactor. The prior implementation's
RED/GREEN record below remains intact; fresh existing regressions validate this
repair. No tests, assertions, suppressions, complexity thresholds, analyzer/CI
configuration, public projection or deferred capabilities changed. The only
four changed files are the three production files above and this report.
There is no design deviation from the authorized repair.

### Commands and fresh results

Commands ran from the repository root. Logs are in the evidence archive attached
to DAL-216; native AADET, GCC 15.2.0, Release, public C++ and portable Excel
contracts enabled. Examples remain disabled, as in the original handoff.
All commands below exit 0.

```bash
gh api repos/wegamekinglc/Derivatives-Algorithms-Lib/check-runs/103626628207/annotations --paginate
git submodule update --init --recursive dal-cpp/externals/googletest dal-cpp/externals/rapidjson dal-cpp/externals/machinist
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF
cmake --build build/Release-linux -j12
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationSimulationTest.*:ScriptFixingPreparationTest.*:ScriptObservationTest.*:ScriptTest.*:SimulationTest.*:ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*:PastEvaluatorTest.*:ModelTest.*'
ctest --test-dir build/Release-linux --output-on-failure -j12
cmake --build build/Release-linux --target dal_check_generated -j8
lizard --version
lizard dal-cpp/dal/model/base.hpp dal-cpp/dal/script/preparation.cpp dal-cpp/dal/script/simulation.hpp
clang-format --dry-run --Werror --lines=36:57 dal-cpp/dal/model/base.hpp
clang-format --dry-run --Werror --lines=181:230 dal-cpp/dal/script/preparation.cpp
clang-format --dry-run --Werror --lines=189:208 --lines=278:296 dal-cpp/dal/script/simulation.hpp
git diff 5d578729386be2038552b73395ab14eef9e206e2 --check
git diff --cached --name-status
git diff --cached --check
```

The initial full build and the incremental rebuild after formatting both pass.
Focused verification passes **327/327** in nine suites (460 ms), adding all
`ModelTest` cases to the prior 287-test filter. Full CTest passes
**1,648/1,648**, 8.00 seconds. This includes retained-F/cancellation, history
read counts, model/mode barriers, settled PAYS replay, today-only RNG/BB,
legacy tree/compiled determinism across threads/requests, and task-draining
regressions. Generated-source checking passes with zero generated files written
and no submodule-pointer drift. Changed ranges are clang-formatted; whitespace
checks pass. Existing Excel source `#pragma once` warnings remain in the clean
build log; the build has no errors.

`complexity-before.log` and `complexity-after.log` preserve all Lizard output;
the six changed/new functions are each below limit 8. The unchanged AAD
`MCSimulation<AAD::Number_>` still measures 13 and is outside these three
reported findings and this repair's write scope. Lizard's default exit status
alone is not a limit-8 assertion; the per-function counts above are the local
evidence. No completed remote Codacy pass is inferred from it. Exactly one
non-blocking CI snapshot is taken after push and delivered in the evidence
archive; remote acceptance and independent F3 review remain outstanding.

The original commit used Cheng Li's GitHub noreply identity. The fresh checkout
had no Git author configured, so its first commit attempt failed without
creating a commit; repository-local configuration restored that existing
identity before the successful commit. No global Git configuration changed.

## Original implementation handoff

Implementation branch: `feature/dal-200-eq-observation-slots`, based on refreshed
`origin/master` at F2 squash `ec8b0072fbf70dab814a543edc625c8e0bf77efa`.
The containing commit is the implementation handoff. DAL-216's final comment
records the exact published SHA and draft PR. Independent tester, documentation,
and reviewer acceptance remain outstanding; this report does not accept F3.

## Behavior and design decisions

- The model-aware core `PrepareScript(data, modelPointer, valuation, simulation,
  snapshot, contract)` collects the original AST, checks naming/lookahead and
  model bindings, constructs the union timeline, and allocates/initializes the
  model before historical I/O. A new core `MCSimulation<T_>(data, modelData,
  paths, valuation, simulation, snapshot, contract)` uses this order. The model
  pointer makes its initialization explicit. The model-aware overload requires
  valuation and simulation arguments, preserving the original two/three-argument
  F2 calls with brace-initialized defaults. Existing model-data constructors
  and public/archive entry points retain their signatures.
- The F2 overload without a model retains its history-preparation semantics and
  explicit nonexpired execution rejection. Pricing does not call that overload
  and then attempt a late model-capability check.
- `ModelIndexBinding_` explicitly maps `spot` to one ordinary EQ. BS/Dupire
  advertise that capability and validate requested index outputs. Missing,
  conflicting, duplicate and unknown-asset bindings, future FX/delivery/second
  EQ requests, and malformed names fail before history and workers. Multiple
  historical EQ/delivery/FX requests coexist with the future EQ.
- The plan owns sorted unique sample dates, existing `DAYS_PER_YEAR` year
  fractions, requested output definitions, and a separate `eventToSample`
  payment mapping. Each AST observation receives a request ID resolving to a
  history value or `(sampleId, outputId)`. Values remain in the whole scenario
  until path evaluation ends. Reads perform checked integer addressing only.
- The plan has a stable heap address across `PreparedScript_` moves because the
  initialized model retains a pointer to its definition vector. Callers of the
  low-level model-aware preparation retain the prepared object while using that
  initialized model. Preparation exposes const product/plan access.
- Named tree double evaluates the unoptimized AST with sealed history. Past
  events replay once into a double initial state; historical PAYS evaluates and
  consumes its RHS without accumulating cash. Stable same-date statement order
  is preserved. Model-aware preparation accepts separate core contract settings
  for the legacy SPOT default identity; archive/public projection is deferred.
- Named double deliberately does not run the existing domain/constant-folding
  pipeline. This avoids introducing the parameter-dependency and fuzzy/AAD
  optimization work assigned to later stages. Named compiled/fuzzy and live
  prepared AAD execution explicitly reject. Legacy historical AAD also rejects
  instead of silently freezing parameter-dependent historical state. Valid
  future-only legacy SPOT tree/compiled/AAD entry points remain supported.
- Samples allocate only requested index outputs. Dupire supports empty forward
  arrays. Historical-only future payments retain payment numeraires and discount
  normally. Expired products validate structure, names, lookahead and ordinary
  configuration, then return zero PV/risk without history, model Allocate/path
  generation or accepted tasks; unused output capability is not required.
- Today-only paths use valid t=0 samples and no zero-dimensional RNG or BB.
  RNG names still validate before workers, including expired/zero-dimensional
  cases. Model domains and finite deterministic initialization are checked;
  non-finite generated spots/observations/numeraires and payoffs raise path
  errors through the existing draining task group.
- F2's virtual fixing, exact-midnight, today-policy, immutable explicit snapshot
  and no-global-fallback semantics remain intact. Global capture remains a
  sequential per-series snapshot; concurrent writes during capture are outside
  the established contract.

## RED then GREEN evidence

Configuration:

```bash
git submodule update --init --recursive dal-cpp/externals/googletest dal-cpp/externals/rapidjson dal-cpp/externals/machinist
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF
```

Native AADET, GCC 15.2.0, Release, public C++ and portable Excel tests enabled.
Examples were disabled after the initial default configure reported the absent
pinned XAD sources required by examples. No submodule pointer changed.

Each cycle built with
`cmake --build build/Release-linux --target dal_cpp_tests -j12` before running
`./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='<filter>'`.
Only a successful build was used for runtime RED/GREEN evidence.

- `ScriptObservationSimulationTest.TestFutureOnlyNoHistory`: RED build failed
  because `modelBindings_` and `MonteCarloSettings_` were absent. GREEN: 1/1.
  Final coverage also injects observation 123 with unrelated legacy spot 999,
  for both global-source and explicit-empty-snapshot preparation.
- `ScriptObservationSimulationTest.*` at the nine-test boundary stage: RED
  passed the first four value/addressing tests, then past PAYS overflowed the
  static stack and today-only RNG setup terminated with exit 139. A separate
  filter selecting `TestExpiredConfigurationAndNoWork`,
  `TestInvalidModelBeforeHistory`, and `TestDupireOutputCapability` failed 3/3:
  invalid RNG/spot were accepted and Dupire rejected the EQ output. GREEN: 9/9.
- `ScriptObservationSimulationTest.*:PastEvaluatorTest.TestUnboundHistoricalSpotRejected:ScriptTest.TestEventWithPastDate:ScriptTest.TestScriptProductWithPastDate`:
  RED 11 passed / 7 failed. Failures covered the old 30.0 placeholder, missing
  SPOT/FIX deduplication, expired/unbound guards, silent non-finite paths and
  overflowing deterministic model initialization. GREEN with the entire
  `PastEvaluatorTest.*` suite: 21/21.
- Filter selecting `ScriptObservationSimulationTest.TestUniqueHistoryAcrossPathsAndThreads`,
  `TestPreparationValidatesConfiguration`,
  `TestRawHistoricalDeadBranchSpotRejected`,
  `TestTodayLegacyModesAndRngBarrier`, and
  `TestRequestedOutputsAreValidatedByAdapter`: RED 1 passed / 4 failed. Invalid
  preparation RNG and raw dead-branch historical SPOT were accepted; invalid
  AAD RNG submitted one worker; the BS adapter accepted an FX output. GREEN
  for `ScriptObservationSimulationTest.*:PastEvaluatorTest.*`: 24/24.
- `ScriptObservationSimulationTest.TestHistoricalAadRejected`: RED 0 passed /
  1 failed because historical AAD silently ran. Its new pre-worker rejection
  passes in the final focused and full runs.
- `ScriptObservationSimulationTest.TestOriginalPreparationBraceDefaults`: RED
  0 passed / 1 failed because the new pointer overload intercepted the original
  `PrepareScript(data, {})` call and raised `InvalidModel`. Requiring both
  valuation and simulation arguments on the new overload restores both F2
  brace-default call forms; final focused/full runs pass.

Additional final assertions cover exact retained-F cancellation to zero with
F=120/P=999; H=80 + F=120 = 200; duplicate H=160 with one read; discounted
H-only payment at r=.05; 300 uses across three events and paths `{1,257,8193}`
with threads `{1,2,4}`; frozen historical EQ/delivery/inverse-FX data; all three
RNG names with both BB settings for today-only named double and legacy
double/AAD tree/compiled; and every accepted task completing before a path
error returns. Observation buffer addresses remain stable across repeated
evaluation. This is not a claim of a global allocator/performance benchmark.

## Fresh final verification

```bash
cmake --build build/Release-linux -j12
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationSimulationTest.*:ScriptFixingPreparationTest.*:ScriptObservationTest.*:ScriptTest.*:SimulationTest.*:ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*:PastEvaluatorTest.*'
ctest --test-dir build/Release-linux --output-on-failure -j12
cmake --build build/Release-linux --target dal_check_generated -j8
git diff --check
```

All commands exit 0. Focused: **287/287**, eight suites, 327 ms. Full CTest:
**1,648/1,648**, 7.29 seconds, including core, public C++ and portable Excel
contracts. Generated-source verification passes with no generated diff. Changed
ranges and the new test file were clang-formatted. The log archive attached to
DAL-216 preserves RED, GREEN, build, full CTest and generation outputs.

No named AAD/compiled integration, alternative AAD backend, Windows XLL,
Python, examples or performance acceptance is claimed. The independent tester
owns broader verification. CI is read once after publication; merge and overall
stage acceptance belong to the parent orchestrator.

## Changed-file scope

- `dal-cpp/dal/model/{base,blackscholes,dupire}.hpp`: explicit bindings,
  capability/domain/timeline validation and EQ output filling.
- `dal-cpp/dal/math/aad/sample.hpp`: requested output storage.
- `dal-cpp/dal/script/{settings,preparation,event,simulation}.{hpp,cpp}`:
  preparation order, settings, replay, timeline and MC integration.
- `dal-cpp/dal/script/{node,observationplan}.hpp` and
  `dal-cpp/dal/script/visitor/{evaluator,pastevaluator}.hpp`: observation IDs,
  integer reads and historical PAYS/legacy guards.
- `dal-cpp/tests/script/test_observation_simulation.cpp`: 23 focused F3 tests.
- `dal-cpp/tests/script/test_event.cpp` and
  `dal-cpp/tests/script/visitor/test_pastevaluator.cpp`: replace superseded
  placeholder expectations with required errors, retaining future-only cases.
- This implementation report. No published documentation, public/binding API,
  generated file, agent definition or submodule pointer is changed.
