# DAL-407: public script bindings

## State and controlling inputs

The implementation and focused CI repair are ready for the coordinator's
code and documentation review. The unique PR and exact head are in the handoff.
The earlier report-scope blocker was resolved by coordinator comment
`01a162ef-c839-7407-93d9-b408c14c9f6b`; it is no longer an implementation blocker.

The original dedicated branch is `agent/dal-implementer/b164bddcd2f2`.
Implementation starts at `f50db2e93eb16823190ed18b5d73a115f0e9c48f`, then fetched
master, with an initially clean tree. Both prerequisite ancestry checks pass:
DAL-406 merge `f50db2e9...` and DAL-401 merge
`15a3b445362062e75099de3bf9b4353fa2225873`. No sibling or unmerged
DAL-408/DAL-412 changes were imported.

Inputs read: the current issue and parent authorization/coordination,
original expert review, research attachment
`01a15c20-3723-7ddd-8ef1-c0ac706cdc16` sections 6 and 9–10, the
[DAL-398 contract](../api-notes/dal-398-script-contract.md), current
[methodology](../../../docs/methodology/script_calibration_contract.md),
DAL-399/400/401/404/405/406 controls, implementer/tester contracts, style,
test, component-boundary, benchmark and publication references, and nearby
source/tests. The historical research SHA is not the implementation base.

## Design and changed behavior

The original final `CalibrationReport_` now owns a domain-neutral typed
snapshot. The existing partial-report constructor, Stage/Axes/Errors,
schema, failure handle and archive rejection remain. The new constructor
copies nested containers and validates finite numbers and matrix axes.
Snapshots contain immutable instrument handles and values, never a borrowed
result/spec/tape, global pointer map or result/report ownership cycle.

Curve Evaluate, success and failure publication copy the real existing
result. The two existing final-value/Jacobian refresh sites forward their
already-produced report so the final snapshot retains actual per-plan
identities. There are no additional pricing/Jacobian calls or solver changes.
Solver and diagnostic budget accounting is unchanged. The summary copies
the solver counters; direct Evaluate has not run the solver.
A common compiler/context identity is copied into the summary when all
published plans agree. Distinct contexts retain their per-plan identities.

The public facade exposes the existing definition, bindings, serialization,
quotes, observations and preparation types, plus typed native import,
report JSON/tables, curve calibration and explicit MC request assembly.
Native Deposit/FRA/IRS/period-forward OIS/Basis and Gaussian rate-option
exports use their already-merged core implementations. Importers copy terms
once and retain no native fallback. FRA payment is explicit.

Python adds immutable handles, keyword-only spec/settings constructors,
independent container getters and typed factories. Existing typed currency,
collateral, tenor and day-basis conventions remain accepted; new textual
enum tokens are exact. Conversion and ownership finish before releasing
the GIL. CalibrationError owns its code/diagnostic/report/domain candidate
after unwind; type errors remain TypeError. Legacy APIs and exception
translations remain unchanged.

Excel adds immutable typed settings/observation/outcome wrappers,
field/value and named-column readers, matching markup names and generated
registration. Readers reject unknown/duplicate keys or columns, wrong
handles, bool-as-number, nonfinite values and invalid table shapes.
The native wrappers deliberately use Cell conversions, which preserve
numeric versus boolean types; their raw validation hooks do not rely on
a prior coercion to number. Input tables are consumed during construction,
not retained. Execution catches only CalibrationError into an owned Failed
outcome; wrong-kind/absent candidate and unavailable matrix getters fail.

Report JSON includes all frozen schema groups. Empty tables keep headers;
missing metrics/matrices remain null/blank. Raw dMeasure/dNativeParameter
and the distinct-quote map are separate. Quote risk stays unavailable.
Rank/condition/objective or execution metadata not published by the existing
adapter are not recomputed or fabricated. Definition/instrument archives
continue through the existing validators; session values reject persistence.

MC uses the same bound instrument, frozen valuation and matched Gaussian
model. None/AAD are explicit; AAD labels and values come from the runtime's
native parameter axis. MRG32 seed, precise-normal mapping, path offsets,
batch uncertainty and admission/derivative refusals are inherited.
Standalone repricing executes only B batches of P requested paths. To satisfy
the merged core bridge, it reserves a larger unused validation range of
B*(P+1), with total B*(2P+1)<=INT_MAX. It neither executes that range nor
claims numerical-validation acceptance. Nonzero smoothing, BB, other
precision modes, unsupported derivatives/models and new deterministic
curve families' MC requests remain explicit refusals.

