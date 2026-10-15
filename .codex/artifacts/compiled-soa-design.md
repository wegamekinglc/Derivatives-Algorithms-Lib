# Compiled double SoA study (DAL-357 / #576 / WP-11)

Status: isolated research prototype; adoption depends on the paired evidence and
independent focused review. This artifact controls the active research decision.
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

Pending correctness and paired measurement. Record immutable source/build
identities, raw evidence, noise, numerical error and the adoption boundary here
before delivery.
