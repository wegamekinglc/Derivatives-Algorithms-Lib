# DAL-411: Gaussian Caplet/Swaption fast-path implementation boundary

Status: **implementation blocked by a frozen shared-definition prerequisite**.
No Caplet or Swaption exporter, matcher, descriptor or shared fast Price is
enabled by this change. This record controls the prerequisite coordination and
subsequent implementation; it is not a numerical acceptance report.

## Startup and controlling inputs

The clean task branch `agent/dal-implementer/3c688a76ade6` starts at actual HEAD
and fetched master `1f6eaa32424d0c71efb148cfb953f1634ff209cf`. It contains the
DAL-404 squash merge. Multica's linked-PR table confirms PR #612 merged at
2026-10-22T00:30:25Z, with 36 successful checks. The research source baseline
`0f1feabee193d0976ab7a5336c9e9e29cb2aa28d` is historical, not this build's source.

Controlling inputs read:

- DAL-411's current scope, prerequisite acceptance and parallel-write rules;
  DAL-396's current authorization/scheduling description and bounded discussion.
- DAL-396 research attachment `01a15c20-3723-7ddd-8ef1-c0ac706cdc16`, especially
  §§3.2–3.4, 5, 7.3, 8 and 10–11, and its expert critique.
- [DAL-398 contract](../api-notes/dal-398-script-contract.md),
  [current methodology](../../../docs/methodology/script_calibration_contract.md),
  DAL-399–403 activity records and
  [accepted P0 validation](../validation/dal-404-p0-same-source-validation.md).
- Repository AGENTS, implementer, component-boundary, C++ and unit-test rules;
  definition/binding, preparation, MC, native Gaussian source and nearby tests.

The simultaneous DAL-405 run was verified through the sibling-run read. No
curve/provider/parameterization/optimizer, shared documentation/index, build or
example file has been changed. The only tracked change is this activity record.

## Reproduced prerequisite

Both options need an annual **Rate** strike. The immutable definition currently
accepts only N/Notional, K/Coupon or K/Strike, and fixed ALPHA for Coupon. In
`dal-cpp/dal/script/instrument.cpp`, `ParameterUnit` maps every Strike to
BondPrice; `ValidateDeclaration` enforces it before parsing or matching.
`ValidateParameter` additionally requires every non-Coupon value to be strictly
positive. That is the deliberately narrow P0 contract, not a newly introduced
Gaussian pricing defect.

Two focused executable probes request positive K=.03 with Strike/Rate:

1. Caplet-shaped cashflow at its payment event, with an explicit earlier Libor
   fixing date and a literal coupon accrual. There is no delayed-PAYS header
   reinterpretation in this witness.
2. One-period, single-curve payer Swaption-shaped expiry cashflow
   `N*MAX(1-(1+K)*DF(E,T),0)`, with a literal fixed accrual of one.

Both fail identically at definition construction:

```text
InvalidScriptInstrumentDefinition: /parameters/1/unit;
expected BondPrice; actual Rate
```

The same source texts parse through the existing bare DSL. A separate passing
probe copies the CalibrationError, destroys its input spec, and verifies its
owned code, field, expected/actual, parameter K and source row 2. These witnesses
isolate declaration admission; they do not establish complete coupon semantics,
native/MC equality, or a new supported product subset.

The existing preparation regression
`TestBondOptionDeclarationUnitAndExtraParameterFailuresAreOwned` explicitly
requires Strike/Rate to fail at this definition boundary. The existing
BondOption nonpositive-strike and rebind regressions also preserve its domain.
Changing an expected failure/stage to make a general relaxation pass would
change accepted P0 behavior. Declaring an option's strike as Coupon, disguising
it as BondPrice, or hiding it in a second native object would bypass the typed
contract rather than satisfy DAL-411.

