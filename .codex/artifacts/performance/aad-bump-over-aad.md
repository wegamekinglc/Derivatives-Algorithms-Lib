# Native bump-over-AAD scoped cost control

## Selection before measurement

The new driver is additive: ordinary tape, first-order pricing, calibration,
random/path generation and existing recorded operators are unchanged. Extend
the existing `tape_perf` target with one explicit curvature command. Select only
two whole requests: four inputs/one direction and 32 inputs/three directions.
Each uses a smooth coupled polynomial with an independent analytic Hessian,
fresh native recordings, explicit steps, owning result metadata and budgets.

Use identical complete-request benchmark harnesses on isolated frozen baseline
`faccea0b0d3a4112329436ede84cd7ec90ebf83d` and implementation builds. The baseline
reference performs the same 1+2M independent native gradients and central secants;
match result ownership, request copying and tape-capacity scope. Report this as
new-entry overhead, since the baseline has no native curvature API. One selected
existing segmented short request covers the shared benchmark dispatch boundary.
No ordinary native library hot body changes, and no unrelated target is selected.

For each selected comparison, retain two rounds of ten interleaved process
samples per side, alternate first side, reduce by minimum and apply the existing
+4% sustained threshold to the comparable old caller. New capability overhead is
reported separately. Keep full values/gradients/products and work counts in each
raw sample. Explicitly enable benchmarks in both Release CMake configurations.
Record source/dependency/archive/executable hashes, compiler, host/affinity and
thread settings. Timing is read-only after correctness passes and identities freeze.

Peak tape payload and cleanup reserve are separate; numeric payload is exactly
the documented owning doubles. Neither bounds allocator overhead, stacks or RSS.
Exclude RNG/PDE/curve/LSM, a dense Hessian matrix sweep, alternative worker counts
and unchanged heap matrices. Expand only for a concrete failed selected case.

## Frozen builds and environment

Baseline is the merge base `faccea0b0d3a4112329436ede84cd7ec90ebf83d`;
implementation is `62baa9e3f05a7772a9da70a140486ca4daa88a37`.
Evidence root is `/home/wegamekinglc/.cache/dal-aad-evidence-20261008/`;
all paths below are relative to `bump-over-aad-performance/` under that root.
Sources are isolated `base-source/` and `head-source/`; builds are independent
`base-build/` and `head-build/`. Both Release CMake caches explicitly enable
`DAL_CPP_BUILD_BENCHMARKS=ON`. GCC 15.2, CMake 4.2.3, `-O3 -DNDEBUG`, native AAD,
Eigen and `-ffp-contract=fast` match. No native-architecture flag is enabled.
The host is WSL2 on an i9-13900HX, pinned to CPU 1 with `DAL_NUM_THREADS=1`.
It is a shared host; the process snapshot shows no competing build or benchmark.

The head archive adds one object to the accepted 180-member native archive;
every old member occurrence remains byte-identical. Three unchanged benchmark
runner objects are reused with recorded hashes. New main and curvature benchmark
objects are compiled for both isolated builds. The baseline overlay contains only
the new public result types and common benchmark harness/dispatch, with its
reference-gradient implementation selected by `DAL_BUMP_OVER_AAD_BASELINE`.
It does not compile or call the new production driver. Old-caller control uses
the actual frozen pre-change binary, not the overlaid dispatch executable.
Its native implementation is identical to the merge-base implementation.

`environment.json` records source/dependency/build commands and archive/object/
executable hashes. Curvature binary SHA-256 values are
`182273a38a5699e0c33f540323d9711edf7668d11bc06401cc129a1011a2b745` (base) and
`4a644f1f739941ee34db99d9e8c739e50482e0110a58e9d35f2cee34e61ca842` (head).
Each process invocation verifies its executable hash before timing.

## Results

There are 120 process samples: two independent rounds of ten per side for each
of three selected comparisons. Sides alternate first position and minima are
reduced within each round. Sampling takes 1.844 seconds. Every new-capability
process independently checks analytic values, gradients and Hessian products;
the paired collector also checks numerical agreement and work/payload counts.

