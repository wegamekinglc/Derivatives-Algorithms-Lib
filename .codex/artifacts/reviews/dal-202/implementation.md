# DAL-202 / F5: native allocation repair and remaining performance gaps

The authorized native three-input allocation boundary preserves one shared allocation algorithm and restores the model phase close to master. Returning compiled AAD evaluation to the existing caller-side template reduces a separate evaluator cost. Fresh correctness passes across native, all three alternative AAD backends, Python, Clang sanitizers and static/shared/LTO consumers. This remains a partial performance repair, not F5 acceptance.

**The fresh complete C++ gate is RED, 61/63, and Python is RED, 87/90.** Two tape clear/rewind cases newly fail; compiled barrier AAD, mixed-calibration diagnostics and a quote-risk case fail in Python. The supported allocation boundary therefore does not yet satisfy the complete performance contract.

PR #372 remains open against master in its owner-set ready-for-review state. No closing lines, merge or F6 advancement is included. Parent DAL-202 owns acceptance and the serial DAL-229 tester, DAL-230 documentation/CHANGELOG decision and mandatory DAL-231 review. Earlier reports and the scope preflight do not approve this code.

## Scope and revisions

- Baseline: merged F4 master `b4e8b56135b5cfcbbe2ddd8d753921dd40d6caa2`, tree `f531d1858b881d3cf352c05c4e461e34f4263502`.
- Starting published head: `622ac82efa9185896093015ed3e2432653080fbc`, tree `4aca497271e642aecabf3aa1daceeb35c588ade0`; retained stock product `dad5e6ae417a47bade877230d8f0383287d68e1d`, tree `a7097403da513f35389ad248c8761bb9c6222466`.
- Freshly tested product: `0a7455c24afc77ef61e074bc0a06acd7a6323e14`, tree `da47d7954f67339183df6adf739745a09013bccd`.
- This report is the only subsequent repository change. The published SHA/tree and equality with tested code are recorded in `repair/published-identity.json` and the delivery comment. A report cannot contain its own commit hash.

Exactly four source/test files change from the starting head:

| File                                  | Change                                                |
|---------------------------------------|-------------------------------------------------------|
| `dal-cpp/dal/math/aad/tape.hpp`         | Shared private allocator and three-input call boundary. |
| `dal-cpp/dal/script/event.hpp`         | Remove compiled AAD specialization; retain tree entry. |
| `dal-cpp/dal/script/evaluation.cpp`    | Remove corresponding compiled wrapper definition.     |
| `dal-cpp/tests/math/aad/test_tape.cpp` | Add three focused native tape semantic regressions.   |

The fifth changed file is this report. All 1,150 inherited non-artifact tracked file hashes were recomputed; only these four source/test files differ. No enum regeneration is needed. There are no changes to bindings/API/model formulas, alternate backend adapters, curves, RNG, threadpool, benchmarks, thresholds, skips or CI. Diagnostic PR #373 was not imported.

The F4/F5 contracts remain: conservative folding, exact adjacent-float comparisons and tiny signed fuzzy divisors, hard historical replay/PAYS discard, typed seeds and terminal roots, fractional future weights, eager booleans and syntax-wide prefetch, zero-worker preparation errors, exception drain, LocalCheckedPaths and tape lifetimes. The native internal extension was explicitly authorized by the parent before implementation. No public contract or semantic design deviation is proposed.

## Allocation design and actual emitted effect

`Tape_::RecordNode<N>()` delegates to one private `AllocateNode<N>()` containing the original body. Node creation, multi-adjoint reservation/initialization, compile-time derivative/pointer reservations and their order are unchanged. `N=0` retains its no-derivative/no-pointer-allocation path. Positive arities retain compile-time capacity checks. Member layout, public signatures, caller-owned tapes, node counts and expression wiring remain unchanged.

An inline explicit `RecordNode<3>()` specialization appears immediately after the class, before consumers can instantiate it, and delegates to the shared body. It is ODR-safe across translation units. Compiler controls use guarded MSVC `__declspec(noinline)`, GCC 9+ `__attribute__((noipa))`, Clang/older GCC `__attribute__((noinline))`, and a plain inline fallback otherwise. The shared body uses the repository's existing `FORCE_INLINE`. No new macro, public helper, global flag, runtime reservation, `noexcept`, `cold` annotation or TLS cache is introduced. Other arities are not marked non-inline.

