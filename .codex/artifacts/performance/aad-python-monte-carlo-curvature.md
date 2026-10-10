# Segmented Monte Carlo financial boundary costs

## Selected scope, before measurement

The new public preparation/result adapter is in
`dal-public/src/montecarlocurvature.{hpp,cpp}`. Its actual new Python callers are
`dal-python/src/bindings/montecarlocurvature.cpp`; registration changes only add
that binding. No existing native algorithm, public header or first-order caller
changes. Core source/header/archive identity and unchanged public/binding object
identity support reuse of accepted old timing rather than another old matrix.
The installed script consumer also exercises the new public entries.

Select two complete prepared Python boundaries, both N=5, 35 paths and absolute
offset seven (a partial final 32-path batch):

1. Mean risk: first-order adapter, parameter conversion, owning point/settings/
   plan and detached price/gradient extraction.
2. M=3 curvature: mixed/signed directions, shared bump request, full segmented
   common-path gradients, owning result and detached products extraction.

The baseline is a minimal ctypes bridge over the accepted merged native core
and public archives. It constructs the same sealed preparation, calls the same
native mean/curvature algorithms and copies the same numerical outputs into a
caller-owned buffer. It omits Python strict conversion, financial plan retention
and the additional owning financial metadata. Both costs are informational:
different ownership/boundary contracts do not support a speedup claim or an old
caller regression verdict. The baseline bridge source is retained alongside
the self-contained Python driver.

Cold preparation/invalid admission, alternate RNG matrices, all historical and
zero-dimensional combinations are excluded from timing: correctness tests
cover those boundaries, while this change adds no corresponding native hot
algorithm. The two selected cases cover both new production evaluation entries.
No unchanged native segmentation, calibration, PDE or LSMC cases are repeated.

## Measurement contract

Only after correctness passes, use separate fresh baseline/head processes and
equivalent single-thread settings. Calibrate each loop to at least 25 ms;
collect two rounds of ten alternating interleaved process pairs per case.
Retain raw samples, loop counts, minima, environment, executable and input
identities. Do not weaken sampling to reduce the number of cases. Capture the
independent finite-path analytic value/gradient/actual-step secant checks in
both paths before and after timing.

The first two-case run retains 80 samples in 7.322207429 measured seconds.
Local review then found that by-value plan copies duplicated observation
metadata. Refactor the plan to share that immutable vector, keeping plan copies
constant-size. Rebuild affected public/binding/consumer objects and repeat only
these same two affected cases; retain the first run separately. This does not
change the native algorithms or justify adding unrelated performance cases.

## Final results

GCC 15.2.0 Release/PIC, Python 3.13.9, i9-13900HX, CPU zero affinity, one DAL
and OMP thread. Shared one-minute load ranges from 2.50 to 2.73. Each side runs
in a fresh process. Four calibrations determine fixed loop counts; all 80 final
samples exceed 25 ms (minimum 77.17 ms). The two rounds each retain ten
alternating interleaved process pairs. Values below are per-call round minima
in microseconds. Different ownership contracts keep every row informational.

| Case | Baseline R1 | Head R1 | Delta R1 | Baseline R2 | Head R2 | Delta R2 |
|------|-------------|---------|----------|-------------|---------|----------|
| Mean | 84.62       | 91.94   | +8.65%   | 87.45       | 93.36   | +6.76%   |
| M=3  | 605.21      | 622.23  | +2.81%   | 602.85      | 642.99  | +6.66%   |

Final samples total 7.151321984 measured seconds. Both sides validate independent
finite-path analytic prices, every gradient and actual-step gradient secants
before and after their timed loop. Complete final samples, calibration trials,
commands, load averages, executable and input identities are in
[the raw evidence](aad-python-monte-carlo-curvature-results.json). Initial and
final runs together retain 160 samples in 14.473529413 measured seconds; the
initial raw outputs remain in the external evidence directory.

The final installed extension hash is
`7a656aef14fdcfa94ec0cbd3fc64655d0188403702a3e1d0c6b230a721af8351`.
Accepted core archive/header/source/configuration identities remain unchanged.
All 26 old public archive members are identical. Of 20 old Python binding
objects, 16 are identical, the registration object changes, and three differ
only in public-header diagnostic path strings: code and relocation sections
are identical and normalized readonly strings match. This establishes unchanged
old computation rather than claiming omitted timing cases ran.

The native bridge has a local-symbol export map exposing only its three C
entries, isolating its native registries from the separately loaded baseline
Python module. Its source/archives/executable are identified in the raw record.
The first bridge setup failed before timing for an uninitialized/overlapping
index registry; the isolated, initialized bridge passes every recorded oracle.

During object inspection, an objcopy invocation rewrote three cached objects.
Accepted baseline objects were restored from their identical accepted prior
cache. The three head objects were rebuilt to their exact original hashes, and
the relinked installed module retained its original hash before the subsequent
intentional shared-metadata refactor. Recovery records and original/final raw
outputs remain under `python-monte-carlo-curvature` in the established external
evidence root. No accepted archive or installed baseline module changed.

Overall: new financial boundary overhead is measured and disclosed; no native
speedup or unequal-contract regression verdict is claimed. Unchanged native
timing remains applicable by identity. Remote CI waiting is separate from
measured compute time.
