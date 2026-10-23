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
