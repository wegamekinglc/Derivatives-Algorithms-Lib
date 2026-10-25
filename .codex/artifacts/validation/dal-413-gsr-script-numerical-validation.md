# DAL-413 GSR script numerical acceptance

## Frozen scope before fresh execution

Input master and branch base: `aa9ba13a338f65b66f0fc2620562abfc17736666`
(DAL-412 / PR #618). Only a dedicated model acceptance test and this controlling
record are writable. No production, benchmark, binding, example or build changes
are planned. This record controls the remaining DAL-414 handoff.

The ten source hashes in the final Coupon repair identity manifest match the
checkout exactly. The tested product head `a7976021d4c3d8c7765d74e427ff5effe5b32402`
differs from this master only in its implementation record. Its accepted
175-case integration and later 72-case Coupon evidence are historical evidence,
not fresh runs. The old performance measurements are informational; they are
not a pass of the nine-target regression gate.

Fresh coverage is deliberately narrower: audit the actual three-normal mapping,
every retained fit/validation/held-out path against independently reconstructed
Gaussian endpoints and numeraire, raw pathwise derivatives and CRN FD at
g=.016; reconstruct batch means/SE and all value/gradient acceptance indicators
from retained paths at g=.02. Run selected existing independent oracle and driver
state tests, rather than repeat the full matrix. The general instrument is the
accepted N=100, K=.04 arithmetic average of USD 6M Libor fixings 2027-10-21 and
2028-04-21, paid 2028-10-21; valuation=2026-10-21 and flat curve logDF=-.12 at
2029-10-21, H=1, one constant g. Use Coupon metadata to exercise repaired generic
admission. The path audit uses 2x32 fit, 4x64 validation and 4x64 held-out solely
for deterministic oracle/CRN checks, without a precision claim.

The numerical audit freezes MRG32 seed 1024, Precise, no bridge; fit 8x8192,
validation 16x16384 and held-out 16x16384 (589824 paths/view), PriceScale=.1,
parameterScale=.01, sigma=3, nu=nuG=.25, epsilonV=1e-10, epsilonG=1e-8,
h=1e-6 and h/2=5e-7. Batch-statistic reconstruction tolerances are 1e-10
for values/SE and 1e-8 for native gradients/SE; non-root CRN tolerance is 2e-6.
Both N=100 and the N=200 scaling refusal retain the original .025 value budget.
No acceptance settings will be selected from observed results.

Fresh build: Release, GCC default compiler, native AADET, native-arch OFF,
public/examples OFF; initialize the repository-pinned submodules. This differs
from the historical public-enabled workspace configuration and will be identified
separately. Reuse historical results only for unchanged source/dependency paths;
record fresh executable/configuration identities separately.

## Results

Numerical acceptance passes within the admitted hard-payoff domain, with the
frozen budget refusals below retained. No product defect was found. This does
not certify arbitrary scripts, mixed-dimension baskets, degenerate two-sided
derivatives, fuzzy targets, quote risk or every possible precision budget.
Performance caller coverage and native smoke pass; a timing regression verdict
is unavailable from these informational measurements. There is no production
delta or applicable changed target in the nine-target regression gate, and no
actual gate failure was observed. Do not describe this as a new gate pass.

### Evidence reuse and provenance

| Input                                            | Verified evidence                                                                                                                               | Applicability and exclusions                                                                                                                                                                                                                          |
|--------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Complete implementation `557415ed`               | Attachment `01a1627a-0391-718c-bd81-70d5d5cb65e4`: 186 file hashes, original 168-case scoped XML, 3,737 properties, source/config/binary hashes | Native pricing, RNG, MC statistics/evaluation and the BondOption/rate-option test files are unchanged through current master. Driver ownership/mapping repairs are covered separately; the old binary is not a current build.                         |
| Accepted repairs `ac089b63`                      | Attachment `01a162be-9283-721e-8799-31707e467c74`, including repair RED/GREEN and accepted numerical evidence                                   | Common actual dimension, owning axes/causes/candidates and missing held-out sampling repairs; final source is present in merged master.                                                                                                               |
| Integration `38b04278`                           | Attachment `01a1634b-4cc8-76b7-92fe-40eab0ed0741`: 130 hashed evidence files, 175-case XML, five identities and 40 actual-caller samples        | Structural integration evidence. The 175-case run does not contain the full statistical matrix.                                                                                                                                                       |
| Coupon product `a7976021`, final head `4a4317e8` | Attachment `01a1638a-a011-7d55-a8c7-448622a4b036`: 64 file hashes and final 72 checks                                                           | All ten recorded source hashes match this checkout; only the implementation Markdown differs between tested product and current base. Generic Coupon/RateStrike, curve-family/Deposit refusal and owned lowerer errors retain this accepted evidence. |

The complete archive calls its source entries `complete-source/` in its manifest
but stores them under `source/`; resolving that layout alias verifies all 186
entries, with no missing or mismatched hashes. Five submodules retain exactly
the recorded pins (Eigen 3.4.0, Google Test d72f9c8a, Machinist e76b0ef2,
pybind11 8a099e44, RapidJSON ea152ded; nested Google Test ba96d0b1).

Fresh core configuration: GCC 15.2.0, CMake 4.2.3, Unix Makefiles, Release/O3,
static native AADET, Eigen, native-arch OFF; public, examples and portable Excel
tests OFF. Dedicated test/probe translation units use C++17/O2 and link the
fresh canonical `libdal_cpp.a` and `libgtest.a`. This is a scoped runner, not a
claim that `dal_cpp_tests`, public, Python or Windows suites ran. The attached
`dal413-identities.json` records source, archive, executable, CMake cache and
compile-database SHA-256 values. Historical executables retain their original
identities; none were executed as fresh binaries.

### Running existing tests

The fresh scoped runner passes **28/28**: 26 existing checks plus the two new
acceptance cases. After adding fail-fast retained-array shape assertions,
only the two affected new cases were rebuilt/rerun: **2/2**. The unchanged 26
results remain applicable. `verify_dal413.py` retains the exact source list,
compile/link commands and explicit filter in `dal413-commands.json`.

Selected existing checks independently exercise the conditional expectation,
non-root CRN FD, six-date grid refinement, single-fixing degeneration, actual
one/two-observation fits, native Caplet/Swaption fits, raw rows under solver
scaling/penalties, bounds/rank, MaxIterations/NoDescent/StepStalled, missing
held-out, dimension refusal and complete-candidate/tape failure recovery.
Affine covariance, denominator confidence and Price/Rate prefix refinement
also pass. The remaining full-AST, binding, native-only, one-sided stencil and
admission boundaries reuse the accepted structural/repair checks above.

The separate Python audit recomputes mathematical BondOption, delayed Caplet
and Swaption prices/derivatives, batch means and SE from the original raw data.
All **42 historical cells** (14 each) pass oracle-fast value tolerance 1e-10,
fine native FD/oracle tolerance 2e-4, and separate value/gradient parity and
precision conditions. This is fresh independent analysis of historical data,
not a rerun of the MC matrix. Maximum oracle-MC value indicators are
.010615558 / .002594756 / .016668400 against .025; maximum gradient precision
indicators are .402539397 / .095533776 / .831229121 against 2.5, respectively.
Definition/binding/context/plan/mapping identities, all 16 batch ranges and
raw means/gradients remain in `dal413-historical-numerical-audit.json` and the
original hashed XML.

### Authoring and qualifying acceptance tests

`dal-cpp/tests/model/test_gsr_script_acceptance.cpp` adds two tests on the
unchanged product. Its Coupon-average test independently reconstructs Gaussian
endpoints, the event-grid numeraire and native dPV/dg; every small-budget path
in all three phases matches tree and compiled AAD (value 1e-10, derivative
1e-8). At non-root g=.016, both fixed-mapping CRN steps agree within 2e-6.
The full-budget test reconstructs each value/gradient batch mean and its SE
from retained paths, checks raw Jacobian normalization, computes each acceptance
indicator independently and executes an actual N=200 immutable rebind.

Two disposable test-only negative controls qualify this coverage: advancing
the oracle by one extra normal vector at the first held-out path causes a
2.226295137 price mismatch; doubling the reported validation gradient SE
causes a .0788490363 mismatch. Both exit 1 at the intended assertion. The
unchanged real tests pass. These are mutation RED/GREEN controls, not product
defects or production fixes. A per-test RAII scope restores the USD day-basis
singleton; execution owns its recording and leaves the tape empty.

### Draw mapping and statistics

Every stream is MRG32 seed 1024 / Precise / no bridge / native GSR numeraire.
Coordinate order is sample-major, then factor name. The fresh mapping probe
confirms BondOption and Swaption D=1 with timeline [1], Caplet D=2 with timeline
[1,1.2904109589041095], and average D=3 with timeline
[1,1.5013698630136987,2.0027397260273974]. Coordinates are
`sample:i:factor:level`. Complete mapping hashes are retained in the probe log;
the corresponding old mapping identities agree. This is not a claim of shared
economic Brownian paths across different product timelines.

For a common dimension D, path p consumes sequential coordinates
[pD,(p+1)D). Fit reserves [0,65536D), validation [65536D,327680D), and
held-out [327680D,589824D). For the average these are exactly
[0,196608), [196608,983040), [983040,1769472). The small pathwise oracle
checks all 576 vectors (1,728 coordinates) with no seek in the oracle RNG.
Mixed D=6/D=1 fit or held-out is rejected during Prepare at
`/mc/mapping/dimension`; disjoint path numbers alone do not make mixed strides
safe. Same-dimension baskets need not share a union timeline.

Full-budget fresh N=100 statistics at g=.02:

| Phase      | Mean              | Value SE          | Raw dPV/dg        | Gradient SE       |
|------------|-------------------|-------------------|-------------------|-------------------|
| Fit        | .7702208563669103 | .0043820304481176 | 38.70368205353215 | .2123347024278280 |
| Validation | .7721131537480934 | .0016463818133896 | 38.80543998184061 | .0788490362854637 |
| Held-out   | .7722434233633595 | .0021752106274668 | 38.81063521078943 | .1080748765285304 |

SE is sampleSD(batch means)/sqrt(K), separately for values and gradients.
The price parity condition passes, precision passes, and total value indicator
is **.0159356187587350 < .025**. Gradient parity passes and its precision
indicator is **.679506267376796 < 2.5**. The N=200 rebind preserves parity and
raw scaling but its value indicator is **.0318712375174701 > .025**: numerical
validation fails with budgetExhausted=true. Its scale remains .1.

The actual average-driver N100 candidate is g approximately .02 with
StepStalled/converged=false, fit=true, numerics=true, held-out=true. N200 in
the two-row fit has fit=true and held-out=true but numerics=false. The native
Swaption fixed-fit comparison again rejects **.0426379742749434 > .025**;
its independent native-reference validation passes. Native Caplet fixed-fit
comparison passes with .00440509370714021. Fits, solver termination, independent
numerics and held-out are never collapsed into one flag. Without held-out input,
requested=false/passed=false remains NotRequested. Driver fit values and raw
Jacobians retain the fit budget; no validation/fine value is substituted.

Path/budget and FD refinement retain separate interpretations. The accepted
two-factor native quadrature 16/32 delta is 6.06e-15; its independent Sobol
integration differs by about 6.03e-5 against the frozen 2e-4 check. That Sobol
comparison supplies no estimator SE. The average's independent Simpson
8192/16384 expectation and FD h/h2, and its D=3 to D=6 event-grid refinement,
pass their existing checks; changing the grid changes the mapping. Affine
ParRate uses -mean(B)/mean(A), full paired covariance and quotient gradients;
prefix-budget ratio-bias checks and denominator confidence refusal pass, with
no claim of unbiased ratios. FD/quad/grid indicators are not absorbed into SE.

Boundary/zero-vol two-sided derivatives remain unavailable. IF/comparison
controls and nonzero context smoothing are rejected, so fuzzy-hard bias is
**not applicable in the admitted domain**, not a claimed zero measured bias.
Quote risk remains unavailable. Budget/execution failures preserve their own
stage, row axes, causes and last complete candidate; tests do not weaken old
assertions to produce acceptance.

### Minimal caller performance

Changed paths are only tests and this record: no nine-target hot path changes.
Use the existing `script_mc_perf --gsr-european` native-only profile and the
accepted prepare/evaluate/Jacobian probes for 1/2 BondOption or average rows;
do not add a benchmark target. The native profile's seven built-in workloads
retain one warmup/three repetitions and all numerical checks; they pass.
Caplet has no separate current CLI timing case, and its unchanged kernel is
covered numerically, without a new performance claim. Curve/XCCY, SLV, public,
bindings, tape/RNG kernels and the full matrix are excluded by caller analysis.

Fresh rebuilt probes run pinned to CPU 4, DAL_NUM_THREADS=1, on an Intel
i9-13900HX / WSL2 host. Ten rounds per row count alternate FD/AAD order and
retain the original budgets. Eighty measured rows include actual refresh work.
Median microseconds (informational, not the gate's best-of-ten reduction):

| Caller/rows  | Method | Prepare | Evaluate   | Jacobian   | Jacobian batches/paths |
|--------------|--------|---------|------------|------------|------------------------|
| BondOption/1 | FD     | 164.717 | 285.346    | 1112.065   | 8/512                  |
| BondOption/1 | AAD    | 172.553 | 320.596    | 323.987    | 2/128                  |
| BondOption/2 | FD     | 339.273 | 556.000    | 2217.530   | 16/1024                |
| BondOption/2 | AAD    | 341.572 | 657.990    | 631.059    | 4/256                  |
| Average/1    | FD     | 417.382 | 14752.450  | 29495.450  | 16/131072              |
| Average/1    | AAD    | 414.540 | 64434.900  | 64733.200  | 8/65536                |
| Average/2    | FD     | 700.692 | 29283.250  | 59095.300  | 32/262144              |
| Average/2    | AAD    | 717.233 | 129212.000 | 128693.500 | 16/131072              |

AAD peaks are 89/96 BondOption nodes and 250 average nodes, with 1,966,080
capacity bytes. The one-parameter average AAD Jacobian is slower than FD despite
fewer paths; no speedup or basket optimization is promised. Raw CSV, minima,
medians, execution/tape counters and process RSS high-water observations are
retained. RSS is process-level, not per-call allocation. WSL2/shared-machine
noise and the absence of an isolated paired baseline preclude a latency
regression verdict. Neither a single native smoke nor these informational
times are promoted into the nine-target gate's two-round +4% decision.

### Commands, repairs and delivery boundary

```sh
git submodule update --init --recursive
cmake --preset=Release-linux -S . -B build/Release-linux \
  -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_PUBLIC=OFF \
  -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DDAL_CPP_BUILD_BENCHMARKS=ON
cmake --build build/Release-linux --target dal_cpp gtest script_mc_perf -j 8
python3 evidence/verify_dal413.py
python3 evidence/measure_dal413.py
python3 evidence/audit_dal413.py
DAL_NUM_THREADS=1 build/Release-linux/dal413-scoped-tests \
  --gtest_filter='GSRScriptAcceptanceTest.*'
python3 .github/scripts/check_docs.py
git diff --check
```

The `evidence/` scripts are supplied in the issue attachment, outside the
repository; extract them beside this checkout. Exact compile/link/filter,
negative-control and measurement arguments are retained in their JSON records.
The final two tests, GCC14/15 warning-clean syntax checks using repository
warning exclusions, CCN<=8, formatting/diff and Markdown checks pass.

Repairing failures: no product repair. Initial missing submodule/configuration
and include-order errors, an unquoted zsh wildcard and a warning command missing
the repository's aggregate-initializer exclusion were harness/setup errors;
they are not product RED evidence. No threshold, existing assertion, production
source, benchmark source, dependency pin or build registration was changed.
No full repository/Windows/public regression was repeated for this handoff.
CI and focused review of this test-only PR remain coordinator delivery gates;
DAL-414 is not started by this acceptance record.
