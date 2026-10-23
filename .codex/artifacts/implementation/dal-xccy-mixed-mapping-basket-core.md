# DAL-427: prepared mixed-mapping XCCY basket core

## Authority and delivery boundary

Implemented the approved disjoint scalar basket on master baseline
`197e33df818a14f5f8fae2f0f92ec52453c7463d`, in this issue's separate checkout
and branch `agent/dal-implementer/b3c90240203b`. The controlling DAL-426 note
has SHA256 `3c5ae6eed21ed0582ef0ba70811f4692efbcead2b69a184bbe5e0b5081a140ad`;
DAL-410 remains `c2ff68e1a696b620682b896ca9b292ec6d577bf92cadb3597991b427267733d5`.
Read DAL-398, the methodology, role/style/boundary references, parent threads,
R1 review and coordinate/statistics critique before implementation. M5 had no
active/queued author at startup. No unmerged M5 or acceptance-helper code was
imported. R2 fixture constants were inspected through its authenticated attachment.

This delivers the core, with a separate base-master PR identified by DAL-427.
The delivery comment supplies its exact final head and one CI snapshot. It
does not merge, dispatch another role, extend public bindings, or accept V1.

## Changed files and design

- New `dal-cpp/dal/script/xccymcbasket.hpp` defines the owning prepared plan,
  allocation, execution/member records, complete result, independent partial
  payload and `XccyMcBasketError_ : CalibrationError_`. Inspection is const;
  complete results cannot be constructed from caller ledgers.
- `dal-cpp/dal/script/xccymcbasket.cpp` prepares every source and both coupon
  bindings, preflights every candidate member before drawing, executes atomic
  coefficient batches and freezes success or failure ownership.
- `dal-cpp/dal/script/xccymcbasketranges.cpp` derives all full blocks/phase
  ranges, fingerprints allocation and execution separately, checks immutable
  ledgers and provides the separate independent/declared-CRN checker.
- `dal-cpp/dal/script/xccymcbasketscalars.cpp` checks ordinal/storage arithmetic
  and implements dimension-one MRG32 Precise absolute scalar seek/fill. D=0
  constructs no RNG. Matrix/native-map/vector/retention capacity checks include
  non-scalar storage for all-deterministic baskets.
- `dal-cpp/dal/script/xccymcbasketstatistics.cpp` joins exactly B complete rows
  in member order, publishes every cross-member C_batch block and C_mean,
  and projects the full Price/par/native-gradient Jacobian. No inversion,
  forced-zero cross block, member-count normalization or portfolio payoff is added.
- `dal-cpp/dal/script/xccymcbasket_internal.hpp` contains private producers,
  checked helpers and the test-only traversal/fault access seam. No product
  setting, public callback or global fault state is exposed.
- Narrow extraction in `dal-cpp/dal/script/xccymc.cpp` and
  `dal-cpp/dal/script/xccymc_internal.hpp` shares frozen member preparation,
  native coefficient execution, derivative admission and batch completion.
  Single dimension, seeks, seed/stream, arithmetic order and `/1` outputs remain.
- Six new `dal-cpp/tests/script/test_xccymcbasket*.cpp` files and private
  `xccymcbasketfixture.hpp` / `xccymcbasketrngoracle.hpp` provide 28 focused tests.
  This record is the only documentation change.

Allocation uses T=2359296 and O_next=O+T*D, independent of execution counts,
phase, numeric candidate, AAD and compiled selection. IDs are byte-exact and
input order is authoritative. Canonical blocks remain `[0,70778880)`,
`[70778880,200540160)`, `[200540160,389283840)`; the middle start has remainder
40 modulo 55. Its absolute scalar coordinate is never divided by D.

On failure, planned reservations, generated spans, consumed scalars, completed
paths and committed rows remain separate. A failed path's generated vector is
counted, its unfinished batch is absent, and later members retain NotStarted.
Only Complete members have summaries. An annuity-summary failure retains
CoefficientsComplete and raw rows. Every failed basket lacks a joint estimate,
covariance, SE and acceptance payload. Partial.Report and Failure.report share
the same owner. Candidate ownership starts only after all candidate preflight
checks pass; rejected refreshes never substitute the prepared model.

