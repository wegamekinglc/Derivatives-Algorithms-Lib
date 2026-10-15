# Compiled double SoA study (DAL-357 / #576 / WP-11)

Status: measured isolated research prototype; pending independent focused review.
The general 2x whole-request target is a no-go for the weekly barrier workload.
This artifact controls the active research decision.
It adds no public feature and changes neither the default tree engine nor the
production compiled evaluator, model or RNG.

## Prototype and supported domain

`dal-cpp/experimental/compiledsoa` is a separately configured CMake project.
`Plan_` consumes a copy of the existing `ScriptCompiled_` legacy hard-opcode
streams, validates operands and stack balance, and resolves stack slots once.
Each double stack slot, boolean stack slot and variable has 8, 16, 32 or 64
contiguous path lanes. Dispatch occurs once per opcode, followed by a lane loop;
there are no explicit ISA intrinsics or approximate math functions. Compiler
auto-vectorization is permitted under the existing DAL floating-point flags.

IF and IF/ELSE produce disjoint 64-bit masks. The 64-lane mask avoids a shift by
64; tail blocks evaluate only their valid lanes. Nested branches retain parent
masks. Unselected lanes do not execute branch arithmetic. Validated branch bodies
are statements with balanced stacks, so they do not depend on a parent's stack.
When both outcomes contain at least a quarter of the active lanes, the optional
fallback copies variables into a reusable scalar state, runs just the selected
branch through the existing `EvalCompiledRange<false>`, then copies variables
back. The masks-only mode is measured separately for the divergent case.

Supported operations are spot/constants/constant variables, scalar variables,
assignment, PAYS, arithmetic excluding powers, extrema with preserved operand
order, sqrt/log/exp/negation, hard comparisons and boolean operations. Other
opcodes, malformed operand/range/stack layouts, and IFs inside expressions are
rejected when constructing the plan. This is a deliberately bounded prototype,
not a replacement interpreter with silent unsupported-operation fallback.

`BatchModel_` implements exact single-factor Black-Scholes log-spot recurrence
using the production step coefficients and deterministic numeraire helpers. It
gathers the same path-major Gaussian doubles into an event-major lane output.
No inverse-normal implementation is added or changed. Model tests compare each
sample and variable with `BlackScholes_<double>` and compiled scalar evaluation.
Paths remain in original order, including the final serial PV summation.

The boundary excludes AAD/tapes, fuzzy control, prepared observation plans,
discounted-on-date PAYS, vectors, EXERCISE/LSMC, portfolios, multiple workers,
correlated BS, local volatility, rates and hybrid models. Extending to those
domains needs model state/observation layout, validation and independent numerical
evidence; this study does not confer admission. Legacy forward placeholders are
unused by the supported opcode set. Finite, valid BS paths are the measured
domain; the prototype does not replace production per-path validation.

## Scope and measurement contract

The changed paths are experimental opcode dispatch, SoA BS generation and the
study harness. Their closest existing caller is `script_mc_perf`'s compiled double
vanilla and weekly barrier. The study repeats those two contract shapes with
fixed Sobol DefaultFast (`precise=false`, `polish=false`), path offset zero, no
Brownian bridge, and BS spot/vol/rate/div = 100/0.2/0.05/0.02. Dates are
2024-01-01 to 2025-01-01, with weekly monitoring and a 150 barrier. A separate
110 barrier case supplies frequent divergence without making every payoff zero.
No #574 inverse-normal work or earlier #567 samples are used.

Selected cases: evaluator and end-to-end at every lane width for vanilla and
weekly barrier; model-only and fixed-shock pipeline at width 32; frequent
divergence with fallback on/off at width 32; small requests of 1, 7, 9 and 65
paths. Main requests have 16,387 paths to include a tail. Requests below eight
paths select scalar execution. The prototype supports shorter tail blocks.

The stages have distinct timing boundaries:

- `eval`: same scalar-model-generated samples, materialized as native scalar
  scenarios and SoA rows before timing. Scalar evaluation reads its own native
  scenarios directly. Layout conversion is excluded for both engines.
- `model`: same precomputed Gaussian paths; generate all spot samples and expose
  the output buffer to the benchmark optimization barrier. Includes SoA gathering.
- `pipeline`: fixed Gaussian paths, generation plus evaluation; includes layout
  work and fallback transfers.