No pricing/compiler/preparation/model/MC algorithms or existing core
examples changed. The core extension stays within the coordinator's
approved protocol, curve publication and report-test paths.

## RED, GREEN and refactor evidence

The evidence attachment retains the actual logs, including failed
infrastructure/probe attempts, rather than relabeling them as product passes.

- The first lifecycle test's syntax-only RED reports missing report Data().
  GREEN retains only the report after destroying result/spec/plan and checks
  actual values, residuals, AAD/backend, native axis/Jacobian and counters.
- A runtime RED shows nonfinite objective data was accepted. Finite snapshot
  validation makes the same test GREEN; simulation nonfinite coverage,
  copy independence and partial/archive behavior also pass.
- A late stronger assertion exposes omitted summary compiler/context IDs.
  Copying the already-owned common identities makes that exact assertion GREEN.
- Public/Excel compile REDs identify the missing facade/header; the public
  curve/JSON and strict Excel table tests then pass.
- Python runtime RED identifies the missing typed API. Subsequent tests cover
  keyword-only/type rules, owned errors, bind/rebind, duplicate/unknown/fixed
  keys, wrong handles, archive identity and read-only/copy properties.
- The initial standalone MC test fails on the core's larger-validation
  reservation requirement. Correct request mapping makes the deterministic
  cashflow oracle and explicit K rebind test pass.
- A Python MC-error RED loses instrument context after unwind. The public
  boundary now owns the original diagnostic plus instrument/measure row;
  the same test passes without an invented model value.
- FRA import runtime RED identifies the missing factory. The merged native
  export adapter and wrong-native refusal then pass.
- An existing typed-convention RED fails at collateral input. Accepting the
  already-registered exact C++ type, before textual parsing, makes it pass.

Refactoring extracts shared typed readers, field/property conversion,
snapshot validation and report serializers. No assertion or numerical
tolerance was weakened. Existing tests were preserved; fixture-only build
issues were corrected before their relevant behavioral checks.

Representative actual commands:

```sh
g++ -std=c++17 -I dal-cpp -I dal-cpp/externals/googletest/googletest/include -fsyntax-only dal-cpp/tests/curve/test_script_curve_calibration.cpp
cmake --build build/Release-linux --target dal_cpp_tests dal_public_tests dal_excel_portable_tests _dal script_calibration -j 10
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptCurveCalibrationTest.*:CalibrationReportTest.*:ScriptInstrumentTest.*:CalibrationProtocolTest.*'
./build/Release-linux/dal-public/dal_public_tests --gtest_filter='PublicCalibrationInstrumentTest.*:ScriptArchiveTest.*:ScriptTest.*:CurveSpecTest.*:PublicGSREuropeanTest.*'
./build/Release-linux/dal-excel/dal_excel_portable_tests --gtest_filter='ExcelCalibrationInstrumentTest.*:ScriptExcelContractTest.*:ExcelGSREuropeanTest.*:ExcelCurvePricingTest.TestBatchSpillLongFormContract'
PYTHONPATH=build/Release-linux/dal-python python3 -m pytest dal-python/tests/test_calibration_instrument.py dal-python/tests/test_curve_calibration.py dal-python/tests/test_curve_instrument.py dal-python/tests/test_gsr_european.py dal-python/tests/test_script.py dal-python/tests/test_script_settings.py -q
```

The directed cohort is 80 core, 30 public C++, 34 portable Excel and 322
Python checks (315 legacy Python plus seven new tests). After the final
identity-only curve publication correction, 32 core, two public, four Excel
and the new Python cases were rerun. The final typed-convention correction
reruns all seven new Python cases. The remaining compatibility evidence is
reused only for unchanged source/dependency/configuration paths. This is
not a claim that the entire repository test matrix ran.

GCC 14.3/15.2 warning checks cover the eight changed production translation
units with CI's existing -Wall/-Wextra/-Wpedantic/-Werror exclusions.
The Python checks use their actual hidden-visibility configuration.
The first standalone warning probe omitted that flag and failed on pybind
visibility; fixing the probe configuration added no warning suppression
or product workaround. Only the final changed adapter/Python checks are
repeated after their respective repairs. Lizard CCN<=8 and formatting pass.

## Component, install and generation evidence

