# DAL-201 / F4 implementation handoff

## Caller snapshot allocation repair; complete acceptance remains open

This round identifies and repairs a specific cross-worker allocation collision.
The fresh complete paired gate passes both previously failing vanilla-double
cases, but finishes **87/90**, so **F4 and the full performance gate are not
accepted**. Baseline A/A and head A/A each finish **89/90**. Their failures are
retained rather than treated as exemptions. This section supersedes the previous
unresolved-object diagnosis; all earlier results remain below.

Starting SHA/tree: `41db438898a054b91df48e333bb2a96b4cd8de08` /
`cd89c6b292b6471eaebed76e86d2e5f3487e11a1`. The prior product code is
`b332fea5f0d9bc9810847ecedcc63e1ac854a9f7`; fixed master baseline is
`8ee1b09dcaf531add9695d466941026da87a4942`. Tested code commit/tree:
`1f9a8b91871365f247297be8fe24a3d535a5beda` / `a38785b9ca91523eda0685e8d768b5318682c6ae`. Publication SHA/tree, binary and source
hashes are attached in `publication.json` and the evidence manifest. The existing
draft PR remains https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371,
base master, on `feature/dal-201-historical-aad-state`; history is preserved.

### Object, ownership and causal intervention

The responsible object is caller state zero's 104-byte
`BlackScholes_<double>::CheckedPaths_`. State zero is constructed on the caller;
states 1-3 are constructed by their worker. All four states are destroyed on
the caller after draining. Traces show caller allocations reusing addresses in
regions previously occupied by worker allocations. Allocator-arena ownership
itself is not inferred solely from an address region.

One measured original in `trace-panel/lllllllllllllllllllllllll/prefix/01-base`
has caller metadata at `0x7fa188002600` and worker 1's `Sample_::spot_` at
`0x7fa188002670`. The metadata's path-vector fields and flags at offsets 72-97
share the 64-byte line beginning `0x7fa188002640` with that writable spot.
Generated code reads metadata flags at offsets `0x60` and `0x61`; the diagnostic
disassembly is retained. The relevant model and sample definitions are in
`dal-cpp/dal/model/blackscholes.hpp` and `dal-cpp/dal/math/aad/sample.hpp`, while
the owning allocation is in the permitted private runner in
`dal-cpp/dal/script/simulation.hpp`.

The full trace records actual addresses, byte extents, creator thread/TID,
constructor-completion and destruction boundaries, and batch owners/timestamps.
It covers caller/worker evaluators, Gaussian buffers, compiled streams, RNG
object/state/directions, CheckedPaths metadata/drifts/stds and samples. The
shared original model is not read by the selected CheckedPaths hot loop.
Constructor timestamps bound observed lifetimes; they are not malloc timestamps.
Records use fixed per-thread static buffers and are written to CSV at process
exit. No trace allocation or logging is added inside the per-path loop. Tracing
still changes code layout and adds boundary overhead; no hardware coherence
counter measurement or exclusive-host claim is made.

A single runtime switch in one diagnostic binary isolates **only caller zero's
CheckedPaths allocation**, with both its address and extent rounded to 64 bytes.
Original / isolated / withdrawn measurements retain the same first-seven suite
prefix, workloads, validators, warmups and interleaved 20 fresh processes each.
The targeted overlap occurs in **18/20 / 0/20 / 12/20** measured calls; compiled
medians are **2.351830 / 1.896406 / 2.325133 ms**. This supports a causal
cache-line sharing mechanism, rather than address proximity alone. Minimum
reductions remain in the raw reports; withdrawal has fast samples and is not a
clean two-round minimum-based regression. One isolated run exposes a separate
possible caller RNG-object/worker Gaussian overlap; no RNG fix is claimed.

The exact old short-path-slower result did not reproduce after relocation:
short/long compiled deltas were -0.69%/-1.20% with an input symlink and
+0.54%/+2.61% after using direct package paths. Later identical-head controls
alongside the full trace reproduce the reversed direction, with long paths
+14.32%/+4.33% slower. Direction is context dependent; changing paths is never
the product fix. Full tracing can remove a minimum-based RED even while many
individual calls still collide, so observations and interventions are reported
separately from gate acceptance.

### Minimum production change and uninstrumented validation

The only product diff is **6 added / 2 removed lines** in
`dal-cpp/dal/script/simulation.hpp`: a local `alignas(64)` CheckedPaths-derived
type gives each snapshot its own complete cache lines, and the existing owning
`unique_ptr` allocates that type. On this host, extent becomes 128 bytes instead
of 104. C++17 aligned new/delete supplies matching allocation and destruction.
There is no global allocator, pool, benchmark, default, parameter or error change.
No alignment is imposed on public types. This targets observed sharing on the
64-byte-line test host; it is not a universal performance guarantee for every
architecture. Historical typed seeds, native roots, task draining and all risk
oracles remain unchanged.

An uninstrumented same-toolchain original / candidate / withdrawal panel gives
compiled minima **2.1698/2.1377**, **1.7598/1.7718**, and **1.7457/1.8731 ms**;
medians are **2.3311**, **1.8233**, and **2.3094 ms**. Tree candidate minima differ
from original by less than 1%. An earlier two-line embedded-optional candidate
improved compiled median but worsened tree median to **4.2703 ms** versus
**2.1737 ms**; it is rejected and is not in the product diff. Merely aligning
the address without rounding the extent also retained the collision and is
preserved as an unsuccessful diagnostic intervention.

RED/GREEN evidence is the allocation/performance regression, not an invented
functional pricing failure: `diagnosis/causal.py` establishes the single-object
intervention and withdrawal; `diagnosis/candidate_panel.py line` validates the
uninstrumented candidate. Existing semantic assertions are unchanged. No new
unit test merely mirrors a private type's alignment. All **560 diagnostic
processes** exit zero with unchanged workload validators; all samples survive.
The initial partial trace missed weak template bodies selected from an existing
binding object. It is retained and labeled incomplete; later isolated builds
use distinct diagnostic template names and capture all state/batch records.
Uninstrumented builds recompile both public and Python value translation units.

### Clean builds, full gate, and remaining failures

Final production validation uses clean builds from this checkout with GCC
14.3.0, CMake 4.2.3, CPython 3.13.13, pybind11 3.0.4 and the pinned submodules.
Correctness uses default-architecture Release; Python performance uses the
original Release/native tuning and four threads. Build jobs finish before the
full performance runs. `verification/correctness.py`, its execution manifest
and logs preserve every literal command and result.

- Native full CTest: **1716/1716**, zero failures (11.27 s).
- Python: **402/402**, zero skips (8.31 s), including the rebuilt test fixture.
- Adept: **425/425**; CoDiPack: **425/425**; XAD: **424/424** script/AAD regressions.
- All **16 F4 cases** are present in each backend's successful complete/regression
  log; `f4-coverage.json` checks the exact required names. A separate focused
  native run passes 13 historical replay cases plus the 257-rewind root test.
- Range formatting with clang-format 22.1.5 and whitespace checks pass.
  Documentation checks pass; report changes are checked again before publication.

The unchanged 90-case gate uses `--samples 10 --confirmation-rounds 2
--threshold-percent 4`. `verification/run_gates.py` runs both exact-copy A/A
controls; `gate-execution.json` also records the final paired command. The first
paired preflight fails because CMake canonicalized `g++-14` to
`/usr/bin/g++-14`. Reconfiguring the spelling preserves the compiler and binary;
the failed preflight and both caches/logs are retained. No measured paired run
was discarded or repeated to obtain green.

| Case                              | Round 1 | Round 2 | Result |
|-----------------------------------|---------|---------|--------|
| Vanilla double compiled           | +0.07%  | +0.17%  | pass   |
| Vanilla double tree               | -0.83%  | -2.46%  | pass   |
| Barrier AAD compiled              | -2.53%  | -1.00%  | pass   |
| comparison.mc_barrier_price_16384 | +38.82% | +17.61% | FAIL   |
| mc.vanilla.aad.tree               | +4.04%  | +5.43%  | FAIL   |
| xccy.joint.BUMPED.solve           | +5.69%  | +4.37%  | FAIL   |