- `end_to_end`: includes fresh Sobol construction/filling, generation and
  evaluation in the same path order. No claim that evaluator gains equal RNG or
  whole-request gains is permitted.

All stages reuse compiled product/plan and workspaces. Parsing, preprocessing,
compilation, plan decoding and allocation are outside timing, including the small
request measurements. Cold request latency is not measured. Each process warms
up once; calibration selects a common iteration count giving the faster engine
at least 50 ms of measured work. Raw time is divided by that count.

The nine-target regression gate contains no changed production caller; none of
its targets is run. These results are informational research admission evidence,
not a scheduled-gate pass. Full regression and cross-platform checks belong to
existing CI. Tests are scoped to the prototype's numerical and control-flow
boundaries. A dedicated sanitizer executable instruments the new header and test
code, while the unchanged DAL library remains a Release build.

## Reproduction

From the repository root, initialize the pinned Eigen, rapidjson, Machinist and
Google Test submodules. Use a dedicated task directory, preserve other work, and
hold the roadmap's shared flock for each build/test and the entire formal sample
round. Check lock-external load before timing.

```bash
cmake -S dal-cpp/experimental/compiledsoa -B build/soa \
  -DCMAKE_BUILD_TYPE=Release -DDAL_ENABLE_NATIVE_ARCH=OFF
cmake --build build/soa --target compiled_soa_study compiled_soa_tests \
  compiled_soa_sanitized_tests --parallel 8
ctest --test-dir build/soa -R '^compiled_soa_tests$' --output-on-failure
ASAN_OPTIONS=detect_leaks=1 build/soa/compiled_soa_sanitized_tests
build/soa/compiled_soa_study --workload weekly --phase end_to_end \
  --engine soa --lanes 32 --paths 16387 --iterations 10
```

For paired measurement, use separate detached source worktrees and build roots.
The baseline is production master `01dcadace1180629e3b4467f5334c021c530054a`.
As no production file changes, overlay only the exact committed experimental
directory onto the baseline worktree to provide the common workload. Record the
overlay file hashes, both source SHAs, submodule identities, CMake caches, compiler
and linked executable hashes. Both executables include identical study code;
select `scalar` from the baseline executable and `soa` from the head executable.
Never modify production baseline code or compare installed binaries.

```bash
DAL_NUM_THREADS=1 python3 dal-cpp/experimental/compiledsoa/pairedstudy.py \
  --baseline <baseline-build>/compiled_soa_study \
  --head <head-build>/compiled_soa_study --output <new-evidence-directory>
```

The runner collects two rounds of ten paired process samples, alternating first
engine on every pair. It retains commands, stdout/stderr and return codes,
validates configuration and Gaussian fingerprints, and requires PV relative
agreement of 1e-12 (absolute 1e-14 only for exactly zero reference PV). Each round
uses the minimum; sustained movements beyond 4% receive an informational
improvement/regression classification. The 2x target is achieved only when both
rounds exceed 2x after a 4% noise margin; nearby results are inconclusive.

## Evidence and decision

Measured source: `f4681d6f72919f5fef8025ff2c94c3dc62b6f45f`; baseline:
`01dcadace1180629e3b4467f5334c021c530054a`. The experimental directory Git tree is
`1681e58a7555df81e437cfa594ace7e462a28a66`. Only this design note changes after
sampling; its final commit carries identical executable inputs. Detached sources
and separate build roots are under the task's `measurements/{baseline,head}-source`
and `measurements/{baseline,head}-build`. Baseline overlay file hashes exactly
match the committed head study files; production baseline files are untouched.

Environment: Intel Core i9-13900HX, 32 logical CPUs, Linux under Microsoft WSL2,
GCC 15.2.0, CMake 4.2.3, Unix Makefiles, Release `-O3 -DNDEBUG
-ffp-contract=fast`, portable architecture (`DAL_ENABLE_NATIVE_ARCH=OFF`), static
DAL, native AAD backend, Eigen enabled, benchmarks explicitly ON. Both programs
initialize one worker, with `DAL_NUM_THREADS=1`; the sampling process and children
are pinned to CPU 2. No AAD calculation is performed. Pinned dependency commits
and complete caches are in `provenance.json` in the evidence attachment.

Build-tree executable SHA-256:

