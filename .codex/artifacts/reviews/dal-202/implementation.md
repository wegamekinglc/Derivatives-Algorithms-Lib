# DAL-202 / F5 implementation: actual Python module attribution

The actual Python module reproduces the remaining script cost. Phase measurements locate a substantial part in model-path recording and compiled evaluation, with a smaller, directly repairable preparation cost. This delivery removes the redundant preparation traversal; it does not claim to resolve the dominant execution regression. Parent DAL-202 retains acceptance and the existing specialist sequence. PR #372 remains open against master in the owner's ready-for-review state; no merge, closing intent or F6 advancement is requested.

## Revisions and scope

- Baseline: merged F4 master `b4e8b56135b5cfcbbe2ddd8d753921dd40d6caa2`, tree `f531d1858b881d3cf352c05c4e461e34f4263502`.
- Inherited tested code: `5395b75e31789d733ff879aa948faf2d6a86d4ac`, tree `0ba8901fc1628503ac62740914a3248210828962`. Inherited published head `cc9c2242a80193ba3599eeeac59ece3203442c57`, tree `d474514680809d91188eb297456ee8648b22a730`, added only its report.
- This tested code: `dad5e6ae417a47bade877230d8f0383287d68e1d`, tree `a7097403da513f35389ad248c8761bb9c6222466`. The publication commit adds only this report. Its immutable SHA/tree and the comparison of all 1,150 tested source hashes are recorded in the delivery comment and `published-identity.json`; a report cannot contain its own commit hash.
- Product diff this turn: two guard expressions in `dal-cpp/dal/script/visitor/constcondprocessor.hpp`. Report diff: `.codex/artifacts/reviews/dal-202/implementation.md`. Diagnostic sources, counters, link overlays and timing scripts stay outside the product tree.

Design: search recursively for eager AND/OR only when domain flags permit constant folding. Otherwise both existing paths visit all children already. The guard remains mandatory for folding candidates, so eager operand evaluation, syntax-wide history prefetch and exception semantics remain unchanged. No benchmark, threshold, skip, CI, model, curve, RNG, threadpool, binding or public contract change is shipped. Existing historical roots, typed seeds, exact/fuzzy fixes and core AAD evaluator placement remain intact.

## Actual-workload evidence

`workloads.py` imports the canonical benchmark cases, invokes their unchanged preparation and validators, performs two warmups, then measures all 16 script MC cases. Wrappers retain every measured public/native argument, constructor cost, PV and risk map, with the loaded module path and hash. Across diagnostic variants every native call's arguments and every PV/risk key and value agree (relative 1e-10, absolute 1e-9); canonical independent oracles and same-path parity checks also pass.

The eight native cases use vanilla double 200,000 / AAD 20,000 paths and barrier double 100,000 / AAD 10,000 paths. Native barrier has 54 event samples, H=150, K=100, explicit `:0.1`, epsilon .01, r=.05, q=.02. Comparison cases use 16,384 / 65,536 paths; vanilla greeks use AAD, whereas barrier greeks run seven bumped double valuations. Comparison barrier has 52 weekly observations plus initialization, H=130, no explicit smoothing width, epsilon .01, r=.03, q=.01. Both use Sobol path IDs, Brownian bridge false, four threads and batches `min(8192, ceil(paths/4))`. These differences prevent treating the older 65,536-path shared probe as equivalent evidence.

Python's binding value translation unit emits weak Monte Carlo template instantiations too. Instrumenting only the public archive yielded public records but **zero** worker records: the linked module selected the binding-emitted bodies. Rebuilding that binding TU as a diagnostic overlay produced all 940 batch records and 120 public calls per process. This is a concrete reason public-library-only probes missed the real execution path. The overlay does not change the shipped binding.

An independent atomic-counter overlay in core `evaluation.cpp` verifies execution, not merely symbol presence: every actual AAD valuation reaches the intended tree/compiled specialization exactly once per path, and double valuations produce zero hits. All 120 valuations pass this assertion (`reach.log`); the measured 28 native calls retain their complete results.

## Phase attribution and its limits

Ten interleaved process samples per diagnostic variant compare original modules, unchanged relinks, public-only tracing, full tracing without per-path clocks, and full tracing every 64th path. A separate ten-sample, one-thread experiment measures both every path and every 64th path. These are diagnostic controls, not acceptance gates. Phase summaries use the phase records from each variant's minimum end-to-end sample, not independently selected phase minima.

One-thread, every-path measurements in the real linked module (nanoseconds per path; master -> inherited F5):

| Workload | Model path / recording | Evaluation | Reverse propagation |
| --- | ---: | ---: | ---: |
| barrier AAD compiled | 1406.9 -> 1724.8 | 1242.2 -> 1905.0 | 281.3 -> 280.3 |
| barrier AAD tree | 1399.5 -> 1741.3 | 1913.2 -> 1910.7 | 283.1 -> 278.6 |
| vanilla AAD compiled | 52.0 -> 58.2 | 61.8 -> 82.3 | 38.1 -> 37.7 |

