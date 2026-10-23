# DAL-421: XCCY observation planning and same-source path execution

## Contract and implementation boundary

Implementation base is merged master
`a5201cc95440a26a852dfa9db10cfc112d958060` (DAL-420 / PR627), including
DAL-409 / PR626 and DAL-410 / PR625. The controlling decision is
`.codex/artifacts/api-notes/dal-410-xccy-mc-model-decision.md`, sections 4, 5,
7 and the M2 portion of section 8, SHA-256
`c2ff68e1a696b620682b896ca9b292ec6d577bf92cadb3597991b427267733d5`.
DAL-398, the accepted DAL-410 critic/reviewer decisions cited in the M1 record,
and `docs/methodology/script_calibration_contract.md` retain authority over
source, immutable binding, observations and owning errors.

`PreparedXccyScript_` first calls the existing complete bound-AST XCCY
admission. It owns the original bound product and the immutable admitted
descriptor. The descriptor is derived from source; there is no retained native
swap, alternate payment script, second matcher, schedule reconstruction at
evaluation, or global observation lookup after preparation.

The dedicated composite plan owns sample definitions, sorted dates, source
uses, observation resolution and compiled statement streams. Its inner
`ObservationPlan_` is the existing request/known-value reader. The same reader
and model-output slots serve the existing tree evaluator and compiled opcode
evaluator. The composite owns the sample layout because the generic planner's
history classification and payment/default-SPOT binding are unsuitable here.
No shared preparation/factory hook or shared schema change was necessary.
DAL-422's moment lowerer, adapter and record remain untouched.

New core files are `dal-cpp/dal/script/xccyobservations.hpp` and
`dal-cpp/dal/script/xccyobservations.cpp`, with dedicated tests at
`dal-cpp/tests/script/test_xccy_observations.cpp`. Machinist generates the
`XccyObservationKind` header, implementation and Java enum in `dal-cpp/dal/auto`.
The only documentation change is this record. Examples, CMake, public facades,
solver, archive schema, legacy defaults and RNG are unchanged.

## Execution semantics

- Requests deduplicate by economic kind, canonical identity and exact time.
  `PaymentFxSpot` and `ResetFxFixing` retain separate IDs even at the same pair
  and timestamp. Canonical/original identity, row, statement and node uses are
  owned by the resolution plan. Rate metadata retains currency, explicit 3M/6M
  route, source start/end and accrual.
- The existing source-derived unsettled closure captures omitted strict-past
  history once. An explicit empty snapshot remains authoritative. At-V supplied
  FIX values are passive; absent at-V values use active initial rates/spot.
  Future supplied entries do not resolve future FIX. Each active fixing reads
  its own model state once and remains available at later payments.
- Payment conversion always uses model spot at its actual payment date.
  Discounting uses the domestic collateral numeraire once. Coupon notional,
  spread, initial/final exchange switches and signed MTM expressions execute
  directly from the source. In particular MTM remains new-minus-old.
- The grid contains V, outstanding payment dates, active fixing dates and M1's
  intervening dynamics boundaries. Rate bond maturities enter coverage checks.
  A permitted leading literal-zero receiver initialization uses the zero seed
  and adds no sample date or innovation interval. Settled PAYS have no execution
  or observation demands.
- Complete source admission precedes partitioning and compilation. Intraday V,
  generic today overrides, wrong route/pair/collateral/conventions, unsupported
  source shapes and missing exact history fail through owned diagnostics.
  Geometry-incompatible candidates fail before AAD model/tape allocation.
  At-V errors report the actual timestamp and midnight constraint.
- Candidate numeric models require M1 geometry compatibility. A typed path owns
  its scenario, candidate handle, source-plan identity and numeric evaluation
  identity. Other source/binding/date plans cannot consume it. Provider and
  execution failures retain stage, instrument/measure, original diagnostic and
  validated candidate where available; reports remain owned after unwind.

This is pathwise execution for the admitted domain, including double and AAD
receivers. It does not enable generic ScriptMC, MC statistics, expected A/B or
par estimation, selected-partial statistical acceptance, quote-risk, stochastic
staged fitting, language facades or V1 acceptance. Those capabilities keep their
existing refusal boundaries pending their own implementation.

## RED / GREEN controls

The attached evidence preserves these runtime REDs and their GREEN successors:

1. `TestDedicatedPlanIncludesFixingDatesBeforePaymentEvents`: the initial
   dedicated planner stub raised `MCCrossCurrencyUnsupported`; source-derived
   dates then included the fixing and delayed payment dates.
2. `TestIndependentPathwiseCashflowsForBothDirectionsAndSpreadLegs`: the path
   provider stub raised `MCCrossCurrencyUnsupported`; both evaluators then
   matched the independent per-cashflow oracle in all 12 combinations.
3. Candidate rejection initially allocated 18 AAD nodes before failure, against
   an expected zero. Candidate checking now precedes AAD model construction.
   A numerical provider failure initially had a null candidate; the owned
   Evaluate report now retains it and its instrument/measure/time context.
4. A different convention token was initially accepted at preparation and
   compatibility checking. Both operations now reject it.
5. Missing live projection initially lost the instrument ID in the propagated
   M1 diagnostic. The new boundary now owns the complete Prepare report. The
   same test checks the exact intraday timestamp and required constraint.
6. A leading receiver initialization initially added an extra grid date and
   five innovation slots. Its regression now requires the original grid,
   dimension and bitwise-equal tree/compiled prices.

The first path attempt also exposed an empty evaluator seed when constructing
directly from an indexed raw product; the admitted live-only executor now
creates its explicit zero receiver seed. A started-MTM test initially assumed
domestic spread despite the fixture's foreign-spread default; the fixture now
explicitly selects domestic spread before asserting its +15 USD signed delta.
No financial rule or tolerance was changed to accommodate either failure.