Baseline A/A fails `comparison.mc_barrier_price_16384` at **+56.79%/+52.87%**;
head A/A fails `rng.sobol_uniform` at **+5.69%/+4.04%**. These are limitations,
not proof that any paired failure is unrelated to F4. The old 88/90 failures and
same-value native-tuning rate failures remain preserved as historical evidence.
The targeted vanilla allocation fix does not explain or waive the three fresh
paired failures. No full C++ benchmark gate, sanitizer, Windows XLL or full
alternative-backend public/Excel run is claimed.

Parent DAL-201 must route the remaining failures and accept this scoped handoff
before resuming DAL-225 independent testing, DAL-226 documentation/CHANGELOG
decision and DAL-227 independent review. A potential RNG-level allocation fix
would require extending scope to `dal-cpp/dal/math/random/sobol.cpp`; this turn
makes no such product edit. The patch remains a candidate in the existing draft
PR, without merging, closing F4, launching F5, or changing any gate policy.

## Suite-prefix diagnosis: allocation-sensitive vanilla double timings

This diagnostic handoff supersedes the unresolved-context discussion below;
it does **not** replace the original complete gate or establish acceptance.
DAL-224 resumed at published `ea5f982e4d27e4599ce6fda1c43da4bedc275a20`,
tree `97b175c3a77b5f2703ab7f213fb0c9f72e14ef31`. This round changes only this
report. Production, tests, docs, benchmark/CI scripts, tolerances and submodule
pointers are unchanged. The publication manifest attached to DAL-224 records
the resulting report-only commit/tree. The existing draft PR remains
https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371, base master.

### Fixed inputs and original failure

The prior evidence attachment `01a09fed-5b50-795b-8d6e-aadc41f6da4b` was
downloaded with the authenticated Multica CLI. Its SHA256 is
`1e494258379c20cf3b18a4c0c4b247d65f4fd8142bba907f1e4e4038a2ad8079`.
All diagnostic timing uses its immutable Python packages, without rebuilding:

- Base: master `8ee1b09dcaf531add9695d466941026da87a4942`, tree
  `6c7d19beee7efebf43d537a6842fb3268a3d6fc6`; native module SHA256
  `02b98cec1d79657fd12bb6b442e40d4bab1139d09ca922abb47da5fb81eef258`.
- Head: code `b332fea5f0d9bc9810847ecedcc63e1ac854a9f7`, tree
  `fef5bfae1961cc2bd39ad7f09b55f6d314ec4075`; native module SHA256
  `0450825e53382ae28699b8861990c03ecc858d435805973323d008bfb334cf04`.

`input-identity.json` verifies both native hashes against the original full
gate and checks every benchmark-suite source hash. The same existing CPython
3.13.13 interpreter is used. Original build provenance remains GCC 14.3.0,
pybind11 3.0.4, Release/native tuning. Default runs use `DAL_NUM_THREADS=4`,
the same thread environment, and the unchanged full-size workloads. The host
is the same WSL2 Intel i9-13900HX machine; this is not hosted-hardware parity.

The original full gate remains **88/90**: compiled vanilla double
**+19.21% / +19.70%**, tree vanilla double **+14.63% / +14.10%**. The original
baseline A/A 89/90 and head A/A 90/90 are preserved, not used as exemptions.
No new full gate or A/A was run: this turn diagnoses the requested state
dependency, and no causal product fix was available to validate with those gates.

### Unmodified workloads with controlled prefixes

The diagnostic harness calls the repository's existing `build_cases(False)`
and `runner.run_cases`/`measure`, including its validation run, two warmups,
one measured sample, correctness validators and report writes. Every comparison
uses 20 fresh processes per side, alternating base/head order, divided into two
groups of ten with the original minimum reduction. Scenario order is reversed
on alternating samples. All measured prefix samples are retained as well.
These are diagnostic comparisons, not substitutes for the 90-case gate.

The original prefix contains exactly the first seven cases, in suite order:
four 100,000-by-10 RNG workloads, `script.construct_and_parse`, then vanilla
double tree and compiled, each with 200,000 paths. The narrow scenario contains
only the two vanilla cases in that same order. The prior narrow scenario
recreates the five named workloads and order from the preceding handoff's
`narrow-cases.py`, including its calibration and barrier preconditioning.

| Diagnostic context             | Tree, rounds 1 / 2 | Compiled, rounds 1 / 2 |
|--------------------------------|--------------------|-----------------------|
| Two vanilla cases only         | +1.05% / -14.36%    | +1.17% / +2.14%       |
| Original first-seven prefix     | +0.84% / +0.18%     | +29.35% / +24.13%     |
| Four RNG cases then vanilla     | -1.63% / +0.49%     | +1.99% / +1.70%       |
| Parse case then vanilla         | +1.18% / -0.81%     | +18.43% / +0.51%      |
| Previous five-case narrow order | +1.41% / -0.23%     | +1.04% / +1.49%       |

This supplies a real compiled RED after the original prefix with unchanged
binaries. It does not reproduce the original tree failure. Absolute minima and
all raw reports are in `initial/` and `prefix-components/`.

An additional controlled panel compared the full prefix, the same prefix with
glibc tcache disabled, and a diagnostic single-thread variant. The unchanged
control itself became -1.47% / +0.60% for compiled vanilla, so that panel cannot
attribute an improvement to either intervention. Neither tcache disabling nor
one thread is presented as a fix. The complete inconclusive panel is retained
under `allocator-thread-controls/`.

### Path and allocator intervention on the same binaries

The changed control motivated a predeclared two-by-two comparison: short versus
long report/current-working-directory paths, using the default allocator versus
the existing system jemalloc through `LD_PRELOAD`. Both sides have equal-length
package paths; changing the report-directory component from 7 to 25 characters
changes each worker's output/cwd path by 18 characters. Commands otherwise use
the same worker mode, suite, interpreter, package, sizes, warmups and threads.
The parent interleaves all four scenarios, 20 processes per side each.

| Allocator and output/cwd path | Tree, rounds 1 / 2 | Compiled, rounds 1 / 2 |
|------------------------------|--------------------|-----------------------|
| Default, short               | -1.86% / -0.27%     | +10.42% / +26.68%     |
| Default, long                | -0.19% / -0.96%     | -0.27% / +2.57%       |
| jemalloc, short              | +0.21% / -0.03%     | -3.20% / +0.30%       |
| jemalloc, long               | -1.05% / +1.66%     | -0.03% / +0.55%       |

Within the **same head binary**, compiled short-path minima are **+9.30% /
+25.46%** slower than long-path minima. The same base-binary comparison is
**-1.28% / +1.58%**. This is direct evidence of context sensitivity without a
source, binary or workload change. The allocator intervention supports an
allocation-layout explanation. It does not identify which live allocations
are responsible, prove false sharing/cache conflicts, or prove F4 is innocent.
Changing the report directory or allocator is **not** a proposed repository/CI
fix, and these outcomes do not turn the original gate green.

The parent sampled `/proc/<pid>/task/*/stat`, load, and process mappings every
10 ms, and captured `/proc/stat` around each of the 160 processes. Every process
reached exactly four observed threads and retained the same four IDs across
all four-thread samples. Aggregate host CPU utilization was approximately
3.2%-4.7% across those process intervals; process snapshots show no concurrent
compiler job. CPU migration is recorded, not controlled. These coarse samples
cannot resolve individual 2 ms MC calls, prove zero worker lifecycle events
between samples, or observe host activity hidden from WSL. No hardware-counter
or exclusive-host claim is made. `analysis.json` retains the calculation and
per-process paths; each raw `process.json` includes the underlying data.

Source inspection confirms the pool implementation and legacy double runner
are unchanged against master. Worker-owned `ThreadState_` objects are recreated
per valuation and reused across batches; state zero is allocated by the caller,
other states by their workers. Both selected evaluators and their backing
vectors are still allocated, even when only one evaluator mode is used.
The double historical-seed specialization stores no extra vector. None of this
alone establishes absence of allocator-dependent interactions.

The preserved base/head `value.cpp.o` files were also compared: 19 of 21 selected
double-related text sections have identical pre-relocation bytes, including
the worker-state constructor and task machinery. One 1,834-byte path helper
differs only in an exception-source-line immediate (223 versus 229). The outer
double simulation section differs (5,596 versus 5,782 bytes). Relocations and
linked addresses differ, so this is **not** whole-binary or linked-code
equivalence. Section bytes, disassemblies and hashes are delivered under
`object-comparison/`. This evidence does not justify random inline/layout edits.

### Delivery, verification limits and next discriminating step

