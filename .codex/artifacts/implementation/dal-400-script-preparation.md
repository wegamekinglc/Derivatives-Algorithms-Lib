# DAL-400: bounded script preparation

Controlling contract: `.codex/artifacts/api-notes/dal-398-script-contract.md`,
especially §§1, 3, 4, 6, 9, 10; DAL-396 research attachment
`01a15c20-3723-7ddd-8ef1-c0ac706cdc16`, §§3, 4.1, 6, 8.1, 9, and its
expert critique. Upstream implementation/evidence:
`.codex/artifacts/implementation/dal-399-script-contract.md`.
Startup master/HEAD: `45f957af49fb802a3e2dea9fba11b237f15eea5c`, containing
merged DAL-399 PR #607. Checkout was clean. Work is on the isolated task branch
`agent/dal-implementer/0553621b6951`, PR base `master`.

## Design and changed paths

The additive core module `dal-cpp/dal/script/calibrationpreparation.hpp/.cpp`
owns preparation only. It reads the instrument's effective typed slots and
unfolded AST, validates all receiver writes before replay or matching, then
derives narrow cashflow records, affine coefficient views and a disposable
Deposit/BondOption descriptor. The immutable instrument remains the only
contract authority. No second native pricing object or shared mutable cache is
retained. Model/context data contribute content fingerprints, not equivalence
certificates; complete AST/model admission remains mandatory.

The whole-history receiver guard propagates a may-have-paid state across events
and unions both branches. Historical PAYS count for reset prohibition. Ordinary
receiver writes require literal zero before every payment; extra receivers and
vector output fail. Validation precedes template rejection so a branch cannot
hide a later reset. The legacy Product, parser, preparation, MC and archive
implementations are unchanged.

Deposit coefficients use symbolic K dependence independently of the current K
value. A K-dependent multiplier/divisor, nonlinear operator or branch is
rejected. Price amounts are evaluated from the original PAYS AST with unit
numeraire, independently of the A/B calculation. Native curve/model partials
hold the contract fixed. Independent dA/dB feed the complete quotient rule;
every value evaluation validates annuity sign/floor, sizes and finite results.

The Gaussian matcher checks the entire single-payment expression, CALL/PUT
direction, N/K slots, concrete DF maturity/currency, fixing identity and payment
date. Exact native one-/multi-factor GSR types and equal model/context curves
are required. Hard payoff admission rejects nonzero smoothing. A descriptor
retains E as payment/expiry and T as observed bond maturity.

Successful preparation records rule, backend, derivative, compiler/context/plan
identities and effective sampling. RequireCompatible rejects changed source,
header, binding, routes, fixing contents/timestamps, actual model/curve data,
valuation/today policy, conventions or smoothing. Same-value contexts built
with different pointers remain compatible. No mutable market handle is stored
in the result.

Additional changed paths are the focused tests
`dal-cpp/tests/script/test_calibration_preparation.cpp`, three Machinist enums
and their generated C++/Java files, the existing methodology guide and
CHANGELOG. There are no facade, Python, Excel, CMake or example changes.

## Deliberate capability limits

- AutoFast/RequireFast admit only the two complete P0 templates.
  GenericDeterministic admits Deposit; a bond option cannot use forward
  substitution. Unknown source never silently selects MC or a native fallback.
- Deposit exposes analytic coefficient partials; Gaussian descriptor admission
  selects existing native finite differences. Explicit unsupported derivatives
  fail. This module does not advertise AAD for either new plan.
- Explicit ScriptMC admission requires opt-in, shared Price, a matching Gaussian
  context, irn sampling with paths >= 2/nonnegative seed, and derivative None.
  This is preparation admission, not a new MC executor or statistical acceptance
  claim. The MC wrapper and domain calibration adapters remain out of scope.
- The P0 Deposit rule remains origination-only. BasisSpread stays reserved as
  specified by the accepted DAL-398 schema; no additional product template or
  arbitrary affine-language optimizer was introduced to expand that schema.
- Discount values and their native partials are supplied by the compatible
  caller in cashflow order. This module does not add a curve interpolation or
  calibrated quote-risk convention.

These limits follow the controlling API note and the coordinator's scope; they
are not replacements for downstream MC equivalence or calibration acceptance.

## RED / GREEN evidence

