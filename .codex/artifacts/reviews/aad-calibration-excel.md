# F01 common calibration Excel review

Verdict: Comment Only pending this increment's own publication-head CI.
Local correctness, actual MSVC declaration/registration compilation, generation
integrity and incremental Linux performance pass. This review accepts neither
the full F01 request integration nor the entire PR's merge readiness.

## Findings

No remaining source finding in the reviewed capture/common binding increment.
The review corrected two Windows-specific issues before publication:

- `dal-excel/tests/test_calibration_risk.cpp`: the XLL and test executable have
  separate thread-local native tape and recording state. Move test graph ownership
  into the XLL's test-only runtime helpers, keeping passive node-count callbacks
  and every nesting/recovery assertion. The actual two-module MSVC probe confirms
  unequal tape addresses in `aad-calibration-excel-dll-probe-02.log`; the first
  probe's missing project WIN32 define is retained as a compile failure.
- `dal-excel/src/__dupireinput.hpp`: use a literal identifier argument in the shared
  NUL validator and construct its string only on the error path, avoiding a new
  allocation for successful old Dupire text inputs. Existing error tokens remain.

Read the capture overloads, dispatcher, common header/implementation, raw-cell
validators, generated inc/HTML pairs, tests, source native APIs, controlling
spec/API/critique and numerical acceptance. Four const-value handle types use
one template; factories compute/check before replacing outputs. Detached seed
and contribution getters preserve ownership; typed source projections retain
complete identity. Metadata is a passive projection without record parsing,
history/worker work or full-record spill. All new common types reject archives.

Old six-argument typed export bodies remain. Seven-argument worksheet paths
add strict Boolean/blank capture, retaining old argument prefixes. The shared
legacy dispatcher preserves cast order and generic/staged exclusions. Native
headers precede Windows SDK macros, with markup protected from format reflow.
Seed dimensions are checked before range conversion/allocation; integer cells
normalize in a local copy. Bool/text/error/blank/nonfinite cells reject with
one-based worksheet coordinates; native matrix diagnostics keep their existing
zero-based coordinates. NUL names/selectors reject before result replacement.

## Open questions

None requiring user input. Windows runtime and all platform checks must pass
for the published Excel head. F01 request/budget/market integration and P01
production performance remain separate open work. The user authorizes whole-PR
fixes and merge after full F01 acceptance, then subsequent work in new PRs.

## Tests

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.
All names below have prefix `aad-calibration-excel-`.

- Capture/common missing-API RED logs and actual GREEN runs are retained.
  `capture-green-01.log` ran zero tests and is not acceptance; later actual
  capture/common runs verify cases. Keep fixture build errors, SDK macro-order
  failure and the error-coordinate expectation failure without weakening checks.
- Thirteen portable common tests plus three Windows raw-input tests cover owning
  Dupire/captured-curve sources, detached/getter projections, null/wrong/uncaptured
  sources, complete case-sensitive identity, direct quote-only Dupire identity,
  zero/extreme seeds, finite-output overflow, partial-result rejection/recovery,
  serialization, NUL text, strict cells/shape and XLL-local recording preservation.
  Actual independently priced signed joint portfolios cover both modes, all four
  representations and plain/layered bases. Mixed actual PV currencies remain in
  separate seed groups and match old aggregation exactly. Legacy captures retain
  canonical bytes and exclusions. No original oracle tolerance or quote step changes.
- Full local nonbenchmark/nonslow CTest passes 2,428 OFF and 2,442 combined in
  `{off,combined}-full-01.log`, including 33 regular examples. These runs precede
  the final test-helper repair; final `module-{off,combined}-all-01.log` reruns
  all 99 portable cases per mode after the repair. No native production code changes.
- Final leak-enabled ASan/UBSan passes all 41 selected cases in
  `module-sanitized-01.log`. Actual MSVC OFF/combined syntax compiles all six
  production/test/support units, 12/12, in `msvc-module-01.json`, including
  generated registrations and DLL imports. This is not Windows runtime proof.
- Stage only all 36 intended generated files; `dal_check_generated` passes in
  `generated-check-01.log`. CCN of 65 reviewed new/changed functions is at most
  seven in `ccn-03.json`; new files are clang-format idempotent.
- The [performance report](../performance/aad-calibration-excel.md) records all
  48 accepted old/default rows, 32 new-entry costs, 40 processes, 640 numeric
  checks and 1,477 unchanged inputs. Both native archives and all nine accepted
  gate executables remain byte-identical; final measured objects also match
  after the Windows/test-helper correction.

## Summary

The Excel common boundary preserves native math, scaling, currency semantics
and lifecycle. Publication-head Windows runtime/ARM/wheel/quality acceptance
remains required. Earlier Python or C++ green heads cannot satisfy this gate.