No shared protocol, old range guard, solver, RNG, model, admission, archive,
defaults, public/Python/Excel file, example, CMake file or tester evidence changed.
Existing CONFIGURE_DEPENDS globs discover the new core files and tests. There
was no build/example wiring change requiring a new standalone/install closure.
No financial/API design deviation was necessary. Raw parameter units follow
the existing frozen single convention; the legacy statistics file stays untouched.

## RED, GREEN and retained controls

1. Initial focused adapter syntax build exited 1 because the new adapter header
   did not exist. After the minimal adapter, its linked focused test passed.
   The singleton test likewise first failed on the missing basket surface.
2. The first basket run exposed missing owning instruments in report rows:
   `/report/observations`, 13 failing tests. Owning original instruments,
   completed observations and source plans repaired that actual product error.
3. `XccyMcBasketFailureTest.TestStaleBlockStartAndEqualDimensionDateAreRejected`
   exited 1: incrementing a member block start was accepted. Fingerprinting the
   actual allocation content closed that gap; the test now passes, alongside
   equal-D date, stale dimension and batch-range mutants.
4. Strengthening singleton parity to compare Price batch values exited 1:
   basket batch value 0 versus legacy `-9.8269259455799709`. Shared FinishBatch
   restores full batch Price/native derivatives; None and AAD exact comparisons pass.
5. An actual temporary cross-block-zero mutant was built and run. Independent
   reconstruction rejected 0 versus `99.565173906012788`; exit 1. Original
   production bytes were restored and compared before the final GREEN build.
   The package retains mutant source, binary hashes, build log and XML.

Intermediate fixture corrections did not change product gates: nonflat dynamics
add bucket dates, giving D40/65/90 for the partial-failure fixture; a changed
mean reversion is a legal numeric refresh, so the rejection test changes a
bucket date instead. APPROXIMATE uses R2's existing lambda=1e8 and eta=.5 and
the existing AAD solver option. SPOT source uses correctly have no explicit
FIX date; their event-time key and legacySpot marker are checked separately.
No assertion, budget, tolerance or admission was relaxed to obtain GREEN.

Final commands from the repository root:

```bash
cmake --preset=Release-linux -S . -B build/basket-make \
  -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF \
  -DDAL_CPP_BUILD_EXAMPLES=OFF -DCMAKE_CXX_COMPILER=g++-14
cmake --build build/basket-make --target dal_cpp_tests -j 12
build/basket-make/dal-cpp/dal_cpp_tests \
  --gtest_filter='XccyMcBasket*:XccyMcTest.*:XccyMcBoundaryTest.*:XccyObservationTest.*:XccyGaussianStagedTest.*:XccyGaussianExecutionTest.*:XccyGaussianDataTest.*:PseudoRandomTest.*'
ctest --test-dir build/basket-make -R '^XccyMcBasket' --output-on-failure -j 4
```

Results: build exit 0; **109/109** targeted tests pass, including **28 new**
basket tests; **28/28** discovered basket CTests pass. No full repository,
long MC, benchmark or unchanged mathematical matrix rerun is claimed.

Additional verification:

- Independent chronological MRG32 modular recurrence/power reconstruction
  matches **23,760 new-adapter normals**, at all 432 canonical phase/refinement/
  batch starts. Serial NextUniform/Precise, non-aligned seek, missing/extra
  vector, cursor, D0, nine reservations, x4 capacity and exact mapping hashes pass.
- Actual retained paths agree across regrouped prefixes by member/path/scalar
  coordinate. Reverse traversal, all phase replays, overlap/CRN rejection,
  reordered/duplicate economic rows and byte-distinct IDs pass.
- All native axes, measured cross blocks, per-member diagonal blocks, Price/par
  and full gradient SE/Jacobian reconstruction pass, including singular full
  covariance with two batches. Zero-dynamics A/B/Price and par use the existing
  1e-10/1e-12 deterministic tolerances. Selected CRN h/h2/h4 checks are small
  technical smoke, not independent binary64 FD qualification.