The independent path oracle has explicit dates, coupons, principal switches,
projection/bond algebra, FX and discount formulas. It does not call the exporter,
matcher, descriptor, provider or model to calculate expected amounts. Controlled
nonconstant endpoint/integral states make using payment-date rates or FX fail.
The reversed 6M-domestic/3M-foreign case adds a lagged FX reset and actual payment
lags. At-V supplied reset 1.6 versus payment spot 1.1 protects the kind split;
future snapshot FX=9 is ignored. Started MTM requires the previous FX notional
but not settled-period rate history. One-leg-settled and all-expired Price work
with no projection routes; their historical closure is checked independently.
No par capability is inferred from these Price cases.

## Reproduction and verification

The evidence attachment contains commands, raw RED/GREEN output, build/config
logs and `provenance.json`. The focused runner is an explicit Google Test
executable linked to the completed CMake `dal_cpp` target and `gtest`; it is not
a partially built or empty `dal_cpp_tests` target.

```bash
cmake --preset=Release-linux -S . -B build/m2 -DCMAKE_CXX_COMPILER=g++-15 -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF
cmake --build build/m2 --target dal_cpp -j 8
cmake --build build/m2 --target gtest -j 4
g++-15 -O3 -DNDEBUG -std=c++17 -pthread -DDAL_USE_EIGEN -DEIGEN_DONT_PARALLELIZE -Idal-cpp -Idal-cpp/externals/eigen -Idal-cpp/externals/rapidjson/include -Idal-cpp/externals/googletest/googletest/include dal-cpp/tests/test_main.cpp dal-cpp/tests/script/test_xccy_observations.cpp build/m2/dal-cpp/libdal_cpp.a build/m2/lib/libgtest.a -o build/m2-observations
build/m2-observations --gtest_filter='XccyObservationTest.*'
cmake --build build/m2 --target dal_check_generated -j 8
```

Final acceptance is the focused new selection plus the legacy compatibility
selection logged in the attachment. Sanitizer evidence instruments the new
planner/provider translation unit and test caller with ASan/UBSan, linking the
Release core dependencies; it is not a fully instrumented repository build.
Both value and AAD tree/compiled paths are exercised. AAD/CRN spot FD is a
focused pathwise chain check, not M4's complete partial/SE qualification.

Final results: **20/20 new Release tests**, **42/42 compatibility tests**, and
**20/20 repeated new tests under the partial ASan/UBSan instrumentation** pass.
The latter is a configuration repeat, not 20 additional independent tests.
Compatibility covers 38 existing XCCY tests, two scalar preparation cases and
two GSR/Deposit MC context/execution cases. Relinking the compatibility caller
against the final archive retained its exact SHA-256, recorded by the before/
after files; its accepted run remains applicable. The completed core build,
generated-drift, format, CCN and final scoped warning checks all pass. No full
repository test target, native Windows binary or public install matrix is
reported as locally executed.

GCC 14.3/15.2 warning checks use `-Wall -Wextra -Werror`, with dependency include
directories marked `-isystem`, leaving diagnostics on new source/test bodies
enabled. Earlier blanket `-I` attempts exposed existing unused-parameter,
signed-comparison, debugger aggregate, domain-parentheses/member-order and
compiled-control sequence-point warnings. A new GCC dangling-reference hint was
resolved with a named owning currency string; omitted optional test fields were
made explicit. No legacy source or warning settings were changed. Clang 21's
implicit-float-conversion check also covers the new source and tests.
Format, CCN <=8 and generated drift are checked separately.

## Evidence identity and applicability

`provenance.json` records hashes for the new source/generated files, test,
unchanged model/matcher/evaluator/history dependencies, exact gitlinks, selected
CMake configuration and actual executables/archive. `delivery-head.json`
records the published commit and one CI snapshot. The code inputs are hashed
after final verification; the implementation record is versioned in the PR.

M1 mathematics and original independent kernel evidence remain applicable to
the unchanged model source and dependency graph. This is not a claim that its
old test binaries equal this new archive, or that its full numerical matrix was
executed here. The new caller uses the model's existing five-slot transition
directly. Old P0/P3 statistical, performance and numerical matrices were not
rerun or relabeled as new passes. The local surface is Linux core; native
Windows/MSVC CI and XLL/workbook execution cannot be inferred from it.

## Caller cost and remaining work

The informational caller times only this new API on an MTM 3M/6M lagged-payment
fixture, D=30. Five rounds use 50 preparations, 2,000 generated paths, and
20,000 evaluations per backend; tree/compiled order alternates. The driver
prints a checksum. Preparation, model/path generation and execution are
reported separately. There is no pre-existing baseline or calibrated regression
gate for this new caller; these observations make no speedup claim. Unchanged
solver/RNG/tape benchmark gates and full parameter matrices are outside this
diff. Examples and build configuration were not edited, so the example/install
closure matrix is not an applicable new-change requirement.

On the local i9-13900HX host, the final five-round medians are approximately
727 microseconds per preparation, 87.9 microseconds per generated path,
1.01 microseconds per tree evaluation and 0.874 microseconds per compiled
evaluation. Raw samples and the final caller hash are attached. These values
are informational and do not establish MC statistical efficiency.

M2 is delivered for focused review. M3/M4/M5 and the independent V1 numerical
acceptance remain separate dependencies of the complete XCCY calibration-to-MC
promise. DAL-396 remains in progress. No experts or downstream agents are
dispatched here, and this PR is not self-merged.