No behavior RED test was invented and no product GREEN is claimed. The actual
RED is the preserved complete gate plus the reproduced compiled prefix
slowdown. This round ran **480 fresh diagnostic workers**, all exiting zero
with their existing workload validators passing. No failed or slow sample was
discarded. The initial 80, component 120, allocator/thread 120, and final path
panel 160 workers are all retained. There were no new native, Python test-suite,
alternative-backend, sanitizer or full performance-gate runs; unchanged source
does not warrant repeating the previous full builds. The earlier 1716 native,
402 Python (zero skips), 425 Adept, 425 CoDiPack and 424 XAD results remain
historical evidence for the identical product/test source, not new executions.
All sixteen F4 tests, 257 root rewinds, 8193-path oracle and 16385-path exception
drain assertions remain untouched. Existing native-tuning rate failures remain
preserved for parent routing.

Reproduction, from the task workdir after extracting the original attachment
into `evidence-input/` and placing this checkout at `Derivatives-Algorithms-Lib/`:

```bash
"$PYTHON" diagnosis/suite_state.py --output diagnosis/initial
"$PYTHON" diagnosis/suite_state.py --modes rng,parse,prior --output diagnosis/prefix-components
"$PYTHON" diagnosis/suite_state.py --modes prefix,prefix-notcache,prefix-single --output diagnosis/allocator-thread-controls
"$PYTHON" diagnosis/path_control.py
```

The attached scripts and execution manifests record literal commands and paths.
`PYTHON` is the existing CPython 3.13.13 bench-venv interpreter recorded there.
The path-control test deliberately depends on path length; relocation to another
workdir is a new environment and may change the outcome. Final script versions
include all scenario selectors; worker timing logic uses the unchanged suite.

The next discriminating experiment is allocation-address tracing on the fixed
short/long **head** pair, distinguishing caller-state-zero mutable buffers from
shared read-only product/model buffers and worker-arena buffers. Establish
whether live objects share cache lines, then intervene only on the implicated
allocation while preserving suite order. A pool restart after the tree case
can separately distinguish retained worker-arena state from caller-arena state.
Such instrumentation must be reported as perturbing the measurement; a resulting
minimal production candidate must reproduce the effect without instrumentation
and pass the original complete gate/A-A and correctness matrix before acceptance.
Current data do not justify a specific allocation or runner patch, or exemption
as a proven non-F4 problem. The unresolved tree result also remains an explicit
limit. Parent review must decide the next bounded investigation; no later role,
F5, merge, issue closure, benchmark policy change or allocator deployment was
started by this diagnostic task.

## Remaining compiled barrier regression: native root inlining

This section supersedes the previous unresolved-barrier handoff below. DAL-224
resumed at published `8b21cfa3260e42912eab37150fabbcb33dd6777d`, tree
`d2365d406c000697cfaa15c879fb03ed052f6c89`. The code under test is
`b332fea5f0d9bc9810847ecedcc63e1ac854a9f7`, tree
`fef5bfae1961cc2bd39ad7f09b55f6d314ec4075`. Publication adds only this report;
the accompanying publication manifest records its exact SHA/tree.

The existing branch remains `feature/dal-201-historical-aad-state`, and
https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371 remains a
draft targeting master. The fixed baseline is
`8ee1b09dcaf531add9695d466941026da87a4942`, tree
`6c7d19beee7efebf43d537a6842fb3268a3d6fc6`. No additional merge, rewritten
history, second PR, F3/F5 change, or downstream role execution was performed.

### Causal isolation and minimal change

The only production change is `dal-cpp/dal/math/aad/aad.hpp`: native `PayoffRoot`
uses ordinary `inline` instead of `FORCE_INLINE`. It remains defined in the
header; the compiler can choose whether to inline it. Its terminal-node check,
mark check, and registered-zero fallback are unchanged. Adept, CoDiPack and XAD
retain their original forced-inline root expression. Public valuation
signatures, defaults, errors and mode support do not change. Historical seeds,
visitor state, simulation runners, tests, tolerances, benchmarks, CI configuration
and documentation are unchanged in this repair.

The diagnosis used the existing Python `prepare_mc` workload with its full
10,000-path compiled AAD barrier input. `evidence/narrow.py` runs 20 interleaved
processes per side, two warmups and one measured sample per process, taking each
round's minimum of ten. It calls the existing workload's correctness validator;
it is a diagnostic harness, not a replacement or alteration of the complete gate.
Every sample and PV/risk result is retained in the corresponding JSON.

| Controlled comparison                                | Round 1 | Round 2 | Interpretation                                  |
|------------------------------------------------------|---------|---------|-------------------------------------------------|
| Starting published binary versus master              | +8.30%  | +8.46%  | Reproduced RED                                  |
| Fresh starting-code build versus master              | +8.23%  | +5.98%  | Reproduced after rebuilding                     |
| Seed moved behind working state versus starting code | +0.74%  | +0.57%  | No improvement; discarded                       |
| Root bypass versus starting code                     | -7.19%  | -8.79%  | Isolates root call cost; unsafe diagnostic only |
| Root moved to implementation file versus master      | -1.01%  | -2.75%  | Barrier improved; candidate later rejected      |
| Ordinary inline root versus starting code            | -8.71%  | -6.56%  | Selected candidate's barrier GREEN              |

The layout and bypass patches were never committed and are absent from the final
diff. Root bypass is explicitly unsuitable for historical/constant payoffs and
was measured only on this legacy barrier input. A separate native probe executes
the same 10,000 paths and finds **10,000 terminal payoffs**, so this slowdown is
not caused by additional fallback nodes on that workload. The final fix keeps
all safety checks rather than adopting the bypass.

Moving the native routine into `aad.cpp` first passed the narrow barrier check,
but its complete gate was **86/90**, including a newly regressed double compiled
vanilla. A direct comparison against the fresh starting build reproduced that
side effect at **+12.41% / +13.60%**. That candidate was rejected; its local-only
commit `6fa16533` was replaced before publication, preserving the published
ancestor `8b21cfa3`. Its patch, tests, original gate and both A/A controls are
retained as diagnostic evidence, not final acceptance. Baseline A/A passed 90/90;
head A/A failed a separate vanilla-price case (+23.67% / +9.31%). This variability
does not waive the original base/head failures.

The smaller ordinary-inline candidate was compared directly with the starting
build on those four flagged cases and the original barrier, using the same two
rounds of ten interleaved processes. Calibration changed -1.82% / -0.27%; barrier
price -10.12% / +1.05%; double compiled vanilla +13.16% / -1.28%; double tree
vanilla -2.20% / -2.36%; AAD compiled barrier -8.71% / -6.56%. All five pass the
existing rule that both rounds must exceed +4% to fail. This does not claim that
each individual round improved. `narrow-ordinary-inline-vs-start.json` retains
every sample, and the complete gate remains a separate required result.

The generated path-loop helper sizes in the public valuation object shrink from
3442–3651 bytes to 2137–2437 bytes; the outer batch function remains 7798 bytes.
GCC emits the ordinary-inline root as a separate 821-byte callable function in
the public valuation object. The controlled call-site change and measurements
support a code-generation cost from forced inlining. No hardware-counter claim about a particular cache or
branch-prediction mechanism is made. Maximum PV/risk differences across diagnostic
samples are `3.552713678800501e-15`, below the unchanged `1e-8` MC tolerance.

No new failing behavior test is invented for this behavior-preserving relocation:
the actual performance RED above precedes the production change. The existing
`AADTest.TestPayoffRootReusesTerminalPathNode` passes against the new library,
preserving zero extra nodes and SCALE sensitivity 240 over 257 rewinds. The
complete native suite also exercises historical direct roots, constants and empty
suffixes. Formatting, whitespace and Lizard's unchanged complexity limit 8 pass.

### Verification environment and reproducibility

All new builds use GCC 14.3.0, CMake 4.2.3, C++17, Linux/WSL2 and the repository's
pinned submodules. Python timing uses the same CPython 3.13.13 interpreter,
pybind11 3.0.4, Release configuration, native CPU tuning and `DAL_NUM_THREADS=4`
on both sides. Compiler, Python/header paths and CMake identities are captured in
`environment-final.json`, `build-identities.json` and `configuration-matched.json`. This Intel i9-13900HX host does not
reproduce hosted GCC 14.2.0 / CPython 3.13.15 / AMD hardware exactly.

