# DAL-420: frozen XCCY Gaussian model checkpoint

## Controlling contract and design

Implementation base: `87d8954eefd2033a7829976a35d536ba439d4748` (merged
DAL-410 / PR625). The controlling API note is
`.codex/artifacts/api-notes/dal-410-xccy-mc-model-decision.md`, particularly
sections 2, 3, 7 and the M1 controls in section 8. DAL-398 and
`docs/methodology/script_calibration_contract.md` retain their instrument,
observation and owning-error conventions. The accepted focused critic and
reviewer results are parent comments `01a166d3-2b9e-7a06-a91f-7ab92cac9df3`
and `01a166d5-79ec-74a0-9b11-89a8a8dff77e`.

The new model owns normalized, dated log-linear log-discount snapshots, explicit
projection routes and deterministic basis. Market/dynamics/model data have const
contents. Construction never retains solver pointers or reads global dates or
fixings. Curves are checked for exact representation rather than sampled into
an approximate interpolation. Numeric refresh creates new data with identical
geometry; kernels and conditional bonds are recomputed from that content.

The state is `(xd,xf,Id,If,Z)`. Constant-bucket exponential kernels give exact
means and joint covariance, composed across explicit bucket boundaries. Three
economic Brownian factors have five fixed interval-major innovation slots.
Scaled covariance factorization preserves small positive rate variances and
cross terms. It introduces neither a volatility cutoff nor jitter. The foreign
drift is negative quanto under Qd; effective foreign discount is Pf/B.

The dedicated model interface exposes states, transition moments, bank accounts,
spot, conditional bonds and projection bonds. Generic source observations and
MC mappings belong to M2/M4 and remain unsupported. This checkpoint does not
qualify same-source cashflows, staged stochastic calibration, MC statistics,
cross-language requests or quote risk.

## Interface and write boundary

`xccygaussiandata.hpp/.cpp` adds four immutable, owning archive types: curve
snapshot, market, dynamics and model data. The required market spec has an
explicit optional pair marker so omission cannot inherit the legacy USD/EUR
default. V, collateral, spot, OIS routes, basis and named dynamics are validated.
Missing projection routes never fall back to discount curves. Unused projection
routes may be absent. Numeric content contributes to the model identity.

`xccygaussiansnapshot.cpp` accepts LOG_LINEAR DiscountLogDF with an actual-days
linear axis (ACT365F or ACT360), DiscountPWC and exactly constant segments of
DiscountPWLF. Compound bases retain the union of their knots, explicit common
coverage, native labels/values and the complete normalization Jacobian. It
rejects extrapolation, cubic interpolation, sloping forwards and a nonzero
pinned native logDF anchor, including values tolerated by the legacy curve
constructor. The latter refusal avoids silently losing the fixed anchor value.
Native curves and legacy admission remain unchanged.

`xccygaussianmath.hpp` contains stable exponential-integral kernels and exact
Gaussian transition composition. Covariance is assembled once per symmetric
entry. Scaled fixed-order Cholesky retains small positive variances and cross
terms. An uncertain normalized cancellation fails numerically; it never becomes
a clipped direction. Exact kernel-zero rows stay zero. Underflow or overflow
that prevents a representable transition fails with an owning diagnostic.

`xccygaussian.hpp/.cpp` exposes typed states, `Prepare`, `Transition`,
`GenerateStates`, conditional effective/own/projection bonds, realized forward
rates, spot, FX forwards and bank accounts. Supplied normals have exactly five
slots per advancing interval, including zero-volatility intervals. The grid
includes every interior dynamics bucket boundary. Failed preparation preserves
the last valid grid. Numeric mutation requires `RefreshNumeric`; preparation
rebuilds all coefficients. AAD callers register parameters and prepare/refresh
inside each recording. Zero constants and pinned anchors are materialized on
that recording. Snapshot logDF, spot and sigma labels are stable; a/R are fixed.
The native snapshot Jacobian is preserved for downstream qualification, rather
than being represented as calibrated quote risk.

**Intentional factory wiring deviation:** M1 provides
`NewXccyGaussianModel<T_>` in its own header. It does not edit the shared
`model/factory.hpp`; generic `Allocate`, `Init` and `GeneratePath` on the typed
model fail with `MCCrossCurrencyUnsupported`. This keeps the model checkpoint
usable without granting unimplemented observations or mappings. A generic
factory case belongs with the M2 integration under its coordinated shared-file
window. No shared factory/build/schema/legacy archive source was changed.

