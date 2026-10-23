# DAL-424: public stochastic XCCY bindings

## Authority and boundary

The branch starts at accepted master
`4dbdff636bcd92d2a236abe644a3e0c73b9c208a`, containing the actual M1–M4
and DAL-409 merges. The controlling DAL-410 decision has SHA256
`c2ff68e1a696b620682b896ca9b292ec6d577bf92cadb3597991b427267733d5`.
Its sections 7/8, DAL-398, the incorporated critic/reviewer decisions,
current methodology, component boundary contract and accepted M1–M4 headers
and records control this change. DAL-425's tests, helpers and record are not
modified. No unmerged sibling product is imported.

This change adds public callers of the accepted core. It does not change core
financial algorithms, observations, RNG, solver, thresholds, archives, legacy
aliases or generic dispatch. `git diff` against the starting point has no
`dal-cpp` changes. The original imported source/AST/event dates and immutable
binding remain authoritative; no native trade is retained beside them.

## Design and public behavior

`dal-public/src/xccymontecarlo.hpp/.cpp` supplies the typed model constructor,
explicit MC request, same-source moment caller, owning MC extension, native
partials, validator, capabilities and staged Gaussian candidate getter.
M1 market/dynamics/model specs remain the model authority. Required V, pair,
domestic collateral, curves, basis, spot and named dynamics are not defaulted
by a binding. Snapshot normalization and supported/rejected geometry stay in
core. The wrapper has no separate valuation/today-policy override.

`XccyMonteCarloResult_` derives from the existing
`CalibrationMonteCarloResult_`; it retains an immutable owning core result.
This additive type provides the established mean/SE/report interface without
changing the generic result or shared report `/1` protocol. Its separate JSON
schema is `dal.xccy-gaussian-public-result/1`. It reports the original core
result version, model/mapping identities, five-slot transition layout, phase
ledger/version, exact path/scalar-coordinate ranges, A/B, paired batch
covariance, native labels/partials and gradient SE. Ordinals are decimal
strings. Covariance order is B,A,dB[0],dA[0],... . Quote risk remains false.

Phase has no valid default. Existing sampling types are reused without adding
phase or offsets to them. Core validates counts, products, capacity, mapping
and ledger before draws. Simulation defaults are local to the XCCY request:
zero smoothing, Precise normals, no bridge and the existing compiled default.
Unsupported simulation/method settings fail explicitly. AAD requires explicit
ordered native labels; fixed a/R and rank/zero-vol boundaries remain refused.
No FD optimizer or automatic fallback is introduced.

Explicit Gaussian dynamics and basis parameterization are exposed on the
existing staged spec. Absence preserves deterministic selection. The validated
candidate model contains the fitted basis and can reprice the same instrument.
Python staged failures preserve the original curve-valued `candidate` and add
an owning `gaussian_model`; Excel outcomes similarly retain both candidate
curve and model. An absent model or partial is unavailable, never fabricated.

Python constructors require typed keyword fields, reject unknown keys and
invalid nested/numeric/int32/bool inputs, and copy before releasing the GIL.
None/omitted fixings preserve omission; a typed empty snapshot stays explicit.
No Python callback or borrowed container crosses the execution boundary.
Results and exceptions retain their owned data after input destruction.

Excel has typed model-spec/dynamics/request/outcome handles and generated XLL
registration sources. Required market curve handles are separate arguments.
Dated vol buckets and named correlation tables retain their actual row/column
locations. The extracted XCCY field reader preserves existing behavior while
serving the new factories. Unknown/normalized duplicate fields, wrong handles,
numeric booleans, nonfinite numbers and invalid phase/settings are rejected.
Execution outcomes own reports and validated candidates. Error spills retain
the existing 14-column `/1` order. Native partial and validation flag spills
are separate domain extensions; unrequested gradient acceptance is blank/None.

The validator forwards the owning core estimate; it does not reconstruct or
relabel its phase. Fit/HeldOut cannot become Validation. Parity, precision,
combined error, gradient flags and budget exhaustion are distinct. Failed
precision preserves the original estimate/report, with no hidden path increase.

