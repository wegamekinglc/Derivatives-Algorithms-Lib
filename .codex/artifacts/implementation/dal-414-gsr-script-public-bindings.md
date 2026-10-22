# DAL-414: GSR instrument public bindings

## Control and scope

The unique branch is `agent/dal-implementer/170e83586ef4`, PR #623, base master.
Merged prerequisites are DAL-407/PR621 `acadd8ee44fdb6a0bb2e70de9d559db59de066f8`
and DAL-413/PR622 `bf4cabea8575206375047e51993a5aa5aeb54c0c`.
DAL-398 sections 6–9 and the current DAL-414 description control this work.
The coordinator accepted the minimal domain settings decision at 2026-10-22
18:46 UTC, resolving the earlier batching/held-out gap at input head
`aab4016c5e5138e95a8e478e277af82677c12292`. That head's owning core report
foundation and accepted RED/GREEN evidence remain unchanged.

The new four-required-argument public facade delegates to existing
`CalibrateGaussianBondOptions` or `CalibrateGSRScriptPrices`. AutoFast probes
existing fast admission first; only explicit opt-in and complete MC settings
permit fallback through the existing MC AST/model guard. RequireFast never
falls back. Fast calibration supports BondOption; Caplet/Swaption fast pricing
does not imply a fast calibration driver. MC Auto uses AAD, fast Auto uses
native FD, and explicit MC FD retains value-only per-view admission while
reporting the basket's actual FD method.

All three languages invoke the domain semantic validator. The accepted
settings contain copied native solver, required frozen valuation, evaluation,
three optional shared batch sampling values, held-out observations, explicit
path budget, basket limit, sigma/floors and compiled. Sampling uses exact
PseudoRandom/MRG32/Precise version, common seed, checked F/V/H reserves, V>F,
and int32 budget bounds. Supplied aggregate sampling must match fit
paths/seed/generator. Orphan held-out, unsupported derivatives, irrelevant
nondefault controls and nondefault MC quadrature fail explicitly. No shared
wire type, report column, archive, RNG or numerical algorithm was added.

Results own model/report, stable axes, raw native-g Jacobian, independent
fit/validation/held-out values and flags. Failed reports retain stage/code/field,
source/row context, work counts and last complete candidate. Unavailable
quantities stay null. Report publication copies real source/definition/binding
and final driver plan identities, plus MC context identities from final fit
results. Rule/compiler identities and fast context identities are unavailable
because the drivers do not expose them. No report-only admission, pricing or
simulation occurs; `reportPreparations_` remains zero. Fast selection still
performs its existing financial capability probe, which can price native rate
options; driver counters cover driver work, not that route-selection probe.
Report composition remains in public.
The earlier authorized core change only publishes owning summaries and saves
completed finite summaries across failure.

Python has keyword-only domain settings and strict selected-g factory, complete
owning conversion before GIL release, copied getters and GSR candidates on
CalibrationError. Old positional parameters and native defaults remain.
The new Jacobian getter raises DerivativeUnavailable when the native driver
has no matrix. Excel adds typed settings and solver/pricing wrappers, strict
factor/knot/lower/upper rows, owning Succeeded/Candidate/Failed outcomes,
model getter and five generated registrations/help files. Invalid factories
remain worksheet errors; valid execution failures retain owning reports.

The separate public `gsr_script_calibration` example fits and prices one
binding with plain/AAD MC, then explicitly rebinds K with unit BondPrice.
Core example directories, targets, tests and install names remain unchanged.
The existing methodology guide/CHANGELOG and artifact index are updated.
Existing docs/CLAUDE guide links remain valid; no new guide index is needed.

## Adapter RED/GREEN and compatibility

`dal414-complete-evidence.zip` contains commands/configuration, source hashes,
logs, XML and caller rows. Exploratory failures remain separate.

- API surface RED: new C++ test failed for absent settings/facade, Python
  failed for the absent constructor, Excel compilation for the absent header.
