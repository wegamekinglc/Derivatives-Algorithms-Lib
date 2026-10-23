# DAL-409 staged XCCY instrument calibration

Active implementation and acceptance record for the new deterministic staged
XCCY observation adapter. Base: `d3bf3043b2385b70a1fcd4fb32723a2b86c549bd`
(accepted DAL-408/418, with merged DAL-405/407). Branch:
`agent/dal-implementer/14b681fcb1e4`; one PR targets master. This record controls
review; publication and exact-head CI snapshots are in the delivery attachment.

## Scope and authority

The controlling inputs are DAL-398's accepted contract/API note, parent research
sections 4, 5–6, 7.2, 9–10, the DAL-405/407/408 implementation records and the
current DAL-409 issue. No speculative model/API design has been reopened.

The original run stopped for model capacity with uncommitted source and evidence
preserved. Recovery resumed that same worktree and branch, confirmed the actual
report-objective RED and kept the original logs. No checkout was recreated,
no source was discarded, and no sibling's unmerged product was imported.
The recovery attachment and preserved source manifest identify that starting
state; original-run executions are distinguished from final-tree verification.

## Implementation

- `dal/curve/xccybasiscalibration.hpp` declares the copied XCCY spec, prepared
  evaluator, owning result/candidate and solve entry. The new adapter lives in
  `xccybasiscalibration.cpp`; existing native staged functions and joint solver
  bodies are unchanged. It delegates bounded solving to `Underdetermined::Fit`.
- Shared immutable observations carry Price or affine signed decimal BasisSpread.
  Import happens once. The existing complete source/AST/date admission remains
  authoritative; no native shadow product is retained by the new spec or plans.
- Domestic/foreign blocks are cloned and frozen at Prepare. Each candidate
  rebuilds the domestic-currency piecewise-constant basis-forward curve in the
  existing native knot layout and evaluates the same source-derived plans.
- Native parameters, ordered observation rows and deduplicated quote columns are
  distinct. Raw Jacobians are d(model measure)/d(native parameter), with repeated
  observations retaining repeated rows and a shared quote column. Error scales
  change residual normalization/plan identity without changing quote fingerprints.
- Initial theta0 stays fixed, including per-node overrides. Positive S transforms
  theta=theta0+S*z and Wz=S'Wtheta*S. The existing smoothing metric introduces no
  knot-gap factors. EXACT accepts finite scaled infinity <=1; APPROXIMATE requires
  explicit positive lambda/eta and accepts scaled L2 <=eta, without an objective
  optimality claim or legacy raw/RMS gate substitution.
- The APPROXIMATE report publishes actual final-residual data objective plus the
  native displacement penalty. Finite zero is retained; nonrepresentable metrics
  are unavailable. Reports own axes, values, plans, frozen fixing/route identities
  and raw partials. Objective identity includes plan, theta0, scales and controls.
- FD solve perturbations are performed by the existing Fit in z coordinates and
  consume its evaluation budget. AAD columns are transformed by S. Independent
  raw Evaluate FD uses representable native two-sided spacing. The one final
  diagnostic refresh is separate from Fit counters in the C++ result; optional
  diagnostic raw Jacobian work is also separate.
- Failure reports and validated candidate basis curves outlive input destruction.
  A budget failure retains the last solver candidate, residuals and counters.
  Session candidates/settings are not new archive schemas. Shared instrument
  strict JSON load/save and the old native archives/aliases remain unchanged.

## Public callers and supported boundary

The public facade adds native-once import, solve, result basis/report access and
capability getters under `xccycalibration.*`. Python adds copied keyword-only
context/import/spec types, their `_New` aliases, Evaluate/Solve/capabilities and
owning exception candidates. Every wrapper completes conversion before its
`gil_scoped_release`; RAII reacquires the GIL before Python result conversion.
Lifecycle tests exercise actual results/errors after inputs are destroyed; no
workload-based thread-progress timing is presented as a GIL proof.

Excel adds exact field/value typed session factories, import, solve/outcome and
capability functions, with committed Machinist declarations/stubs. Tests exercise
both direct solve and the actual repository Store/Fetch bridge for context,
curve blocks, dates and observation handles. Initialization uses the portable
XLL test runtime bridge; this is Linux evidence, not native Windows execution.

Supported cases remain exactly DAL-408's domestic collateral, explicit routes,
canonical 3M/6M mapping, fixed notionals and native FX-reset/MTM period-changing
notionals/principal deltas. There is no arbitrary new amortization convention.
USDperEUR and the existing basis sign are retained. NativeXccyTimestamp keeps
strict-past exact history, same-time supplied-first/otherwise-active and future
active-forward behavior, with explicit-empty versus omitted snapshots and
frozen closure. Intraday valuation/observations and generic today overrides
remain rejected. XCCY ScriptMC is explicitly `MCCrossCurrencyUnsupported`;
no joint expansion, stochastic FX/quanto model or calibrated quote risk is added.