## Changed paths

- Public facade and dedicated tests: `dal-public/src/xccymontecarlo.*`,
  `dal-public/tests/test_xccy_monte_carlo.cpp`.
- Python binding/registration and dedicated tests:
  `dal-python/src/bindings/xccymontecarlo.cpp`, `xccyinstrument.cpp`,
  `calibrationinstrument.cpp`, `dal-python/CMakeLists.txt`,
  `dal-python/tests/test_xccy_monte_carlo.py`,
  `dal-python/tests/native/xccymontecarlo.cpp`.
- Excel binding/reader and tests: `dal-excel/src/__xccymontecarlo.*`,
  `__xccyinput.hpp`, `__xccyinstrument.cpp`,
  `dal-excel/tests/test_xccy_monte_carlo.cpp`, and two native-only cases in
  `dal-excel/tests/test_script_raw.cpp`; 44 generated `.inc/.htm` outputs for
  the 22 additive logical functions.
- Public examples/build: the new `xccy_script_monte_carlo` program,
  `dal-public/examples/CMakeLists.txt`, a shared public example contract helper,
  and the existing public deterministic example's use of that helper. Core
  examples stay in their original directories and use core APIs.
- Current guide/index, public header catalog, CHANGELOG and this record.

## RED, GREEN and harness corrections

Raw logs are retained in the delivered evidence archive; these names refer to
that archive rather than tracked repository files.

1. The first public caller failed compilation at the missing
   `dal-public/src/xccymontecarlo.hpp`. The original syntax RED is
   `red-public-syntax.log`. The completed public caller then matched direct
   `PreparedXccyMc_` mean, SE, mapping and ledger exactly.
2. The first Excel caller similarly failed at its missing typed API header
   (`red-excel-syntax.log`), then passed the typed owning outcome comparison.
3. The baseline Python module failed the focused new-constructor assertion
   (`red-python.log`). The binding implementation made that caller pass.
4. Runtime edge REDs retained the missing staged model getter and the erroneous
   True value for unrequested gradient-combined acceptance. The qualified
   control is `red-python-flags-qualified.log`; final getters return the owned
   candidate and None/blank for inapplicable gradient flags.

The first precision control itself had an invalid epsilon larger than its
deliberately tiny budget. It was qualified to epsilon=1e-16 while preserving
errorScale=1e-12; parity=true and precision/combined/numerics=false remain the
required negative result. This is a unit control using a supplied reference,
not independent financial acceptance.

The first direct-core Python probe was a second static extension. It could
not share initialized convention state, including after an attempted separate
initialization. The original first log/XML were later overwritten by final
verification; its tool-returned excerpt and the complete second failed log
are supplied, with that distinction explicit. The final test-only native
entry point lives in the tested `_dal` runtime and calls `PreparedXccyMc_`
directly with the same copied input. All 18 mode/measure/phase cases execute;
none is skipped. The probe is enabled when Python tests are enabled, including
standalone Python test builds.