The benchmark base is the preserved independently built master checkout from the
previous delivery; its source is clean at the fixed SHA above. The head Python
library is freshly built. Each diagnostic version has its own preserved package
and hash. Local build jobs finished before each paired timing run; compilation
overlapped correctness work and the informational smoke run. Process snapshots record observed load, without claiming
exclusive control over the shared machine or pinning CPU frequency/affinity.

Native correctness uses a separate default-architecture build:

```bash
cmake --preset=Release-linux -S . -B build/NativeTests \
  -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 \
  -DDAL_CPP_BUILD_EXAMPLES=OFF
cmake --build build/NativeTests -j6
DAL_NUM_THREADS=4 ctest --test-dir build/NativeTests --output-on-failure -j4
```

The earlier implementation-file candidate's native full CTest passed
**1716/1716**, zero failures, 21.66 seconds. The additional focused run covered
the sixteen F4 cases plus the terminal-root regression. Fresh verification for
the selected ordinary-inline candidate is recorded separately below.
No new native-CPU-tuned full rate-suite pass is claimed. The prior four rate
failures and identical master-library reproduction remain preserved in
`ctest-fix1.log` and `master-nativearch-rate-failures.log`; that reproduction is
not a full master CTest run, and rate changes remain outside this task.

The earlier candidate's Python run passed **402/402**, zero skips, 21.46 seconds. An initial run had
401 passes and one skip because the test-only `_dal_quote_risk_test` module had
not been built by the `_dal` target. Building that existing fixture and rerunning
the suite resolves the skip; both logs are preserved. No test was changed.

```bash
cmake --build build/Release-linux --target _dal _dal_quote_risk_test -j4
PYTHONPATH=build/Release-linux/dal-python DAL_NUM_THREADS=4 \
  "$PYTHON" -m pytest dal-python/tests -q -rs
```

`PYTHON` denotes the shared CPython 3.13.13 bench-venv interpreter whose absolute
path is recorded in `environment.json` and the gate execution manifest. Python
configuration enables `DAL_BUILD_PYTHON=ON`, `DAL_ENABLE_NATIVE_ARCH=ON`, uses
explicit GCC 14 compilers, and sets `Python3_EXECUTABLE` to that interpreter.

Alternative builds use the same Release preset, GCC 14 compilers,
examples OFF and the selected backend option ON:

```bash
cmake --preset=Release-linux -S . -B build/Adept -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_ADEPT_AAD=ON
cmake --preset=Release-linux -S . -B build/CoDiPack -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_CODIPACK_AAD=ON
cmake --preset=Release-linux -S . -B build/XAD -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_XAD_AAD=ON
```

Each uses `cmake --build build/<backend> --target dal_cpp_tests -j3` followed by
`DAL_NUM_THREADS=4 ./build/<backend>/dal-cpp/dal_cpp_tests` with the unchanged filter:

```text
Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*
```

The earlier candidate's runs were:

- Adept fork 4.1.1, pin `1e29edc6e16f969e99145f0cbff34ff0de5fe699`:
  **425/425**, 1988 ms.
- CoDiPack 3.1.0, pin `86b94d3f3c3b6659a36f8e640945a7ebe1884a4a`:
  **425/425**, 1866 ms.
- XAD 2.1.0-dev, pin `ca0146061726745870aac71f3108c7d14129d1b3`:
  **424/424**, 3507 ms.

No selected backend test failed or skipped. `f4-coverage.json` mechanically
confirms the same sixteen F4 tests executed on all four backends, retaining
threads 1/2/4, paths 1/257/8193, direct and constant roots, the independent
8193-path recording oracle, and 16385-path exception drain/recovery. The existing
`script_mc_perf` target was also built and smoke-run successfully with four
threads; its timings are informational and do not replace the paired gate.

The full gate first rejected a cache spelling mismatch (`g++-14` versus
`/usr/bin/g++-14`) before measuring any case. The dependent A/A run was stopped
and retained as incomplete under `preflight-aborted`. Reconfiguring the head with
the same compiler option spelling makes the gate's configurations exactly equal;
the head library SHA256 is unchanged. No cache was edited manually, no gate
validation changed, and no completed timing verdict was rerun. The corrected
configuration proof is `configuration-matched.json`.

### Fresh verification of selected code b332fea5

The selected code was rebuilt after replacing the rejected implementation-file
candidate. All following results are fresh runs of `b332fea5`, not reused counts:

- Native default full CTest: **1716/1716**, zero failures, **11.89 seconds**.
  Command: `cmake --build build/NativeTests -j4`, then the full CTest command
  above. Logs: `build-final-native.log`, `ctest-final-native.log`.
- Focused native: **17/17** (all sixteen F4 cases plus the terminal-root test).
  Log: `f4-final-native.log`. The standalone root regression also passes;
  log: `green-root-final.log`.
- Python: **402/402**, zero skips, **21.18 seconds**, after rebuilding the
  test fixture. Log: `pytest-final.log`.
- Adept: **425/425**, 1944 ms; CoDiPack: **425/425**, 1816 ms;
  XAD: **424/424**, 3682 ms. Builds use `-j3` and the same filter above.
  Logs: `build-final-<backend>.log`, `regression-final-<backend>.log`.
- `f4-coverage-final.json` confirms sixteen required F4 tests on every backend.
  The final `script_mc_perf` build and smoke run also completed successfully.
- Final formatting, whitespace, Lizard limit 8, and documentation checks pass.
  The empty `preserved-scope.diff` and ancestry checks prove tester assertions,
  tests, docs, CI/benchmark policy and the integrated master remain preserved.

The only amended commit was the unpushed rejected candidate. The final branch
is a descendant of the original published head; no remote history was rewritten.
`final-code.diff` is the complete six-line production diff. The implementation
file, evaluator state and runner are byte-identical to the starting version.
No production-scope or public-contract deviation was required.

### Final complete gate: 88/90, not accepted

The unchanged complete gate on **b332fea5** exits **1: 88 passes / 2 failures**.
The originally assigned compiled AAD barrier passes both rounds, but this does
not establish overall performance acceptance:

| Case                       | Round 1 | Round 2 | Verdict |
|----------------------------|---------|---------|---------|
| mc.barrier.aad.compiled    | +1.72%  | +2.04%  | pass    |
| mc.vanilla.double.compiled | +19.21% | +19.70% | FAIL    |
| mc.vanilla.double.tree     | +14.63% | +14.10% | FAIL    |

The final baseline A/A control exits 1, **89/90**: its separate
`comparison.mc_vanilla_greeks_16384` case is +7.83% / +6.11%. The final head A/A
control exits 0, **90/90**. Controls use exact package copies and the unchanged
gate, not independently rebuilt substitutes. A/A variability is recorded as a
measurement limitation; neither control replaces or waives the two base/head
failures. All three complete runs and all raw samples are delivered.

From the repository, the complete gate command is:

```bash
DAL_NUM_THREADS=4 "$PYTHON" .github/scripts/check_python_benchmark_regressions.py \
  --base-source "$BASE_SOURCE" --head-source . \
  --base-root "$BASE_SOURCE/build/Release-linux" --head-root build/Release-linux \
  --output-dir ../evidence/paired-final \
  --samples 10 --confirmation-rounds 2 --threshold-percent 4
```

`BASE_SOURCE` is the preserved master checkout at the fixed SHA above.
`gate-execution.json` records the exact absolute interpreter/source/build paths,
all three original commands, exit codes and durations. `run-gates.py` only
orchestrates those commands and creates distinct A/A copies as in CI; it changes
no benchmark implementation or policy. The source revisions, suite hashes,
module hashes, environments and complete results are also in each gate report.

The narrow RED and final-candidate commands, run from the enclosing workdir, are:

```bash
"$PYTHON" evidence/narrow.py Derivatives-Algorithms-Lib/dal-python/benchmarks \
  "$BASE_SOURCE/build/Release-linux/dal-python" evidence/package-start \
  evidence/narrow-fresh.json
"$PYTHON" evidence/narrow-cases.py Derivatives-Algorithms-Lib/dal-python/benchmarks \
  evidence/package-start evidence/package-ordinary-inline \
  evidence/narrow-ordinary-inline-vs-start.json
```