- Behavioral RED: fast admission lost observation/quote ownership; Python
  NaN field lacked `/settings`; inherited Python Jacobian returned an empty
  matrix; MC FD report admission incorrectly requested a direct per-view FD.
  Each failed before its adapter fix. The example's initial Dimensionless K
  rebind also failed, then passed with source-declared BondPrice.
  Final review added RED/GREEN for validation differences relative to fit
  and the exact numerical budget, plus duplicate Excel fields retaining
  their actual second-row/first-column location. Nonfinite derived report
  indicators/differences remain unavailable instead of masking a candidate.
- Actual targets built with GCC 15.2, CMake 4.2.3, native AADET/Eigen,
  native-arch off, CPython 3.13:
  `cmake --build build/Release-linux --target dal_public_tests dal_excel_portable_tests _dal gsr_script_calibration -j 6`.
- `dal_public_tests --gtest_filter='PublicGsrInstrumentTest.*:PublicCalibrationInstrumentTest.*'`:
  **11/11**, nine new and two shared. Fast/native and MC/native parameters,
  prices, raw Jacobian and work counts match. Includes held-out, aggregate/
  seed/overflow/budget conflicts, FD, general Price fallback and owned errors.
- `PYTHONPATH=build/Release-linux/dal-python python3 -m pytest dal-python/tests/test_gsr_instrument.py dal-python/tests/test_calibration_instrument.py -q`:
  **32/32**, twenty new and twelve shared. Includes strict types/values,
  nonfinite/int32 diagnostics, copied settings, destroyed-input candidate/
  report/held-out lifetime, unavailable matrix and actual GIL release. Both
  GIL tests use synchronization and a held-GIL negative control, never retry
  or load-based scheduling assumptions.
  A final current-source rerun passes the twenty new cases; three existing
  `test_gsr_european.py` cases also pass, including the old positional
  Calibrate_GSRVolatility call, option parity and legacy error behavior.
- `dal_excel_portable_tests --gtest_filter='ExcelGsrInstrumentTest.*:ExcelCalibrationInstrumentTest.*'`:
  **8/8**, four new and four shared. Includes normalized duplicate keys,
  wrong nested handles, boolean/fraction/text/blank indices, exact public
  report parity, candidate/failure/model/error spills and unavailable getter.
- Changed domains pass GCC 14/15 syntax checks with the repository's actual
  warning-as-error policy and existing exclusions. The initial local Lizard
  limit 10 was incorrect: the actual Codacy gate is CCN <=8, and rejected
  two functions at the complete-delivery head. The correction and current
  results are recorded below. Markdown/link/diff and Machinist drift checks
  were scoped to the complete delivery.
  The first generated check correctly rejected not-yet-staged new files;
  they are staged with their controlling markup before the final check.

The original **25/25** core report-foundation tests (twenty existing, five new)
are reused. Their real REDs were loss of work counts on budget failure and
loss of completed fit summary during held-out failure. WorkSnapshot retains
finite zeros, omits unavailable/nonfinite summaries and does not change solver
or tape work. The original four GCC 14/15 policy checks pass; separate stricter
exploratory failures in old headers are not claimed as passes. No core source
changed after the foundation head; frozen source identity checks are included.
Full suites/statistical matrices were not rerun.

## Independent component closure

- Public-disabled workspace, Python/Excel off, core examples on: both
  `gsr_swap_swaption` and `gsr_slv_calibration` build; CTest **2/2** passes.
- Independent `cmake -S dal-cpp`: both targets build and executables run.
  Existing top-level mode with core tests off creates no CTest registration;
  its no-tests output is retained and not counted as a pass. Workspace
  registration above verifies unchanged names.
- Independent dal-public against the scoped installed core package builds
  all three public examples; CTest **3/3** passes. Public install includes
  existing names plus the separate GSR example.
  Workspace public example CTest also passes 2/2 for the new and existing
  script examples; the component binding boundary test passes 1/1.
- A fresh installed-package consumer links only DAL::public and executes
  fast/MC calls, report/Jacobian getters and parity: CTest **1/1** passes.