The initial supported noinline version restored the model phase alone, but GCC LTO cloned its allocator when combined with caller-side compiled placement. The `.constprop` clone reintroduced TLS access inside allocation. The GCC noipa boundary prevents this cloning; forced inlining of the shared body avoids an additional generic helper boundary. One shared algorithm remains.

The actual final CMake Python module was relinked unstripped from its own objects and original flags. Its `.text` matches exactly: SHA256 `11a6884b7dbbc72a46d70bcdbfa0198a8b9eafb188cc675f091e2f3c5af4413c`. The actual module SHA256 is `6ba4aabf2051d819b702be794c0b87fb128f1e9f5bb2b5aab95038a048d7a398`. The selected three-input allocator is unique, has no clone and performs no TLS lookup. The model calls it with the resolved tape pointer, restoring the four ordinary per-step TLS resolutions seen in master, versus eight in inherited F5. Whole-function site counts include initialization/fallback branches and are not dynamic counts. The intervention supports an allocation/inlining cause, without assigning precise per-lookup cost or excluding spill/layout effects.

`allocation-clone-ledger.json`, `noipa-emitted.json`, `final-emitted.json`, selected disassembly and maps retain the bodies. Allocation overlays rebuilt all 64 affected core/public/binding translation units, including the binding value TU that emits workers, followed by full final CMake builds. No baseline objects were transplanted into current modules.

## Evaluator investigation

The shipped evaluator change removes only the compiled AAD specialization; its existing generic header implementation remains available and the tree specialization stays in core. The selected binding/LTO compiled interpreter calls outlined `Number_::FromExpr` arithmetic helpers. Zero direct TLS sites in the dispatcher do not mean zero TLS use: recording helpers still resolve their tapes.

Fresh untimed actual-module capture compared event opcode streams and exact hexadecimal constant-pool values for 100 prepared valuations. Streams, event lengths and constants match master (`pool-comparison.json`). This rules out those prepared-work differences for the measured cases, not every possible graph/metadata difference.

Five fresh matrices retain 3,840 sampled case comparisons with identical native arguments, path counts, PVs and all risk keys/values (relative 1e-10 / absolute 1e-9). They retain the original 16 canonical script MC financial cases, validators and two warmups. Native barrier/vanilla AAD use 10,000/20,000 paths; comparison cases use 16,384/65,536. Comparison barrier greeks are seven bumped double valuations; comparison vanilla greeks use AAD. Unchanged relink/stock controls remain. Each matrix uses ten alternating process samples per variant; these are diagnostic subsets, not complete acceptance gates.

Four-thread uninstrumented minima in milliseconds from `matrix4-boundary`:

| Workload                        | Master  | Stock relink | Allocation only | Combined |
|---------------------------------|---------|--------------|-----------------|----------|
| Barrier AAD compiled            | 9.0182  | 11.5820      | 10.8331         | 9.5964   |
| Barrier AAD tree                | 10.9077 | 11.5387      | 10.6257         | 10.5606  |
| Vanilla AAD compiled            | 0.8192  | 0.9303       | 0.8888          | 0.7949   |
| Comparison vanilla greeks 65536 | 2.4278  | 2.7895       | 2.6160          | 2.3469   |

Allocation-only is the initial supported plain-noinline version; combined adds forced shared-body inlining, GCC noipa and caller compiled placement. Individual variants and all raw data remain in the seven-variant matrix; effects are not assumed additive. A separate capture experiment measured master/combined barrier compiled at 8.9101/9.6553 ms. Both matrices remain, without selecting a best result across matrices.

One-thread phases, ns/path, from `matrix1-capture`, using phases from each minimum end-to-end sample:

| Variant      | Compiled model | Compiled evaluation | Tree model | Tree evaluation |
|--------------|----------------|---------------------|------------|-----------------|
| Master       | 1420.2         | 1262.1              | 1408.1     | 1901.0          |
| Combined     | 1410.1         | 1574.7              | 1418.5     | 1893.9          |
| Capture copy | 1415.1         | 1567.1              | 1412.1     | 1887.6          |

The model phase is close to master; compiled evaluation retains about 313 ns/path. Inherited overhead controls showed material clock perturbation for small workloads. Instrumented totals are not acceptance timings; phases are not independently minimized.

Bounded evaluator interventions were rejected:

- Relaxing forced inlining of the arithmetic group outlined an extra helper without improving the target financial cost (`caller-supported-arithmetic`).
- Copying the small sample accessor capture removed the intended reference indirection, but did not materially improve phase/total cost. Four-thread compiled barrier was 9.8093 versus 9.6553 ms for its control (`caller-supported-forcebody-noipa-flatcapture`). This change is not shipped.
- GNU flatten on the sum dispatcher did not remove selected add/sub `FromExpr` call boundaries. `flattensum-no-effect.json` records that the intended intervention failed. It was not timed and does not disprove a successful arithmetic-inlining intervention.

The evaluator residual is unresolved. Earlier rejected model helper, root outlining, tape caching, extern and LTO/semantic-interposition experiments remain inherited and were not repeated. Further work should target actual selected compiled arithmetic/dispatch bodies with a verified intervention while preserving the established allocation control. A universal cause for all timing failures is not claimed.

After the complete C++ gate exposed the static tape-chain cost, one final allocation diagnostic replaced GCC noipa with noinline,noclone. This preserves the inlining/cloning constraints while allowing other IPA analyses. All 64 affected core/public/binding TUs and the original static tape benchmark TU were rebuilt. The selected Python allocator/model and static allocator/BuildChain retain identical instruction sequences after removing relocation addresses; registers, constants and branch offsets remain compared (`noclone-instruction-comparison.json`). The intended register-analysis improvement did not occur. No additional timing or full-gate rerun was used, and this diagnostic is not shipped. The static clear/rewind regression remains an explicit unresolved tradeoff of the current boundary.

## RED, GREEN and fresh verification

Timing-only TDD follows the parent's explicit adaptation: the inherited complete Python RED is 83/90, with original commands/raw samples retained. No semantic failure was fabricated. Before production edits, all three new tests and existing tape tests passed 13/13 under `build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=AADTapeTest.*` (`semantics-before.json/log/xml`). These protect uncovered semantics while performance supplies the RED oracle.

New tests exercise three-input expressions with multiple results, aliasing and compound self-assignment; independent node/derivative/pointer/multi-adjoint block rollover; mark/rewind/re-recording and stale-adjoint reset; and caller-owned arities 0/1/3/5. They use existing interfaces and analytic derivatives, without new hooks. Final native and sanitizer suites include them.

| Fresh final-revision verification                 | Result                                  |
|--------------------------------------------------|-----------------------------------------|
| Native Release CTest                              | 1748/1748, including Python CTest entry  |
| Python within native CTest                        | 402/402                                 |
| Adept / CoDiPack / XAD full CTest                  | 1735/1735, 1735/1735, 1734/1734           |
| Clang 21 ASan/UBSan AAD/script/MC                  | 400/400                                 |
| Shared Python                                    | 401 passed, one missing fixture covered next |
| Rebuilt private shared fixture, quote-risk module | 8/8, including previously skipped case   |
| Unchanged independent exact/tiny/allocation       | 4/4, 6/6, 2/2                            |
| GCC static/shared LTO and Clang sanitizer LTO users| All builds and executions pass           |
| Documentation/whitespace                          | 58 Markdown files and whitespace pass                              |

Native CTest comprises 1,747 non-Python entries plus the Python entry. Shared Python initially skipped one test because `_dal_quote_risk_test` was not built; after building it, all eight tests in that module passed. All 402 unique shared cases were therefore exercised without modifying skips. All 27 lifetime combinations (threads 1/2/4, 8,193 paths, fixings 80/90/80), 33 legacy/parity/fuzz cases and fixed F4/F5 financial/error oracles remain covered by fresh suites.

GCC 14 Release correctness uses native-architecture OFF. Performance retains ON and original binding LTO/visibility. Four ON curve failures were previously reproduced identically on master; curves are unchanged. Clang ASan enables leak detection, UBSan halts on error. Two-TU consumers use GCC LTO against static/shared core and Clang LTO plus sanitizers. Baseline/current size 368, alignment 8 and member offsets 0/8/16/88/160/232/304 match. Windows/MSVC is unavailable locally; guarded spelling and hosted Windows consumer/benchmark checks still require validation.

Commands, cwd, exit status and elapsed time are retained in corresponding `repair/*.json`. Principal GREEN commands after final source edits:

```sh
cmake --build build/Release-linux -j 8
ctest --test-dir build/Release-linux --output-on-failure -j 4
python3 repair/verify_backends.py
cmake --build build/clang-sanitized --target dal_cpp_tests -j 6
env ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 build/clang-sanitized/dal-cpp/dal_cpp_tests '--gtest_filter=AAD*:Script*:MonteCarlo*:MCSimulation*'
env PYTHONPATH=build/shared/dal-python DAL_NUM_THREADS=4 python3 -m pytest dal-python/tests -q
cmake --build build/shared --target _dal_quote_risk_test -j 6
env PYTHONPATH=build/shared/dal-python DAL_NUM_THREADS=4 python3 -m pytest dal-python/tests/test_joint_quote_risk.py -q
python3 repair/verify_oracles.py
python3 repair/verify_consumers.py
python3 repair/inspect_final.py
python3 .github/scripts/check_docs.py
```

Evidence scripts run from the evidence root; repository commands run in the checkout. Configuration records retain all options/compiler paths. Diagnostic setup failures remain in `diagnostic-setup-notes.md`: a Clang core-only configuration initially retained portable Excel, and a trace transformation relied on a moving allocator spelling. Both were corrected before affected testing/sampling; the harness now pins `supported-tape.hpp` to commit `31ad288`. Neither was a product failure or completed timing result.

## Original performance gates and provenance

Both original gates use final product binaries after local builds/tests finished, sequentially with `DAL_NUM_THREADS=4`. They retain the full inventories, two rounds, ten alternating samples per side, minimum reduction and strict 4% rule, including all samples/per-round results. No unchanged completed gate was rerun to seek green.

```sh
env DAL_NUM_THREADS=4 python3 .github/scripts/check_benchmark_regressions.py --base-root BASE/build/performance --head-root build/performance --output-dir EVIDENCE/repair/full-cpp --samples 10 --confirmation-rounds 2 --threshold-percent 4
env DAL_NUM_THREADS=4 python3 .github/scripts/check_python_benchmark_regressions.py --base-root BASE/build/performance --head-root build/performance --base-source BASE --head-source . --output-dir EVIDENCE/repair/full-python-canonical --samples 10 --confirmation-rounds 2 --threshold-percent 4
```

`BASE` is the verified F4 checkout, `EVIDENCE` this working directory. Exact commands/paths are in `gate-cpp-final.json` and `gate-python-canonical.json`. The first Python invocation stopped before sampling because CMake stored four false booleans as `off` instead of baseline `OFF`; its case-sensitive configuration guard rejected them. Canonicalizing those unchanged false values fixed setup without changing flags/binaries or the gate. The failed zero-case invocation is retained in `full-python` and `python-config-mismatch.json`.

All 63 C++ and 90 Python cases completed. Failures exceed 4% in both confirmation rounds:

| Gate/case                              | Round 1 | Round 2 |
|----------------------------------------|---------|---------|
| C++ Clear + re-record, 100K nodes        | +5.33%  | +4.91%  |
| C++ Rewind + re-record, 100K nodes       | +7.60%  | +7.71%  |
| Python calibration.MIXED.BUMPED.diagnostics | +4.47% | +5.60% |
| Python mc.barrier.aad.compiled          | +14.58% | +9.63%  |
| Python quotes.single.n5.ANALYTIC.t120    | +5.07%  | +4.88%  |

The six other historically failing Python cases pass this fresh complete gate, but that does not waive these three failures or establish their cause. The new calibration/quote-risk failures are retained without attributing them to noise or expanding product scope. The actual static tape-chain loop allocates inline on master and calls the selected three-input allocator on this candidate; this exposes a cost of the same boundary that helps the Python model. The complete C++ result is a regression from inherited63/63 and must be resolved before acceptance.

`final-build-identity.json` retains source/binary hashes, compiler flags, link commands and environment. All 23 retained binary inventory entries per baseline/stock were freshly verified before reuse. WSL2 host load and unavailable hardware counters limit attribution. Inherited C++63/63, Python83/90 and A/A90/90 remain intact; A/A does not waive baseline regression. New local results do not replace required hosted checks or independent review.

The attachment retains commands, source overlays, selected instructions, maps, raw financial/phase/gate data, identities, reconstruction scripts and this matching report. Generated binaries/objects/full-module disassemblies are excluded with hashes/rebuild commands. It nests the preceding archive unchanged, SHA256 `03adfb403e9ed6119b2d5ee12f091c9e03b757b1be160c2414856fe71cf181ad`; all 1,603 outer manifest entries were freshly verified. The nested phase archive remains `643f12959c87308ff24a8d8e54eff47b9af615f7c89eeb7ac6c099e697cce45d`; its 1,560 entries were parent-verified, not freshly reverified here. The new manifest/checksum covers this delivery. Publication records one CI snapshot without watch/polling.
