# DAL-423: phased XCCY Monte Carlo and native partials

## Authority and implementation boundary

The implementation starts at accepted master
`9c43f20c30a668390edf35bfe2dfbf052564464f`, containing the actual DAL-421
and DAL-422 merges. The controlling decision is
`.codex/artifacts/api-notes/dal-410-xccy-mc-model-decision.md`, SHA256
`c2ff68e1a696b620682b896ca9b292ec6d577bf92cadb3597991b427267733d5`,
especially sections 3, 7 and 8. DAL-398, the current script-calibration and
sampling methodology, and the accepted critic/reviewer decisions
`01a166d3-2b9e-7a06-a91f-7ab92cac9df3` and
`01a166d5-79ec-74a0-9b11-89a8a8dff77e` retain authority. Parent scheduling
and the issue's full current description were read before implementation.

`PreparedXccyMc_` owns the original admitted instrument, frozen execution
history, two immutable coupon bindings, settings, five-slot mapping and checked
phase ledger. Both coupon views derive from the original definition through
the existing immutable binding API. No second native swap, hand-maintained
script, independent schedule, financial matcher or global lookup is retained.
Complete admission is preserved. Canonical unbounded coupons use K=0/1;
bounded coupons use two admissible distinct values and recover A/B by the
affine identity. A nonrepresentable coefficient interval fails explicitly.

The dedicated mapping enumerates `xd,xf,Id,If,Z` for each advancing interval,
using the accepted M2/M1 grid. Its numeraire is
`dal.domestic-collateral-bank/1`; transition layout is
`dal.xccy-gaussian-exact/1`. Three economic drivers never determine its draw
dimension. MRG32, Precise normals, no bridge and ordinary normal-vector seeking
retain the existing RNG implementation and stream.

Production changes are six new core files under `dal-cpp/dal/script/`:
`xccymc.hpp/.cpp`, `xccymc_internal.hpp`, `xccymcranges.cpp`,
`xccymcstatistics.cpp`, and `xccymcvalidation.cpp`. Machinist generates the
three new phase-enum outputs. Dedicated tests/reference tooling live under
`dal-cpp/tests/script/` and `dal-cpp/test-support/`. This record is the only
documentation change. No shared stats, dispatch, preparation, RNG, model,
solver, admission, sampling schema, legacy archive, public factory, example,
build file, public guide or sibling implementation was edited. No coordinated
shared-path exception was necessary.

## Sampling and identity

`XccyMonteCarloPhase_` is generated with invalid `_NOT_SET` default and canonical
Fit/Validation/HeldOut tokens. `ParseXccyMonteCarloPhase` owns malformed-token
diagnostics. The internal core settings copy existing count/seed/version
semantics; they do not introduce or modify the facade's shared sampling type.

The immutable reservations are Fit `[0,262144)`, Validation
`[262144,1310720)`, and HeldOut `[1310720,2359296)`. Counts select their
contiguous prefixes, independent of call order. Preparation checks integer
counts, multiplication, capacity, seed/version, size_t and the actual MRG32
uint64 draw range before seeking. D=0 preserves phase/path reservations but
consumes zero scalar coordinates. Independent empty draws are not claimed.
Mixed mappings are rejected because no union-basket/disjoint-coordinate
allocator is delivered. Explicit identical CRN reuse is accepted; overlap
between independent ledgers and stale ledger fields fail.

Mapping, reservation, prepared source/model context and evaluation identities
are separate. Mapping identity includes exact dated intervals, each innovation,
clock, seed/stream/precision/numeraire and dimension. Reservation identity adds
phase/layout, counts and path/scalar intervals with exact integer encoding.
Numeric candidates must retain model geometry and obtain a new model/evaluation
identity. The owned `XccyMcResult_` extension explicitly identifies
`dal.xccy-gaussian-mc-result/1`; existing report `/1` columns are unchanged.
The result owns mapping, ledger, A/B, covariance, native partials, optional
retained path coefficients, first normal vectors and scalar consumption.

## Estimation, derivatives and failures

A/B, tree/compiled and CRN bumps use the same normals. Price is B+K*A.
Par is the ratio of overall means, never a mean of path ratios. Its signed
denominator must satisfy the three-sigma confidence/floor test. Existing
`CalibrationMcInternal::AffineEstimate` supplies the complete paired
`(B,A,dB,dA,...)` covariance and delta-method quotient/gradient SE. Price uses
paired coefficient influences; native gradient SE comes from gradients, not
price dispersion. Covariance and partial uncertainty must remain finite.
For par, legacy batch scalar/derivative fields are unavailable zero/empty;
the owned paired coefficients are authoritative, as in the existing affine
bridge. Retained par paths expose coefficients, without pathwise ratios.

