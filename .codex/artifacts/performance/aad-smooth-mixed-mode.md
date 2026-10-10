# Smooth mixed-mode scoped performance plan

Status: scoped local acceptance complete; current-head publication gates remain.

Baseline is merged #527, `19a34172a23dd85041d7bfcfd694b63bf6529c6d`.
Its accepted native/public archives and complete build provenance may be reused
only with verified identities. The new module does not change ordinary native
node layout, first-order operations or existing financial adapters.

Map `dal-cpp/dal/math/aad/forwardoverreverse.hpp` and its implementation to two
complete smooth requests: one polynomial with a single direction and one smooth
European Black-Scholes kernel with multiple directions. Compare complete
request latency, actual recording/sweep counts, checksums against independent
analytic products, and tape/cleanup peaks with the existing bump-over-AAD
alternative. Record that the methods have different approximation semantics.

Use two rounds of ten interleaved pairs per selected case, warmup, calibrated
repetitions and alternating first side. Keep every sample and immutable
source/dependency/configuration/archive/executable identity. These measurements
characterize the new prototype; they do not establish a generic speedup or a
regression in unchanged native callers.

Empty inputs/directions, signs, tiny directions, primitive domains, aliased
operands, recording modes, budgets and failures are functional boundaries.
Unchanged tape kernels, market calibration, PDE, LSMC, portfolios and complete
parameter matrices are excluded. Expand only for a concrete failure or newly
identified caller gap, recorded before execution. Required current-head CI
and actual diagnostic/platform test logs remain merge gates.

## Evidence and measurements

Implementation `047fab26` is compared with the immutable merged #527 baseline.
Separate detached source worktrees and Release configurations explicitly enable
benchmarks. Accepted baseline archives are reused with verified identities;
all 183 existing native archive members and the entire public archive are
byte-identical. The new native module is the only added member. Source, dependency,
compiler, CMake, archive, object and executable identities, calibration outputs,
all 80 raw samples and commands are committed in
[the complete evidence](aad-smooth-mixed-mode-results.json). Documentation-only
publication changes do not alter those measured dependencies or binaries.
The final namespace-comment correction has a fresh rebuild proof under
`identity_reuse`: the module, consuming harness object, archives and measured
executable retain their exact hashes, with refreshed dependency identities.
No repeated timing is needed for that identical executable.

Both sides use one thread and a fixed caller CPU on the same shared host, with
O3, NDEBUG, fast FP contraction and diagnostics/native architecture flags OFF.
Each process warms up and consumes complete request outputs. Calibrate both
sides to at least 25 ms, then take two rounds of ten alternating interleaved
pairs per case, reducing each side to its best-of-ten minimum. Calibrated repeats
are 65,536 for the polynomial and 16,384 for Black-Scholes. Total measured time
is 3.202635571 seconds; process startup and calibration are separate.

| Case                 | Bump request, round 1/2 (microseconds) | Smooth AD request, round 1/2 (microseconds) | Delta, round 1/2  |
|----------------------|----------------------------------------|---------------------------------------------|-------------------|
| Quartic, N=1/M=1     | 0.7330 / 0.6648                        | 0.5229 / 0.4857                             | -28.67% / -26.94% |
| European BS, N=2/M=3 | 2.1775 / 2.1344                        | 2.0112 / 1.9901                             | -7.64% / -6.76%   |

Both workloads satisfy independent price/gradient/Hessian references. The
polynomial's maximum product error is 0.000399999999388 for bump and zero for
smooth AD. The European product errors are 2.9026784e-6 and 6.6613381e-16.
Actual recording/sweep counts are 3/3 versus 1/2 and 7/7 versus 3/4. Each case
reports 1,966,080 bytes peak tape capacity and 655,360 bytes cleanup reserve
on both sides. Block capacities do not imply equal occupied node counts or RSS.

Overall: no changed legacy regression path; scoped new-feature characterization
is accepted. These two cases show lower prototype latency, without a generic
speedup claim. Methods have different approximation semantics, so their deltas
are informational rather than a same-algorithm +4% regression verdict. The
ordinary-caller sustained +4% two-round policy is unchanged; excluded cases
are not claimed as newly measured passes.

## Codacy harness repair scope

Codacy reports cost-driver main complexity 12, above limit 8. Extract request
construction without changing workloads, output consumption or the timed loop.
Rebuild both consuming cost executables and refresh only the same two cases
under the unchanged calibrated paired-sampling contract. Preserve the initial
raw measurements and identities. Core/test/installed executables are unchanged;
their accepted evidence remains applicable and no full matrix is justified.

The repaired driver at `158af75a` is freshly compiled in both isolated builds.
The `complexity_repair` section of the raw evidence retains all 80 refreshed
samples, alongside the original 80. Calibration remains 65,536/16,384 repeats;
sampling, warmup, fixed affinity, independent checks and two best-of-ten rounds
are unchanged. Refreshed measured time is 2.970270252 seconds.

| Case                 | Bump request, round 1/2 (microseconds) | Smooth AD request, round 1/2 (microseconds) | Delta, round 1/2  |
|----------------------|----------------------------------------|---------------------------------------------|-------------------|
| Quartic, N=1/M=1     | 0.5793 / 0.5862                        | 0.4388 / 0.4106                             | -24.25% / -29.96% |
| European BS, N=2/M=3 | 2.1107 / 2.0704                        | 1.9749 / 1.9003                             | -6.43% / -8.22%   |

Analytic error, actual work and capacity counts are unchanged. These refreshed
observations supersede the initial cost summary for publication; the same
informational interpretation and scope exclusions apply.

## Review repair scope

Copilot requests direct standard headers in the cost driver and new test. Its
review body also identifies missing direction context when callback scalar
values differ. Add the direct headers (including the native implementation's
algorithm/exception uses) and a focused RED/GREEN assertion for direction one.
Only the new module, its tests, installed consumer and the same two consuming
cost cases are affected. Refresh those builds and paired cases with unchanged
sampling; preserve both earlier measurement sets. No old native archive member
or unrelated benchmark workload changes.