The workspace Release build uses GCC 15.2, C++17, native AAD, native-arch off,
static core/public, Python 3.13.9 and pybind11 3.0.4; Python has hidden
visibility/LTO. Pinned submodules are unchanged.

The affected core target is dal_cpp. Existing core example locations,
targets and dependencies are unchanged. Both independent core
configurations build shared dal_cpp and run a core-only client that fits
the Deposit and reads its owning report:

```sh
cmake -S . -B build/dal407-core-only-make -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release -DDAL_ENABLE_NATIVE_ARCH=OFF -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_PYTHON=OFF -DDAL_BUILD_EXCEL=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DDAL_CPP_BUILD_TESTS=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_CPP_BUILD_BENCHMARKS=OFF
cmake --build build/dal407-core-only-make --target dal_cpp -j 6
cmake -S dal-cpp -B build/dal407-standalone-core-make -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release -DDAL_ENABLE_NATIVE_ARCH=OFF -DDAL_CPP_BUILD_TESTS=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_CPP_BUILD_BENCHMARKS=OFF -DCMAKE_INSTALL_PREFIX=build/dal407-core-install
cmake --build build/dal407-standalone-core-make --target dal_cpp -j 6
cmake --install build/dal407-standalone-core-make
```

The initial Ninja configure was unavailable on this host; the successful
separate Makefile configurations above are the evidence, not that attempt.

Standalone dal-public finds only the installed core package, builds and
runs script_calibration, registers/passes its CTest entry, and installs the
new program, library and header. The first install lacked the existing
aad_public_risk executable because only the affected target was built;
building that installation prerequisite fixes the install. A separate
installed consumer finds DAL::public, whose interface links DAL::cpp.
The imported core has no public dependency, and both core-only clients
include/link core alone. No build-tree facade headers are needed by the
installed consumer.

The example prints decimal ParRate 0.035, original fixed-coupon Price:USD
-0.979142914627 and explicitly rebound Price:USD 0; quote risk is false.
Workspace CTest also passes script_calibration and dal_binding_boundary.

Machinist regenerates the 50 new Excel registrations and their HTML help.
Existing generated files do not drift. The generator check is run after
staging its exact outputs, as required by its tracked/untracked checks.
Published methodology, CHANGELOG and active-artifact index are synchronized.

## Performance scope and provenance

The actual new caller set is one/eight-node Deposit curve fits with FD/AAD
and explicit Price repricing with None/AAD. No algorithmic benchmark in the
nine-target regression allowlist changed, so no full benchmark matrix or
new regression verdict is claimed.

The attached informational collector pins one available CPU, sets OMP/BLAS
threads to one, warms each case three times and takes ten samples of
20 calls. MC uses two batches of 256 paths, seed 1024, compiled Precise MRG32.
It preserves every raw sample and actual method/counter outputs. The final
binding-dependency/extension rebuild after the typed-convention repair
justifies refreshing all six selected callers. Initial timings are retained
as pre-final information, outside the final executable cohort.
There is no pre-existing baseline for these new facade APIs, so these
measurements are latency evidence, not a relaxed +4% regression gate.

The provenance attachment records SHA-256 of actual changed source,
pinned dependencies, CMake caches/flags and freshly built libraries,
tests, extension and clients. Prior accepted financial/statistical
evidence is reused only for the unchanged core MC/lowering/solver paths;
no old executable is presented as a new run.

## Handoff limits

Writing of the public methodology, CHANGELOG and artifact index is complete.
The coordinator's reserved narrow window for DAL-412's mechanical integration
of accepted DAL-406/412 documentation is now safe to resume. That handoff
does not import unmerged DAL-412 code into this branch.

Native Windows/XLL raw OPER execution and Excel workbook execution were
not performed on this Linux host. Portable tests, generated registration
and GCC checks do not replace those results. New-head CI is handed off
without a watch/poll; it does not replace the coordinator's focused review
or merge gates. No author merge, specialist dispatch or downstream start
is performed.

## Necessary CI repair on the delivered head

The repair input is PR #621 head
`2005597f65f7540e2c75ba752bb587f351b93520`, branch
`agent/dal-implementer/b164bddcd2f2`, base master
`f50db2e93eb16823190ed18b5d73a115f0e9c48f`. The coordinator's issue update and
attachment `01a16341-9713-7ed7-b400-13517458cdf7` identify actual GCC,
Extended-build and Linux/macOS wheel failures, plus three Codacy annotations.
This is a source/build repair, rather than an external service or permission
failure. All six supplied log/annotation hashes were verified.