After the complete gate, two additional bounded diagnostic comparisons tested
the unchanged starting code, without rerunning the full gate. Fresh starting
code versus master gave double compiled +6.41% / -4.13%, double tree +0.68% /
+3.48%, and the original AAD barrier +6.92% / +7.74%. Fresh versus previously
published starting binaries gave double compiled -3.04% / +20.66%, double tree
+1.97% / +4.37%, and AAD barrier -0.37% / +1.20%. The narrow double cases pass
the existing two-round rule, including the final candidate's comparison against
starting code; the complete final suite still fails both. These results narrow
the open question to full-suite execution context/build effects, but do not
prove a particular allocator, layout, scheduler or source-level cause.

**Remaining work:** reproduce the two double vanilla failures with the original
suite prefix and matched immutable binaries, then isolate worker allocation/layout
and code-generation effects before changing a double runner. The current repair
does not modify that runner, the state layout, RNG, or any double valuation
source. If diagnosis needs production changes outside the authorized F4 root/
state/private-runner boundary, DAL-201 must route that scope with these samples.
No further full-gate sampling was performed to seek a favorable verdict. The
two failures remain unresolved; **this is a candidate handoff, not acceptance**.

### Publication and limits

Published SHA/tree and the sole post-push CI snapshot are in `publication.json`
and the DAL-224 final comment. The PR stays draft and unmerged. No rejected
diagnostic code is included in the published branch's new commits; rejected
variants remain only in the attached evidence. The original hosted benchmark
job log request returned HTTP 404 while that run was unfinished, so its actual
executed base could not be verified from raw logs. The new push targets the
already-retargeted master PR; no old-base result or pending check is called green,
and no CI watch or polling loop is used.

The unchanged historical CPU-tuning rate failures are retained for separate
parent routing. No new full master CTest, native C++ paired gate, Windows XLL,
sanitizer or full alternative public/Excel suite is claimed. Independent DAL-225
testing, DAL-226's existing native-root documentation decision and DAL-227 review
still require the parent's serial handoff. Prior approval does not cover this
candidate, and neither F4 nor the PR is closed by this implementation report.

## Previous master integration and targeted regression repair (historical)

This section supersedes the historical implementation and complexity reports below.
DAL-224 resumed the existing F4 branch on 2026-09-14 after F3 #369 was merged.
The previous DAL-227 approval covers the old head only; DAL-225, DAL-226 and
DAL-227 must independently accept this new revision.

### Revisions and merge

- Starting published F4: `5c954ca2fdded2ca35ad15c7fef44d90a002007d`,
  tree `2ad4c408e84bb6323528ac3e9b2f66bb77e40ce0`.
- Integrated master/F3 squash: `8ee1b09dcaf531add9695d466941026da87a4942`.
- Merge commit: `c02d7545e6c7d91a29503db627c933340d611a4a`,
  tree `d03a769bdff3a638681e98ac233a0d6d99c44378`.
- Final tested code: `6725ae209d1f9a434b273e30ef161246f71ea1da`,
  tree `78acb71d71dbb142329a85b131134f92a121b8f0`.
- Published branch: `feature/dal-201-historical-aad-state`; existing
  https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371 targets master.
  The publication commit changes this report only. Attached `publication.json`
  and the final DAL-224 comment record the exact published SHA/tree, which cannot
  be embedded self-referentially in the commit that contains this file.

Both the old F4 head and current master remain ancestors. No shared history was
rewritten, no F3/F5 branch was changed, and no second PR was created. The observed
F5 branch remained at `3b7b1accc2b09fc2aec282ed623d796ba6add68c` before publication.
Initial F4 family reads showed only this writer active; later DAL-229 compilation
in a separate workdir was observed and accounted for in timing evidence.

The production sources and tests in master were byte-identical to the old F3
baseline `e8943ee2a399229cf36be412accb92ea1872b10e`. Squash ancestry caused
conflicts in preparation, simulation, evaluator and observation tests; retaining
the F4 versions preserved all approved behavior and tester assertions.
Documentation conflicts retain F4 capability descriptions plus master's
unconflicted revisions. The F3 implementation report takes master's version;
master's retired-artifact removals are preserved. The merge itself changes no
production source or test versus the starting F4 head, so no behavior RED is
claimed for integration. Full patches are `integration.diff`,
`repair-final.diff`, and `full-f4.diff` in the evidence archive.

### Actual inherited failure and attribution

Downloaded the original job log with:

```bash
gh api repos/wegamekinglc/Derivatives-Algorithms-Lib/actions/jobs/103891510913/logs
gh api repos/wegamekinglc/Derivatives-Algorithms-Lib/actions/artifacts/10337278750/zip
```

The archived Python gate ran 90 cases and failed five, not all 90:
`comparison.mc_vanilla_greeks_16384` (+6.27%, +8.84%),
`comparison.mc_vanilla_greeks_65536` (+8.40%, +12.15%),
`mc.vanilla.aad.compiled` (+7.45%, +8.27%),
`mc.vanilla.aad.tree` (+4.83%, +4.54%), and
`mc.vanilla.double.tree` (+30.74%, +31.69%).
C++ paired and the two Python A/A results do not replace this base/head failure.

Archive head `6f20d47721e9cb44d8d001589b13af4b1f225dc4` was GitHub's synthetic
merge of e8943ee2 and 5c954ca2. GitHub's commit API confirmed its tree equals the
published F4 tree `2ad4c408...`. Raw log, zip, source identities, summaries,
per-process samples and package reproductions are retained.

### Production changes and design

Only these product/test files change beyond the mechanical integration:

- `dal-cpp/dal/math/aad/aad.hpp`: native `PayoffRoot` reuses the payoff only
  when it is the terminal tape node and the recording has a nonempty suffix
  after the mark. Otherwise it retains the registered-zero root. Thus direct
  historical seeds cannot overwrite accumulated adjoints and empty suffixes
  remain safe. Adept/CoDiPack/XAD keep the original root expression.
- `dal-cpp/dal/script/visitor/evalstate.hpp`: typed historical-seed storage
  belongs to active scalar state. An empty double specialization restores
  the original double state layout and initialization loop. Double seed setting
  writes its passive initial-value vector; production historical AAD replay
  still owns and restores its typed seed on each worker recording.
- `dal-cpp/tests/math/aad/test_payoff_root.cpp`: focused native regression
  requires zero extra tape nodes for an existing terminal path payoff, while
  checking value 480 and accumulated SCALE sensitivity 240 across 257 rewinds.
- This implementation report records integration, exact revisions and evidence.

There are no public valuation signature/default/error changes, no simulation
validation-order changes, and no new execution modes. Historical hard and future
fuzzy behavior, typed worker seeds, task draining, one risk normalization, RNG
ordering and prepared/named compiled rejection are retained. All existing tester
tests and tolerances remain byte-identical. No CI, benchmark script, threshold,
skip, suppression, branch protection or submodule revision changed. The final
implementation does not restore the rejected RTTI experiment.

The new root behavior needs a DAL-226 documentation decision: the AAD/script
methodology currently says every payoff adds the registered zero. Native terminal
post-mark payoffs can now reuse that existing path-local node. Other backends and
the fallback retain the documented addition. No methodology semantics were
edited by this implementation repair.

### RED, GREEN and refactor evidence

The focused resource/lifecycle regression was added before changing the root:

```bash
g++-14 -std=c++17 -O3 -DNDEBUG -I dal-cpp \
  -I dal-cpp/externals/googletest/googletest/include \
  dal-cpp/tests/math/aad/test_payoff_root.cpp \
  build/Release-linux/dal-cpp/libdal_cpp.a \
  build/Release-linux/lib/libgtest_main.a build/Release-linux/lib/libgtest.a \
  -pthread -o ../evidence/payoff-root-test
../evidence/payoff-root-test
```

RED exit 1: `Tape()->nodes_.End() == end` was false because the old helper
always records another node. GREEN exit 0 after terminal-node reuse: 1/1 passed,
including all 257 paths and the analytic sensitivity. See `red-root.log`,
`green-root.log`, and their build logs. The final full native suite includes
the same regression.

The unmodified integrated code's full local paired gate was the performance RED:
the original five vanilla cases failed again; double/compiled and construction
also failed on this host. Initial root/copy refinement at c5e228be passed all
four original AAD failures but retained double/tree (+90.07%, +85.19%) and
reported barrier/AAD compiled (+8.52%, +6.95%). This was not accepted as complete.
The final refactor reserves typed seed storage for active numbers and restores
the double fast path structurally. A probe using the old header reports double
state size/stack offsets `1264 / 96 / 1128`; both final code and master report
`1240 / 72 / 1104`. Probe source and outputs are attached; it is diagnostic
layout evidence, not a timing substitute.