Sampling every 64th path confirms the direction. Four-thread uninstrumented barrier AAD compiled takes 8.924 ms on master versus 11.464 ms on inherited F5. A rough scale estimate from the measured model/evaluation deltas, 10,000 paths / four workers plus about .18 ms preparation, gives 2.63 ms versus the observed 2.54 ms difference. This explains a meaningful portion at phase level; it is not an exact additive decomposition across differently instrumented runs.

The tree case's model cost also grows while evaluation and reverse stay similar. Every-path native tape counters show equal node counts on both versions, both execution modes and every matched batch: vanilla has 3 model nodes and 7 post-evaluation/root nodes per path; barrier has 107 model nodes and mean 220.3277 post-evaluation/root nodes. Extra node volume or terminal-root growth is therefore unsupported as the cause. Equal totals do not prove identical graph topology or memory layout.

Setup and result conversion do not explain the dominant barrier gap. Public result timing includes map construction and destruction. Python minus public time bounds a residual containing binding conversion, dispatch/GIL and measurement overhead; it does not isolate conversion itself. Per-path clocks materially perturb small vanilla work (nearly doubling its one-thread total); no absolute clocked duration is used for gate acceptance. Four-thread timing also includes load imbalance and scheduling. One-thread reproduction rules out scheduling as the sole explanation.

