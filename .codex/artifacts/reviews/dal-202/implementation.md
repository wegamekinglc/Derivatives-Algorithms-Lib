# DAL-202 / F5: review fixes and unresolved allocation tradeoff

The prepared-mode diagnostic, supported allocation-test registration and tape-test complexity finding are repaired. The final report tables are aligned. No new performance implementation is accepted: six timed compiler/allocation candidates failed to repair the combined native/Python tradeoff, and a seventh produced identical selected instructions and was not timed.

**The last complete acceptance gates remain RED: C++ 61/63 and Python 87/90.** Those are inherited results on product `0a7455c24afc77ef61e074bc0a06acd7a6323e14`, not fresh gates on this review-fix revision. No unchanged complete gate was rerun to seek green. The next proposed allocation boundary requires one additional AAD file; the concrete scope request appears below.

PR #372 remains open against master in its owner-set ready-for-review state. Parent DAL-202 owns acceptance and the serial DAL-229 independent tester, DAL-230 documentation/CHANGELOG decision and mandatory DAL-231 review. This partial handoff does not approve F5 or advance F6.

## Revisions and shipped scope

- Baseline F4 master: `b4e8b56135b5cfcbbe2ddd8d753921dd40d6caa2`, tree `f531d1858b881d3cf352c05c4e461e34f4263502`.
- Starting published head: `4998cf30c10ad8e3f9f3381da2c1787f487e53fd`, tree `e0a46296647b40345ea0d4e144ddbad6e10fc446`.
- Freshly tested product: `3de188ba0d2d5dc2778a20c22052c614f8d71cba`, tree `54fed4f31fcbbdd040dffd993c85f67c7418a8ee`.
- The subsequent repository change is this report only. `repair/published-identity.json` and the delivery comment record the published SHA/tree and equality with tested source.

Four source/test files change from the starting head:

| File                                        | Change                                                                      |
| ------------------------------------------- | --------------------------------------------------------------------------- |
| `dal-cpp/dal/script/simulation.hpp`         | Apply the exact approved mismatch diagnostic.                               |
| `dal-cpp/tests/script/test_past_replay.cpp` | Cover both mode directions, defaults, AAD/smoothing and expired behavior.   |
| `dal-cpp/CMakeLists.txt`                    | Register the dedicated script observation allocation executable with CTest. |
| `dal-cpp/tests/math/aad/test_tape.cpp`      | Extract meaningful rollover/allocation assertion helpers.                   |

The fifth file is this report. No enum regeneration is needed. The prior shared tape allocation algorithm and three-input noipa boundary remain unchanged. There are no new model, curve/calibration, backend, binding/signature, benchmark, threshold, skip, required-check or global compiler-flag changes. All experimental overlays are outside the product tree.

## Review fixes and RED/GREEN evidence

The public diagnostic follows the parent's completed API decision exactly:

`UnsupportedExecutionMode: AAD mode, smoothing, or compiled/tree mode differs from preparation`

The predicate, validation order and ScriptError_/UnsupportedExecutionMode contract are unchanged. Focused coverage checks compiled-to-tree and tree-to-compiled rejection, nullopt resolving to tree in preparation and execution, direct MCAADSimulation with AAD-disabled preparation, changed smoothing, matching analytic PV and the pre-existing all-expired zero-PV/zero-risk fast path. Earlier wrapper diagnostics remain separate.

`diagnostic-red` ran the strengthened `ScriptPastReplayTest.TestPreparedCompiledModeCannotChange` against the old production string: exit 1 because the expected compiled/tree wording was absent. The minimum string replacement then passed the same command (`diagnostic-green`, exit 0). Additional edge cases preserve existing behavior rather than inventing semantic failures.

The existing allocation fixture is now a dedicated `dal_cpp_script_observation_allocation_tests` target with `bcg_allocation_probe.cpp`, test main, dal_cpp/gtest, platform options and `gtest_discover_tests`. Its global allocator replacement is isolated from ordinary dal_cpp_tests. The pre-change discovery command failed with exit 8, No tests found (`allocation-discovery-red`). A clean supported build now discovers and runs both unchanged assertions, 2/2 (`test-allocation-clean`).

The tape test was green before extraction, 13/13. `ExhaustAllocationStream` and `CheckThreeInputAllocation` preserve all four independently exhausted streams, three reuse cycles, multi-adjoint propagation/reset, mark/rewind/new-recording checks and existing alias/self-assignment tests. Fatal helper assertions propagate through ASSERT_NO_FATAL_FAILURE. Local lizard measurement reduces the flagged test from complexity 11 to 4; helpers are 5 and 4. This is local evidence, not a claimed fresh Codacy acceptance.

Principal commands (repository cwd unless the command starts with repair/):