The other additions are 12 Machinist-generated files for these four new archive
types, two dedicated test sources, their independent kernel helper and this
record. Existing core examples, solver, RNG, hybrid guards, curve/public/binding
sources and shared documentation are untouched. No unmerged DAL-409 work is
included.

## RED, GREEN and refactor

The initial focused syntax commands used C++17 with `-I dal-cpp` and
`-I dal-cpp/externals/googletest/googletest/include`:

```sh
c++ -std=c++17 -fsyntax-only -I dal-cpp -I dal-cpp/externals/googletest/googletest/include dal-cpp/tests/model/test_xccy_gaussian.cpp
c++ -std=c++17 -fsyntax-only -I dal-cpp -I dal-cpp/externals/googletest/googletest/include dal-cpp/tests/model/test_xccy_gaussian_data.cpp
```

Both exited 1 with the expected absent new header, respectively the math and
data interface. The second command was repeated for the absent model interface.
After each minimum implementation, the corresponding focused executable passed.
The first numerical test agreed with independent 48/64-point wider-arithmetic
Brownian-kernel integration; no implementation covariance or Cholesky is its
oracle.

Subsequent focused RED cases were run after recompiling the changed test runner:

- `TestGridIncludesAllDynamicsBoundaries`, `TestPairMustBeExplicit`,
  `TestRejectInconsistentNativeCoordinateMap` and
  `TestUnderflowIsNumericalFailureRatherThanZeroVolatility`: four expected
  failures. Fixed missing bucket insertion, implicit pair, inconsistent native
  provenance and variance underflow; the small-positive-variance test stayed green.
- `TestNumericRefreshRequiredAfterParameterMutation`,
  `TestFailedPreparationPreservesThePreviousValidGrid` and
  `TestExponentialOverflowIsNumericalFailure`: three expected failures. Added
  stale-content rejection, transactional preparation and finite positive outputs.
- The interior AAD test initially exited 139. A focused ASan run identified
  unrecorded default Number zeros during reverse propagation. Explicit recorded
  zeros and fresh pinned anchors fixed it; ordinary and sigma_d=1e-8 AD/FD,
  lifetime diagnostics and the final sanitizer caller pass.
- `TestNumericalFailureRetainsStageAndEventDate` failed on Input versus Evaluate;
  numerical diagnostics now retain Evaluate, the event date and interval cause.
- `TestAnchorRiskAndChangedNativeMapCannotMasqueradeAsNumericRefresh` failed on
  anchor-risk admission. Anchors now have a zero Jacobian row; numeric refresh
  additionally checks the native Jacobian geometry.
- `TestUnequalBucketCompositionRetainsSymmetricFactorizableCovariance` failed
  on a roundoff-asymmetric composed covariance. Assembly now stores each
  mathematical covariance entry once and mirrors it. The lossless-admission
  test also failed for an unsupported nonzero native anchor and now rejects it.
- `TestNonfiniteComposedMomentsFailBeforeReturningAValue` failed because an
  overflowing composition returned infinities. Final means and covariance are
  checked before return.

These ran through `build/m1-focused --gtest_filter=<qualified test names>` or
`build/m1-model-focused --gtest_filter=<qualified test name>`. The focused runner
was rebuilt using the two new test sources, `tests/test_main.cpp`, the freshly
built core archive and Google Test. Refactoring extracted scalar kernels,
factor entries and finite-difference helpers while green. No numerical acceptance
threshold was relaxed. One invalid test fixture used the nonexistent LIBOR
collateral token; it was corrected to the real GC alternative. Diagnostic-path
assertions were corrected to the actual qualified USD:OIS route.

## Final scoped verification

GCC 15.2.0, CMake 4.2.3, C++17, Release, native AAD, portable x86_64 Linux/WSL2.
Workspace configuration:

```sh
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_BUILD_PUBLIC=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF
cmake --build build/Release-linux --target dal_cpp_tests -j 8
ctest --test-dir build/Release-linux -R 'XccyGaussian|XccyScriptTest.TestMonteCarloEntryRejectsCrossCurrencyBeforeModelExecution|XccyScriptIntegrationTest.TestGeneralMcAdmissionDoesNotEscapeIntoXccyContext' --output-on-failure -j 8
```

**30/30 passed:** 28 new cases and two existing generic XCCY MC refusal guards.
This is the full selected scope, not a full-repository or old statistical matrix
claim. The 28 cases cover joint means/C and split composition, zero/near-zero a,
small positive variance and normalized L*L^T reconstruction at 1e-12, zero rows,
nonflat/bucketed inputs, domestic/foreign units and density martingales under
Qd/Qf, conditional bonds, projection/basis routing, immutable lifetimes,
archives, explicit refresh, input refusal and interior kernel AD/FD.

