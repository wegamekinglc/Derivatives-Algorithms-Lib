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
initialization; both failed logs are retained. The final test-only native
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