An independent recording scope constructs both active M2 providers and retains
their prepared prefix behind a checkpoint. Each path rewinds its suffix;
separate A/B reverses collect and add the two copies' model-input adjoints.
The accepted M3 snapshot Jacobian maps the complete spot, discount, projection,
basis and interior-sigma chain into requested ordered native coordinates.
Runtime/snapshot axes must match exactly. No shared model refresh hook is needed.
R/a requests, zero-vol/rank boundaries, duplicate/unknown labels and incompatible
caller tape modes fail explicitly. Small positive sigma has no added cutoff.
Nested recording refusal preserves the caller graph and owns its diagnostic.
Prepared/evaluation failures preserve instrument, measure, stage, report and
only an actually validated candidate model.

`ValidateXccyMc` keeps value parity, precision and combined error separate,
and additionally enforces gradient parity, precision and combined error.
It uses independently supplied value/gradient numeric indicators and existing
native-unit precision settings. Insufficient budgets retain estimates and
return numerical-validation failure; there is no tolerance relaxation or
automatic path extension. A bare estimate report makes no numerical acceptance
claim. AAD reports own native parameter axes and the raw Jacobian; this is
not calibrated quote risk.

## RED/GREEN evidence

The attached evidence retains actual runtime REDs and corresponding GREENs:

- Phase selection: reservation stub failed with `InvalidCalibrationPlan`;
  exact Validation/HeldOut prefixes and scalar coordinates then passed.
- Unset phase initially escaped as a null-string logic error; overflow was
  accepted. Checked errors now own the phase/sampling diagnostic before draws.
  A final diagnostic regression preserves `_NOT_SET`, rather than formatting
  its negative enum sentinel as an unsigned path ordinal.
  Exact stream/version and identity bytes are checked explicitly instead of
  the library string type's default case-insensitive comparison.
- Dedicated execution stub failed with `MCCrossCurrencyUnsupported`; zero-vol
  tree/compiled A/B/Price then matched Gaussian-fast values.
- Requested spot AAD initially failed at the value-only boundary; the active
  recording/native map passed CRN h/h2 and batch-gradient-SE comparisons.
- The prequalified combined-gradient example passed separate parity/precision
  but incorrectly passed combined error. The dedicated validator now rejects it.
- Nested recording initially leaked a generic exception; it now returns an
  owning `DerivativeUnavailable` without changing the caller graph.
- K=0 initially violated admitted `[.0005,.002]` bounds. Admissible coupon
  views now match unbounded-source Price and A/B without changing the bounds.
- AAD reports initially omitted parameter/Jacobian axes. Those axes now own
  the same ordered native partials as the estimate.
- Finite Price with overflowing partial covariance initially returned; it now
  fails `CalibrationNumericalFailure` rather than publishing nonfinite risk.

The first all-native/PWC FD attempts also retained real double-subtraction
failures around 1.2e-8 to 1.9e-8. No h, parameterScale or assertion tolerance
was enlarged. A separate scalar-Brownian/cashflow reference now evaluates
the fixed normals at 50 and 80 decimal digits, with the prescribed four-point
h/h2 differences. Its covariance integration, Cholesky and explicit dated
cashflows call neither the production transition/moment builder nor the
script evaluator. It consumes exported immutable numeric inputs and normals;
production observations are used only for final comparison assertions.

The frozen Python reference policy hash was saved before its first execution.
The 30 nonflat native axes and 10 PWC axes agree with AAD at both h and h/2
within the unchanged 1e-8 floor; observed refinement indicators are approximately
1.64e-42 and 9.32e-43. Portable literals preserve these assertions where MSVC
long double has only double precision. Linux additionally runs wider actual
CRN cashflow replay. PWC retains two original forward axes and a three-node
normalized snapshot, with no artificial anchor risk and zero unused last-node
sensitivity. These small fixed-path checks do not replace independent V1's
statistical/gradient qualification.

Additional tests cover exact scalar trace and extra-normal mutation, full paired
par covariance and wrong-SE detection, all three modes/both spread legs/directions,
immutable N/K rebinding, supplied/empty/omitted/future history, active at-V payment
SPOT versus reset FIX, changed globals after preparation, one-leg settlement,
all expiry/D=0, owned par refusal, candidate geometry, tiny positive sigma,
quanto-removal and reversed-basis negative controls. Existing M1/M2/M3 mathematics
and admission evidence is reused only for unchanged dependencies; old P0/P3 and
accepted M1–M3 matrices were not rerun for role handoff.

## Frozen numerical observations and acceptance limit

The dedicated ordinary-MRG32 driver runs the base N=110 domestic-spread MTM
fixture against accepted independent high-precision expectation literals and
Gaussian-fast values. It first checks the direct deterministic gate with the
references' separate numeric indicators. Price scale remains .01 USD, spread
scale 1e-4, sigma=3 and the numerical fraction .25. There is no gradient
statistical qualification in this long-path value-only driver.

Validation x1/x2/x4 uses 16 batches of 16384/32768/65536 paths, D=30 and seed1024.
Observed Price means are -3.687756213580767, -3.735062508394368 and
-3.719869698700487, with SE .05847234587325, .03661206533341007 and
.01972497549801866. Both independent and fast comparisons pass parity at each
prefix. **Price and par precision and combined acceptance fail at all three
prefixes.** At x4, 3*SE_Price=.05917492649405597 exceeds the frozen .0025 USD
budget; the par precision indicator is .001072359542351817 against .000025.
Both numerical-validation flags remain false, with estimates retained. These
budgets were candidates, not guaranteed precision. No sampling/model/financial
threshold was changed and no variance-reduction extension was added.