Negative controls omit the foreign quanto drift, reverse Pf/B to Pf*B, or zero
the below-global-tau positive Cholesky column. Each exceeds its corresponding
independent assertion budget. Martingale checks use analytic Gaussian
expectations with independent kernel moments; these are not MC estimates.

Standalone core was configured from `dal-cpp` with `BUILD_SHARED_LIBS=OFF`,
`DAL_USE_EIGEN=OFF`, tests/examples/benchmarks OFF and
`DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS=ON`. After building `dal_cpp`, a separately
compiled test caller with the matching macro ran **28/28 successfully**. This
is a real caller execution, not a no-tests CTest result. Installing that build
and building an independent `find_package(dal-cpp)` consumer against `DAL::cpp`
passed its **1/1 CTest**, including actual model construction, preparation,
five-slot state generation and the initial foreign bond.

Focused ASan/UBSan caller: `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer`, linked to the ordinary uninstrumented core archive;
the interior AAD and native normalization/lifetime cases passed **2/2** with
`ASAN_OPTIONS=detect_leaks=0`. This instruments the new template caller, not the
entire core library and not a leak audit.

`clang-format --dry-run --Werror`, `lizard -C 8 -w`, staged `git diff --check`
and `dal_check_generated` pass. All authored functions have CCN <=8. An initial
blanket `-Wall -Wextra -Werror` syntax check hit pre-existing unused parameters
in `math/aad/expr.hpp`; after excluding that category, the generated archive
instantiation hit the pre-existing signed comparison in
`storage/archive.hpp::SetMultiple`. The scoped check passes with
`-Wno-unused-parameter -Wno-sign-compare`; no shared legacy source was changed
to suppress those warnings. Normal builds report no new warnings.

The first public-disabled configuration also needed the documented portable
Excel switch disabled; its initial missing-public-target configuration was a
failed attempt, not a passed build.

## Performance and evidence applicability

Changed callers are exclusively the new model/data/math and their tests. There
is no baseline implementation of this model and no changed established benchmark
caller. A small informational Release caller measured preparation separately
from 10,000 state-path evaluations, retaining a printed discounted-spot checksum:

- Flat, one interval, D=5: prepare 26.38 microseconds; path 0.481 microseconds.
- Nonflat/bucketed, 26 intervals, D=130: prepare 1067.35 microseconds;
  path 9.094 microseconds.

These observations establish executable coverage only, without a speedup or
formal regression verdict. No old P0/P3/GSR numerical, statistical or performance
matrix was rerun or relabeled as evidence for this model. Public/Python/Excel
and example closures are outside the unchanged caller scope. Native Windows,
XLL and workbook execution were not performed locally.

Frozen submodule identities: Eigen `3147391d946bb4b6c68edd901f2add6ac1f31f8c`,
Google Test `d72f9c8aea6817cdf1ca0ac10887f328de7f3da2`,
Machinist `e76b0ef243ca9bd506bb2f3743d8c9b52ee8d01d`,
RapidJSON `ea152ded01708ff1657b275ed0bcbf0681d64519`,
pybind11 `8a099e44b3d5f85b20f05828d919d2332a8de841` (unchanged, unused by M1).

Product/test/generated-source manifest SHA256:
`0b75b40468aa73687d18e20553ed14503cd8f3162f6af373eb19d5da44b20199`.
Its input is the sorted 21 source paths, each encoded as
`path + NUL + sha256(file bytes) + newline`; this Markdown is excluded.
The evidence archive retains every individual source hash, configuration and
actual executable hash. Final binaries:

- Workspace tests: `6355f2c856b0dc8ff0acda444ae00b2ce68719c4b8b2536f466888555ede330e`.
- Lifetime caller: `17fb42dabe0cafbae141ee43b165476dab251439feba2831611f7e96ef9e51d8`.
- Sanitizer caller: `5fd6c777dfbeeb94c301e91408ea4090ff563b93da2cd7a044cdc979690165eb`.
- Performance caller: `119ee3667da5838d6cffcc59245f5c4349fa3739faf5b4d8178d2689e2848b6a`.
- Installed consumer: `cd443209dc40296034efe8debf7ce97d7982444d6eae872df9f411402256be13`.

CI state and exact PR head are reported at delivery. Local evidence applies to
these exact product sources/dependencies/configurations/executables; subsequent
repairs require the directly affected checks again. Independent model-math and
lifetime review remain necessary. M2-M5 and V1 still own actual same-source
cashflow execution, stochastic staged fitting, MC/partials, language interfaces
and full independent product acceptance. This checkpoint is not complete XCCY MC.