```sh
build/Release-linux/dal-cpp/dal_cpp_tests '--gtest_filter=ScriptPastReplayTest.TestPreparedCompiledModeCannotChange'
ctest --test-dir build/Release-linux -R ScriptObservationAllocationTest --no-tests=error --output-on-failure
cmake --build build/allocation-clean --target dal_cpp_script_observation_allocation_tests -j 8
ctest --test-dir build/allocation-clean -R ScriptObservationAllocationTest --no-tests=error --output-on-failure
cmake --build build/Release-linux -j 12
ctest --test-dir build/Release-linux --output-on-failure -j 4
python3 repair/verify_backends.py
python3 repair/verify_sanitizers.py
python3 repair/verify_oracles.py
python3 .github/scripts/check_docs.py
```

## Fresh applicable correctness

| Verification                                        | Result                                       |
| --------------------------------------------------- | -------------------------------------------- |
| Native GCC 14 Release CTest                         | 1753/1753, including the Python entry.       |
| Python within native CTest                          | 402/402.                                     |
| Focused review-fix/tape CTest                       | 19/19.                                       |
| Clean supported allocation CTest                    | 2/2.                                         |
| Full Adept / CoDiPack / XAD CTest                   | 1740/1740, 1740/1740, 1739/1739.             |
| Clang 21 ASan/UBSan AAD/script/MC                   | 403/403.                                     |
| Clang isolated allocation CTest                     | 2/2.                                         |
| Unchanged independent exact/tiny/allocation oracles | 4/4, 6/6, 2/2.                               |
| Documentation and whitespace                        | 58 Markdown files and git diff --check pass. |

Fresh native coverage includes all 27 lifetime combinations (threads 1/2/4, 8193 paths, fixings 80/90/80), all 33 legacy/parity/fuzz cases, fixed F4/F5 financial/error oracles, history prefetch, zero-worker preparation failure and task-group exception drain. Exact-boundary and tiny-divisor oracles were rebuilt unchanged against current core. GCC correctness uses native-architecture OFF; experiments retain original ON performance flags and binding LTO/visibility. Clang uses leak detection and UBSan halt-on-error.

Shared Python and separate static/shared/LTO consumer results from the previous product remain inherited, not freshly claimed here: this turn ships no AAD implementation/layout/linkage change. Windows/MSVC is unavailable locally. Hosted checks and independent review remain required. Every executed verification retains command, cwd, exit status and elapsed time. Initial missing-submodule configuration, the makefile-regeneration target lookup and the diagnostic serializer setup failure are retained as setup failures and were corrected before their successful runs.

## Bounded performance experiments

The starting stock binaries are those of product 0a7455; all 23 retained stock hashes were verified. The baseline checkout is clean at F4 master. Stock Python module SHA256 is `6ba4aabf2051d819b702be794c0b87fb128f1e9f5bb2b5aab95038a048d7a398`; tape_perf is `01bcd0e5a23a51a579bea89c1b8a3410e903a5455ef192199c2c5bcf04856839`.

Each overlay rebuilds every affected core/public/binding translation unit identified by original dependency files, including the binding value TU, then relinks the actual Python module and static tape benchmark with the original flags. Rebuilt object inventories, selected instructions, maps and hashes are retained. No baseline objects are transplanted into current modules. These are reconstructed diagnostic binaries, not the final review-fix correctness build.

All four completed matrices retain ten alternating process samples per variant, original canonical workload arguments/path counts, validators and two warmups. They run sequentially with DAL_NUM_THREADS=4 and no competing builds/tests. Each contains the original 16 script MC cases plus the two calibration/quote failures, and the five unchanged tape cases. Numeric comparison covers PV and every risk key/value plus calibration rates/Jacobians/inverses and quote buckets. All 151050 numeric comparisons pass at relative 1e-10 / absolute 1e-9. These diagnostic subsets do not replace the two-round complete gates.

| Candidate             | Actual intervention                                                        | Outcome versus its same-matrix baseline                                                                                    |
| --------------------- | -------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| multi-outline         | Inline recording; outline multi-adjoint allocation across arities.         | Clear +0.19%, rewind -0.32%; compiled barrier AAD +43.49%. Rejected.                                                       |
| multi-outline-record3 | Keep RecordNode<3> noipa; outline multi-adjoint allocation across arities. | Clear +5.57%, rewind +4.67%; compiled barrier AAD +31.67%. Rejected.                                                       |
| multi3-outline        | Outline only the three-input multi-adjoint body.                           | Clear +2.88%, rewind +4.71%; compiled barrier AAD +12.17%, stock +7.93%. Rejected.                                         |
| multi3-noclone        | Retune noipa after reducing the three-input body.                          | Selected chain/allocator/model instructions identical to multi3-outline; not timed or shipped.                             |
| flatten-events        | Flatten the actual emitted compiled event dispatcher.                      | Removes arithmetic calls but expands 1653 to 74883 instructions; compiled barrier AAD +41.94%. Rejected.                   |
| flatten-bounded       | Flatten events while bounding recursive range calls.                       | Dispatcher 15168 instructions; compiled barrier AAD +7.28%, stock +8.41%; native rewind +7.73%. Insufficient, rejected.    |
| flatten-sum-call      | Outline and flatten only the sum dispatcher.                               | Arithmetic recording calls 26 to 21; compiled barrier AAD +9.78%, stock +8.41%; double compiled barrier +22.22%. Rejected. |