Disassembly and link maps retain the module code-generation, TLS and inlining differences. Core evaluation rebuilt with LTO partially improves one barrier diagnostic (11.447 -> 10.559 ms, still behind master's 8.698 ms), but does not repair the vanilla comparison; `-fno-semantic-interposition` also fails as a complete explanation. Neither flag is shipped. The earlier event-only preparation overlay left weak visitor definitions in other TUs and is insufficient attribution; it is retained and superseded by the full ten-TU preparation rebuild. Earlier root outlining, tape-pointer caching and extern-instantiation experiments remain rejected and preserved in inherited evidence.

Remaining evidence needed: a controlled intervention that explains the model-recording code-generation increase and the compiled evaluator increase together in the real module without changing the canonical work. There is no instruction-level causal proof yet, and this delivery does not propose a speculative binding or model edit. Parent should route any needed expansion beyond F5 scope. The host is WSL2; all own timing runs were sequential and excluded builds/tests, but external host load cannot be excluded. `perf` is unavailable on this machine.

## Supported preparation repair and TDD

The inherited complete Python result (81/90, nine failures) is the controlling timing RED. The issue explicitly permits timing RED plus unchanged correctness tests as the TDD adaptation; no financial behavior or test tolerance was changed. Before editing, the verified inherited native binary passed the focused 454 tests. The new guarded traversal was then tested with all affected core/public/binding TUs rebuilt, with identical flags and complete financial-output checks.

Ten interleaved process samples on the fully rebuilt preparation overlay show:

| Workload | Before / after condition-fold phase | Before / after end-to-end minimum |
| --- | ---: | ---: |
| native barrier double compiled | 187.169 / 1.449 us | 22.290 / 21.260 ms |
| native barrier AAD compiled | 166.816 / 1.341 us | 11.393 / 11.159 ms |
| comparison barrier greeks 16384 (seven calls) | 1149.874 / 9.762 us | 26.370 / 24.982 ms |
| comparison barrier greeks 65536 (seven calls) | 1304.735 / 11.583 us | 99.071 / 94.829 ms |

Only the removed traversal's measured time is causally attributed to this repair; the larger total movement can include module layout and host variability. Vanilla's fold time is already negligible. The change addresses fixed setup per valuation, and seven-fold repeated setup in bumped barrier risk; it is not expected to erase a per-path AAD regression.

Reproduction commands from the evidence root's parent (all command/cwd/exit records and raw outputs retained):

```text
python3 phase-evidence/collect.py phases base:stock:0,candidate:stock:0,base:profile-full:0,candidate:profile-full:0,base:profile-full:64,candidate:profile-full:64 10
python3 phase-evidence/summarize.py phases
python3 phase-evidence/build_prep.py
python3 phase-evidence/collect.py preparation candidate:prep-before:0,candidate:prep-after:0 10
python3 phase-evidence/summarize.py preparation
python3 phase-evidence/final_correctness.py
python3 phase-evidence/reproductions.py
python3 phase-evidence/final_gates.py
```

The separate overlay builders and their captured commands are required before diagnostic collection. `build_overlays.py`, `build_reach.py`, `build_interventions.py`, `build_prep.py`, `trace.hpp`, and the generated overlay sources reproduce all probes. Their original absolute source/build locations are provenance, not portable defaults. The first Python preflight rejected preset `off` versus baseline `OFF` before collecting any cases. `align_cache_and_python.py` normalizes those three backend booleans through CMake, verifies every binary and the core archive are byte-identical, then runs the complete comparison into `python-paired-complete`. The corrected configure script passes the uppercase values explicitly. The failed zero-case preflight remains in `python-paired`; it is not a completed timing run.

## Fresh correctness and acceptance

Fresh correctness passes: native CTest 1744/1744; focused native/Adept/CoDiPack/XAD 454/453/453/452; native Python 402/402; exact-boundary 4/4, signed tiny-divisor 6/6 and double allocation 2/2. The focused suites retain 30 legacy parity/fuzz and 3 observation parity cases per backend, eager AND/OR, history prefetch, zero-submission preparation failures, exception drain and all 27 compiled lifetime combinations (threads 1/2/4, 8193 paths, fixing sequence 80/90/80). No oracle was weakened.

Correctness uses the established Release native-architecture-OFF build. An initial native-architecture-ON CTest run reported four curve numeric failures and one incidental benchmark timing abort while alternative builds were running. Linking byte-identical curve-test objects to the verified master core reproduces all four numeric failures with identical values (`native-arch-master-curve.log`); no unrelated curve fix is included. The incidental `rate_risk_perf` timing is not a valid gate measurement. All initial logs remain retained. The canonical correctness rerun excludes the benchmark label, as the repository workflow requires. Performance builds retain the original native-architecture-ON flags on both sides.

Fresh isolated performance-build Python correctness also passes 402/402 on each side. The informational `script_mc_perf` smoke passes. Documentation checks pass for 58 Markdown files and whitespace checks pass.

Complete original gates: **C++ 63/63 PASS; Python 83/90 FAIL**. Each retains all inventory entries, two rounds of ten interleaved processes per side, minimum reduction and the original strict +4% rule. No sampling was concurrent with own builds or tests. Byte-identical candidate Python A/A is **90/90 PASS**; it does not waive any master/candidate failure. The fresh Python failures are:

| Case | Round 1 | Round 2 |
| --- | ---: | ---: |
| comparison.mc_barrier_greeks_16384 | +6.45% | +4.87% |
| comparison.mc_vanilla_greeks_16384 | +24.79% | +14.68% |
| comparison.mc_vanilla_greeks_65536 | +17.75% | +20.76% |
| comparison.mc_vanilla_price_16384 | +6.80% | +13.56% |
| mc.barrier.aad.compiled | +28.07% | +29.90% |
| mc.barrier.aad.tree | +8.47% | +9.26% |
| mc.vanilla.aad.compiled | +13.49% | +21.35% |

Barrier double compiled and barrier greeks 65536 pass this complete run, but comparing separate historical gates is not causal proof that the repair removed either regression. The controlled preparation measurements above support only the redundant-scan benefit. The overall performance acceptance verdict remains **regression found / gate RED**; attribution to precise instructions remains incomplete. No unchanged complete gate was repeated to seek a passing result.

Builds use GCC 14, CMake 4.2, Release, native AAD, core `-O3 -march=native -ffp-contract=fast`, static core/public libraries and the original pybind LTO/visibility flags. Baseline and candidate use isolated detached sources/builds; their absolute locations, module hashes, cache options, CPU identity and four-thread environment are in `base-build-identity.json`, `candidate-build-identity.json` and `environment.json`. The inherited baseline binaries were verified byte-for-byte before reuse. Full C++ results are in `cpp-paired`, complete Python results in `python-paired-complete`, and control results in `python-aa-paired`.

## Evidence and acceptance boundary

The attachment `dal-228-python-phase-evidence.tar.gz` contains scripts, raw samples, all phase records and complete PV/risk maps, generated overlay sources, relevant disassembly, commands, build/module/source identities, fresh correctness/gate results and a SHA256 manifest. `inherited/dal-228-aad-evidence.tar.gz` is carried forward byte-for-byte (SHA256 `73380a7e315394eba48e0b9ecbf817bde4084b5a6e2b861afffbd206e17b2b93`), including all older failed gates and A/A controls; its 842-entry current manifest was freshly verified. Large generated executables, archives and object files are excluded from delivery; hashes and reconstruction commands remain. The attached report equals the report in Git.

The nine C++ targets do not directly cover this preparation traversal. Existing Python native and comparison MC cases and the informational `script_mc_perf` executable provide the relevant coverage; no new benchmark or acceptance rule is introduced. Fresh full inventories and all round deltas are in each gate's `results.json` and `summary.md`.

Windows XLL, sanitizer and full alternative-backend public/Excel suites were not run locally. Allocation evidence covers prepared double observations, not arbitrary malloc or AAD tape allocation. Named public binding/archive additions and DAL-223 remain outside scope. Specialist approvals on older revisions do not approve this change. Parent review is required before further specialist acceptance or merge.