- Scalar baseline: `ec5e60efa5aed5011ce0859378b580ea16bdadc20c384967dd94ebb6c34edd00`.
- SoA head: `495c32370228f115daf87da3d192e3726b8057cf8feda3245df06f17764074a0`.
- Raw `results.json`: `6e263106effa1ace6bea5847bcd53f5733a92af4925756ea3a9f990b7f9b219c`.

The exact configure/build/CTest commands above passed in both isolated builds.
All seven focused tests passed; the separate ASan/UBSan executable also passed
all seven tests with `ASAN_OPTIONS=detect_leaks=1`. Sanitizer coverage applies to
the prototype/header/test code, with unchanged DAL linked as Release code. The
first local attempt had a test-input grammar error (`NOT` instead of `!=`), fixed
before these results and before any timing. No production algorithm was repaired.

Formal sampling ran `pairedstudy.py` once under the shared flock, with the two
build-tree executable paths, `--output paired-results`, CPU affinity 2 and the
thread setting above. All 28 cases completed: 560 pairs / 1,120 timed processes,
plus 56 calibration processes. Both rounds have ten samples per side, alternating
first position, reducing with `min` and retaining every output. Executable hashes
were unchanged after sampling. Maximum observed scalar/SoA PV difference was
exactly zero, satisfying relative 1e-12. Vanilla PV was 9.236220800417145, weekly
PV 7.139904685510628, and the 110-barrier stress PV 0.187833111152561. Weekly has
54 merged event dates. The 16,387-path Gaussian fingerprints were
`5a00cb6adcd2f117` (vanilla) and `7ceacc7a1ee1c5dd` (both barriers).

This was a shared WSL2 host, not a quiet dedicated performance machine. Before
sampling an external Rust build used about one core; the host was 95% idle in
the interval snapshot and 99% idle afterward, with no observed interval swap I/O.
The maximum per-round median/min ratio was 1.174. The divergent masks-only
evaluator's scalar minimum moved 20.6% between rounds; its precise gain is
inconclusive. Lane rankings and cross-case absolute times are also inconclusive.
The key within-case endpoint ratios are much steadier: vanilla L16 minima drift
by -1.1%/-1.6% (scalar/SoA), weekly L32 by +0.9%/+1.5%. Borderline 2x results are
left inconclusive; no blanket machine-independent throughput claim is made.

The table shows each side's overall minimum in ms, both round speedups, and both
duration deltas in percent. `F` means the high-divergence fallback is enabled.
All rows use 16,387 paths except the final four small-request rows.