DAL-411 permits edits to `dal/script/calibrationpreparation.*` but explicitly
freezes shared definition/compiler/MC interfaces and requires the coordinator
to establish an independent prerequisite or serial ownership before extension.
Thus changing only the Gaussian model and matcher files cannot admit either
correctly typed template.

## Smallest coordination request

Establish a shared-definition prerequisite or grant an explicit serial write
window for an additive **rate-strike declaration**, keeping the accepted
Strike/BondPrice behavior and its failure tests intact. A distinct RateStrike
role is one possible small extension; this record does not freeze a new enum
or silently amend the accepted API contract.

The concrete likely write set for that option is:

- `dal-cpp/dal/calibration/types.hpp`: additive Machinist role markup;
  regenerate the affected `MG_ScriptParameterRole_enum` outputs using
  `dal_generate`, including its tracked Java counterpart.
- `dal-cpp/dal/script/instrument.cpp`: key/role/unit validation and numeric
  domain for the added declaration. Allow finite zero/negative rate strikes
  subject to explicit bounds, retaining positive N, accrual and BondPrice.
- `dal-cpp/tests/script/test_instrument.cpp` and focused preparation tests:
  new-unit creation/rebind/domain/owned-error and JSON round-trip coverage,
  while preserving the P0 rejection assertions.

Existing unit serialization is token-based, so no new unit or arbitrary
parameter-slot mechanism is requested. Verify the actual affected generated
paths during regeneration. Any additional interface need must be identified
before writing it. No general compiler, stochastic model or curve-provider
extension is requested by this handoff. The coordinator can resolve ownership
without asking the user to redefine the financial product.

## Implementation design after prerequisite resolution

Keep native construction as one-time import. Emit concrete source dates,
literal fixed accruals, canonical observations, one N binding and one correctly
typed rate strike. Derive disposable Caplet/Swaption descriptors from complete
ASTs and effective bindings; construct native kernel inputs only from those
descriptors. Do not retain the original option as another pricing authority.

For Caplet, distinguish fixing E, index start/end S/T, coupon accrual and payment
P. A payment-event PAYS with explicit fixing E is a candidate AtEvent encoding;
prove its exact index start/end/day-count and projection mapping before enabling
it. Reject conventions that cannot be frozen and verified. For Swaption, derive
the full fixed annuity and floating conditional-PV cashflows at exercise, with
direction and settlement checked. Do not infer arbitrary schedules from a swap
index name. Prefer a narrow proven subset to an unverified convention mapping.

Default admitted products to FastGaussian and the existing native double
Black/quadrature kernels; finite differences are not fast AAD. Preserve the
three-effective-direction limit, including rank-deficient larger models. Keep
MC opt-in, native entry points, old aliases/archive behavior and independent
model/curve/numeraire identity checks. Rebinding, source, model or smoothing
changes must invalidate plans. Payment lag/cash settlement, unsupported
conventions, SLV, conditional/early exercise and smoothing need explicit
admission or precise refusal, never a changed payoff or silent MC fallback.

## Evidence obtained in this run

Fresh Release core and gtest were built in this checkout with GCC 15.2.0,
C++17, static libraries, Eigen ON, native architecture OFF, native AAD,
public/examples/benchmarks OFF. Dependency pins remain Eigen `3147391d`,
googletest `d72f9c8a`, Machinist `e76b0ef2`, pybind11 `8a099e44` and
rapidjson `ea152ded`; no dependency pointer was changed.

```bash
git submodule update --init --recursive -j 8
cmake --preset Release-linux -S . -B build/Release-linux -DCMAKE_CXX_COMPILER=g++-15 -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_CPP_BUILD_BENCHMARKS=OFF -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/Release-linux --target dal_cpp gtest -j 10
g++-15 -O3 -DNDEBUG -std=c++17 -fPIE -ffp-contract=fast -I dal-cpp -I dal-cpp/externals/rapidjson/include -isystem dal-cpp/externals/googletest/googletest/include dal-cpp/tests/test_main.cpp ../evidence/dal411_contract_probe.cpp dal-cpp/tests/model/test_gsr_european.cpp dal-cpp/tests/script/test_instrument.cpp dal-cpp/tests/script/test_calibration_preparation.cpp build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -pthread -o build/Release-linux/dal411_contract_tests
```