- Four inputs/one direction: round minima are 738/845 ns and 783/897 ns
  (base/head), giving +14.50% and +14.56%. Three gradient evaluations retain
  144 owning numeric bytes. This measures new-entry overhead against manual
  native secants, not regression of an existing curvature API.
- 32 inputs/three directions: 10,472/12,250 ns and 10,434/11,784 ns,
  giving +16.98% and +12.94%. Seven gradient evaluations retain 2,080 numeric
  bytes. This is also informational new-entry overhead.
- Existing short segmented MC control, 128 paths/16 steps/one worker:
  1,079,026/1,098,729 ns and 1,133,337/1,134,592 ns, giving +1.83% and +0.11%.
  Both rounds pass the calibrated +4% gate: no regression.

Both curvature shapes report 1,966,080 peak tape bytes plus 655,360 cleanup
reserve, equal on both sides. These are reusable backend block capacities;
they exclude the result's numeric payload, allocator overhead, stacks and RSS.
No general memory saving or speedup is claimed for the new API. The reference
omits complete bump admission and exponent-scaled quotient protection, so its
cost difference includes those deliberate protections.

Raw commands/stdout/stderr remain in `raw/`; parsed observations in `samples.json`,
the complete reduction in `results.json` and the build identity in `environment.json`.
The independent installed-only recorded-solve consumer passes 1/1; its build
uses only the installed `DAL::cpp` target and no source-tree include path.
Installation/build/test logs remain beside the sampling evidence.

## Verdict and coverage

Overall: **no regression** for the affected comparable old caller. New capability
overhead is disclosed separately, with no old curvature-API gate implied.
The unchanged ordinary library/tape/pricing/calibration/RNG/PDE/LSM bodies retain
accepted evidence; no unrelated target or parameter matrix is rerun. The existing
`tape_perf` target now covers the new request path directly. Financial estimator,
recalibration and policy costs require their later focused implementations.

## Allocation-recovery repair selection

The allocation-failure audit found an entry-mode leak and replaces only the new
driver's heap guard with the existing stack resetter. The shared native headers,
180 old library members and all benchmark bodies remain unchanged. The private
test-probe seam is not linked into benchmark or installed library targets.
Repeat the same two new request shapes and one linked old-caller control for the
repaired binary, retaining the original baseline executable and raw study above.
Keep two best-of-ten interleaved rounds and the +4% old-caller threshold. No other
target or parameter shape is added.

Repaired functional head is `b92b5b265e521f9f21fd2137c31b489e3580db27`.
Its evidence is in sibling `bump-over-aad-allocation-costs/`, with a detached
`head-source/`, independent `head-build/`, `environment.json`, `raw/`,
`samples.json` and `results.json`. Baseline build/source identities are reused
from the original frozen study. All five benchmark objects retain their bytes;
only the new native object is rebuilt and the head executable is relinked.
The 180 old archive members still match individually. Head executable SHA-256
is `5dd3ed90f9dc973f3821c7d3a9d81d026e81f951d13fa4f89b8db4ccea89e9c7`.
Compiler, Release flags, affinity, backend and one-thread environment match.

The same 120 observations finish in 2.180 seconds:

- Four inputs/one direction: 872/963 ns and 890/1,033 ns (base/head),
  giving +10.44%/+16.07% informational new-entry overhead.
- 32 inputs/three directions: 11,627/13,228 ns and 11,723/13,312 ns,
  giving +13.77%/+13.55% informational new-entry overhead.
- Existing short segmented MC control: 1,132,506/1,104,775 ns and
  1,147,847/1,138,188 ns, giving -2.45%/-0.84%. Both +4% rounds pass.

Analytic checks, paired numeric/work/payload checks and executable hash checks
pass. Numeric payload and tape/reserve metrics match the initial study.
The repaired installed-only recorded-solve consumer passes 1/1 in the separate
`bump-over-aad-allocation-install/` prefix. Overall: **no regression** for the
affected old caller; new capability overhead remains informational.

## Upstream integration selection