The two facade implementation files already use the shared canonical JSON and
calibration validation helpers. Their core-private RapidJSON include directory
was not part of the facade target's declared compilation dependencies. This
repair makes RapidJSON an explicit **PRIVATE** dependency of `dal_public`:
`find_path` prefers the pinned repository submodule, can take an explicit build
dependency path, and fails configuration when unavailable. The path is not
added to public interfaces, core dependencies or installed consumer targets.
No report serializer, validation rule, core API or JSON/SHA implementation is
copied or changed. This preserves the original validation/error behavior and
canonical report serialization while closing the actual facade build closure.

A first fresh standalone configure/build passed only because this host has
RapidJSON in `/usr/local/include`. That result is retained as ambient-dependency
evidence, not the clean RED. The controlled reproduction removes CPATH,
CPLUS_INCLUDE_PATH and C_INCLUDE_PATH, uses `-nostdinc`, and explicitly restores
the compiler's standard header directories except `/usr/local/include`. Both
original facade TUs then fail on `rapidjson/document.h`, matching CI. A separate
new build directory using the same isolated system headers and installed core
passes after the target-private dependency is declared. Its real CMake compile
commands select the pinned vendored include; no global system-header fallback
is needed.

The isolated public build also builds/runs the script-calibration example,
passes its CTest entry, builds the existing installation prerequisite, and
installs the facade. A separately configured installed consumer compiles and
runs with only installed core/facade headers, isolated standard headers and
`DAL::public -> DAL::cpp`. Neither the exported target file nor the consumer's
compile command contains a RapidJSON path. The consumer's first configure
omitted the existing harness's DAL_EXAMPLE_SOURCE variable; passing that source
path fixes the harness without changing product or example code. Both attempts
are retained.

The Codacy changes preserve behavior:

- `Fields_::Default` takes its fallback by const reference and still returns T
  by value, with the same strict reader and unknown-key tracking.
- `init_calibration_types` calls four local registration groups in the original
  order. The complete concatenated registration bodies, including constructors
  and properties, match the original body after whitespace normalization.
  All 25 type-registration blocks and their order are retained. The longest
  helper is 131 lines and the entry point is six lines, below the original
  200-line gate.
- The Excel test delegates source, parameter and measure table construction
  to local helpers. All 30 assertion statements remain byte-identical and in
  their original order. No formula, test input, failure expectation, tolerance,
  threshold or quality exclusion changes. Maximum CCN across these edited
  C++ files is seven, below the unchanged limit of eight.

Actual repair validation is **2 public / 4 portable Excel / 7 Python** checks,
all passed on the freshly linked workspace libraries/test runners/native
extension. The workspace native build, public-disabled workspace core and
standalone core builds and their owning-report clients pass. Machinist
regeneration/drift and the binding-boundary CTest pass; generated content is
unchanged. Ten GCC14/15 strict-warning checks cover the two facade TUs, the
two Python TUs affected by the helper header, and the Excel test. Formatting
and diff checks pass. Commands, controlled RED, successful builds, XML and
compiler/configuration/binary identities are in the CI-repair attachment.

The original 80 core/30 public/34 Excel, 315 legacy plus seven new Python,
and subsequent 32/2/4/7 executions keep their original source/executable
identities. They are historical evidence, not all rerun on the repair head.
Prior numerical/statistical evidence applies to unchanged core algorithms;
this repair writes no core source, preparation, MC/model or report publication
path. The original six informational latency callers construct settings before
their timed numerical loops. Those loops and algorithms are unchanged, so
their earlier timing records remain historical information; no new numerical
performance matrix or new-head latency result is claimed for this build/static
repair. The direct native-build and interface tests cover the changed boundary.

Only the facade CMake file, Python helper/type-registration files, dedicated
Excel test and this record change in the repair. No DAL-408/412 unmerged
content is imported. Shared methodology, CHANGELOG and artifact indexes remain
byte-identical to the delivered input and stay in DAL-412's writing window.
The final complete PR path list, exact repair diff (also ignoring whitespace),
new head, source/dependency/configuration and actual binary identities, and a
single exact-head CI snapshot are in the handoff manifest. Native Windows,
macOS wheels, XLL/workbook and pending cross-platform CI are not inferred from
local Linux success. Coordinator code/doc review and current-head gates remain
required before merge; the author performs no review dispatch or merge.