- Both actual fitted EXACT and APPROXIMATE models execute the original three
  maturities with selected raw basis partials. Original 2028-11-06 admission
  refusal remains; inherited recovery still prints third-node error
  `-0.000149141888694` logDF. No fixture curve or threshold was changed.
- Late-member, late-batch and actual high-floor annuity-summary failures retain
  owned rows/report/model after all caller/error handles die. The failing vector
  is counted without a padded batch. Last-member preflight and rejected candidate
  have zero draws/null candidate. AAD cleanup and active caller-graph preservation pass.
- A separately compiled original baseline `xccymc.cpp` and current library run
  the same 24 small single cases: three phases, two measures, tree/compiled and
  None/AAD. Identities, first normals, batches, coefficients, gradients and SEs
  are byte-identical. Shared output SHA256:
  `e3cec84ccb084b5728179cf5b1c4562335cbaa0d509df4e425d49b499364479b`.
- GCC14 and GCC15 production syntax checks pass with `-Wall -Wextra -Wpedantic
  -Werror`; clang-format, CCN<=8, documentation and diff checks pass.

## Provenance, cost and outstanding acceptance

The attached package has a 1,250-file source/reference/control manifest, clean
initialized dependency gitlinks, exact CMake cache/compiler/link configurations,
caller sources, original single source, XML/logs and executable hashes. Manifest
SHA256 is `865074ee38e1da719a1cd0c51fa9acf2f632f7c051169f20a6dc6c118a3690eb`.
Final core archive SHA256:
`1e40eacec3980ab7a1803954a837a4bf0a3bfe94f38add52ab987f199b21167c`;
final test executable SHA256:
`fc56b52391dadeeba75f5cb26989e740a110b5ac9992f413f396ae4274eb595d`.
The original and current single executables are distinct and separately hashed.
The archive before the controlled mutant and after restoration is byte-identical.

The minimal core-only HeldOut 2x3 AAD caller completes D30/55/80, 990 scalars,
full rows, first normals and selected basis partials. Informational preparation
and evaluation were about 20.5/22.7 ms in one run. With no isolated paired
baseline, performance is **inconclusive**; no speedup or benchmark gate is claimed.

Every core row in DAL-426's executable handoff has focused execution above or
direct unchanged-single/model evidence in the 109-test selection. Synthetic
32-bit arithmetic limits are tested; this Linux 64-bit run is not native 32-bit,
Windows, XLL, Python, workbook or sanitizer evidence. Frozen 16x16384/x2/x4
independent successful-basket captures and full independent numerical/oracle
qualification remain with the original tester after actual core merge. Public
partial/result projections remain a separately scheduled task after core/M5 acceptance.

All prior/new Price/par precision/combined, FX-sigma precision/combined, five
binary64 fixed-path FD REDs and full binary64 roundoff/eFD qualification gaps
remain open. No new paths, VR, scales, estimator or thresholds are authorized.
Old HeldOut is already observed; this new layout does not reset that identity,
and its first member reuses old coordinates. This is an implementation Estimate
checkpoint awaiting independent review, not complete V1 or parent acceptance.

## R1 revision: preserve source locations

Revised the same PR #637 and original author checkout from fixed
`d2c8e25fd7910fbb37ab933d12c9456c89abbada`. Remote master was still
`197e33df818a14f5f8fae2f0f92ec52453c7463d`; no integration or financial
choice was required. Read the updated DAL-427 scope, bounded DAL-425/396
threads, active/queued family, reviewer report and original probe. The tester's
fixed-head 28/28 plus two probes and critic's limited scalar/statistics/oracle
Looks fine are received evidence, separate from this revision's execution.

The production change removes only `Own()`'s assignment of a zero-based member
index to `diagnostic.sourceRow_`. The copied cause retains its original optional
source row, statement, event date, code, field and other cause fields. Existing
member IDs, instrument/measure IDs and message index still identify the member.
There is no generic protocol/schema or model change and no design deviation.