A newly linked x4 caller after coupon/report fixes reproduces the x4 numbers
exactly. The final later production delta adds nonfinite covariance/partial
guards and corrects the unset-phase diagnostic text. The prior sources and
exact archive/caller hashes are retained.
The same bounded delta makes version/identity comparisons byte-exact; the
retained caller's canonical versions and identities already satisfy them.
The retained 16 A/B batches are independently checked finite with finite full
covariance, and this value-only run has no gradient axes: both new checks are
therefore satisfied without altering any arithmetic or draw. This is explicit
evidence reuse across that bounded guard delta, not a claim that an old binary
executed new guards. Fresh targeted tests execute the final guard. Final source,
dependency, configuration and executable manifests distinguish those stages.

## Reproduction and final verification

The completed CMake core archive uses GCC 15.2, C++17 Release, Eigen, native AAD,
`-O3 -DNDEBUG -ffp-contract=fast`, with native architecture tuning disabled.
Inverse-normal translation units retain their existing contraction controls.
Configuration used the Release-linux preset with public, examples and portable
Excel tests disabled. No CMake or example change requires an install/standalone
component matrix in this issue.

```bash
cmake --preset=Release-linux -S . -B build/m4 -DCMAKE_CXX_COMPILER=g++ \
  -DDAL_BUILD_PUBLIC=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF \
  -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/m4 --target dal_cpp gtest -j 8
g++ -O3 -DNDEBUG -std=c++17 -pthread -I dal-cpp -I dal-cpp/test-support \
  -I dal-cpp/externals/googletest/googletest/include \
  dal-cpp/tests/test_main.cpp dal-cpp/tests/script/test_xccy_mc.cpp \
  dal-cpp/tests/script/test_xccy_mc_boundaries.cpp \
  build/m4/dal-cpp/libdal_cpp.a build/m4/lib/libgtest.a -o evidence/m4-tests
./evidence/m4-tests --gtest_output=xml:evidence/m4-final.xml
```

The explicit focused executable passes **21/21**. It is a complete runner
linked to the completed core and GTest targets; it is not an unfinished or
zero-test `dal_cpp_tests` target. The same **21/21** pass with ASan/UBSan and
leak detection enabled. New M4 production TUs and tests are instrumented;
unchanged core and GTest dependencies remain Release. This is partial
instrumentation, not a claim that the complete dependency tree was sanitized.

Four existing Gaussian MC affine/statistical/range compatibility tests pass.
Relinking their runner against the final archive produces the same executable
SHA256, so those four results apply without a redundant rerun. Generated drift,
format, scoped `-Wall -Wextra -Werror` syntax checks and diff checks pass. The
warning command excludes existing unused-parameter/sign-compare diagnostics;
it found and fixed one new test's dangling-else macro warning. The complexity
record retains the measured functions, without claiming a new complexity gate.

Reference reproduction builds `dal-cpp/test-support/xccymcreferencedata.cpp`
against the same core, exports `reference-input-final.json`, and runs:

```bash
python dal-cpp/tests/script/xccy_mc_reference.py \
  evidence/reference-input-final.json evidence/regenerated-reference.hpp \
  evidence/native-reference-audit-final.json
clang-format -i evidence/regenerated-reference.hpp
cmp dal-cpp/tests/script/xccymcreference.hpp evidence/regenerated-reference.hpp
```

The reference header regenerates byte-for-byte. The long-path driver is
`dal-cpp/test-support/xccymcqualification.cpp`; its two required arguments are
an output JSON path and one predeclared refinement token `1`, `2` or `4`.
Per-run JSON retains actual offsets, coefficients, references, numeric
indicators and each independent acceptance flag. Its observed single-caller
times were approximately 44/89/179 seconds for x1/x2/x4 and 184 seconds for
the newly linked x4 replay. These are informational timings, without a baseline,
speedup claim or formal benchmark regression verdict. No old benchmark matrix
was rerun.

`provenance.json` records 882 source hashes, all six pinned recursive gitlinks,
selected non-secret CMake configuration, actual production compile commands,
archive and executable hashes, and evidence applicability. The evidence bundle
retains raw RED/GREEN output, final XML, sanitizer output, reference policy/input/
audit files, numerical JSON, and the exact later guard delta. The completed core
has no changes to original tracked production files: all product changes are
the nine added core/generated paths. The independent reviewer and V1 must still
judge the new implementation and statistical evidence in their own scope.

Linux execution does not establish native Windows/XLL/workbook
acceptance. M5 owns public C++/Python/Excel requests, result tables and guides;
V1 still owns independent full numerical acceptance. Generic XCCY MC dispatch
remains unsupported, while this dedicated core adapter is available. R/a
partials, quote risk, joint/SLV/smile, early exercise, PVAtQuote and RQMC are
outside this delivery.