An early comparison with mismatched pybind11 versions was stopped and excluded.
The later field-reordering candidate at 1a5b0818 was stopped after discovering
concurrent DAL-229 CoDiPack/XAD builds; `paired-fix2/INCONCLUSIVE.txt` and the
process snapshot explain why those incomplete samples are not a verdict.
Neither interrupted run is represented as a successful test.

### Final correctness and static verification

Fresh final-code checks use Linux x86_64 (WSL2), Intel i9-13900HX, GCC 14.3.0,
CMake 4.2.3, C++17, and the pinned submodule revisions.

```bash
cmake --preset=Release-linux -S . -B build/NativeTests \
  -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 \
  -DDAL_CPP_BUILD_EXAMPLES=OFF
cmake --build build/NativeTests -j6
DAL_NUM_THREADS=4 ctest --test-dir build/NativeTests --output-on-failure -j4
```

Native default configuration: **1716/1716 passed**, zero failures, 18.20 seconds.
This includes core/public/portable Excel and the new native root test.
Final Python native-tuned binding: **402 passed**, 23.43 seconds:

```bash
PYTHONPATH=build/Release-linux/dal-python DAL_NUM_THREADS=4 \
  ../bench-venv/bin/python -m pytest dal-python/tests -q
```

Each alternate backend was configured with the same Release preset, explicit
GCC 14 compilers, examples OFF, and its one backend option ON:

```bash
cmake --preset=Release-linux -S . -B build/Adept -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_ADEPT_AAD=ON
cmake --preset=Release-linux -S . -B build/CoDiPack -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_CODIPACK_AAD=ON
cmake --preset=Release-linux -S . -B build/XAD -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_XAD_AAD=ON
```

For each backend, `cmake --build build/<backend> --target dal_cpp_tests -j4`,
then `DAL_NUM_THREADS=4 ./build/<backend>/dal-cpp/dal_cpp_tests` with filter:

```text
Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*
```

Adept (pinned 1e29edc6): **425/425**; CoDiPack 3.1.0 (86b94d3f):
**425/425**; XAD 2.1.0-dev (ca014606): **424/424**.
No selected test failed or skipped. `f4-coverage.json` verifies the same
**16 F4 tests** ran in all four backends, preserving threads 1/2/4, paths
1/257/8193, the 8193-path per-recording oracle, and 16385-path exception recovery.

Lizard 1.23.0, `lizard -C 8` on the two changed headers and new test, exits 0
with no threshold violations. Formatting and whitespace checks pass.
Documentation checking passes for 55 Markdown files. Hosted Codacy remains a
separate result; local Lizard is not its equivalent.

### Pre-existing native-tuning failures

The extra native-CPU-tuned full run at c5e228be had 4 failures among 1718
CTest entries, all in unchanged rate-cashflow tests: future forecast PWC,
passive-curve gradient exclusion, FRA/future active/passive bitwise PV, and
projected/supplied/missing future fixings. No script or AAD lifecycle test failed.

Linked the same unchanged native-tuned test-main and rate-cashflow test objects
against the independently built master library. All four failures reproduced
with exactly the same values, including 1142.6304840269083 versus
1142.6304840269095, and the 3.637978807091713e-06 gradient partition residual.
This is a focused master-library reproduction, not a claimed full master CTest
run. Sources, test objects and library identities are recorded; the two test
source files and all curve sources have an empty master diff. See
`master-nativearch-rate-failures.log` and `ctest-fix1.log`.
These failures are outside the F4 repair scope and are handed to DAL-201 for
routing. No test tolerance or compilation policy was changed to hide them;
the required default-preset full suite above passes.

### Final paired Python performance result and remaining gate

The unchanged full gate at final code 6725ae20 exits **1: 89 passed, 1 failed**.
All five cases that failed in the original hosted run pass locally:

| Case | Final round changes | Result |
|---|---|---|
| comparison.mc_vanilla_greeks_16384 | -2.44%, +3.22% | pass |
| comparison.mc_vanilla_greeks_65536 | +5.39%, -2.32% | pass |
| mc.vanilla.aad.compiled | +6.49%, +0.84% | pass |
| mc.vanilla.aad.tree | +1.94%, -2.71% | pass |
| mc.vanilla.double.tree | -1.59%, -1.71% | pass |
| mc.barrier.aad.compiled | **+8.21%, +10.05%** | **FAIL** |

A case fails when both confirmation rounds exceed +4%; a passing row does not
claim that each individual round is faster. Remaining compiled barrier AAD
minima are 9.173498 ms base and 10.095170 ms head. This case also failed both
rounds at c5e228be (+8.52%, +6.95%); the integrated unmodified F4 run was
+3.12%, +8.38%. Its causal attribution is unresolved. It is not waived as
DAL-223, labeled pre-existing, or replaced with an A/A result. Further disposition
belongs to the parent; this candidate is not ready for unconditional acceptance.

Both sides use master 8ee1b09d / code 6725ae20, the head's unmodified suite,
GCC 14.3.0, CPython **3.13.13**, pybind11 **3.0.4**, Release and native-CPU tuning
ON, with DAL_NUM_THREADS=4. These are matched locally, but not an exact recreation
of hosted GCC 14.2.0 / CPython 3.13.15 / AMD EPYC hardware. The requested 3.13.15
download was unavailable; its failed install log is retained. No hosted-version
equivalence or cross-host percentage comparison is claimed.

Both source roots were configured with the Release-linux preset plus explicit
GCC 14 compilers, examples OFF, DAL_BUILD_PYTHON=ON, DAL_ENABLE_NATIVE_ARCH=ON,
and the same absolute Python_EXECUTABLE and Python3_EXECUTABLE in bench-venv.
The configure/build logs and CMake cache identities are retained. From the F4
repository, the gate command was:

```bash
DAL_NUM_THREADS=4 ../bench-venv/bin/python .github/scripts/check_python_benchmark_regressions.py \
  --base-source ../benchmark-base --head-source . \
  --base-root ../benchmark-base/build/Release-linux --head-root build/Release-linux \
  --output-dir ../evidence/paired-final \
  --samples 10 --confirmation-rounds 2 --threshold-percent 4
```

Each complete run measured all 90 cases using 20 interleaved processes per side.
The complete before, first repair and final raw reports are attached; all
interrupted candidates are explicitly excluded. No further sampling was done
after the final verdict to seek a favorable result.

### Handoff and verification limits

The report-only publication commit has identical production/test trees to the
tested code above. Its exact SHA/tree, existing PR's base/head, and one post-push
CI snapshot are in the accompanying publication evidence. No CI watch, retry or
merge was performed. All run-owned builds and tests completed before delivery.

Default native correctness, Python tests and selected alternative regressions
pass. The final local Python performance gate remains failed as detailed above.
The four separately reproduced native-tuning rate-test failures need parent
routing. No final C++ paired benchmark rerun, Windows XLL, sanitizers, full
alternative public/Excel suites, installation or examples are claimed.

DAL-201 should evaluate the remaining performance evidence and then resume the
existing DAL-225 independent tests, DAL-226 documentation decision and DAL-227
review against the actual published revision. Their old approval does not cover
this revision. F4 remains unmerged and unaccepted; no follow-on role or F5 work
was started by this implementer.

## Complexity remediation after independent testing (historical)

Starting published head: `5b199ca0917d5c8cacb544114668d0e919137e53`, tree
`441dd13639b702e04822309b8af3e35753af71b2`. Tester commit
`203114eebf6303c7ea7513ed32516d8689fb3f33` is an ancestor; all its tests remain
byte-identical. The supplied testing archive was downloaded through Multica and
verified as SHA256
`064a6fb45c7e07b7be8609f4b9d8967a942ad87e5fce4bfdeb6be1d6766ec445`.

Tested refactor commit: `5db6705672424699d56f72a8ee76cc56a6533d29`, tree
`705985ed9cae6c3a00ead2a2d9a34f069ff790f2`. The publication commit changes only
this report; the attached `publication.json` records its exact SHA/tree. The
existing draft PR remains
https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371 on
`feature/dal-201-historical-aad-state`, stacked on
`feature/dal-200-eq-observation-slots` and dependent on #369.