The six revision paths are the runner, `test_xccymcbasket_failures.cpp`, this
record, and include-only edits in `test_xccymcbasket_execution.cpp`,
`test_xccymcbasket_trace.cpp` and `xccymcbasketfixture.hpp`. Standard/DAL/local
includes now follow the repository order. The private fault seam is unchanged.

Two new tests cover members 0, 1 and 2, each with source row 23 and with absent
source row. Through the existing private Preflight hook, each injects a cause
with statement 4, event date 2028-05-04 and explicit diagnostic fields. After
the plan and caught error leave scope, the retained failure, report error and
partial member failure must retain the location and cause. They also assert
member identity, shared report owner, zero draws/consumed scalars, null
unvalidated candidate and restored caller tape size. These are six zero-draw
error cases, not new Monte Carlo acceptance samples.

### Revision execution and provenance

The evidence package contains exact argument arrays, working directories,
timestamps and exit codes in `commands.jsonl`, plus logs and XML. With
`revisionEvidence` set to that directory, the focused test commands are:

```bash
cmake --build build/basket-make --target dal_cpp_tests -j 8
build/basket-make/dal-cpp/dal_cpp_tests \
  --gtest_filter='XccyMcBasketFailureTest.TestPreflightPreservesOriginalSourceLocation:XccyMcBasketFailureTest.TestPreflightDoesNotInventSourceRow'
build/basket-make/dal-cpp/dal_cpp_tests \
  --gtest_filter='XccyMcBasketFailureTest.*'
ctest --test-dir build/basket-make \
  -R '^XccyMcBasketFailureTest.TestPreflight(PreservesOriginalSourceLocation|DoesNotInventSourceRow)$' \
  --output-on-failure
python3 "$revisionEvidence/check_compilers.py" g++-14
python3 "$revisionEvidence/check_compilers.py" g++-15
python3 .github/scripts/check_docs.py
```

- RED build exited 0; the two new tests exited 1 for the intended source-row
  defects: row 23 became 1, and absent became 0. The exact failing runner and
  test source are retained separately. No test assertion was weakened.
- Removing the overwrite made both tests pass, exit 0. After the include-only
  refactor, the public-disabled core/test target rebuilt successfully; the full
  affected failure suite passed **11/11**, exit 0. Discovery/execution of the
  two new CTests passed **2/2**, exit 0.
- The original reviewer probe source is retained unchanged. A strict contract
  variant changes its success predicate to row 23 in error/report/partial:
  linked against the retained original archive it prints row 1 and exits 1;
  linked against the final archive it prints row 23 and exits 0. Both report
  statement 4, member 1, draws 0 and consumed 0. Exact compiler/link commands
  are recorded; original and revised binaries have separate hashes.
- GCC14/15 each passed five affected runner/test translation units with
  `-Wall -Wextra -Wpedantic -Werror -fsyntax-only`. Clang-format and diff checks
  pass; Lizard's 49 inspected functions have maximum CCN 7, with the new helper
  at 6. Documentation checks pass for 112 Markdown files.

Before editing, all 1,250 source/control hashes matched the original manifest.
The revision package supplies a new source manifest and explicit six-path delta.
The four CMake cache/compiler/link configurations and initialized dependency
gitlinks/clean worktrees remain byte-identical; public remains disabled. The
original archive `1e40eace...` and runner `fc56b523...` were copied and verified
before any incremental build. They and the original evidence ZIP remain intact.
The final rebuilt archive SHA256 is
`0d3c42fcd3a4edc79fab87bc4573398011d7745e36d90c760249618b63b6812a`;
the final runner is
`8ae878339815a76b5d5a896225ffb851dc003ddd978ec6af4b68a03c8536463d`.
The delivery comment and package identify the enclosing commit and its one CI
snapshot; the original binaries are not presented as execution of that commit.

