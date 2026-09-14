# DAL-202 F5 master integration

DAL-228 integrates the existing F5 branch with merged F4 master and removes
two extra comparisons from legacy opcode dispatch. Correctness passes, but
the complete Python performance gate still fails. This report supersedes earlier
implementation reports. Independent testing, documentation acceptance and
review of this candidate remain with DAL-202.

## Revisions and integration

- Starting F5: `643dd76cbe4b5eb02c6da8c40f9c6803ebb3a467`,
  tree `e6ca70b9c06453fe5c20193c75666e4566049cc5`.
- Master baseline: `b4e8b56135b5cfcbbe2ddd8d753921dd40d6caa2`,
  tree `f531d1858b881d3cf352c05c4e461e34f4263502`.
- Merge candidate: `270d275ae237c040e54bb4adeb45992fff27f394`,
  tree `577f85ab80574d593320a4929b1eba3c14ad817e`.
- Final tested code: `337ee079ddf608b3f64099e30a56f20173adaffe`,
  tree `a341265e451ea391095c958aee98308a01f17dc5`.
- Publication adds only this report. The attachment's
  `published-revision.json` records the published SHA/tree and source equality.
- Existing [draft PR #372](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/372)
  remains on `feature/dal-202-compiled-observations` and is retargeted to master.

The merge retains both parents; no force push, dependency rewrite or diagnostic
PR #373 content is used. Sixteen reported conflicts largely came from squashed
F4 ancestry. Reconstructing each overlap against F5's original F4 base
`5c954ca2fdded2ca35ad15c7fef44d90a002007d` gave clean source resolutions.
Two methodology passages required combining F5 compiled-history text with
F4 terminal-root rules. Master documentation and completed-artifact cleanup
are retained rather than resurrecting superseded material.

The merge changes three product files relative to the starting F5 head:
`dal-cpp/dal/math/aad/aad.hpp`,
`dal-cpp/dal/script/simulation.hpp`, and
`dal-cpp/dal/script/visitor/evalstate.hpp`.
It also imports `dal-cpp/tests/math/aad/test_payoff_root.cpp`.
The AAD adapter, typed seed storage and root test match master byte-for-byte;
simulation combines master's `LocalCheckedPaths_` with F5 execution-mode checks.
All other F5 product/test changes are retained unchanged by the merge.

The only subsequent product edit is
`dal-cpp/dal/script/visitor/compiler.hpp`; the remaining new edit is this report.
The full diff against master stays within the existing 24-file F5 scope.
No new documentation/CHANGELOG decision, public argument/default/error contract,
benchmark, CI, threshold, skip, RNG, curve or threadpool change is introduced.

## Dispatch change and RED/GREEN boundaries

The first integrated candidate passes correctness but fails the unchanged
complete Python performance gate: **81/90**, versus **63/63** C++ cases.
The nine Python failures include compiled barrier prices and bumped barrier
Greeks, with the larger repeated deltas approximately 12–34%.
Baseline A/A is **88/90**, failing barrier AAD tree and generic n5/t100 quote
risk; original candidate A/A is **90/90**. These controls limit attribution
of small changes and do not turn the failed paired gate into a pass.

Source and GCC 14 assembly show that `LoadObservation` and `Discard`
comparisons precede every ordinary instruction. The repair restores the
existing ordered opcode ranges first, then handles the two prepared-only
operations in `EvalCompiledPrepared`. Opcode integers, operand consumption,
observation addressing, eager booleans, kernels and errors are unchanged.
The repaired assembly confirms ordinary instructions bypass these two tests.

The performance gate itself supplies RED evidence; no output regression was
invented for an ordering-only performance change. This is the justified TDD
adaptation: use the real failing timing contract and retain unmodified
independent correctness oracles rather than assert source structure in a test.
After the initial reorder, the focused native suite passes **454/454**.
While green, the prepared-operation helper was extracted to satisfy the
complexity limit: dispatcher **7**, helper **3** (initial reorder **9**).
Final verification below follows that refactor.

Final Python paired result is **FAIL, 82/90**. The eight failures are:

| Case | Round 1 | Round 2 |
| --- | ---: | ---: |
| calibration.PWL.BUMPED.solve | +4.70% | +7.27% |
| comparison.mc_barrier_greeks_16384 | +23.41% | +21.65% |
| comparison.mc_barrier_greeks_65536 | +20.02% | +17.39% |
| comparison.mc_barrier_price_16384 | +10.80% | +11.53% |
| comparison.mc_vanilla_greeks_65536 | +6.04% | +7.24% |
| comparison.mc_vanilla_price_65536 | +14.68% | +8.00% |
| mc.barrier.aad.compiled | +6.23% | +9.22% |
| mc.barrier.double.compiled | +21.85% | +15.07% |

Final C++ paired result is **PASS, 63/63**, with Sobol precise/fast ratio
**8.59x** against the unchanged **10x** limit. Final candidate A/A is
**FAIL, 87/90** on byte-identical binaries: LOG_LINEAR BUMPED solve
**+4.15%/+11.18%**, comparison multi-joint calibration 15
**+5.15%/+4.35%**, and vanilla AAD tree **+5.14%/+6.77%**.
These are distinct from the eight final paired failures. The A/A failures
confirm local timing instability; they do not exonerate the repeated paired
barrier failures or establish that the candidate meets the timing contract.

Assembly establishes the removed dispatch overhead; these whole-suite timings
do not establish that it was the dominant cause or quantify its isolated
benefit. In particular, repeated barrier regressions remain unresolved.
The timing contract has RED evidence but **no GREEN performance result**.
The correctness results below are GREEN only for functional behavior.
No wider repair is inferred from the changing near-threshold calibration and
vanilla cases. The failed gate remains an acceptance blocker for DAL-202;
this draft publication does not waive it or claim a completed performance fix.

No unchanged gate is rerun to seek a green outcome. The original failures,
raw samples and controls remain in the evidence alongside final results.
The baseline A/A control uses the unchanged baseline binary; it is not rerun.

## Final correctness verification

All results here are fresh on final tested code `337ee079`.

- GCC 14 Release build/install and full native CTest: **1744/1744**.
- Focused script/simulation/AAD/compiler/visitor tests: native **454/454**,
  Adept **453/453**, CoDiPack **453/453**, XAD **452/452**.
- All **33** legacy/parity/fuzz suite cases pass on every backend.
- Unchanged exact-boundary reviewer reproductions: **4/4**.
- Unchanged signed tiny fuzzy-divisor reproductions: **6/6**.
- Isolated native-double allocation fixture: **2/2**.
- Python tests on final performance build: **402/402**; baseline and original
  integrated binaries independently passed the same **402** tests.
- Documentation structure checks: **58** current Markdown files.
- Whitespace checks pass. Gate infrastructure tests: **160**, with six
  pre-existing skips; no skip or test was edited.

The 27 compiled lifetime combinations retain threads 1/2/4, 8193 paths,
80/90/80 fixing state, discounted/direct/constant roots and every price/risk
assertion. The lifetime test's complexity remains **4**, helper **1**.
Native full/targeted counts increase by one over old F5 because the merged
F4 terminal-node test is included. Exact backend inventory differences are
captured in `backend-inventory-differences.json`.

Independent oracles remain controlling: T06/T09/T10/T18 path values and all
risks; same-width fuzzy-double and smooth K finite differences; signed tiny
and computed divisors; adjacent exact comparisons; nested and cross-event
fractional state; hard history and settled PAYS discard; typed historical
parameter seeds; strict syntax-wide prefetch; eager AND/OR; zero worker
submissions on preparation/compiler failure; and exception drain.
Direct, constant and empty-suffix roots and per-path rebuild references pass.

The allocation fixture counts C++ allocation requests after evaluator/scenario
construction for 8193 exact/fuzzy tree/compiled evaluations, with a positive
control and rejecting history/index seams. It does not measure AAD tape
allocation or arbitrary malloc. Integer-addressed observation reads remain
source-inspected; this is not a dynamic count of every load. The compiler
failure seam remains immediately before bytecode construction.

## Reproduction and evidence

`commands.md` contains exact commands, working directories, timestamps,
exit codes and log names for every captured invocation. Principal commands:

```sh
cmake --preset=Release-linux -S . -B build/Release-linux -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14
cmake --build build/Release-linux -j8
DAL_NUM_THREADS=4 ctest --test-dir build/Release-linux --output-on-failure -j4
DAL_NUM_THREADS=4 ./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
```

Alternate builds select exactly one backend, disable examples, and run the
same filter. Separate detached performance sources/builds identify baseline,
original integration and repaired code. Each uses GCC 14, Release, native AAD,
native CPU tuning, Python 3.13.9, pybind11 3.0.4 and four threads.
Sampling uses the original full inventories, ten interleaved samples per side
per round, two rounds, minimum reduction and the unchanged strict 4% rule.
Builds/tests finish before timing. Linux is WSL2 on this local machine; no
other CPU-heavy Linux process was observed, but Windows-host load is not
controlled. Local evidence does not replace hosted required checks.

Authenticated inherited CI triage archive SHA256:
`623e9c528c8305843211d6f3e1d162336db7750d087bad2d815cc3d24fa99601`;
all nine entries verified. Its old hosted F5 Python **82/90**, C++ **63/63**,
baseline A/A **90/90** and head A/A **89/90** are preserved as historical
evidence, not candidate acceptance. The prior fuzzy-repair archive SHA256
`f1d3ded23bde9782688cfcbb8c60c96f6c2c165a5f4c67b449d70e0942268bb8`
and all 73 entries were verified before reusing its unchanged reproducers.

The final archive includes source hashes, both source lineages, conflicts,
source snapshots, exact commands, build identities/binary hashes, assembly,
all fresh logs, raw paired/control samples and SHA256 manifest. Report-only
publication must match `final-source-hashes.json`.

## Handoff boundaries

One published-head CI snapshot is attached. No CI watch, sleep/retry polling,
merge or closing intent is performed. Hosted checks and independent
DAL-229 testing → DAL-230 documentation/CHANGELOG decision → mandatory DAL-231
review remain for parent acceptance. Old approvals do not approve this code.

No Windows XLL, sanitizer, full alternative-backend public/Excel suite or
public named-fixing binding validation is claimed. DAL-223 remains deferred
without waiving required gates. DAL-228 returns to in_review for report
acceptance; DAL-202 owns acceptance and eventual guarded master merge.
