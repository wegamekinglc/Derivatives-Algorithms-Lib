# WP-4c performance scope

Baseline: 226b4b3569fc6646bc2987fb244bd5a3f8e02eba (#561). Its tree equals
reviewed head 1f8cc0dac1543dc140f3c0761791dec961e0545a. The frozen #561 source,
normal/profile libraries and dependency identities establish applicability for
baseline RED and initial correctness controls; no old timing is accepted as a
measurement of the new replay implementation.

## Caller map before measurement

- lsmc.cpp fuzzy AAD workers/harvesting/reduction: frozen BS/LV pricing, historical
  active constants, multiple batches and tails, RQMC/precision/bridge, dense risks.
- lsmc.cpp value-only fuzzy workers: RetrainedBump common-path policy repricing.
- lsmc.cpp curvature callers: frozen native BS gradient bumps, tape budgets and
  worker cleanup maxima. Existing independent nested passive-price tests remain
  numerical controls rather than a substitute for selected timing.
- simulation.hpp generic plan support: four ordinary controls cover small AAD,
  weighted double, scalar double and dense AAD with slot reuse. The ordinary
  algorithm is unchanged; do not repeat the previous full 25-case set.
- Hard replay, training/regression kernels, tape primitives, RNG algorithms,
  PDE and calibration are unchanged. Exclude the nine scheduled kernel targets
  from local timing; scheduled full monitoring and exact-head CI still apply.

## Planned smallest set

Ordinary controls: raw BS 257 paths/repeated requests/AAD/T4; weighted BS 65536
paths/double/T4; scalar BS 1M/double/T8; dense LV 1M/AAD/T32.

LSMC: small single-batch BS/compiled/AAD/T1; BS 262K/compiled/T1,
1M/tree/T8 and 262K/compiled/T32; LV 65K/tree/T4; dense 64x64 LV
2M+1/compiled/T8 (legacy batches cross the 256-slot limit); RetrainedBump BS
65K/compiled/T4; frozen curvature BS 65K/compiled/T4; RQMC BS 32768/compiled/T3
with two replicates, validation, precise normals and Brownian bridge.
The four controls use 512, 8, 1 and 1 timed requests per process respectively.
The small LSMC case uses 64 timed requests; each other LSMC case uses one.
BS and LV multi-batch cases use the existing 53-week put fixture. Small, dense,
RetrainedBump, curvature and RQMC cases use two-date July 2024/January 2025 puts.
All LSMC cases explicitly train on 1024 paths, preserving the API default.
RQMC uses validation 128, two replicates, seeds 17/29, Precise normals and bridge.
Curvature uses a spot direction and step 0.1, comparing every Hessian-product
coordinate alongside PV and all first derivatives.

The fourteenth case is a 3-factor GSR cash-settled 5Y swaption, exercised on
the same two dates, 262144 pricing paths, compiled AAD and T8. It uses the
existing GSR model-data factory and core Bermudan test's annuity construction,
explicit IR swap regression and smoothing 0.001. This covers the large rate-model
prefix caller without adding a benchmark target or a parameter cross product.
The exact 14 tuples are retained in /tmp/dal486-wp4c-paired.py and its recorded
hash; /tmp/dal486-wp4c-probe.cpp is compiled identically against both libraries.

Explicit training/validation budgets stay identical on each paired side; defaults
are unchanged. Keep selected cases, rather than a Cartesian model/mode/thread
matrix. Expand only for a documented new failure, dependency change or gap.

## Protocol and current state

Measure after correctness: isolated equivalent Release/static/native AAD/Eigen
builds, source/dependency/configuration provenance and executable hashes, no
concurrent local build/test/analysis load. Two rounds x ten alternating interleaved
pairs, min reduction and strict +4% in both rounds remain unchanged. Preserve
every raw output, compare PV/all risks/curvature rows and record dense peak RSS.

Actual baseline normal RED has two task-bound failures; profiling RED adds a
worker-profile failure. The independent fresh-pricing-segment price/all-risk
oracle passes on both frozen images. First normal GREEN passes unchanged RED
tests, then all 91 nearby exercise/curvature tests. Final normal/profile contracts
pass 9/11 tests; the focused profiling selection passes 100. Full native/public/
portable Excel CTest passes 3457/3457, focused tests 686/686 and CLI tests 7/7.
The real 257-batch replay observes 18512 bytes of numeric result arrays, at its
bound; this excludes model/path/tape and is not process RSS. No timing verdict
existed at that correctness checkpoint; accepted measurement follows below.

## Accepted measurement

Overall: **no regression under the repository's two-round gate**. All fourteen
cases have two rounds of ten alternating paired processes, 560 raw outputs,
unchanged min reduction and strict +4% in both rounds. No local build/test/static
analysis overlaps accepted sampling. This WSL2 workstation is a noisy environment;
the mixed GSR rounds (+8.449557%/-2.224315%) do not establish a stable gain or
regression. Preserve both rounds rather than reporting only the combined delta.

Baseline is the #561 merge tree 226b4b3569fc6646bc2987fb244bd5a3f8e02eba;
frozen source HEAD 1f8cc0dac1543dc140f3c0761791dec961e0545a has that same tree.
Branch measurements use 226b4b3 plus the recorded production patch. Every tracked
production/benchmark input, four pinned dependencies, matching CMake configuration,
compiler and executable identity are verified before and after sampling. Reconcile
these unchanged content hashes with the subsequent publication commit.

Source/build directories are the isolated dal486-wp4b-deterministic-chunks and
dal486-wp4c-lsmc-workers siblings under /home/wegamekinglc/.cache; each build has
its own -build suffix. Configuration: GCC 15.2.0, CMake 4.2.3, Release, static
native AAD/Eigen, benchmark option ON and profiling OFF. CPU: i9-13900HX,
32 logical CPUs. T1 is pinned to CPU2; other cases use CPUs0-31 and their explicit
DAL thread count, with OMP/BLAS/MKL threads set to one. Both probes compile the
same driver with -O3 -DNDEBUG -ffp-contract=fast. The timed region includes a
complete prepared valuation, including training, rather than only the replay loop;
one untimed warm request precedes the specified repeated requests.

| Case                     | Base min ms | Branch min ms | Round 1 % | Round 2 %  | Verdict     |
|--------------------------|-------------|---------------|-----------|------------|-------------|
| small-bs-aad-t4          | 0.057040    | 0.056436      | -3.002595 | -0.124474  | gate pass   |
| weighted65k-double-t4    | 1.456351    | 1.457383      | -0.503700 | +1.116970  | gate pass   |
| bs1m-double-t8           | 144.333470  | 145.167473    | +0.577831 | -0.793577  | gate pass   |
| dense-lv1m-aad-t32       | 420.341403  | 419.498154    | -0.200611 | +1.014591  | gate pass   |
| lsmc-small-bs-t1         | 0.219069    | 0.221181      | +0.550246 | +0.964080  | gate pass   |
| lsmc-bs262k-compiled-t1  | 2896.055342 | 2906.918469   | +0.223252 | +0.375101  | gate pass   |
| lsmc-bs1m-tree-t8        | 1948.985109 | 1971.602516   | +1.160471 | +0.550601  | gate pass   |
| lsmc-bs262k-compiled-t32 | 177.109667  | 183.326147    | +3.509961 | +1.412094  | gate pass   |
| lsmc-lv65k-tree-t4       | 287.024729  | 285.544160    | -0.705735 | -0.515833  | gate pass   |
| lsmc-dense-lv2m-tail-t8  | 244.514492  | 213.279733    | -9.505404 | -12.774195 | improvement |
| lsmc-retrained-bs65k-t4  | 34.592044   | 32.248268     | -6.775477 | -7.164316  | improvement |
| lsmc-curvature-bs65k-t4  | 25.499012   | 26.055235     | +2.188147 | +2.181351  | gate pass   |
| lsmc-rqmc-bs32k-t3       | 16.409127   | 16.622039     | +1.358148 | +1.297522  | gate pass   |
| lsmc-gsr3f262k-t8        | 228.707013  | 231.545932    | +8.449557 | -2.224315  | gate pass   |

Every sampled PV, Greek, weighted component and Hessian-product coordinate is
exactly equal between sides: maximum absolute/scaled/pure-relative difference 0.
Dense LV has 4100 live inputs; the ordinary barrier control has 4101 because it
also retains the barrier constant. Dense replay median peak process RSS changes
44250 -> 43494 KiB; dense ordinary control 114270 -> 114320 KiB. The replay RSS
movement is modest and does not establish a universal footprint improvement.

Raw commands, all 560 outputs, time/RSS records, samples and source/configuration/
executable identities live under
/home/wegamekinglc/.cache/dal486-wp4c-lsmc-workers-evidence/paired-initial/;
results.json and provenance.json are the machine-readable receipts. Build identities
and native-test-rollup.json are in its parent. Baseline/head normal libraries are
89fddadbdaaf9bb7115a62740f6d69966bc063f6bfba568e664c01b5e1d0a023 and
1c0d00a7ff88418530c4fbb3ce068ad0470ac63e6b199bfa0ec711180a863abc.
The probe driver SHA256 is
247e102f08ec7670843f021245bdc341e6b96e3b5d482fefb1d9d29715d0e0b0.
Original preflight and corrected remaining preflight are diagnostic only. The
first stops on a probe-only 4101-versus-4100 axis assertion, after recording both
dense outputs; the corrected helper retains exact expected extents by fixture.
No accepted samples were discarded or repeated.

Coverage advisory: existing script_mc_perf production/replay fixtures exercise
these paths and remain the appropriate place for future durable coverage. This
local same-driver study adds no near-duplicate benchmark target. The nine scheduled
kernel targets are outside the changed paths and have no new local pass claim.
Exact-head CI and scheduled full-suite monitoring remain required.