The unchanged scalar/range/statistics/model/single source and prior numerical
evidence retain their original applicability. This revision does not rerun or
claim new 109/28-suite, 23,760-normal, 24-case single, long-MC or platform results.
No build/example wiring changed, so no new standalone/install closure is needed.
Zero-draw diagnostic copying does not warrant an algorithm benchmark rerun;
performance remains inconclusive. Shared docs/CHANGELOG and M5 remain with
their scheduled owners. All frozen precision/combined, FX-sigma and binary64
FD/eFD gaps, budgets, thresholds and observed HeldOut identity above remain.
The revision is ready for limited review, without merging or starting downstream.

## R2: integrate accepted M5 master

The normal merge `4d2d83ce46c3dcc945ba2e762b4e2605b5be7415` has first
parent `1302c66d2f74754cb9dd5e7f1c6c5e51ca0fcd98` and second parent
`ccec714f63befee9202fd5b6b324999068e88c1d`. It stays on the original
`agent/dal-implementer/b3c90240203b` branch and PR637, base master, without
rebase, force push or sibling imports. Fetching master found exactly one new
commit since DAL-426: accepted PR632's M5 squash merge, whose parent is
`197e33df818a14f5f8fae2f0f92ec52453c7463d`. GitHub and the direct DAL-424
PR link both confirm its actual merge at 2026-10-23T22:11:10Z. The integration
has no conflict. This appended provenance entry is the only author change
beyond that accepted master; the final delivery identifies its enclosing head.

The complete tree audit covers all 3,141 prior entries and all 3,199 resulting
entries. All 16 reviewed core production/test files remain byte-identical to
the first parent. The 69 accepted upstream paths have no intersection with
the 17 reviewed core paths and equal accepted master exactly, including M5's
generated wrappers, examples, build changes and documentation. No new basket
facade or binding is added. The controlling DAL-426 and DAL-410 hashes above,
DAL-398, core dependencies and five dependency pointers remain unchanged.

Before merging, all 1,251 R1 source/control hashes matched their recorded
manifest. Both prior evidence manifests verify: 77 original and 62 R1 entries.
The before/after inventories preserve 156 evidence/attachment files and 599
actual object, archive, caller, configuration and ZIP files. The four live core
build configurations match the R1 captures. Retained original, failing and R1
archives/runners/probes keep separate identities; no build output is refreshed
or relabeled as execution of the integration head. The audit package supplies
full manifests, tree identities, exact commands/exit codes and complete patches.

New verification uses `python integration_audit.py prepare` before the merge
and `python integration_audit.py audit` after publication preparation, plus
`git diff --check` for both the integration and full PR and
`python3 .github/scripts/check_docs.py`. The audit verifies normal ancestry,
every tracked working-file blob, the record's unchanged original prefix,
accepted-master equality and retained evidence/dependency/product hashes.
No behavioral RED/GREEN is applicable to this conflict-free integration and
record-only change. The original RED/GREEN evidence above remains intact.

Core source, build inputs and dependencies do not change, so the accepted
core tests, scalar/statistics/oracle checks and R1 diagnostic review remain
applicable within their recorded identities. No 109/28 suite, 23,760 normals,
24-case single comparison, long MC or numerical/platform matrix is rerun.
Accepted M5's recorded public-disabled build/1 CTest, standalone shared-core
build/install, standalone public build/install/2 selected CTests and installed
consumer/1 CTest are reused within their original scope; these are not fresh
integration-head runs. No additional performance case is warranted by this
delta; the original performance conclusion remains inconclusive.

The final handoff supplies one actual new-head CI snapshot. Old-head green
does not clear this head; limited integration review and complete exact-head
CI remain coordinator gates. This author neither merges PR637 nor starts
DAL-425 basket acceptance, DAL-428 shared documentation or public wiring.
All precision/combined, FX-sigma and binary64 FD/eFD gaps, observed HeldOut
identity, budgets, thresholds, paths and VR restrictions above remain.
DAL-425 and parent DAL-396 remain in progress; this grants no numerical or
full V1 acceptance and introduces no design deviation or financial question.