Only `dal-cpp/dal/script/simulation.hpp` and this report change in this repair.
The production change extracts the existing worker body into
`Detail::EvaluateAADBatch`, passes passive controls through `AADBatchSettings_`,
and extracts the ordered final reduction into `Detail::AggregateAADResults`.
The public function signatures and defaults are unchanged. This report also
corrects the obsolete prepared-AAD rejection statement in the initial handoff.
The related current-state methodology correction remains DAL-226's scope.

Behavior preservation was checked against the full original and final runner:

- Caller validation stays in the same order: executability, RNG, raw historical
  rejection, metadata-model construction, expired return, prepared AAD/smoothing
  match, optional compilation, metadata allocation/initialization, then tasks.
  Error types and message literals are unchanged; source line locations move
  with the refactor.
- `compiledProduct` is engaged exactly when the original `useCompiled` was true;
  failed compilation still throws before workers. Raw legacy compiled and tree
  dispatch are retained. Named/prepared AAD compiled still rejects explicitly.
- Tape activation/rewind, active model, RNG, path, evaluator, registered zero,
  and historical seed remain batch-local on the executing worker. No shared
  active Number or additional recording is introduced. SkipTo and FillNormal
  retain their ordering, including the absent RNG for zero-dimensional paths.
- `InitModel4ParallelAAD` is unchanged: register inputs, NewRecording, model
  initialization and historical replay precede Mark. Each path rewinds, generates,
  evaluates, creates a PayoffRoot, checks finiteness, seeds and propagates to Mark.
  Mark-to-start propagation still precedes harvesting script and model risks.
  Historical hard and future fuzzy visitors are unchanged.
- Each risk contribution is divided by total nPaths once, in the same loop order.
  Final worker reduction preserves its original order and performs no second
  normalization. The zero-path low-level runner creates no batches or divisions.
- The task group remains after every captured local, including the new passive
  settings. Each task captures its PathBatch by value. Both normal completion
  and exceptional unwinding drain tasks before destroying their inputs.

### Static RED and GREEN evidence

No behavior RED is invented for this responsibility extraction. The existing
Codacy check `103877652329` (`action_required`) reports
`MCAADSimulation` complexity **16 > 8** at the initial F4 implementation head
`260de4cca02db9cf28f4bfabaa4b60f60722d744`. Its original check and annotation
JSON are retained in the attached evidence. The function was unchanged between
that head and the repair starting head.

Local Lizard **1.23.0** independently reproduces that count on the starting
source. Both local static invocations use the same command:

```bash
lizard -C 8 dal-cpp/dal/script/simulation.hpp
```

Before extraction: exit **1**, one warning, `MCAADSimulation` CCN **16**.
After extraction: exit **0**, zero warnings; `MCAADSimulation` CCN **8**,
`EvaluateAADBatch` **7**, `AggregateAADResults` **3**. Full outputs are
`complexity-before.log` and `complexity-after.log` in the new evidence bundle.
No analyzer configuration, suppression, macro, threshold, or ignore rule changes.

Lizard is not claimed to be the exact hosted Codacy analyzer/version/configuration;
that equivalent environment is unavailable locally. A green local complexity
check and green runtime tests do not establish hosted Codacy success. The single
post-push snapshot is delivered separately, without watching or polling CI.

### Refactor runtime verification

The new verification uses Linux x86_64, GCC **15.2.0**, CMake **4.2.3**, C++17,
and repository-pinned submodules initialized with
`git submodule update --init --recursive`. Source did not change during these
builds. Only the report differs from the exact tested code commit above.

```bash
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF
cmake --build build/Release-linux -j8
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptPastReplayTest.*:ScriptFixingPreparationTest.TestAad*:ScriptObservationSimulationTest.TestAad*'
ctest --test-dir build/Release-linux --output-on-failure -j4
```

The focused filter passes **16/16** before extraction on the starting code,
and **16/16** after extraction. Native full CTest passes **1715/1715**, zero
failures, **16.50 seconds**, including core, public C++, and portable Excel.
Logs: `baseline-f4.log`, `green-f4-native.log`, `ctest-native.log`, and the
corresponding configure/build logs. No new behavior or tests were introduced;
the existing analytic, oracle, and failure-recovery tests protect this refactor.

Alternative-backend builds and regression commands on the same tested source:

```bash
cmake --preset=Release-linux -S . -B build/Adept -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_ADEPT_AAD=ON
cmake --preset=Release-linux -S . -B build/CoDiPack -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_CODIPACK_AAD=ON
cmake --preset=Release-linux -S . -B build/XAD -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_XAD_AAD=ON
cmake --build build/Adept --target dal_cpp_tests -j4
cmake --build build/CoDiPack --target dal_cpp_tests -j4
cmake --build build/XAD --target dal_cpp_tests -j4
./build/Adept/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
./build/CoDiPack/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
./build/XAD/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
```

- Adept fork (CMake version **4.1.1**), pin
  `1e29edc6e16f969e99145f0cbff34ff0de5fe699`: **425/425**, 1261 ms.
- CoDiPack **3.1.0**, pin `86b94d3f3c3b6659a36f8e640945a7ebe1884a4a`:
  **425/425**, 1288 ms.
- XAD **2.1.0-dev**, pin `ca0146061726745870aac71f3108c7d14129d1b3`:
  **424/424**, 2608 ms.

Each selected run passed without failed or skipped cases. All sixteen F4 tests
(eleven original plus five tester additions) ran on all four backends. This
includes threads 1/2/4, paths 1/257/8193, 8193-path BS/Dupire fresh-recording
oracles, and the 16385-path seed-failure drain/recovery test. Preparation-read
counts, worker lookup rejection, direct/constant roots, nonlinear and hard/fuzzy
risks, repeated pricing and legacy compiled/tree tests retain their existing
assertions and tolerances. Alternative logs are `regression-Adept.log`,
`regression-CoDiPack.log`, and `regression-XAD.log`; each configure/build log
is also attached. All build and test processes completed before publication.

`git clang-format --diff HEAD^ HEAD` produced no formatting diff for the code
commit. `git diff --check` and staged whitespace checks passed. No production
scope deviation was needed. No docs/CHANGELOG, CI/Codacy configuration, ignore
rules, benchmark code/policy, public bindings, submodule pointers, or tests changed.
This refactor does not claim Python, Windows XLL, sanitizers, full alternative
public/Excel suites, performance measurements, or F5 compiled parity. Native
verification built all configured targets and ran full CTest; it did not run
installation or examples. F3 performance remains deferred in DAL-223. Hosted
Codacy, independent revalidation, documentation and review remain separate gates.

## Initial implementation evidence (historical)

The remaining sections record the original implementation at `4e4c6637`.
Their 1710/420/420/419 counts are historical, not results for the refactor head.

## Revision and scope

- Starting published F3 commit: `e8943ee2a399229cf36be412accb92ea1872b10e`.
- Starting tree: `ab062df7c6897dd594b3b444fc01f5b8172977f4`.
- Tested implementation commit: `4e4c663704d9378befcf69c086940e6e4361c111`.
- Tested implementation tree: `82ee22f351ea6c0f0d19ffed9dc5ca8e4fa9984c`.
- Branch: `feature/dal-201-historical-aad-state`.
- Stacked draft PR base: `feature/dal-200-eq-observation-slots`, dependency
  https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369.
- The publication commit adds this report only. The DAL-224 final comment and
  attached `publication.json` record its exact published head/tree and PR URL.

The controlling scope is the current DAL-224/DAL-201 description. Read the
approved spec, API note, and critique from DAL-197 attachments
`01a09606-bb52-7a4c-842c-80c53efa9a23`,
`01a09606-c6b7-778f-9517-26bb692c7b3f`, and
`01a09606-cc2d-7c77-8a00-4772c19c0c3d`, plus the script/AAD methodology and
repository implementation/test/Git contracts. No new public binding contract
or design deviation is introduced.

## Behavior and changed files

- `dal-cpp/dal/script/visitor/evalstate.hpp`: an optional typed historical seed
  preserves active dependencies when each path restores variables. The existing
  double initialization remains available to legacy and price-only evaluators.
- `dal-cpp/dal/script/visitor/evaluator.hpp` and `pastevaluator.hpp`: allow typed
  observation reads and hard historical replay with active script parameters.
  Historical PAYS still consumes its RHS without adding settled payments.
