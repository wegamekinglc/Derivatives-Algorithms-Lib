# F01 Excel Dupire boundary review

Verdict: Comment Only. No unresolved local correctness or API findings.
Exact publication-head CI, especially Windows DLL/registration compilation,
remains required. This increment does not complete common curve integration,
P01 production acceptance or the full Stage B/C/D plan.

## Findings and design

Read the shared C++ factories, model-builder refactor, Python registration,
Excel handles/entry points/markup, generated signatures, tests, installed
consumer, active API/spec/critique and current-state guides. New worksheet
factories provide both flat-BS and Merton calibration and the complete matching
Hybrid model/valuation/quote-result chain. The model takes frozen carry and
detaches its surface. The common internal builder preserves the old BS
factory's constructor sequence and surface sharing; its temporary input view
does not retain borrowed strings. Both seed factories share copied-value and
shape/finite validation. Per-row Excel constraints retain location diagnostics,
while the shared Merton factory also protects direct C++ callers.

Immutable handles retain passive values and reject archive serialization.
Required handles and typed base dispatch reject null/wrong types. Original
Excel strings are checked for NUL, and numeric integers are normalized locally
before generic conversion. Direct optional handles unwrap one-cell ranges.
Getters copy values/surfaces and do no valuation/history work. Raw mean model
gradients, reversed selected axes and reporting factors preserve the existing
typed Hybrid mapping. Direct PV quote contributions are added once.

The first generation attempt places insertion code before the first argument,
which Machinist rejects; retain `aad-dupire-excel-generate-01.log`. Move checks
after their declared argument and check the original Excel cell before dispatch.
Review also catches an optional string default producing `.value_or()` on the
plain String_ converter. A focused blank-contribution test fails, then passes
with native blank-to-total handling and regenerated markup. No converter,
generator, compiler flag, assertion precision or CI policy changes.

Windows and the statically linked XLL own distinct runtimes. Test-only exports
scope and restore actual XLL workers as well as the native oracle workers;
both use one worker and all original 257 paths. A Windows-only cell test covers
integer normalization and original embedded-NUL rejection. Linux portable
tests cannot certify its DLL imports or registration ABI.

## Tests

Raw evidence persists under
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- Shared factories first fail to compile because the public names are absent;
  `aad-dupire-excel-public-red-01.log`. The initial GREEN compile catches an
  attempted copy of noncopyable storable data; construct detached geometry and
  matrices instead. Final public factory checks pass.
- Excel missing-header/interface RED and minimum zero-seed GREEN precede full
  chain implementation. Keep both expanded RED logs; the first also catches
  use of the matrix's const iterator for mutation, corrected with indexed writes.
  All nine final Excel cases pass; `aad-dupire-excel-focused-final-02.log`.
- Flat/Merton × tree/compiled independent legacy calibration/price oracles:
  all 84 rows pass unchanged steps 2e-4/1e-4/5e-5, abs/rel 1e-3 and adjacent-step
  policy. The shared-builder refactor preserves every numeric trace bitwise.
- Strict settings, negative/zero seeds, direct separation, copied getters,
  mismatched quote content, null/wrong-type errors, output preservation,
  serialization rejection and success/failure/success recovery pass.
- Full OFF/combined: 2,394/2,408 passes;
  `aad-dupire-excel-{off,combined}-full-final-02.log`. OFF includes 857 Python
  cases and 33 regular examples. The unchanged slow BS example retains its
  accepted earlier result and is excluded with existing benchmark labels.
- First OFF full run catches a new test using the absent ModelData_ `.name`
  property. Correct it to the existing native type contract; preserve the
  failure log. Complete Python rerun passes 857. Four one-worker parity cases
  compare the actual old/new factories' PV, complete Jacobian and quote risks
  bitwise. No numeric assertion or oracle tolerance is relaxed.
- Fully instrumented ASan/UBSan with lifetime/profiling: 71 relevant cases pass;
  `aad-dupire-excel-sanitized-final-01.log`, leak detection disabled.
