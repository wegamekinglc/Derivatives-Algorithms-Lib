# Structured-operator language audit and financial handoff

Baseline: merged #532, `6908bdad5853646eb39a1c19a1ce3a18b322218a`.
Source requirements are the native-only roadmap and its
[original detailed plan](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/de5dd8b2/.codex/artifacts/plans/aad-improvement-plan.md),
especially applicability of language projections and H.3's full PDE chain.

## Actual surfaces

| Family                                    | Owning C++ surface                                                                                                         | Python surface                                                 | Excel surface                                | Decision                                                                                                                          |
|-------------------------------------------|----------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------|----------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------|
| Dense/coordinate solves and accuracy      | `dal-cpp/dal/math/aad/linearsolve.hpp`, `dal-cpp/dal/math/aad/linearsolvecoordinates.hpp` and accuracy overloads           | No active recording surface                                    | No active recording surface                  | C++ owns recording, seeds, reports and lifetime; do not equate a matrix container binding with an AD solve                        |
| Implicit roots/stationary fits            | `dal-cpp/dal/math/optimization/implicitroot.hpp`, `dal-cpp/dal/math/aad/implicitroot.hpp`; supplied C++ equation/Jacobians | No equation trampoline or root-recording API                   | No equation/recording handle                 | Retain the existing C++ extension contract; a generic callback is not a financial projection                                      |
| Sampled theta step                        | `dal-cpp/dal/math/pde/sampledthetastep.hpp`, `dal-cpp/dal/math/aad/sampledthetastep.hpp`; fixed samples/mesh               | No sampled-step or complete PDE risk entry                     | No sampled-step or complete PDE risk entry   | Keep generic recording C++-owned; complete financial projection remains open                                                      |
| Financial European PDE                    | `dal-cpp/examples/european_aad_fd/european_aad_fd.cpp` and `dal-cpp/test-support/europeanaadfd.hpp`                        | No corresponding pricing/risk factory                          | No corresponding factory/getters             | Concrete missing closed financial boundary; next separate implementation PR                                                       |
| Calibration pullback                      | `dal-public/src/calibrationrisk.hpp`                                                                                       | Typed calibration snapshots/seeds/results                      | Immutable calibration seed/result handles    | Available with declared native-Dupire or retained-effective-inverse method; not proof of generic exact stationary-fit derivatives |
| Dupire/rate quote curvature               | `dal-public/src/dupirecurvature.hpp`, `dal-public/src/ratecurvature.hpp`                                                   | Closed owning finite-step requests/results                     | Curvature factories/getters absent           | Python accepted; Excel delivery remains open                                                                                      |
| Segmented MC/LSMC curvature               | `dal-public/src/montecarlocurvature.hpp`, `dal-public/src/lsmccurvature.hpp`                                               | Closed owning plans and passive results                        | Corresponding curvature interfaces absent    | Python accepted in #531/#532; Excel delivery remains open                                                                         |
| Structural Jacobian/checkpoint/mixed-mode | Native C++ recording policies and opt-in smooth prototype                                                                  | Existing financial requests only; no generic active scalar API | Existing first-order financial requests only | Preserve named financial estimators; do not enable general higher-order capability                                                |

The published methodology must describe current capability separately from
pending delivery. Existing passive Python `IVS_` calibration overrides remain
supported; this audit does not remove them or claim Python has no callbacks of
any kind. No new Python objective, active scalar or equation trampoline is needed
for the bounded financial projection below.

## Required next increment: closed European PDE risk

1. Reuse the accepted #506 fixed-grid Black–Scholes call/put program, including
   terminal payoff, discounted external boundaries, variance-to-volatility chain
   and damped initial schedule. Share its implementation with the example/tests;
   do not duplicate the financial chain or make core depend on dal-public.
2. A closed owning C++ request supplies finite r/vol/strike and passive physical
   grid, spot/node, expiry, dividend and time schedule. Expose no coefficient
   provider callback, adaptive mesh, American projection or arbitrary objective.
3. Return detached call/put prices and a 2-by-3 r/vol/strike Jacobian with axes,
   units, method, resolved settings and actual forward/transpose diagnostics.
   Spot and the mesh are passive; do not label a node/mesh variation as Delta.
4. Own recording/mode state in C++; reject incompatible active recording before
   mutation, preserve caller state on failure, and publish only complete results.
   Declare caller/recording capacity bounds and report their actual scope.
5. Python takes copied typed settings and checked passive numbers, releases the
   GIL for native work and returns detached settings/matrices/diagnostics. Existing
   interfaces/defaults/capability flags remain unchanged. Excel subsequently uses
   the same C++ result and owning handle/getter contract.
6. Validate every published price/risk against independent same-grid complete
   differences and existing #506 discrete references; separately check continuum
   refinement/parity. Include nonzero rate/vol/strike, boundary contributions,
   ownership, zero/exact/short budgets, failure recovery, GIL and installed use.
7. Select only affected financial program/boundary costs before timing; preserve
   calibration and alternating-pair evidence. No unrelated full benchmark matrix.

Detailed defaults, admission limits and resource formulas belong to that
increment's executable specification/API critique before implementation.
Estimated additional implementation/acceptance effort: 6–10 developer-hours.

## Audit completion and compatibility

This audit closes only the inventory and required handoff. It does not accept
the missing PDE boundary, Excel parity or final integration. Verify all named
source surfaces and registration inventories, published links and the absence
of production/configuration changes. Reuse immutable #532 source/binary evidence;
no new numerical test or performance result is claimed for a documentation-only
change. Exact-head publication checks and complete review remain required.
The three accepted LSMC cost driver/bridge/raw files remain active compatibility
baselines for subsequent Excel/final interface acceptance. Retire them in an
implementation delivery when their applicability ends; this inventory PR changes
only Markdown and does not repeat their measurements.

Open questions: none blocking this bounded handoff.
