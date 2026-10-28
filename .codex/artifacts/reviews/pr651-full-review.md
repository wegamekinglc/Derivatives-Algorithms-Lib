# PR 651 full review and repair

Starting head: `60875c0cd69a0039602efc9d780d69ef25b74d6e`.
Base: `1c23b21a6184798aed4a559968e97d29a8267bc9`.

The review covers solver integration, scaled finite differences, residual and
failure ownership, preparation caching and immutable products, lowered curve
execution, C++ component boundaries, Excel constructors and generated wrappers,
public examples, workbook generation, documentation, and regression coverage.

## Findings repaired

- Preparation context keys used the case-insensitive DAL String_ despite the
  case-sensitive route contract. A regression with USD.OIS and usd.ois showed
  identical context identities for distinct routes. Use std::string byte keys
  and verify distinct identities with identical numerical measures.
- The new initial-fit shortcut could accept a NaN inside the residual vector:
  min/max element comparisons ignore an interior NaN. A focused regression
  reproduced the incorrect successful return. Check every residual against
  the inclusive absolute tolerance, which rejects both NaN and infinities.
- Finite-difference yield and XCCY solver Jacobians multiplied S twice. Remove
  the extra multiplication from the z-space differences. Add independent
  deposit log-discount and native XCCY basis oracles for both solver modes at
  S = 1, 0.3, 0.001, 0.0001, and 10. Correct the copied scale error in existing
  comparison helpers instead of weakening their assertions.
- Workbook handle references were text. Mark cell references explicitly and
  emit formulas when writing handle-valued tables.
- Settings ranges included unsupported field/value headers. Preserve visible
  headers while excluding them from factory arguments.
- Yield routes and knots overlapped. Separate the knot table from route inputs.
- Report formulas were implicitly intersected and their spill areas occupied.
  Use explicit array formulas with correctly sized reserved output ranges below
  the inputs. Native Excel must validate the delivered files without repairs.
- The public XCCY example's residual ran into the preceding value. Widen its
  residual column while retaining units and the numerical fixture.

## Acceptance scope

The two scaled-FD C++ regression tests first failed at the starting head, while the
original 196 focused tests passed. All four initial workbook structure tests
also failed before repair. Portable tests now check references, settings ranges,
route tenor, reserved arrays, and committed workbook/generator agreement. The
documentation CI job runs these tests plus normal/optimized Python validation.

The native Windows runner opens committed workbooks read-only in a separate
Excel process, validates every fitted observation and parameter, perturbs a
quote by 1 bp, validates updated results, restores inputs, and checks source
hashes. No diagnostic patch is applied to the workbooks.

Performance selection follows actual changed callers: three AAD public yield
schemes, AAD XCCY, and the affected FD yield/XCCY entry points at unit scale.
Small non-unit scales are numerical coverage because they failed in the
baseline and cannot support comparable baseline timing. Two rounds of ten
alternating process samples per side retain the repository harness's three
warmups, ten samples, ten inner loops, minimum reduction and 4% threshold.
These cases are informational and outside the scheduled nine-target gate.
The reused baseline core archive is the validated starting-head binary; its
hash and separately linked baseline/candidate executables establish provenance.
Unchanged tape, matrix, PDE, RNG and Monte Carlo engines are excluded.

The PR adds facade examples, so independently configure core examples with
`DAL_BUILD_PUBLIC=OFF`, then build and run their three curve targets. This adds
component-boundary evidence beyond the existing combined-workspace smoke pass.

The initial-fit repair affects the shared Find predicate. Repeat the same six
calibration cases after rebuilding the optimizer; the exact AAD and FD entry
points exercise that common predicate. No unrelated benchmark expansion is
needed for an elementwise acceptance check.

## Verification results

- **200/200** focused C++ tests pass, including four new regressions: scaled FD
  yield, scaled FD XCCY, initial NaN, and case-sensitive preparation identity.