- Old installed headers fail the new consumer as expected. Final OFF/combined
  prefixes each pass both installed consumers, including both shared factory
  exports. Installed joint and freshly built standalone Python each pass 64
  quote/scalar-risk cases. Preserve the first installed-Python probe's wrong
  package-root failure, then use the actual installed root.
- Formatting, production CCN-eight and documentation checks pass; 82 production
  functions have no complexity warnings. All 23 new registration/HTML pairs are generated
  with their markup; final generation/drift checks pass.

## Publication correction

Exact Excel publication `ad9681d20bcdf4c6cdb4efc015437ec52be3c230`
receives a Codacy `action_required`: test helper `CheckDirection` has CCN 12,
above the unchanged limit of eight. The earlier local complexity scan covers
production functions and misses this test helper. Retain the exact check and
annotation in `aad-dupire-excel-codacy-{01,annotations-01}.json` and its local
RED reproduction, `aad-dupire-excel-test-complexity-red-01.log`.

Separate quote-direction construction, row-major directional accumulation and
direction scaling into small test helpers. Every original step, tolerance,
path count, assertion and CSV field remains. The complete corrected test file
passes CCN-eight: 29 functions, no warnings. OFF and combined each pass all nine
Excel cases; both retain every one of the 84 published numeric oracle rows
bitwise (`aad-dupire-excel-codacy-oracle-identity-01.json`). Fully instrumented
ASan/UBSan passes all 71 relevant cases again, with leak detection disabled.
Production sources, generated registration, performance inputs and installed
packages do not change in this test correction. Exact `50d286af` Codacy succeeds;
the complete increment remains unaccepted because its four MSVC jobs fail.

## Windows SDK compilation correction

Both `ad9681d2` and `50d286af` fail Windows compilation before tests. The new
production and test translation units include the Excel input helper before
native Dupire declarations. Its Windows SDK includes define `REGISTERING` as
`0x00` in `nb30.h`, colliding with the native recording-state enumerator.
Retain the actual OFF/combined CI logs and the local MSVC 14.51.36231,
SDK 10.0.26100.0 syntax RED for both affected translation units. Both local
compiles fail at the same generated enum line as CI.

Load native declarations before the Excel input helper in these two files.
A brief local comment preserves the ordering constraint under formatting.
The public enum, SDK macros, generated files, runtime operations and compiler
options remain unchanged. The same local MSVC now passes production and test
syntax checks in OFF and combined lifetime/profiling configurations, including
all 23 generated worksheet entry points and the Windows-only input test.
These four syntax checks do not replace DLL linking or Windows runtime tests.

Rebuilt OFF/combined each pass all nine Excel cases and preserve all 84 numeric
oracle rows bitwise. Rebuilt fully instrumented ASan/UBSan passes 71 relevant
cases again. Formatting, complete affected-file CCN-eight and documentation
checks pass. Existing performance binaries, native archive, installed packages
and measured helpers remain unchanged. The Windows corrective publication
requires fresh exact-head CI acceptance.

## Performance and limits

All nine existing OFF gate executables and the native archive remain bitwise
identical to the accepted `95f0706d` evidence. The changed shared builder also
receives a separate old-entry cost comparison against its frozen installed
headers and libraries. CPU 4, one initialized worker, two rounds of ten
interleaved processes per side, five internal 10,000-call minima and the same
4% two-round criterion are declared before measurement. All 40 process numeric
checks pass and input hashes remain unchanged. Round movements are +0.2891%
and +0.4874%; `aad-dupire-excel-factory-cost-paired-02/summary.json`.
Retain the first runner's initialization-log JSON parsing failure and protocol;
the correction parses the final numeric line and saves raw stdout without
changing workload, threshold or numeric checks.

This measures existing model construction, not total new Excel-wrapper cost or
production MC throughput. P01's prior inconclusive workload evidence remains
open. New-head CI acceptance remains separate from the already accepted
35 checks at `95f0706d`.
