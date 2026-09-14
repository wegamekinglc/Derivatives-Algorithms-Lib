# DAL-202 F5 AAD execution repair

DAL-228 moves the standard AAD script evaluator entry points into core while
sharing their existing generic bodies. Isolated shared-library measurements
support a partial reduction in Monte Carlo cost. The complete gates below
control acceptance; diagnostic improvement alone does not complete F5.

## Revisions and changed files

- Baseline master: `b4e8b56135b5cfcbbe2ddd8d753921dd40d6caa2`,
  tree `f531d1858b881d3cf352c05c4e461e34f4263502`.
- Starting publication: `0fffdc143ed9da1b631115fd28a9b64638ad5372`,
  tree `5d2bca5433506cc0e4a771a4c848353ced2afac2`.
- Previous tested code used in diagnostics: `a0ffbd9a825ed10f678b94f803b659beac8f99fe`,
  tree `600c60447c1127e191b77aa367aa438338eab5f2`.
- Freshly tested repair: `5395b75e31789d733ff879aa948faf2d6a86d4ac`,
  tree `0ba8901fc1628503ac62740914a3248210828962`.
- Publication adds only this report. Attached `publication.json` records the
  published SHA/tree and equivalence of all 1150 tested source files.
- Existing [PR #372](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/372)
  remains open on `feature/dal-202-compiled-observations`, targeting master,
  with the owner's ready-for-review state preserved.

Product changes are limited to `dal-cpp/dal/script/event.hpp` and new
`dal-cpp/dal/script/evaluation.cpp`: 37 added lines. The header shares each
existing body through private EvaluateImpl and declares two explicit standard
AAD specializations. The core definitions call those same bodies. Generic
types retain their existing evaluator behavior. The third changed file is
this report. No public signature, arithmetic, preparation, observation,
exception, payoff-root, typed-state or batch-lifetime rule changes.

F4 terminal roots, LocalCheckedPaths, double seeds and all previous F5 fixes,
including legacy sample/recursive-dispatch specialization, are retained.
Tests, benchmarks, thresholds, skips, CI, bindings, defaults, curves, RNG and
threadpool code are unchanged. No Machinist enum changes require regeneration.

## Cause evidence and design

The controlling timing RED is inherited Python **81/90** on `a0ffbd9a`:
nine repeated failures across AAD, vanilla and one cross-currency case.
That complete gate is not rerun unchanged. Its samples, earlier failures and
A/A controls remain in the inherited archive.

Fixed scenarios isolate recording and propagation from preparation, Python,
path generation and scheduling. Each process evaluates 200000 one-event vanilla
or 54-event barrier paths, with one barrier observation inside the fractional
band. Analytic PV and every spot, numeraire and parameter risk are checked.
MC probes add 65536 Sobol paths and four threads and compare all model risks.
Every comparison retains ten interleaved process samples per variant with
alternating order and minimum reduction.

Native executable probes do not reproduce the full shared-library cost.
Public code uses GCC14 -O3 -fPIC without core's native tuning; shared libraries
also retain dynamic TLS resolution that executables can relax. Shared probes
reproduce the compiled recording regression: for fixed barrier paths, previous
recording minimum is 0.416912 s versus master 0.317871 s; reverse propagation
is approximately 0.022 s for both. Analytic values and risks confirm equivalent financial outputs;
instruction and tape-node counts are not measured.

Assembly identifies changed inlining around payoff roots and compiled
arithmetic. Outlining root handling, caching a batch-local tape pointer, and
extern template instantiation were each tested and rejected as complete fixes:
none improves all affected MC paths consistently. In particular, extern
instantiation still permits caller expansion. Their source overlays, assembly
and every sample are retained; none is imported into product code.

Actual explicit specialization prevents standard AAD entry-point expansion in
public callers and uses core compilation. The supported intervention is
execution placement, not a claim that TLS or one instruction explains all cost.

| Shared MC probe | Master minimum (s) | Previous minimum (s) | Repair minimum (s) |
|-----------------|--------------------|----------------------|--------------------|
| Vanilla tree | 0.004183300 | 0.004689529 | 0.003844925 |
| Vanilla compiled | 0.004167731 | 0.004569020 | 0.003917615 |
| Barrier tree | 0.074567038 | 0.079499663 | 0.072131751 |
| Barrier compiled | 0.067459232 | 0.083418874 | 0.073617393 |

These reductions against previous code are 18.01%, 14.26%, 9.27% and 11.75%.
Limits are material: repaired barrier compiled MC still trails master by
9.13%; fixed barrier compiled total improves 4.19% but still trails master
by 23.52%. Fixed vanilla compiled does not improve. The intervention explains
a partial benefit, not the remaining attribution or the complete timing result.

## RED/GREEN and fresh correctness

The authorized timing-only TDD adaptation uses a genuine failed timing
contract rather than inventing a failing value assertion. Existing independent
correctness tests are unchanged. Fresh before-change native 1744/1744 and
focused 454/454 pass; after the minimum change, focused 454/454 passes and
shared bodies avoid duplicated semantics. No design-contract deviation is made.

Fresh verification on `5395b75e`:

- GCC14 Release build/install and full native CTest: **1744/1744**.
- Focused native/Adept/CoDiPack/XAD: **454/453/453/452**, all passing.
- Each backend passes **30 legacy parity/fuzz cases** plus **3 observation
  parity cases**. This clarifies the prior report's combined count of 33.
- Unchanged exact-boundary reproductions: **4/4**; signed tiny divisors: **6/6**;
  isolated double allocation fixture: **2/2**.
- Python: **402/402** on both candidate and verified baseline.
- Eight script_mc_perf smoke cases complete; they are informational and do
  not replace or extend the original nine-target C++ gate.
- Documentation checker and whitespace checks pass.

Existing tests retain analytic/unoptimized PV and all-risk oracles, hard history,
settled PAYS discard, live parameter seeds, same-epsilon fuzzy double and smooth
finite differences, exact boundaries, signed tiny/computed divisors,
nested/cross-event fractional state, final IF metadata, compile failure before
worker submission, eager booleans, syntax-wide reads, exception drain and
direct/constant/empty-suffix roots. The 27 compiled lifetime combinations retain
threads 1/2/4, 8193 paths and fixing sequence 80/90/80.

Allocation measurement covers C++ allocation requests after construction over
8193 exact/fuzzy tree/compiled paths, including a positive control and rejecting
history/index seams. It excludes AAD tape allocation and arbitrary malloc.
Integer addressing is source-inspected; reads are not dynamically counted.
Initial setup failures from absent submodules and incomplete diagnostic build
dependencies are retained in logs; after initialization all required builds pass.

## Complete performance gates

C++: **PASS, 63/63**, including Sobol precise/fast ratio **8.39x**
against the unchanged 10x ceiling. Python: **FAIL, 81/90**.

| Python case | Round 1 | Round 2 |
|-------------|---------|---------|
| comparison.mc_barrier_greeks_16384 | +11.89% | +10.54% |
| comparison.mc_barrier_greeks_65536 | +4.25% | +4.64% |
| comparison.mc_vanilla_greeks_16384 | +20.91% | +23.08% |
| comparison.mc_vanilla_greeks_65536 | +18.24% | +22.43% |
| comparison.mc_vanilla_price_16384 | +12.24% | +4.12% |
| mc.barrier.aad.compiled | +29.13% | +30.98% |
| mc.barrier.aad.tree | +9.03% | +6.72% |
| mc.barrier.double.compiled | +6.98% | +6.85% |
| mc.vanilla.aad.compiled | +14.69% | +13.58% |

Vanilla AAD tree now passes (+0.56/+2.39%), as do vanilla double compiled
(+2.36/+3.71%) and cross-currency BUMPED solve (-1.17/-1.46%). Three
previously passing double/bumped barrier cases fail this run. Results from
different gate runs do not establish a direct before/after speedup. The
complete gate does not validate the isolated diagnostic improvement.

The built Python module exports both new evaluator specializations; this
confirms that the tested artifact includes the change. The 65536-path
diagnostic excludes public product/model construction and preprocessing,
while actual native Python AAD cases use 20000 vanilla/10000 barrier paths
and time the full public boundary. Link context and workload differences
remain unresolved attribution limits.

Fresh byte-identical candidate A/A: **FAIL, 88/90**.
Both native-module hashes and the copied CMake cache match. It uses the
same full inventory and original two-round/ten-sample/4% rule.

| A/A case | Round 1 | Round 2 |
|----------|---------|---------|
| comparison.calibration_single_15 | +4.02% | +4.67% |
| comparison.mc_vanilla_price_16384 | +13.76% | +6.71% |

The control does not waive any master/candidate failure. There is one
paired gate and one A/A control on this candidate, with every sample kept.

**Overall: the required Python gate finds regressions; timing GREEN is
not achieved.** Functional correctness and a partial isolated benefit do
not satisfy performance acceptance.

Both original complete gates use **two rounds of ten interleaved samples per
side, minimum reduction, strict 4%**. No benchmark inventory, aggregation,
threshold or environment-validation rule changes. No unchanged paired gate is
rerun to seek green. All builds and correctness checks finish before sampling;
the Python and C++ gates run sequentially.

Both sides use GCC14 Release, native AAD/CPU tuning, Python 3.13.9, pybind11
3.0.4 and DAL_NUM_THREADS=4. Candidate source/build are isolated and detached
at the tested code revision. Baseline reuse verifies its clean commit/tree and
all 23 retained binary hashes, with fresh Python correctness. Source equivalence,
submodule revisions, CMake caches, core/public/Python flags and binary hashes
are attached. This WSL2 host has uncontrolled Windows load. Local results do
not replace hosted required checks; host uncertainty does not waive failures.

The inherited timing archive SHA256 is
`ad2c2eef08c6397adf960a35dc696a87bbd1b7610bd2811fb854ade9ef28ecf9`.
It retains previous Python 81/90, C++ 63/63, distinct candidate A/A 89/90,
and nested integration evidence SHA256
`f873433b1d09c6fb0cec0e6f680f110d4cc09772a921ee2fe841b972194d326f`.
These are historical evidence, not current acceptance.

## Reproduction and handoff

Evidence includes exact command argv/cwd/UTC/exit/duration, logs, source/binary
identities, diagnostic sources and rejected overlays, assembly, all raw complete
gate samples, publication/CI snapshots and SHA256 manifests. Principal commands:

```sh
cmake --preset=Release-linux -S . -B build/Release-linux -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14
cmake --build build/Release-linux -j8
DAL_NUM_THREADS=4 ctest --test-dir build/Release-linux --output-on-failure -j4
DAL_NUM_THREADS=4 ./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
python3 .github/scripts/check_python_benchmark_regressions.py --base-source BASE_SOURCE --head-source CANDIDATE_SOURCE --base-root BASE_BUILD --head-root CANDIDATE_BUILD --output-dir python-paired --samples 10 --confirmation-rounds 2 --threshold-percent 4
python3 .github/scripts/check_benchmark_regressions.py --base-root BASE_BUILD --head-root CANDIDATE_BUILD --output-dir cpp-paired --samples 10 --confirmation-rounds 2 --threshold-percent 4
```

The inherited archive records the RED command and failed results. Attached
commands.md resolves all placeholders and records the fresh GREEN correctness
and current complete gate commands. Alternate builds enable exactly one backend.
No installed benchmark executable is used.

One CI snapshot follows publication. Pending/missing checks are reported for
that exact published head; prior runs cannot approve it. No CI watch/poll,
merge, close intent, new specialist chain, PR #373 import or F6 work occurs.
DAL-223 remains deferred. No Windows XLL, sanitizer or full alternate-backend
public/Excel verification is claimed.

DAL-228 returns to in_review for parent report acceptance. DAL-202 owns serial
DAL-229 independent testing, DAL-230 documentation/CHANGELOG decision and
mandatory DAL-231 review. Earlier approvals do not cover this change.
The remaining evidence needed is phase attribution on the actual linked Python
module and exact failing workloads, separating public preparation, recording,
propagation and result conversion with matching path/batch counts. Compiled
AAD recording is a supported focus, but these probes do not prove the complete
cause. No curve/RNG/threadpool or benchmark-policy change is justified. Parent
must decide the next bounded investigation before performance acceptance or
advancement; this report does not approve either.