- Installed GSR example runs with explicit paths for the temporary library
  prefixes. Without them, this machine loads unrelated old
  `/usr/local/lib/libdal_cpp.so.1.0.0` (Adept) and fails a missing native symbol;
  ldd diagnosis and the corrected prefix run are preserved. No system library
  or RPATH policy changed. Broad core install encountered unbuilt unrelated
  examples; scoped top-level core package install supplies the tested closure.

These are Linux and portable Excel results, not native Windows/XLL/workbook.
The unique PR/branch is retained. No unmerged 408/418/419 product, specialist
dispatch, merge or downstream start is included; coordinator owns complete-head
review and CI gates.

## Performance and accepted numerical evidence

The installed consumer provides minimal actual caller timing: one BondOption/
g coordinate, fast and MC AAD, core/public, one DAL thread on CPU 4, twenty
warmups and ten measured calls per case. All **40** rows have identical price,
raw partial, driver work/path counts and final plan identity within each pair.
Median microseconds: fast core/public **1045.451/1197.9325**; MC core/public
**1862.927/2190.438**. This measures metadata overhead and is informational,
not a performance gate or optimization claim.

The prior report-foundation before/after caller evidence remains valid for
unchanged core: forty FD/AAD success/budget-failure rows matched price,
partials, identities, counters and candidate presence. Median microseconds
were FD success 1916.6205/1981.8915, FD failure 424.382/436.9285, AAD success
1693.4095/1761.4440 and AAD failure 580.8205/589.6605, also informational.

Accepted P0/P3 evidence remains in `dal-404-p0-same-source-validation.md` and
`dal-413-gsr-script-numerical-validation.md`, not newly executed numerical
results. N100 StepStalled with converged=false remains separate from
fit/numerics/held-out; N200 0.0318712375 > .025 and old Swaption
0.0426379743 > .025 remain refusals. MRG32 seed1024/Precise, 589824 reserved
paths/view and frozen scales/floors/FD steps are unchanged.

## Complete-delivery CCN gate repair

Input head `bb5e517d26430b1a744cac046d231f5d96e7bba5` failed Codacy check
`120049463882`: `Fast` at line 100 and
`ValidateGSRInstrumentCalibrationSettings` at line 372 each had CCN 9,
against the actual limit 8. The earlier local limit-10 result did not
establish this gate. The old report-foundation CI result does not cover
the complete-delivery head.

Only `dal-public/src/gsrinstrument.cpp` and this record change. Private
`AllowMonteCarloFallback` extracts the same short-circuit predicate in
the same evaluation order. `DerivativeSettings` performs the same check
at its original position, after sampling checks and before backend checks.
No diagnostic strings, route/admission rules, settings, budgets, driver
work, reports or numerical algorithms change; original rethrows and
owning failure/candidate publication remain in place. Tests and thresholds
are unchanged.

Actual repair verification, retained in `dal414-ccn8-repair-evidence.zip`:

- RED: `lizard -C 8 dal-public/src/gsrinstrument.cpp` exits 1 and reports
  exactly the two CCN-9 functions above, matching the actual Codacy
  annotations saved with the evidence.
- GREEN: the same command exits 0. `Fast` is 6, the public settings
  validator is 7, and the new helpers are 4 and 3; the entire file's
  maximum is 8. Formatting uses
  `clang-format --dry-run --Werror dal-public/src/gsrinstrument.cpp`.
- Both GCC 14 and 15 compile the changed translation unit with
  `-fsyntax-only` and the repository warning-as-error policy, including
  the same existing exclusions as the original delivery; both exit 0.
  Exact argument vectors and outputs are saved, without policy changes.
- Rebuild: `cmake --build build/Release-linux --target dal_public_tests -j 4`.
  The actual rebuilt executable passes **11/11** with
  `--gtest_filter='PublicGsrInstrumentTest.*:PublicCalibrationInstrumentTest.*'`.
  These existing cases cover AutoFast fallback/RequireFast, MC FD/AAD,
  invalid settings/methods, budgets, owned row/quote diagnostics,
  report/candidate lifetimes and native price/partial/work parity.
- `git diff --check` passes. Evidence records the input head, exact patch,
  compiler commands, all function metrics and SHA-256 of the changed source,
  build configuration and actual test executable.