| Case                       | Scalar ms | SoA ms    | Speedup R1/R2 | Delta R1/R2 (%) | 2x target              |
|----------------------------|-----------|-----------|---------------|-----------------|------------------------|
| vanilla eval L8 F          | 0.367110  | 0.065863  | 5.574/5.629   | -82.1/-82.2     | achieved               |
| vanilla end L8 F           | 0.675103  | 0.347838  | 1.941/1.936   | -48.5/-48.3     | inconclusive           |
| vanilla eval L16 F         | 0.359437  | 0.038720  | 9.303/9.182   | -89.3/-89.1     | achieved               |
| vanilla end L16 F          | 0.652854  | 0.305723  | 2.124/2.135   | -52.9/-53.2     | achieved               |
| vanilla eval L32 F         | 0.313287  | 0.028319  | 11.017/11.063 | -90.9/-91.0     | achieved               |
| vanilla end L32 F          | 0.592370  | 0.305962  | 1.948/1.936   | -48.7/-48.3     | inconclusive           |
| vanilla eval L64 F         | 0.314909  | 0.034798  | 8.936/9.050   | -88.8/-88.9     | achieved               |
| vanilla end L64 F          | 0.601577  | 0.318627  | 1.898/1.880   | -47.3/-46.8     | not achieved           |
| vanilla model L32          | 0.132716  | 0.072528  | 1.830/1.853   | -45.4/-46.0     | not achieved           |
| vanilla pipeline L32 F     | 0.405038  | 0.102477  | 3.952/3.962   | -74.7/-74.8     | achieved               |
| weekly eval L8 F           | 13.262312 | 3.430540  | 3.884/3.866   | -74.3/-74.1     | achieved               |
| weekly end L8 F            | 14.571106 | 11.034620 | 1.287/1.320   | -22.3/-24.3     | not achieved           |
| weekly eval L16 F          | 12.749144 | 2.514573  | 5.486/5.033   | -81.8/-80.1     | achieved               |
| weekly end L16 F           | 14.914854 | 10.080625 | 1.501/1.472   | -33.4/-32.1     | not achieved           |
| weekly eval L32 F          | 12.366856 | 2.480290  | 4.986/4.872   | -79.9/-79.5     | achieved               |
| weekly end L32 F           | 14.782707 | 9.512032  | 1.554/1.545   | -35.7/-35.3     | not achieved           |
| weekly eval L64 F          | 14.434173 | 2.451363  | 6.161/5.888   | -83.8/-83.0     | achieved               |
| weekly end L64 F           | 14.353319 | 9.324996  | 1.539/1.579   | -35.0/-36.7     | not achieved           |
| weekly model L32           | 4.046778  | 3.514050  | 1.148/1.152   | -12.9/-13.2     | not achieved           |
| weekly pipeline L32 F      | 11.168947 | 6.058577  | 1.843/1.905   | -45.8/-47.5     | not achieved           |
| divergent eval L32 F       | 12.351095 | 9.153504  | 1.354/1.349   | -26.1/-25.9     | not achieved           |
| divergent end L32 F        | 15.473058 | 16.098914 | 0.951/0.965   | +5.2/+3.7       | not achieved           |
| divergent eval L32 masks   | 12.319041 | 4.443111  | 2.697/3.343   | -62.9/-70.1     | magnitude inconclusive |
| divergent end L32 masks    | 14.854306 | 11.052779 | 1.339/1.344   | -25.3/-25.6     | not achieved           |
| weekly end L32 F, 1 path   | 0.001341  | 0.001349  | 0.994/0.995   | +0.6/+0.5       | not achieved           |
| weekly end L32 F, 7 paths  | 0.006198  | 0.006292  | 1.034/0.985   | -3.3/+1.5       | not achieved           |
| weekly end L32 F, 9 paths  | 0.007873  | 0.006168  | 1.268/1.276   | -21.1/-21.7     | not achieved           |
| weekly end L32 F, 65 paths | 0.055601  | 0.035013  | 1.572/1.609   | -36.4/-37.8     | not achieved           |

Informational verdict: improvement for the measured SoA cases, except no-change
for the 1/7-path scalar bypass and the divergent fallback endpoint. The latter
does not meet the repository's sustained >4% two-round regression rule. Its
fallback nevertheless removes the masks-only endpoint benefit: 0.95-0.97x versus
1.34x. It performs 490,328 lane-event scalar transfers per request; the weekly L32
case has 4,452 mixed branches and zero transfers. Reject this quarter-minority
heuristic for production use. Do not interpret its correctness as evidence of a
performance benefit.

Stage attribution at L32: model-only is 1.83-1.85x for vanilla and 1.15x for weekly;
materialized evaluator execution is about 11x and 4.9x; fixed-shock pipelines are
3.95-3.96x and 1.84-1.91x; endpoints including RNG are about 1.94x and 1.55x.
Evaluator gains include predecoding, layout and dispatch amortization, not a
separately identified ISA-only SIMD contribution. Native scalar scenarios occupy
more memory than SoA spot rows. Streaming pipeline working sets differ from
materialized evaluator inputs, so these stage timings are not additive. None of
the improvement is assigned to a changed inverse-normal implementation.

Decision: retain the reproducible isolated prototype and the positive bounded
evaluator result; do not integrate a new public engine or switch defaults. The
general >=2x whole-request goal across BS plus weekly barrier is not achieved.
Vanilla L16 does achieve 2.124/2.135x on this host after the 4% margin, while weekly
misses at all four widths. Small requests have a safe scalar bypass and positive
9/65-path gains, but cold compilation/allocation costs remain unmeasured. No
extension-model or AAD adoption follows from this experiment. Any production
adoption needs a separate authorized implementation, a replacement/rejection of
the fallback heuristic, and caller-specific evidence; it is not part of this PR.

Raw `provenance.json`, `isolated-build.log`, load/VM snapshots, `sampling.log`, and
`paired-results/{manifest.json,results.json,summary.md,case-*}` are retained in
the task evidence attachment. Each case directory contains every command,
stdout/stderr and exit status. No full regression, nine-target sweep, #567 timing
rerun or #574 measurement was performed. Remote required CI and independent
focused review remain distinct publication/acceptance requirements.