The new facade example stays under `dal-public/examples/xccy_script_calibration`.
Existing core example directories, targets, install names and core-only
purpose remain unchanged. It imports once, fits BasisSpread, reprices Price from
the same bound instrument, prints the raw partial and queries the MC refusal.
The existing methodology guide gains a focused staged-XCCY section; CHANGELOG
and the active artifact index link it. No new guide requires another docs index
or CLAUDE methodology-list entry.

## RED and GREEN evidence

The attachment contains original RED/GREEN logs and `resume/` final verification.
The core/public/Excel API absence tests first failed in the original run before
implementation. Original `red-report.log` and resumed
`confirmed-report-red.log` show missing objective publication on an actual fit.
`green-report.log` and final core verification demonstrate the fix.

A second actual RED, `red-fd-budget.log`, exposes the supplied-Jacobian FD adapter
hiding perturbations from Fit's budget: gradients were 2 instead of the required
1 under maxEvaluations=2. Returning no custom FD Jacobian delegates counted
perturbations to the unchanged solver; the final budget/lifetime regression is
GREEN. No test or acceptance threshold was weakened.

Harness failures are retained separately: the first report rebuild lacked a
namespace qualifier; a standalone-example path selected a directory; formatting
reordered a prerequisite test-helper include. Each was corrected and the real
commands rerun. None is relabeled as a product RED or a successful execution.

Final narrow commands/results:

| Scope | Command / result |
|---|---|
| New core | `build/main/final-core-tests --gtest_filter=XccyInstrumentCalibrationTest.*`: 14 pass |
| Old staged/joint | `build/main/final-legacy-core-tests`: 27 pass |
| Public | `build/main/final-public-tests`: 12 pass under the core-initialized custom runner, including 10 old compatibility cases; canonical fresh-process coverage is corrected below |
| Python | `PYTHONPATH=build/main/dal-python python -m pytest dal-python/tests/test_xccy_instrument.py dal-python/tests/test_xccy_calibration.py -q`: 15 pass |
| Old Excel staged/joint | `build/main/dal-excel/dal_excel_portable_tests --gtest_filter=ExcelApiTest.*:JointCalibrationExcelTest.*`: 15 pass |
| Portable Excel | `build/main/dal-excel/dal_excel_portable_tests --gtest_filter=ExcelXccyInstrumentTest.*`: 3 pass |
| Complexity | Lizard `-C 8 -L 1000 -a 100` on affected implementation, new fixture/tests/example: no threshold exceeded |
| Warning policy | GCC 14 and 15 `-fsyntax-only` on all five changed product TUs with repository warning-clean exclusions and `-Werror`: pass |

The nonuniform-scale APPROXIMATE test independently calculates
`0.1*(d0*d0-d0*d1+d1*d1) + 0.5*lambda*sum(scaledResidual^2)` from nonzero fixed
initial parameters and checks the report, rather than merely adding its reported
parts. Mixed units suppress raw aggregate L2; shared quote mapping, candidate
refresh, failure ownership, timestamp/collateral/MC refusal and both spread-leg
orientations have direct tests.

## Numerical evidence and applicability

The accepted DAL-408 independent cashflow/source admission and native timestamp
proofs continue to apply: that provider, importer, parser, compiler, history and
native pricing kernel are unchanged. The fixed Price oracle is
`-0.22000103820950934` at K=.001 and basis=.003; its signed decimal par spread is
`-0.002983490837363863`. New adapter tests fit known .003 basis for fixed,
resettable and MTM and preserve historical reset/MTM snapshots.

At non-root native theta=(.004,.002), each of those three modes is compared with
the existing native par pricer on the freshly rebuilt candidate curve. Both raw
Price and BasisSpread AAD columns are compared with FD h=1e-5 and h/2=5e-6 at
absolute tolerance 1e-8. The Python public caller additionally records raw
BasisSpread partials: max errors at h/h2 are 8.339e-12/2.523e-11 (fixed),
6.239e-11/1.331e-12 (reset) and 1.172e-10/2.044e-12 (MTM). Cancellation need not
improve monotonically; both frozen bounds pass. This adapter/native parity
complements the reused independent provider oracle; it is not called a second
independent cashflow derivation.

## Scoped performance and component closure

The actual Python facade caller records three warmups and ten samples for a
one-observation/two-knot basket, including preparation, identities and owning
report work. Minimum Evaluate AAD times are approximately 407/397/457 us for
fixed/reset/MTM; solve minima are 658/726/751 us, with Fit evaluation/Jacobian
counts 3/2, 4/3 and 3/2 respectively. Exact samples, parameters, residuals and
h/h2 errors are retained in `resume/caller-probe.json` with its source. These
are informational new-API measurements on a shared host, not a performance-gate
claim. Native numerical kernels are unchanged; accepted DAL-408's native
performance comparison remains applicable. No full benchmark matrix ran.