The previous Python/Excel, core-foundation, installed consumer/component
closure and frozen numerical/performance evidence is reused only for its
unchanged scope; none is a new execution or a new-head CI pass. No binding,
header, markup, generated source, public guide or build dependency changes,
so generation, Python/Excel suites and statistical/performance matrices
were not rerun for these two private extractions. The final handoff records
one actual new-head CI snapshot; full-head review and CI acceptance remain
with the coordinator.

## Unified first-review blocker repair

Input head `95435fd8e899184f04dcb3f6e9c75db7c5bdef08`; the coordinator's
2026-10-22 20:12 UTC instruction limits this batch to the two Excel factory
locations and report-only admission/work/documentation. Other accepted review,
binding, component and frozen numerical evidence is reused for its unchanged
scope. Six files change: the public and Excel GSR implementation/test pairs,
the report paragraph in the existing methodology guide, and this record.

Excel now maps semantic paths relative to each factory's actual field domain:
the solver uses `/settings/solver/`, while the instrument-settings factory
retains `/settings/`. Pricing's explicit quadrature check records its own
field after all rows are read. Validation order, strict cell types, unknown
and normalized duplicate keys, exceptions and spill columns are unchanged.
The original reviewer probe is preserved byte-for-byte; only its build paths
and output paths were adapted. Before the fix it exits **2**, with both errors
incorrectly at row 1. The two new repository tests also fail on those exact
row/column/field assertions. After the fix the same probe exits **0**:
`max_iterations` and `quadrature_order` each identify their actual row 2,
column 2 and factory function. Existing Excel assertions are retained.

The original claim that ReportAdmission never priced was false for rate
options: it called Prepare → RequireDomain → NativePrice solely to fill
metadata. `reportPreparations_` did not count that numerical work. The entire
report-only Prepare call is now removed. Report rows copy final plan identities
from the existing fast result or MC fit results, and MC context identities from
those final fit results. Source/definition/binding identities come from the
owned instrument. There is no report-time matcher, financial admission, model
evaluation or new compiler/context fingerprint calculation.

Metadata availability is deliberately truthful, using DAL-398 §9's existing
rule that all unavailable report values are null. Neither driver returns its
rule/compiler metadata, and the fast driver does not return final context
identity, so those fields remain unavailable. The MC FD/general-Price tests
now check unavailable rule metadata and exact final plan/context identity
instead of an identity regenerated by another admission. All numerical,
axis, error and lifecycle assertions remain. No shared type/schema, public
signature, core/driver/RNG, admission rule or numerical threshold changes;
no new shared authorization is needed. The guide states the same availability
and distinguishes the unchanged initial financial route-selection probe from
report publication and from driver counters.

Actual RED/GREEN evidence is in `dal414-review-blockers-evidence.zip`:

- The new Caplet/Swaption MC report test first fails because report preparation
  is **1**. After removal it is **0**, while price/raw partial and actual native
  preparation, refresh, basket/Jacobian, batch and path counts match the core.
  Owned report/quote axes and final plan/context/source/binding identities are
  checked after local inputs are destroyed; JSON explicitly checks unavailable
  rule/compiler metadata as null. Fast report identity/unavailable fields and
  zero report preparations are also checked.
- A separate native-call probe links the actual public/core static archives
  using GNU ld wrapping of the multi-factor `PriceGSREuropeanOption` symbol.
  It counts real native pricing calls, not the public preparation field.
  Before the fix, both Caplet and Swaption show **core 8 / public 9**;
  after the fix both show **8 / 8**. Each pair retains **2** driver preparations
  and **224** paths; report-only preparations change **1 → 0**. Exit status
  changes **2 → 0** on actual call-count parity. This is a small work probe,
  not a new statistical or timing acceptance result.
- Rebuild `dal_public_tests` and `dal_excel_portable_tests` in
  `build/Release-linux`. Focused public GSR/shared cases pass **12/12**
  (existing 11 plus the rate-option regression); portable Excel GSR/shared
  cases pass **10/10** (existing 8 plus the two factory regressions).
