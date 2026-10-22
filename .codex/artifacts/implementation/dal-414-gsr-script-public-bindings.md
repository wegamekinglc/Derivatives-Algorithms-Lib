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
quantities stay null. Report metadata admission queries real source/binding/
compiler/context capabilities without pricing or simulation;
`reportPreparations_` counts those admissions separately from driver work.
Fast selection also performs a capability probe. This adds facade metadata
overhead, not a numerical evaluation. Report composition remains in public.
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