An early overlapping build corrupted `value.cpp.o`; the invalid object and
link failures were not treated as test REDs. The object was rebuilt and builds
in each tree were kept sequential thereafter. Initial configuration failures
(Ninja unavailable, and the public-disabled workspace's portable-Excel default)
are retained separately from successful Make configurations. They required no
product or financial changes.

## Verification and component closure

The main Release configuration uses GCC 15.2.0, CMake 4.2.3, Unix Makefiles,
C++17, static libraries, AADET, portable architecture, Python and portable Excel.
Focused commands are:

```bash
cmake --preset Release-linux -S . -B build/m5-make \
  -DDAL_BUILD_PYTHON=ON -DDAL_BUILD_EXCEL=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/m5-make --target dal_public_tests _dal \
  dal_excel_portable_tests xccy_script_monte_carlo xccy_script_calibration -j8
ctest --test-dir build/m5-make -R \
  'PublicXccyMonteCarloTest|PublicXccyInstrumentTest|ExcelXccyMonteCarloTest|ExcelXccyInstrumentTest|^xccy_script_(calibration|monte_carlo)$|dal_binding_boundary' \
  --output-on-failure
PYTHONPATH=build/m5-make/dal-python python3 -m pytest \
  dal-python/tests/test_xccy_monte_carlo.py \
  dal-python/tests/test_xccy_instrument.py \
  dal-python/tests/test_calibration_instrument.py -q
cmake --build build/m5-make --target dal_check_generated
```

The selected CTest set has 20 cases. The Python selection has 46 cases,
including positive/negative native GIL handshakes, 18 direct-core parity cases,
three notional modes, Price/par, all phases, ownership, strict inputs, history
markers/future entries ignored, capacity and independent validation flags.
The original generator check rejected untracked outputs; after staging the
44 generated files its drift check passed. No enum or old generated schema
was changed.

Component commands and raw outputs are supplied separately:

- Public-disabled workspace: public/Python/Excel/portable-Excel disabled,
  core examples on; actual `xccy_reset_pricing` build and its 1/1 CTest pass.
- Standalone core: actual Release shared-library build and install, tests and
  examples disabled. No nonexistent core CTest pass is claimed.
- Independent standalone public against that installed core: complete build
  including all five public example targets, 2/2 selected XCCY example CTests,
  and actual install.
- Independent installed consumer: `find_package(dal-public)`, compile/link
  only through `DAL::public`, load the once-imported persisted instrument,
  build the typed model, run moments and explicit HeldOut MC; 1/1 CTest pass.

Documentation checks cover 110 Markdown files. Formatting, generated drift,
dependency boundary, diff checks and focused production CCN<=8 checks apply.
The final issue handoff supplies exact head/PR and a single CI snapshot.
Native Windows/XLL cases are authored but not executed on this Linux host;
no workbook execution is claimed.

## Provenance, cost and remaining acceptance

The evidence bundle records source hashes, all five pinned dependency SHAs,
compiler/configuration identities, relevant compile/link commands, and hashes
of the actual libraries, Python module, test runners, examples and installed
consumer. Main static and standalone shared configurations are distinguished.
The source/core/dependency applicability is recorded rather than substituting
old binaries or claiming old numerical matrices were rerun.

Changed paths are facade/input/report/build callers, with no changed algorithm
in the formal nine-target performance gate. The minimal new caller observation
is the same-source synthetic staged→GaussianFast→HeldOut example with 4x256
paths, one raw spot partial and one thread. Its observed wall time is about
0.45 seconds; this is informational, not a calibrated regression verdict.
No old P0/P3, full numerical or benchmark matrix is rerun or relaxed.

The derived MC return type and separate domain JSON/flag extensions are additive
publication choices implementing the frozen versioning constraint. No new
financial design deviation is introduced. The deterministic/default/generic
capabilities remain distinct from the explicit Gaussian path.

M4's frozen N=110 MTM x4 precision/combined failure remains: Price
3SE=0.05917492649405597 > 0.0025 USD; par
3SE=0.001072359542351817 > 0.000025. Large-sample gradient and independent
DAL-425/V1 acceptance are not supplied by these caller tests. Joint/SLV,
volatility calibration, early exercise, PVAtQuote, RQMC and calibrated quote
risk remain outside scope. Review/merge and downstream scheduling belong to
the coordinator; this implementation does not self-merge or dispatch experts.

## R1: complete Excel tables and validation before conversion

The focused repair starts at reviewed PR632 head
`7ad7da045f7b65d42f748b0a01b2ba44458b0b72`, on the original branch/tree.
The original reviewer probe and bundle remain byte-for-byte intact. Its actual
RED reproduces the three selected precision rows being trimmed to one before
the factory, with the invalid request accepted. It deliberately exercises the
old general converter; changing that converter is outside this repair.

Seven M5 matrix inputs now use the existing Machinist alternate-conversion
hook and greedy markup to call the XCCY-local `ToCellMatrix_orXccy` reader:
market/dynamics/request/reference/precision settings, dated volatility buckets
and named correlation. The reader validates original dimensions/storage and
every raw cell before integer normalization or conversion, then converts the
complete selected dimensions. Excel error cells retain the real function,
argument, field where available, and original row/column. There is no new wire
type or shared converter/generator-framework change. Five generated `.inc`
files are regenerated from markup; generated HTML remains identical.

M5 field/value settings accept fully blank rows without compacting the matrix;
unknown/normalized duplicate and invalid numeric fields after them therefore
retain their true positions. Partial blank rows remain invalid. The shared
field reader's new blank-row option defaults to false, preserving the old
XCCY factory behavior. Bucket/correlation tables retain every selected row;
blank or extra rows do not disappear. Optional missing/nil/empty text/1x1 blank
request settings retain the existing semantics. Bool/numeric-text/nonfinite
numeric values remain rejected; raw integer values are normalized after checks.

The five new native XLL tests in `test_script_raw.cpp` call the actual generated
exports through the existing runtime bridge. They cover all five settings
factories, leading/internal/trailing blank rows, unknown and normalized duplicate
keys after blanks, raw error key/value cells, bucket/correlation positions and
full dimensions, numeric types and original rows, malformed storage, and valid
range/optional-blank controls. They initialize both test and XLL runtime state.
One new portable factory test protects typed numeric positions and the old
factory's blank-row refusal.

Actual repair verification:

```bash
cmake --build build/m5-make --target dal_generate
cmake --build build/m5-make --target dal_excel_portable_tests -j8
ctest --test-dir build/m5-make -R \
  'ExcelXccyMonteCarloTest.Test(RequiredPhase|NamedDynamics|NewFields)|ExcelXccyInstrumentTest.Test(StrictImport|.*ErrorsFollowActualFieldCells)' \
  --output-on-failure --output-junit evidence-r1.xml
python3 evidence/r1/run_generated_probe.py final
cmake --build build/m5-make --target dal_check_generated
```

The selected CTest set passes **7/7** (six existing affected input tests plus
one new test). On Linux, the separate probe compiles the unmodified generated
wrapper includes and verbatim source-extracted converters/raw validators with
a minimal storage/session/return ABI adapter, links the rebuilt real Excel
factory objects, and executes the same two existing plus five new XLL test
bodies: **7/7**. Before the production change, its first three new regressions
compiled and failed, respectively exposing truncation, lost field context and
the unlocated converter exception. Initial adapter build errors (constant
extraction and noncopyable test handles) are retained as harness corrections,
not product REDs. An early intermediate run linked the old factory object
before the foreground rebuild completed; its failed result is retained and
superseded by the complete new-object runs. The unchanged original probe is
historical RED; its precision input is reproduced through the new generated
entry in the GREEN tests. The general non-greedy converter remains unchanged.

The local adapter is **not native Windows/XLL or workbook execution**. The
authored native tests must execute under exact-head Windows CI. Scoped GCC14/15
warning-as-error syntax checks include both rebuilt factory callers, both
changed test files and the generated-entry adapter. Production CCN remains
at most 7 in the input header and 5 in the implementation, below the limit 8.
Generated drift, changed-line formatting, documentation and diff checks apply.
The first warning check also found an existing value-copy range loop in the
affected phase test; it now binds a const reference with identical inputs and
assertions. The first generated check ran before staging the regenerated files
and correctly reported their differences from the index; the staged check is
the applicable drift result, with no output patched by hand.

The delivered manifest records the full repair patch, unchanged original
probe, raw commands/exit codes/XML, all changed source hashes, actual callers/
objects/archives, dependency SHAs and five configurations. Regeneration updated
unchanged generated-header timestamps, so the first target build rebuilt
transitive core/public objects as well; both resulting archives are byte-identical
to the original accepted artifacts. The new Excel runner and its objects have
fresh identities. The original public runner, Python module, examples,
standalone/installed libraries and consumer, and all five configurations remain
identical. Existing unchanged C++/Python/GIL/component evidence is reused by
content/configuration/binary applicability; those tests were not rerun.

Only the Excel input path, required generated wrappers, direct regressions and
this record change. Public guides, core financial code, RNG, compiler, solver,
archives, report protocols and performance callers remain unchanged. No long
MC, gradient, sanitizer, installation or performance matrix is repeated. The
original M4 precision/combined failure and pending independent V1 acceptance
remain exactly as described above; this repair supplies no numerical waiver.
The handoff supplies the new commit and one nonblocking CI snapshot, leaving
the parent in progress and review/merge/downstream scheduling to the coordinator.

## R2: initialize the native test's callee runtime

At exact head `4bda6772cc9f7b79e8996d3492324e15f8179774`, three MSVC
configurations compiled successfully but failed the same three M5 Excel cases:
unavailable-partials/owning spill, staged candidate lifetime, and validation
flags/HeldOut rejection. Their full original logs remain in the delivered
bundle. The actual failed step is CTest, not the git-cleanup annotation.

Each case initialized the test executable's DAL state, but omitted the existing
`Excel::ScriptTestInitialize(1)` that initializes the statically linked DAL
state inside the XLL. The same-source/owning case already initializes both.
The normal Linux portable executable shares one state and conceals that gap.

A scoped Linux reproduction compiles the five unchanged Excel factory/getter/
diagnostic/runtime source objects with PIC, links the existing public/core
static archives into a private shared module with `-Bsymbolic`, and links the
actual test translation unit against its own copies of those archives with
`--exclude-libs,ALL`. No production source is rewritten or replaced by a mock.
The local experiment separates the DAL runtime states while sharing the system
C++ runtime. It is an ELF diagnostic, not MSVC/XLL or workbook execution.

Before the initialization fix, the actual three cases each fail in a fresh
process and the already-initialized same-source control passes. The staged
case now also checks its owned model before the getter and includes the full
report on failure. Its actual RED report shows `InvalidCalibrationPlan`,
`Prepare`, `no default for 'USD'`, observation/quote `maturity-0`, zero
evaluations, and no candidate model. The generic getter's old GSR wording was
therefore a secondary symptom; it was not evidence of a wrong model cast.

The minimal repair adds the existing callee initializer to those three cases
before their first binding call. All original statements/assertions and the
maxEvaluations=1 budget remain. The added model-presence assertion improves
failure diagnostics without fabricating a candidate or changing the getter.
The private shared module is byte-identical between RED and GREEN; only the
driver/test source gains the three initializer calls. Each of the three cases
and the same-source owning control now passes in its own fresh process.
For replay from the fixed tree, the bundle reconstructs a frozen RED source
from the reviewed baseline plus the diagnostic assertion; the original RED
compiler commands used the normal test-source path. Those original binaries,
commands and results retain their separate identities.

```bash
python3 evidence/r2/split_runtime.py red
python3 evidence/r2/split_runtime.py green
ctest --test-dir build/m5-make -R \
  '^ExcelXccyMonteCarloTest.Test(SameCoreInputAndOwningOutcome|UnavailablePartialsAndOwnedFailureSpillColumns|StagedCandidateModelLifetime|ValidationFlagsKeepEstimateAndRejectHeldOut)$' \
  --output-on-failure --output-junit evidence-r2.xml
```

The baseline reproduction has three exit-1 cases and one passing control;
GREEN is 4/4 isolated cases. The refreshed ordinary portable runner also
passes the affected four CTests, each in its usual separate process. The
bundle supplies exact PIC/driver/portable compile/link commands, XML/exit codes,
underlying staged report, original native logs, symbol-binding evidence and
source/dependency/configuration/object/library/caller identities. The existing
CMake compile/link recipes refresh only the changed test object and runner;
no unrelated component rebuild/install is repeated.

Only this Excel test file and this record change. All production sources,
generated wrappers, shared test bridge and public documentation remain
identical to the accepted head. Scoped GCC14/15 warning-as-error syntax,
complexity, changed-line formatting, record/documentation and diff checks
apply. Existing raw-input, C++/Python/GIL, component, statistical and performance
evidence remains subject to its recorded identity/applicability; no matrix is
rerun. The exact new-head native CI result remains a merge gate, independent
of the local GREEN. Frozen numerical failures, independent DAL-425 and parent
in-progress state remain unchanged.
