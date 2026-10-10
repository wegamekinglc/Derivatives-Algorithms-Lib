# Python rate-curvature boundary costs

Status: local measurement complete; publication gates remain open.

## Scope

| Changed path | Selected complete boundary | Reason |
|---|---|---|
| `dal-python/src/bindings/ratecurvature.cpp` | N=2, M=0, create snapshot, replay, financial base | Covers both calibration factories, quote conversion, base-only shape and result ownership |
| Same binding | N=2, M=3, reuse snapshot, signed/mixed products | Covers copied trades/settings/bump request and repeated financial execution |
| `module.cpp`, `bindings.h`, CMake source registration | The same installed entry points and affected existing Python tests | Registration changes; inspect old object identity before reusing cost evidence |

Native algorithms, native headers and static libraries are unchanged. Four-family
dispatch/axis checks belong to correctness tests; repeating native curve-family,
trade-family, size or thread matrices would not cover another changed algorithm.
The shared bump implementation is unchanged. No full benchmark target runs locally.

## Comparison contract

Baseline is merged #529 (`4df4a0b3266b0b5c510cc72b5611aee7b4bf868e`),
using its accepted installed extension. The new entry points do not exist there.
The baseline composes existing analytic calibration, quote-provenance and
first-order portfolio-risk APIs with central differences. Native results are
checked against the independent off-knot deposit oracle at actual bump points.
The composition does not provide the new sealed snapshot, strict admission,
recording budget or complete work evidence. Ratios are therefore informational
boundary costs, not a comparable regression gate or speedup claim.

Use identical Release native libraries/configuration and one thread; rebuild and
install the new extension. Record native archives/headers, binding object hashes,
installed module identities, source hashes, CPU/compiler and load. Calibrate
loops to at least 25 ms, retain all process outputs, then collect two rounds of
ten alternating interleaved pairs per case. Reduce each round with `min`.
Preserve the +4% two-round rule for any genuinely comparable affected caller;
do not apply it to different admission/ownership contracts.

## Results and provenance

Environment: Intel i9-13900HX, GCC 15.2.0, CPython 3.13.9, Release, native
AADET, one DAL/OMP thread, affinity CPU 0. This is a shared workstation; load
averages changed from 1.14/3.99/4.39 to 1.35/3.94/4.36. Process warmups and
calibrated loops retain at least 25 ms in every accepted observation.

| Complete boundary | Loops | Round 1 composition/native, microseconds | Round 2 composition/native, microseconds | Informational ratios |
|---|---:|---:|---:|---|
| N=2, M=0, create/replay/base | 128 | 213.40 / 420.89 | 211.08 / 411.81 | +97.24%, +95.10% |
| N=2, M=3, reused snapshot | 32 | 1215.52 / 969.42 | 1267.22 / 976.97 | -20.25%, -22.90% |

All 80 process samples pass independent analytic-gradient and actual-step
secant checks. Total measured loop time is 3.203935253 seconds. Base-only
snapshot construction/replay exposes material admission/ownership overhead.
The three-row reused-snapshot boundary avoids repeated Python composition, but
the contracts differ; neither row establishes a speedup or regression verdict.

The accepted baseline module is `430083a2d77c4fdebda54b66ddf70eb9a38c5593270477571305a300e297b5b8`;
the fresh module is `0b1a3200fbd953fd972bbb233e7033043d950dcedb7b9d99833b3a7f73cb8ffe`.
Both use the same two accepted native archives, 402 installed headers and native
source/configuration. Eighteen old binding implementation objects are identical;
only the registration object changes, with one new binding object added. Accepted
native/old-caller cost evidence therefore remains applicable; no old timing row
is repeated. Four-family and shared-bump correctness is checked separately.

The [raw record](aad-python-rate-curvature-results.json) includes every accepted
sample, calibration trials, driver/module/source/archive/object identities and
commands. Complete stdout/stderr and build/strict/test evidence remain under
`/home/wegamekinglc/.cache/dal-aad-evidence-20261010/python-rate-curvature/`.

Overall: no changed comparable hot path; new boundary costs are informational.
The official scheduled full gate remains independent. Future ongoing monitoring
can extend the existing installed-Python workload driver; no near-duplicate C++
benchmark target is warranted for these thin wrappers.