1. Added `TestAffineCoefficientsAndIndependentQuotientOracle`. RED:
   `c++ -std=c++17 -I dal-cpp -I dal-cpp/externals/googletest/googletest/include -fsyntax-only dal-cpp/tests/script/test_calibration_preparation.cpp`
   failed because the new preparation header did not exist. GREEN after the
   minimal Deposit preparation: one test passed, including Price=B+K*A,
   A/B and independent log-discount derivative oracle off root.
2. `TestReceiverGuardPrecedesTemplateMatchingAcrossHistoryAndBranches` RED:
   a valid branch followed by a reset reported UnsupportedCalibrationProduct
   before examining the reset. Splitting validation from matching made this
   test and the first test pass. Historical and nonzero reset cases also pass.
3. `TestBondOptionDescriptorUsesSameSourceForCallAndPut` RED: the Deposit-only
   matcher returned UnsupportedFastEvaluator. GREEN: CALL/PUT descriptor checks
   passed, preserving payment E, maturity T and bound N/K.
4. `TestExplicitMcRequiresOptInSamplingAndSharedPrice` RED: the old rejection
   did not admit a valid explicit request or diagnose missing sampling. GREEN:
   opt-in/sampling/Price checks and seed-dependent identity passed.
5. Three focused RED edge cases: coefficient overflow returned nonfinite A/B;
   a vector receiver produced only a template rejection; the option lacked a
   contingent cashflow record. GREEN: finite checks, vector guard and explicit
   null-amount/DF cashflow metadata passed.
6. Expanded oracle/refusal coverage exposed a K-dependent branch classified
   only as an unsupported product. Adding the structural parameter-control
   check made it UnsupportedCalibrationMeasure. The independent Gaussian
   oracle explicitly uses ACT/365 and the actual 366 days from 2027-10-21 to
   2028-10-21; it does not round that maturity span to one model year.
7. `TestModelNameCannotBypassStructuralAdmissionOrLeakLegacyError` RED:
   an unrelated ModelData carrying the Gaussian type/name invoked its throwing
   serializer before admission, leaking Exception_. Moving exact model-type
   validation before fingerprint serialization converts this to owned
   UnsupportedFastEvaluator without calling the unsupported serializer.

The first three cycles used a focused executable linked from the new module,
test source, repository test main, Release core archive and gtest archive.
Subsequent cycles use the ordinary `dal_cpp_tests` target and
`--gtest_filter=CalibrationPreparationTest.<test name>`; failed edge-cycle
logs are included in the issue evidence attachment. No assertion/tolerance was
weakened. Helpers were split while green; lizard reports maximum complexity 8.

## Verification configuration

GCC 15.2.0, C++17, Release, native AAD, Eigen ON, native architecture OFF,
static libraries, public ON. Configured with:

```bash
git submodule update --init --recursive -j 8
cmake --preset Release-linux -S . -B build/Release-linux \
  -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_CPP_BUILD_BENCHMARKS=ON
cmake --build build/Release-linux --target dal_generate -j 8
cmake --build build/Release-linux --target dal_cpp_tests dal_public_tests script_perf -j 12
```

Dependency pins are unchanged: Eigen `3147391d`, googletest `d72f9c8a`,
Machinist `e76b0ef2`, pybind11 `8a099e44`, rapidjson `ea152ded`.
Machinist's tracked C++ and Java outputs are included. The generated check
initially flagged the expected untracked new enum files; it passes after those
outputs are staged. The standalone warning check uses the exact warning
categories/exclusions from `.github/workflows/cmake-linux.yml`, with this
runtime's GCC 15 rather than claiming a local GCC 14/MSVC execution.

Final verification passed:

```bash
cmake --build build/Release-linux --target dal_cpp_tests script_perf -j 12
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='CalibrationPreparationTest.*:ScriptInstrumentTest.*:CalibrationProtocolTest.*:ScriptPastReplayTest.*:ScriptExactFoldingTest.*:ScriptCompiledParityTest.*:ScriptFixingPreparationTest.*:GSREuropeanTest.*'
build/Release-linux/dal-public/dal_public_tests --gtest_filter='ScriptArchiveTest.*:ScriptTest.*'
cmake --build build/Release-linux --target dal_check_generated -j 4
python3 .github/scripts/check_docs.py
clang-format --dry-run --Werror dal-cpp/dal/script/calibrationpreparation.cpp dal-cpp/dal/script/calibrationpreparation.hpp dal-cpp/tests/script/test_calibration_preparation.cpp
git diff --cached --check
```

