# DAL-202 F5 compiled execution repair

DAL-228 specializes compiler-produced legacy execution to remove prepared
sample selection and opcode handling from its event loops. Prepared programs
retain the full observation interpreter. This supersedes the integration
report, whose failures and controls remain in the attached inherited archive.
Independent acceptance remains with DAL-202.

## Revisions and scope

- Master baseline: `b4e8b56135b5cfcbbe2ddd8d753921dd40d6caa2`,
  tree `f531d1858b881d3cf352c05c4e461e34f4263502`.
- Starting publication: `b21d4e1aede59672f39eaa2fb49e8a8ff1e81b87`,
  tree `d31dafb0a11b2d206864637d475d8db5f2112892`.
- Tested repair: `a0ffbd9a825ed10f678b94f803b659beac8f99fe`,
  tree `600c60447c1127e191b77aa367aa438338eab5f2`.
- Publication adds only this report. Evidence `publication.json` records its
  exact SHA/tree and verification against 1149 tested source hashes.
- Existing [PR #372](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/372)
  stays open on `feature/dal-202-compiled-observations`, targeting master.
  The owner's ready-for-review state is preserved.

Only `dal-cpp/dal/script/event.hpp` and
`dal-cpp/dal/script/visitor/compiler.hpp` change product code this turn;
the third changed file is this report. F4 terminal roots, LocalCheckedPaths,
double seed storage and every F5 correctness fix are retained. No test,
benchmark, CI, threshold, skip, public contract, binding, default, error,
example, curve, RNG or threadpool change is introduced.

## Cause evidence and design

The controlling RED is the prior complete Python **82/90** result on
`337ee079`, including repeated compiled and bumped barrier failures.
The previous two-comparison reorder was already present and did not
satisfy the gate.

The preparation hypothesis was rejected. The affected public entry uses
legacy preprocessing, and a standalone barrier probe emits identical bytecode
on master and the original F5 candidate. It evaluates 54 events on identical
predetermined scenarios 200000 times, checking analytic payoff sum 2300000.
Timing excludes preparation, Python, model generation and worker scheduling.
Ten process samples per variant are interleaved with alternating order.

Source and assembly identify historical/plan sample selection inside every
event and prepared opcode handling in outer and recursive interpreter loops.
Both are unnecessary for compiler-produced legacy programs. The original
outer loop is 0x58f5 bytes versus master's 0x5091. Bounded diagnostics:

| Variant                                  | Base minimum (s) | Original minimum (s) | Variant minimum (s) |
|------------------------------------------|------------------|----------------------|---------------------|
| Outline prepared handler                 | 0.066631         | 0.080570             | 0.091497            |
| Select direct samples outside event loop | 0.063966         | 0.079915             | 0.073355            |
| Also specialize outer dispatch           | 0.062908         | 0.081114             | 0.074818            |
| Also specialize recursive dispatch       | 0.064776         | 0.083297             | 0.074780            |

Outlining is rejected. Invariant sample selection and recursive specialization
produce a partial isolated improvement of about 10.2%, still trailing master
by about 15.4% in that comparison. This supports a related product cause,
not complete timing acceptance. Raw samples, overlays and assembly are retained.

The implementation records a private legacy flag only when Build compiles
without a plan or historical replay. Directly constructed bytecode retains the
general interpreter and its existing errors. EvaluateSamples shares event-view
construction; specialization propagates through hard and fuzzy recursion.
Arithmetic, eager booleans, stacks, branch ranges, smoothing, observation
addresses and payment numeraires remain shared. Public EvalCompiled keeps
its signature and behavior; specialized helpers are internal. Final standalone
legacy outer/range functions occupy 0x5091/0x5010 bytes, matching master.
Size equality alone does not establish performance. Maximum changed
interpreter complexity is 8; event evaluation complexity is at most 4.

## RED/GREEN and fresh correctness

This is timing-only work. The justified TDD adaptation uses the genuine failed
timing contract rather than inventing a failing value assertion or testing
template structure. Existing independent correctness oracles are unmodified.
Initial native focused **454/454** and post-change **454/454** pass; shared
helpers avoid duplicated semantics. No public-contract deviation is introduced.

Fresh verification on `a0ffbd9a`:

- GCC 14 Release build/install and full native CTest: **1744/1744**.
- Focused native/Adept/CoDiPack/XAD: **454/453/453/452**, all passing.
- All **33** legacy parity/fuzz cases pass on every backend.
- Unchanged exact-boundary reproductions: **4/4**; signed tiny-divisor
  reproductions: **6/6**; isolated double allocation fixture: **2/2**.
- Python: **402/402** on both the new performance build and retained baseline.
- Documentation structure: **58** Markdown files; whitespace checks pass.

The suites retain analytic/unoptimized T06/T09/T10/T18 oracles and all risks;
hard history and settled PAYS discard; live parameter seeds; same-epsilon
fuzzy double and smooth finite differences; exact boundaries and signed tiny/
computed divisors; nested/cross-event fractional state; final IF metadata;
compile failure before zero worker submissions; eager AND/OR; syntax-wide
history reads; exception drain; and direct/constant/empty-suffix roots.
All 27 compiled lifetime combinations retain threads 1/2/4, 8193 paths and
fixing sequence 80/90/80.

Allocation measurement covers C++ allocation requests after evaluator/scenario
construction over 8193 exact/fuzzy tree/compiled paths, including a positive
control and rejecting history/index seams. It excludes AAD tape allocation
and arbitrary malloc. Integer-addressed loads are source-inspected, not
dynamically counted. Compiler failure injection precedes bytecode construction.

## Complete performance gates

C++: **PASS, 63/63**, with Sobol precise/fast ratio **8.35x** against
the unchanged 10x ceiling. Python: **FAIL, 81/90**. The nine failures are:

| Case                               | Round 1 | Round 2 |
|------------------------------------|---------|---------|
| comparison.mc_vanilla_greeks_16384 | +16.22% | +12.83% |
| comparison.mc_vanilla_greeks_65536 | +22.40% | +17.40% |
| comparison.mc_vanilla_price_16384  | +9.32%  | +34.89% |
| mc.barrier.aad.compiled            | +23.85% | +25.50% |
| mc.barrier.aad.tree                | +10.42% | +10.59% |
| mc.vanilla.aad.compiled            | +12.39% | +16.92% |
| mc.vanilla.aad.tree                | +9.49%  | +8.89%  |
| mc.vanilla.double.compiled         | +5.26%  | +8.21%  |
| xccy.joint.BUMPED.solve            | +4.34%  | +5.48%  |

The previously repeated double/bumped barrier cases pass under the original
rule: barrier Greeks 16384 **+5.83/-0.24%**, Greeks 65536 **+5.48/-0.89%**,
price 16384 **+3.09/+4.87%**, price 65536 **+0.49/-2.39%**, and barrier
double compiled **+4.42/+3.49%**. Passing does not mean both rounds are below
4%. These observations do not establish a completed repair: AAD and vanilla
failures remain, including tree paths not executed by the specialization.

The performance RED has no complete GREEN. Overall verdict: **regression
found by the required gate; unresolved attribution outside the isolated
legacy execution benefit**. No unsupported additional optimization or
unrelated product edit is made. Parent acceptance remains blocked by the
Python contract, regardless of functional correctness.

Fresh candidate A/A control: **FAIL, 89/90**, on byte-identical native modules
and an unchanged copied CMake cache. Only comparison.calibration_single_5
fails, at **+7.74/+4.65%**. This confirms some local timing instability but
is distinct from all nine candidate failures; it does not exonerate them.
Both A/A native-module hashes and all raw samples are retained. There is no
second paired run or control rerun.

Both original complete gates use two rounds of ten interleaved samples per
side, minimum reduction and strict 4%. Builds/tests finish before sampling.
Both sides use GCC 14, Release, native AAD/CPU tuning, Python 3.13.9,
pybind11 3.0.4 and DAL_NUM_THREADS=4. The candidate has a detached source and
separate build. Baseline reuse follows verification of clean revision/tree
and all 23 binary hashes; fresh baseline Python tests pass. The baseline is
not modified.

The host is local WSL2. No other CPU-heavy Linux process appeared at the
recorded pre-sampling snapshot; Windows-host load is uncontrolled.
Local timing does not replace hosted required checks.

Inherited integration **81/90**, previous final **82/90**, C++ **63/63**,
and byte-identical A/A master **88/90**, integration **90/90**, previous final
**87/90** remain in the nested original archive. Its SHA256 is
`f873433b1d09c6fb0cec0e6f680f110d4cc09772a921ee2fe841b972194d326f`;
all 1662 internal manifest entries were verified. These are historical
results, not approval of this repair. No unchanged paired gate is rerun to
seek green.

## Reproduction and handoff

Evidence includes commands.md, command JSON with exact argv/cwd/timestamp/
exit/duration, logs, source/binary hashes, diagnostic source/overlays/assembly,
complete raw gate samples/reports, publication and CI snapshots, and a SHA256
manifest. Principal commands:

```sh
cmake --preset=Release-linux -S . -B build/Release-linux -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14
cmake --build build/Release-linux -j8
DAL_NUM_THREADS=4 ctest --test-dir build/Release-linux --output-on-failure -j4
DAL_NUM_THREADS=4 ./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
```

Alternate builds enable exactly one backend and disable examples. Full gate
commands and resolved source/build roots are recorded in commands.md.
No installed benchmark binaries are used.

One CI snapshot follows publication. Missing/pending checks are reported for
that exact head; old runs cannot satisfy current Linux/Windows CI gates.
No CI watch/retry polling, merge, close intent, new specialist chain, F6 work
or diagnostic PR #373 import is performed. DAL-223 remains deferred.

DAL-228 returns to in_review for parent report acceptance. DAL-202 owns serial
DAL-229 independent testing, DAL-230 documentation/CHANGELOG decision and
mandatory DAL-231 review. No Windows XLL, sanitizer or full alternate-backend
public/Excel verification is claimed. Old approvals do not approve this repair.

The unresolved work is to establish a supported cause for remaining AAD/vanilla
timing failures and satisfy both complete gates after a justified product
change. The isolated legacy benefit and this report do not complete that work
or approve advancement past performance acceptance.