Master #516 (`97ab613c`) changes normal precision and RNG implementation. The
integration at `75d3f2b9` preserves both CHANGELOG entries; the curvature driver,
tests, private allocation seam and diagnostic coverage retain their repaired
bytes. Dependency analysis selects nineteen upstream library units. Rebuild
those in an isolated upstream source; retain the other 161 pre-upstream archive
members exactly. The implementation then adds the accepted native curvature
object, preserving all 180 upstream baseline members. Rebuild benchmark callers
against the new settings layout and use the actual upstream three-command main
for the old-caller control, with a separate manual-secant overlay for new costs.

The same three-case, 120-observation study takes 3.693 seconds. The existing
control rounds move +7.93%/-0.60%; this is not a sustained gate failure, but the
cross-round spread requires noise confirmation. Select only this existing short
control for another two interleaved best-of-ten rounds with the unchanged +4%
threshold. Retain the full first study; repeat no other target or shape.

The 40-observation control confirmation takes 1.422 seconds and moves
-0.95%/+4.95%. The two-round failure condition is absent, but the host remains
noisy. Preserve these measurements rather than claiming stable equality.

## Baseline fairness repair selection

Review finding `4230078427` identifies the manual-secant baseline's remaining
heap-allocated mode guard. Match the production stack guard and lifecycle check
in the baseline-only benchmark branch; production, tests and head timing bodies
are unchanged. Recompile that baseline object and relink only its executable.
Repeat the two affected new-capability comparisons with the original two rounds
of ten paired samples. Report their overhead against this corrected reference;
earlier overhead numbers are superseded. Retain all prior raw studies.

For the already selected noisy old short control, increase only its sample count
to twenty per side in each of two interleaved rounds. Keep workload, executables,
minimum reduction and +4% threshold unchanged. If minima remain unstable, report
the control as inconclusive. No unrelated matrix or correctness suite is added.

## Integrated acceptance

The integration baseline is `97ab613cc57a477c734aea28c44260f3a9596cf7`;
production integration is `75d3f2b924a34a432d6c92e4692ae4ff6c399fcb` and the
baseline fairness repair is `c96317b093915bffc9051954ab7ece8e1b420a28`.
`bump-over-aad-upstream/library.json` records all nineteen rebuilt upstream
units, source/object hashes and the 161 retained pre-upstream member occurrences.
Both sides use this same 180-member upstream library; head adds only the
accepted curvature object. Integrated native tests pass 12/12, the isolated
allocation test passes 1/1 and installed-only consumption passes 1/1.

`bump-over-aad-fair-reference/` retains the corrected baseline source, three
affected strict compile checks, independent Release build, `environment.json`,
`raw/`, `samples.json` and `results.json`. The baseline-only branch changes;
the head benchmark body and executable remain unchanged. Corrected baseline
executable SHA-256 is
`f8f7143711304a8ac8d1e92dc9e29673a445f3e79ec98151365e8524c6d17b43`;
head executable SHA-256 is
`42bf3b338d42b1ac535b816bddf8b262180c6b470259ef2f140655991d3fe997`.
Compiler, flags, CPU affinity and one-thread settings match the original study.

The 160 scoped observations finish in 4.910 seconds:

- Four inputs/one direction: 1,464/1,693 ns and 1,172/1,347 ns (base/head),
  giving +15.64%/+14.93% informational new-entry overhead.
- 32 inputs/three directions: 20,844/23,145 ns and 16,464/18,801 ns,
  giving +11.04%/+14.19% informational new-entry overhead.
- Existing short control, twenty samples per side per round: 2,178,220/2,051,824 ns
  and 1,892,978/1,960,052 ns, giving -5.80%/+3.54%. Both +4% rounds pass.

Every process verifies executable identity and numerical agreement. New requests
also verify analytic Hessian products, gradient work counts and payloads. Tape
peak/reserve and numeric payload metrics match the prior studies. The host remains
noisy: absolute timings and cross-round differences do not support a precise
speedup or stable equality claim. The calibrated gate reports **no regression**;
new API overhead is informational. Earlier overhead figures use the heap-guard
reference and are superseded by these corrected comparisons. No further local
full suite or performance matrix is required by this benchmark-only repair.