Public-disabled workspace and independently configured standalone core each
build and run the existing `xccy_curve_calibration`, `xccy_mtm_calibration` and
`xccy_reset_pricing` executables. Core-only CTest passes 3/3; standalone core
executables are run directly. Both use GCC 14 Release/O3, static native AAD,
Eigen ON, C++17 and native architecture OFF.

These core closure builds explicitly reuse 218 final main-build objects only
after checking exact source paths and compiler commands (excluding output path),
with every object/dependency SHA recorded. They are incremental build evidence,
not claims of two fresh full compilations. CMake builds each independent archive
and its actual examples. A standalone core install (top-level component without
unbuilt unrelated examples) supplies `DAL::cpp` to independently configured
`dal-public`. The public library and all four facade examples build, run 4/4
CTest and install. A fresh installed-only `find_package(dal-public)` CMake
consumer links `DAL::public` and runs the XCCY example without workspace include
paths. Installed-consumer compile/link commands and run output are retained.

Final identity manifests retain changed source SHA-256, dependency Git pins,
CMake caches/commands and actual library/module/test/example identities. The
generated check preserves mtimes only for SHA-identical outputs so no unrelated
recompilation is inferred from regeneration timestamps. Object
reuse is recorded separately from final refreshed changed TUs. Final validation
also includes generated-file drift (Machinist wrote zero files), docs (105
Markdown files) and diff checks; exact outcomes appear
in the attached logs. Linux portable Excel is not a native Windows XLL/workbook
result. Windows and other platforms are left to the new PR head's real CI.
No merge, downstream task or additional specialist was triggered by the author.

## Canonical public fresh-process CI repair

The initial head `b1c21036d3b6a01d2912e4b79f4cb7cb35c1a7d3` subsequently
failed five jobs in run `39896324745`: clang-20 AADET and the four Extended
lifetime/profiling configurations. Compilation succeeded; every failure was
`PublicXccyInstrumentTest.TestNativeFacadeAndCapabilityGetter`, with
`currencydata.cpp:41: no default for 'USD'`. The original custom public runner
used the core's initialized test main. Its 12 passing tests established the
assertions under that setup, but did not establish canonical fresh-case behavior.
The old CI snapshot and original execution logs remain historical evidence.

The actual CMake `dal_public_tests` target was built without changing its
sources, linkage or discovery policy. Its link command contains `gtest_main`;
the executable confirms Google's `gtest_main.cc` at startup. Before editing,
both new cases were invoked separately in fresh processes, then as their suite
and through the existing CTest discovery. The empty-input case passed; the
native-facade case and suite exited 1 with the same USD error, and scoped CTest
exited 8 with one of the two cases failing. This is a product-test RED in the
canonical executable, rather than another custom initialized runner.

The fix adds only the public `global.hpp` include and `Dal::InitGlobalData()`
at the start of each existing case, following neighboring public fixture setup.
Initialization now precedes `TestXccy::Spec()` and its currency convention reads
in every fresh process. All original assertions are byte-identical. No fixture
financial terms, production defaults, global initialization policy, CMake,
workflow, solver, scale, schema or acceptance threshold changes.

`cmake --build build/main --target dal_public_tests -j4` then recompiled only
the affected test TU and relinked the canonical runner. Each case alone, the
two-case suite and `ctest --test-dir build/main -R
'^PublicXccyInstrumentTest\.' --output-on-failure` passed. The actual RED/GREEN
commands, exit statuses, source hashes, runner hashes and CMake link/flags are
in the repair attachment (`canonical_cases.py`, `red-results.json`,
`green-results.json` and their logs). The first canonical-target build required
its previously unbuilt test objects; no unrelated tests were executed.

The changed TU passes the existing GCC14/15 warning policy with `-Werror`,
CCN<=8 and formatting/diff checks. The former 86-test delivery is not presented
as a new execution: unchanged code/dependency/configuration and all 18 recorded
original binaries are verified byte-identical, preserving their scoped core,
Python, Excel, component, value/derivative and performance applicability. The
corrected two public cases have fresh canonical evidence above. No unrelated
matrix, native Windows XLL/workbook test or performance measurement was rerun.

Normal master integration brings only the already accepted DAL-410 decision
from `87d8954eefd2033a7829976a35d536ba439d4748`. It merges without conflicts;
the imported file SHA256 is
`c2ff68e1a696b620682b896ca9b292ec6d577bf92cadb3597991b427267733d5`, identical
to master. No DAL-420 unmerged model or sibling product is imported. The existing
public guide and CHANGELOG are unchanged. The first merge command stopped for
missing Git identity before modifying the tree; the corrected command uses
Cheng Li's identity per command, without writing shared/global configuration.

The new exact-head CI snapshot is delivered separately and does not replace
the coordinator's required review or final merge gates. This repair is not a
full DAL-409 code approval; the first complete focused code review and necessary
documentation judgment remain with the coordinator. DAL-420's independent
model run is untouched.