- `dal-cpp/dal/script/visitorlist.hpp`: registers `PastEvaluator_<AAD::Number_>`
  with the AST visitor machinery. This minimal integration file is necessary
  for the typed historical program to execute.
- `dal-cpp/dal/script/preparation.hpp` and `preparation.cpp`: prepare a
  conservative tree and IF metadata, replay doubles once before workers, and
  expose historical replay using the sealed observation plan. No domain or
  constant folding is added. These builder changes are necessary to admit F4
  through the existing core preparation entry points.
- `dal-cpp/dal/script/simulation.hpp`: shares the existing AAD batch runner
  between raw legacy and prepared products. Each batch constructs all active
  model, parameter, evaluator, and seed state on its worker, registers inputs,
  opens the recording, replays history, and marks. Path contributions propagate
  to the mark, then each batch propagates to the start. Risk normalization still
  occurs once against the total requested path count. Task-group draining is
  retained. Expired products return before unsupported execution dispatch;
  changed prepared AAD/smoothing settings fail explicitly.
- `dal-cpp/dal/math/aad/aad.hpp`: `PayoffRoot` adds a registered zero input to
  produce a fresh path root, including for passive constants. The extra input
  is worker-local, has value zero, and never enters reported risk labels.
- `dal-cpp/tests/script/test_past_replay.cpp`: analytic risk, direct seed,
  constant/empty suffix, hard past decision, repeated batch/pricing, independent
  per-path recording, future fuzzy, expired and mode-boundary regressions.
- `dal-cpp/tests/script/test_observation_simulation.cpp`: exact preparation-read
  counts, rejecting worker lookup seams, stable observation storage, preparation
  failure and path drain tests. The obsolete blanket named-AAD rejection becomes
  a compiled-mode rejection for both double and AAD; the newly supported AAD
  behavior has positive analytic and fuzzy tests.

The historical program and plan contain no shared active Number. The typed seed
exists only within its batch evaluator. Model-aware prepared AAD tree execution
is supported with matching AAD mode and smoothing. Named/prepared AAD compiled
execution remains explicitly rejected; raw future-only legacy compiled/AAD remains covered.
Raw historical AAD requires preparation. Compiled/domain integration belongs to F5.

## RED, GREEN, and refactor

All commands below run from the repository root. Native focused builds used
`cmake --build build/Release-linux --target dal_cpp_tests -j8`.

1. RED: `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=ScriptPastReplayTest.TestParameterRisk`
   failed with `UnsupportedExecutionMode: prepared AAD evaluation` before the
   production change. GREEN: the same test passed after typed replay and AAD
   preparation/runner integration. Logs: `red-parameter.log`, `green-parameter.log`.
2. RED: `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=ScriptPastReplayTest.TestDirectSeedPayoff`
   returned SCALE risk `0.31246185768338824` instead of `80` for 8193 paths.
   GREEN: after the fresh payoff root, `--gtest_filter='ScriptPastReplayTest.*'`
   passed both initial tests. Logs: `red-seed.log`, `green-seed.log`.
3. RED: `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptPastReplayTest.TestExpiredAad*:ScriptPastReplayTest.TestPreparedAadRejects*'`
   failed both tests: expired compiled dispatch threw, and changed smoothing did
   not throw. GREEN: after moving the expired return and guarding prepared mode,
   the 12-test integration filter below passed. Logs: `red-mode.log`,
   `integration-native.log`.
4. The first per-path oracle run used the RNG constructor's initial sequence
   offset, while MC explicitly calls SkipTo. Corrected the oracle to
   `SkipTo(0)` after inspecting the generator, keeping the required 1e-8 tolerance.
   No production RNG ordering changed. Edge tests then passed without further
   production changes. Build integration also required registering the new
   visitor and using DAL Vector's exposed accessors.
5. Refactor while green: shared the AAD runner and existing hard visitor instead
   of duplicating either; formatted changed lines with git-clang-format and the
   new test file with clang-format. `git diff --check` and formatting checks pass.

Integration filter:

```bash
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptPastReplayTest.*:ScriptFixingPreparationTest.TestAad*:ScriptObservationSimulationTest.TestNamedCompiledRejectsBeforeModelSetup'
```

## Verification on the tested implementation

Linux, GCC `15.2.0`, CMake `4.2.3`, C++17. Submodules were initialized at the
repository-pinned revisions with `git submodule update --init --recursive`.

```bash
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF
cmake --build build/Release-linux -j8
ctest --test-dir build/Release-linux --output-on-failure -j4
```

Native: **1710/1710 passed**, including core, public C++, and portable Excel;
0 failed, 16.37 seconds. `ctest-native.log` contains the full test list/results.

Alternative configurations and builds:

```bash
cmake --preset=Release-linux -S . -B build/Adept -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_ADEPT_AAD=ON
cmake --build build/Adept --target dal_cpp_tests -j4
cmake --preset=Release-linux -S . -B build/CoDiPack -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_CODIPACK_AAD=ON
cmake --build build/CoDiPack --target dal_cpp_tests -j4
cmake --preset=Release-linux -S . -B build/XAD -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_XAD_AAD=ON
cmake --build build/XAD --target dal_cpp_tests -j4
```

Run each alternative binary with this common filter:

```bash
./build/Adept/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
./build/CoDiPack/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
./build/XAD/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
```

- Adept fork `1e29edc6e16f969e99145f0cbff34ff0de5fe699`: **420 passed**.
- CoDiPack `3.1.0`, `86b94d3f3c3b6659a36f8e640945a7ebe1884a4a`: **420 passed**.
- XAD `2.1.0-dev`, `ca0146061726745870aac71f3108c7d14129d1b3`: **419 passed**.
- No failed/skipped tests in these selected runs. Counts differ because Adept's
  operand-stack lifetime and CoDiPack's concurrency test are backend-specific.
  All 11 new F4 tests run on all four backends.

Logs: `regression-Adept.log`, `regression-CoDiPack.log`, `regression-XAD.log`.
Earlier alternative runs during development used a narrower filter and, for
Adept, caught the two mode-boundary RED tests before their production rebuild;
the final common-filter results above supersede those intermediate runs.

## Acceptance evidence and limits

- T18: PV `160*exp(-r*T)`, SCALE risk `80*exp(-r*T)`, rate risk `-T*PV`,
  zero spot/vol risk, and exactly the four model labels plus SCALE.
- T19: 8193-path direct-seed risk is 80; constant payoff and otherwise empty
  suffix root propagation run successfully on every backend.
- T20: historical `>`, `>=`, and `=` at 79.95, 80, and 80.05 against a width-0.2
  threshold use hard selection and preserve selected SCALE arithmetic risk.
- T23: threads 1/2/4, paths 1/257/8193, sequential pricing at H=80/90/80,
  direct and discounted payoffs. The single-thread 8193 case spans two batches.
  A separate full-recording-per-path oracle matches every model/script risk for
  BS and Dupire over 257 fixed Sobol paths and all three thread counts.
- T03/T32: 100 historical nodes across three events, both modes, all requested
  path/thread combinations: exactly one History and one final Fixing read.
  Worker-local rejecting history/fixing seams cover historical seed replay and
  future evaluation; observation storage address/size stays unchanged during
  evaluation. This is a storage/read invariant check, not a general allocator
  profiler or a performance benchmark.
- T31: missing history, invalid model, and unsupported compilation submit zero
  workers. Invalid numeric payoff yields one failed evaluation for every
  submitted batch before error return; a following valuation succeeds.
- Future known-fixing fuzzy primal and K/SCALE sensitivities match the analytic
  smoothing expression and an independent fuzzy-double evaluation.

Deterministic PV uses the requested relative 1e-12 scale; analytic risks use
absolute 1e-10; shared-path MC comparisons use absolute 1e-8 on normalized
results. No tolerances or benchmark policies were weakened.

No docs/CHANGELOG, public bindings, F3 branch, CI policy, or performance work
changed. No Python, Windows XLL, sanitizers, alternative-backend full public
suites, or performance measurements were run in this implementer task. F5
compiled parity is explicitly unsupported rather than claimed here. Independent
testing/review must assess remaining acceptance combinations and lifecycle risks.

F3 performance remains the separate DAL-223 backlog item. Its previously
recorded failing required checks are not erased by this F4 correctness evidence.
The draft F4 PR must retain its dependency and must not gain parent-closing
lines or be merged until the remaining F4 roles and required gates are complete.