- **306/306** Windows Excel binding tests pass against the rebuilt XLL.
- **5/5** combined-workspace curve examples pass; **3/3** core curve examples
  pass in a separate `DAL_BUILD_PUBLIC=OFF` configuration.
- **7/7** workbook contracts pass, including a ZIP/XML comparison proving that
  the committed files match the generator apart from timestamp properties.
  XML is safely parsed and normalized before hashing so lxml and the standard
  library agree on equivalent empty tags, attribute order and namespace prefixes.
  Expanded tags, sorted attributes, text/tails and ordered children are retained.
  Regression checks accept equivalent serialization and reject changed formulas.
  The suite passes with both XML backends; this repairs the documentation CI
  failure that exposed their different byte serialization.
- Python validation controls pass **3/3** normally and **3/3** with `python -O`
  and disabled site packages.
- CI classification tests pass (21, two platform-specific skips); documentation
  checker tests pass (34). Ruff, Bandit, clang-format and generated-code checks
  pass. No analysis threshold, rule, or exclusion was weakened.

Native Excel 16.0 opens the delivered files without diagnostic modifications.
Every array report has its complete header and expected rows. All observations
pass their native tolerance; maximum absolute raw residuals are:

| Scheme            | Observations | Maximum residual |
|-------------------|--------------|------------------|
| LOG_LINEAR        | 13           | 7.17758e-11      |
| LOG_CUBIC_NATURAL | 13           | 1.32857e-11      |
| MIXED             | 13           | 2.73350e-11      |
| XCCY              | 1            | 1.28023e-15      |

XCCY's fitted basis is `0.00299999999999885`. All four calibrations update
their measures and parameters after a +1 bp quote change, then return to their
original results after restoration. Source workbook hashes remain unchanged.
Header wrapping, scientific residual formats and report ranges are specified
by the generator. Hidden-COM picture export produced a blank image and then a
CopyPicture error, so no screenshot-based visual acceptance is claimed.

## Performance evidence

Windows workstation: Intel Core i9-13900HX, MSVC 19.44 Release, C++17, `/O2`,
static runtime, native AAD, native architecture disabled, `DAL_USE_EIGEN=OFF`,
`DAL_NUM_THREADS=1`, and explicit `InitGlobalData(1)` on both sides. The reused
baseline archive SHA-256 is
`92aa712201205152dae8b528fe47fa3984586fa430b9f6cc190ef6febd0f10b5`.

Final samples were collected after compiler, test and Excel processes exited.
Earlier samples overlapped validation or preceded later repairs and are retained
as intermediate evidence only. The final two-round minimum ratios are:

| Case       | Candidate / baseline |
|------------|----------------------|
| LOG_LINEAR | 1.0000               |
| LOG_CUBIC  | 1.0169               |
| MIXED      | 1.0060               |
| XCCY AAD   | 0.9975               |
| Yield FD   | 0.9802               |
| XCCY FD    | 1.0130               |

No selected case crosses the 4% threshold in both confirmation rounds.
Raw outputs and reductions: `build/pr651-review/paired-route-fix/`.
Baseline benchmark SHA-256:
`7baa9c31940a26bb7ebaa82285fedae3382a342f1ec54a968a0551ade3dbca7c`.
The result JSON records the final candidate executable hash. Core archive:
`a498246d6d5872c02844628c4e4790615dfa659becc817674deb2ed64c557245`.
XLL: `8256cf15b27cfe533f6658cf21502065db61bda38c58ad7121d77d7f4b2ebaa5`.

## Remaining risks and verdict

No additional correctness blocker was found in the reviewed scope. Full Linux,
Python binding, sanitizer and compiler-matrix acceptance remains the job of
exact-head CI; the local focused tests are not represented as a full-suite pass.
The earlier informational native-versus-script yield timings showed roughly
2x overhead, and this review does not certify native-equivalent performance.
Current repair timings compare the same scripted entry points on both sides.

**Comment Only pending complete exact-head CI.** Do not merge from the earlier
green Codacy snapshot. The superseded verification and Codacy reports remain
available in Git history; this file controls the active review.