The two expected **RED** tests, each asserting that its Rate strike can reach
the matcher, exit 1 with the exact failure above:

```bash
build/Release-linux/dal411_contract_tests --gtest_filter='Dal411ContractProbe.TestCapletRateStrikeCanReachMatcher:Dal411ContractProbe.TestSwaptionRateStrikeCanReachMatcher' --gtest_output=xml:../evidence/dal411-contract-red.xml
```

There is **no GREEN implementation** of these two tests. They remain external
reproduction probes in the evidence attachment, not committed failing CI tests.
No production modification or assertion weakening was made.

Current-boundary verification exits 0, **86/86 pass**: 2 probe checks, 12 native
European prices/refusals, 5 native calibrations, 33 instrument, 6 protocol and
28 preparation tests. Native coverage includes payment-measure lag, future
coupon convexity, independent two-factor integration, negative strike/zero
volatility, rank-one four-factor reduction and four-independent-direction
refusal. This verifies existing behavior only.

```bash
build/Release-linux/dal411_contract_tests --gtest_filter='Dal411ContractProbe.TestWitnessPayoffsAreAcceptedByBareDsl:Dal411ContractProbe.TestOwnedDefinitionDiagnosticPinpointsRateStrikeUnit:GSREuropeanTest.*:GSRCalibrationTest.*:ScriptInstrumentTest.*:CalibrationPreparationTest.*:CalibrationProtocolTest.*' --gtest_output=xml:../evidence/dal411-current-boundaries-green.xml
```

Fresh binary SHA-256 identities:

- `dal411_contract_tests`: `64c986917e76ec88bb0fbdaa6ed5490522830c8fe25c75ad748ed750c5270caa`.
- `libdal_cpp.a`: `4c78288d28a54cc66374a6b956704d5b9243affd2f1ca35a3e3d36f42b46ae76`.
- `libgtest.a`: `1015854de3a1397beae2a28dc7b0373f47d5c3136d5aff520bf9b0d0ff102bbe`.
- External probe: `9804863428822b075e279a43fe1059ecefbd406febd9aa221e3344865fd4efc2`.

The issue evidence attachment contains source, RED/GREEN logs/XML, build/link
logs, configuration and manifests. Historical P0 evidence is retained only for
its accepted Deposit/BondOption domain; no historical binary is presented as
the binary above and no P0 acceptance is extended to either new template.

## Remaining acceptance and documentation

After the prerequisite, resume implementation on this branch and the same
single draft DAL-411 PR. The prerequisite artifact must not be accepted as
delivery of the original issue or used to start DAL-412.

Still required per enabled template: independent mathematical/cashflow oracle,
native/script-fast Price and applicable native partials, same-binding script
MC/AAD and CRN FD, initial/nonroot/legal bumps, fixed sampling and predeclared
budgets. Report quadrature refinement, FD refinement and batch MC uncertainty
separately; never enlarge scales to hide a semantic difference. No new-template
MC budgets or results have been established in this run.

No executable production caller changed, so no timing matrix or performance
acceptance is claimed. After implementation, select preparation and fast Price
callers for each template plus affected native-only cases, separating parse,
evaluate/FD and MC costs. Reuse valid P0 evidence only by source/configuration
and executable identity; do not rerun unrelated matrices.

Shared methodology, CHANGELOG and indexes remain assigned to DAL-405. Nothing
is integrated into them, and no feature-added release text is appropriate yet.
Once templates are enabled, place exact capability/limitation drafts here for
the coordinator's serial integration. No examples, build or public facade were
changed; standalone/example/public verification was not run or claimed.