The multi3-outline static allocator drops from roughly 270 instructions to 141 and loses its stack canary; the Python allocator has 143 instructions. The selected model retains 10 whole-function TLS lookup sites, versus 14 when multi-adjoint work is outlined across all arities. Site counts include initialization/fallback branches and are not dynamic per-step counts. Removing only exception-related frame overhead improved native recording but did not resolve both clients.

The copied stock-control binaries in matrix-targeted have identical hashes to stock. All control samples are retained. Earlier rejected interventions, prepared opcode/hex-constant equality for 100 valuations and phase measurements remain inherited and were not repeated. No best result is selected across matrices and no universal cause is claimed.

### Calibration and quote-risk failures

182 relevant curve/public/Python Git blobs are unchanged between baseline and current product; checkout verification accounts only for repository CRLF normalization. Unchanged source does not prove identical generated code under changed shared headers or LTO. Build flags, module identities, measured inputs and financial outputs remain recorded.

Stock calibration deltas across the four matrices are +1.58%, +8.62%, -2.66%, -3.49%; quote deltas are +0.63%, +1.78%, -2.00%, +9.69%. Same-binary stock/control measurements also vary. This establishes unresolved attribution and motivates narrower binary investigation; it does not dismiss the original two-round failures as noise or authorize curve changes.

## Concrete scope request to parent

Authorize a bounded experiment at the existing private `BlockList_::NextBlock` in `dal-cpp/dal/math/aad/blocklist.hpp`, alongside the already allowed tape.hpp allocation boundary. Current permitted tape-only outlining either leaves the unconditional three-input call cost in native clients or restores inline recording while increasing Python TLS/dispatch cost. Selected RecordNode<3> instructions still include the inlined node/derivative/pointer block-growth paths. Those paths are the next specific boundary to isolate.

Proposed intervention: apply guarded existing compiler spellings to prevent inlining of that private block transition, keeping its existing single growth/reuse algorithm unchanged, then test whether the smaller recording body permits removing or retuning the unconditional three-input boundary. Preserve all cursor updates, reserve order, node counts, pointer wiring, errors, N=0 behavior, signatures and layout. No new public helper/macro, runtime reservation, TLS cache, cold/noexcept annotation, global flag or other AAD/model/backend file is proposed. This is a hypothesis, not an established performance fix, and it has not been implemented.

Acceptance for that experiment: verify the intended actual-module and static-chain instruction changes first; rerun existing independent stream rollover, alias, mark/reuse and multi-adjoint checks; measure both native recording cases and all canonical Python controls sequentially. Only a supported candidate that improves the combined tradeoff proceeds to fresh full native/backend/sanitizer/static/shared/LTO/Python checks and both original complete performance gates. Parent approval of this exact additional file boundary is the next dependency; the current authorization explicitly confines native AAD changes to tape.hpp/tape.cpp.

## Inherited complete gates and delivery provenance

The original gates on 0a7455 retain all 63 C++ and 90 Python cases, two rounds, ten alternating samples per side per round, minimum reduction and strict 4%. The parent independently recomputed all results from raw samples. Failures remain:

| Gate/case                                   | Round 1 | Round 2 |
| ------------------------------------------- | ------- | ------- |
| C++ Clear + re-record, 100K nodes           | +5.33%  | +4.91%  |
| C++ Rewind + re-record, 100K nodes          | +7.60%  | +7.71%  |
| Python mc.barrier.aad.compiled              | +14.58% | +9.63%  |
| Python calibration.MIXED.BUMPED.diagnostics | +4.47%  | +5.60%  |
| Python quotes.single.n5.ANALYTIC.t120       | +5.07%  | +4.88%  |

The inherited archive SHA256 is `8900540f6d99dd383d67c71b10b355c11d9a595b5d142b64c295e554c455b92c`, attached in five parts to DAL-228 thread `01a0a221-2564-7e6b-a5af-f5531de64066`. Parent verified its 3637 entries and 1150 source hashes. It retains earlier gates, failed zero-case off/OFF setup, all old samples/controls and nested lineage. This delivery references that immutable archive without repacking it or presenting its tests as fresh.

The new attachment includes this matching report, fresh verification logs, scripts, raw matrix samples, overlays, selected disassembly/maps, source and binary/build identities, publication record and an internal SHA256 manifest. Generated binaries/objects and full-module disassembly are excluded with hashes and reconstruction commands retained. Publication records one CI snapshot; pending or failed checks do not imply acceptance. No merge or closing intent is included.