Results: 167 core tests (18 new preparation tests plus 149 nearby contract,
history, compiled, fixing and Gaussian tests); 15 public compatibility tests;
80 Markdown documents; generated-source and formatting/diff checks. The new
translation unit passes the repository warning flags with -Werror. No tests
were disabled and no warnings were suppressed beyond the existing CI policy.
The public executable remains byte-identical after the final isolated
model-type admission fix, so its successful compatibility results are reused.

Executable SHA-256:

- dal_cpp_tests: `d002fc77c121ac76876cc96d0a0437623f237691cb5d08013c3be3f56ba1ba3f`
- dal_public_tests: `1a8c9a391a96942039062a036a65921ab6297ce9899f466f491ae1e5a36852b9`

## Performance scope

Only the new module and its new tests call the new preparation/coefficients API.
Existing parser, preparation, evaluators, JCS/SHA and pricing kernels are not
modified. Therefore no existing regression-gate algorithm needs a timing
matrix. `script_perf` is the relevant front-end/tree/compiled compatibility
smoke, not a benchmark-regression verdict.

An isolation archive was made from the newly rebuilt Release core archive by
removing only `calibrationpreparation.cpp.o`. Re-linking the same script_perf
object with identical compiler/linker settings yields an executable identical
to the head binary: SHA-256
`2ba395042c397957188b464cdca2a3d8776550068da3ad7c0c4ba8976f7d3572`.
All pre-existing source objects/dependency pins/configuration are unchanged.
Thus the new archive member does not change this existing caller's executable.
The informational smoke completed, including scalar/vector tree and compiled
evaluation. This is executable-identity evidence, not an independently rebuilt
full baseline or a new claim about unrelated benchmark cases.

The attached new-path probe reuses the test's Deposit and Gaussian BondOption
construction. Both new preparation callers need coverage: ten rounds measure
1,000 fresh Deposit prepares, 1,000 fresh Gaussian prepares, and 100,000
coefficient/quotient/Price evaluations on two cashflows and one native
derivative column. A checksum
keeps outputs observable. These timings have no pre-existing implementation
baseline and are informational; they do not relax any regression threshold.
The ten-round ranges were 34.86–38.97 microseconds per Deposit preparation,
80.60–82.79 microseconds per Gaussian preparation and 0.624–0.678 microseconds
per coefficient/quotient/Price evaluation. The probe executable SHA-256 is
`80cf98e50f4db0f0f55391cbe457da3dc7d506515320c3fd7647a455d8bd42cb`;
the source, raw timings and executable identities accompany the issue evidence.
Other benchmark matrices, full product suite, MSVC, and standalone/example
builds were not run: their callers/build paths are unchanged.

## Windows namespace lookup repair

Input head: `3b8e41ef0d6eae1826684983ab48daee79acdb94`. The main MSVC job
`119071610813` in run `39554675729` and Windows wheel job `119071627720` in
run `39554675736` both fail with C2872 at `vectors.hpp`'s allocator references.
The instantiation is `Vector_<Payment_>` in `calibrationpreparation.cpp`;
the logged candidates are `Dal::Detail` and `Dal::Script::Detail`. This is
the RED compiler evidence, not an external permission/service failure.

The translation unit no longer introduces the whole Script namespace into Dal.
Its anonymous namespace imports only the 20 script types/functions it uses.
No shared vector/header, compiler flags, gate configuration, template admission,
diagnostic behavior or numerical expressions change. A source comparison
confirms the using-directive/declarations are the entire production diff.
No additional runtime test is needed for this name-lookup-only repair; the
existing preparation suite exercises the affected implementation.

Local GREEN verification:

```bash
cmake --build build/Release-linux --target dal_cpp_tests -j 12
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='CalibrationPreparationTest.*'
cmake --build build/warning-clean --target dal_cpp -j 4
clang-format --dry-run --Werror dal-cpp/dal/script/calibrationpreparation.cpp
git diff --check
```

All commands pass: 18 preparation tests; GCC 15.2 Release core/test link;
GCC 14.3 Debug core with the existing Linux CI warning categories/exclusions
and -Werror; formatting and diff checks. Local Windows execution is not claimed:
main MSVC and Windows wheel success must come from the pushed head's actual CI.

The original evidence attachment `01a15d1e-a38a-7561-8ae7-2a3783daea5a` remains
applicable to unchanged public compatibility, generated sources, documentation
and performance scope. No full test or timing matrix is repeated. The separate
review findings about BondOption parameter declarations and merged-event guard
diagnostics remain open for coordinator-directed follow-up; this repair does
not claim to resolve them. DAL-400 and parent DAL-396 remain in_progress.