- GCC 14/15 repository warning-as-error policy checks pass for all four
  changed C++ translation units (**8/8**). Actual `lizard -C 8`, clang-format
  dry-run and diff checks pass; the production maximum remains **8**.
  The repository documentation checker passes **91 Markdown files**.

The earlier 40 caller timings describe the prior implementation's overhead;
they are historical informational results, not current timing measurements.
Their numerical/core-work/final-plan parity evidence remains applicable,
supplemented by the direct checks above. No Python/Excel full suite, generated
file pass, component/install rerun or numerical/performance matrix is claimed
for this batch. Machinist markup is unchanged; source/configuration/binary
identities, complete patch, exact commands and old/new probe outputs accompany
the evidence. A single new-head CI snapshot and final head are in the handoff.

## Native Windows test-runtime repair

Input head `020fc506a9153555619378cb584e8bb4d8e62688` has accepted code and
documentation review. The actual Windows diagnostics job `120071058825`
(run `39849151878`) compiles successfully but exits 8 in CTest: two of
3971 tests fail at the original fast Succeeded and MC non-Failed assertions.
That log does not print the underlying report, so it alone does not identify
the cause. The original job log/annotations are retained in the evidence.

The Windows test executable and statically linked XLL have separate DAL
runtime state. This new fixture initialized only the executable. Existing
GSR SLV and calibration-risk tests also call the exported
`Excel::ScriptTestInitialize` bridge to initialize the module that executes
the wrapper. The GSR fixture now uses that same bridge. The two original
status comparisons remain unchanged; their failure messages additionally
dump the owning report so future failures expose code/field/cause directly.
No product code, handles, parser, registry policy, CMake/test registration,
workflow, financial setting, threshold or report behavior changes.

A controlled Linux module-boundary probe establishes the runtime cause
without assuming that portable tests exercise the Windows boundary:

- Compile the actual unchanged GSR wrapper and existing runtime bridge into
  a separate ELF module with the actual public/core static archives;
  `-Bsymbolic`/`--exclude-libs,ALL` preserve its independent DAL state.
  The host links its own DAL state and uses the existing store/fetch bridge,
  typed handles and wrapper calls; there is no substitute repository.
- Initializing only the host returns owning Failed outcomes for both fast
  and MC. Their real diagnostic is `InvalidScriptInstrumentDefinition`,
  Prepare, field `/source/rows/2/text`, source row 3, statement 0,
  event `2027-10-21`, observation `obs`, quote `quote`. The preserved cause
  is `UnknownIndex: no parser for 'IR[USD,DF,2028-10-21]'`.
- Initializing the same module through the existing bridge, with the same
  product archive/module bytes and inputs, yields fast Succeeded and MC
  Candidate. Typed settings/model/outcome handles already cross the boundary
  correctly; the missing module-local index-parser registration causes the
  earlier failures.
- Run the original two repository tests separately in fresh host processes,
  reproducing CTest isolation. Before the fixture fix each exits 1 at its
  original assertion and prints the exact diagnostic above. After the fix
  both exit 0; all six GSR tests also pass together. The same module/library
  identities are recorded for RED and GREEN.

These module-boundary results are Linux evidence, not native Windows/XLL
passes. The actual new-head Windows execution remains a CI gate. Local
`dal_excel_portable_tests` GSR/shared checks pass **10/10** after rebuilding;
the changed test TU passes both GCC14/15 repository warning policies,
CCN<=8, clang-format and diff checks. No extra product test is added merely
to mirror initialization: the two original failing assertions supply the
regression. Other accepted code/doc, numerical, GIL, component and performance
evidence is reused only for unchanged scope; no matrix is rerun.

Only this record and `dal-excel/tests/test_gsr_instrument.cpp` change.
`dal414-native-runtime-repair-evidence.zip` retains exact commands, raw owning
reports, the original Windows failure, isolated-module RED/GREEN, portable
results, source/configuration/library/binary identities and the minimal diff.
The handoff records the final head and one new-head CI snapshot; prior P2
approvals remain applicable and native acceptance is not inferred from Linux.
