# DAL Python interface benchmarks

This suite runs 70 workloads through `import dal`, with a coverage entry for every
target in [the C++ benchmark inventory](../../dal-cpp/benchmarks/CMakeLists.txt).
It requires a current DAL Python build and Python 3.9–3.13. The runner uses the
standard library; pytest is needed only for its correctness tests.

## Running

With a current wheel or editable installation, run from `dal-python/`:

```bash
python benchmarks/run_benchmarks.py --coverage
python benchmarks/run_benchmarks.py --list
python benchmarks/run_benchmarks.py --smoke --output-dir ../benchmark-results/python-smoke
python benchmarks/run_benchmarks.py --samples 10 --warmups 2 --output-dir ../benchmark-results/python
python benchmarks/run_benchmarks.py --group script_mc_perf --group curve_calibration_perf
python benchmarks/run_benchmarks.py --group rate_risk_perf --filter generic
```

`--coverage` does not import DAL. `--list` lists selected case names and exact
workloads using the active DAL environment. Unknown groups, empty selections and
invalid sample counts fail. A group with only indirect/unavailable coverage cannot
be selected for timing.

For a Linux workspace build, configure from the repository root using the same
Python interpreter for CMake and execution:

```bash
cmake --preset=Release-linux -S . -B build/Release-linux \
  -DDAL_BUILD_PYTHON=ON -DPython3_EXECUTABLE="$(command -v python3)"
cmake --build build/Release-linux --target _dal --parallel 4
cd dal-python
DAL_NUM_THREADS=4 PYTHONPATH=../build/Release-linux/dal-python \
  python3 -m pytest tests -q
DAL_NUM_THREADS=4 PYTHONPATH=../build/Release-linux/dal-python \
  python3 benchmarks/run_benchmarks.py --output-dir ../benchmark-results/python
```

In PowerShell, select the corresponding build-tree package before running:

```powershell
cd dal-python
$env:PYTHONPATH = (Resolve-Path ../build/Release-windows/dal-python).Path
$env:DAL_NUM_THREADS = "4"
python benchmarks/run_benchmarks.py --smoke
```

Run pytest from `dal-python/` to avoid an old installed `dal/` directory at the
repository root shadowing the selected build. Verify `environment.native_module`
in the JSON report. An older published wheel may lack required APIs; missing APIs
are errors, never silently skipped workloads.

## Workload alignment

| Native target            | Python cases | Full workload and timing boundary                                                                                                                                                      |
|--------------------------|--------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `rng_perf`               | 4            | 100,000 paths × 10 dimensions; Sobol normal fast, normal precise with polish, uniform, and MRG32 normal; fresh generator and output matrix each invocation                             |
| `script_perf`            | 1            | Same three-year weekly barrier event table; `Product_New` + `Product_DebugJson`, including frontend construction, indexing and JSON serialization                                      |
| `script_mc_perf`         | 8            | Vanilla: 200,000 double / 20,000 AAD paths; weekly barrier: 100,000 / 10,000; tree and compiled evaluators; product/model creation and preprocessing included                          |
| `curve_calibration_perf` | 21           | Same 23 annual swaps and 24 future knots; PWC, PWL, LOG_LINEAR, LOG_CUBIC_NATURAL, MIXED × ANALYTIC/BUMPED × diagnostics/solve-only, plus LOG_LINEAR APPROXIMATE                       |
| `xccy_perf`              | 8            | Joint/staged × ANALYTIC/BUMPED × diagnostics/solve-only; adapted square fixture with five quotes per calibrated block                                                                  |
| `rate_risk_perf`         | 19           | 120-IRS batch and 240 single-component calls; five-year OIS daily compounding; 24-XCCY × five-component batch; nine single/joint/staged quote portfolios; six generic-joint portfolios |
| `quote_risk_perf`        | 9            | Single, joint XCCY and staged provenance at N=8/16; additional generic joint provenance at total N=5/10/16                                                                             |

All these target mappings are marked `partial`: the timed boundary is the Python
interface, and some native-only subcases remain inaccessible. This is not a claim
of full native case parity.

`Product_New` only stores events; the timed `Product_DebugJson` call is needed to
execute the native preprocessor/parser. Its indexing and serialization cost is
also included. Python's MRG32 constructor fixes `precise=true`, while the native
RNG benchmark selects `precise=false`; this policy difference is recorded rather
than treated as equivalent timing. Sobol precision/polish flags match the native cases.

The single-curve quote portfolios retain the native N=2/5/16 and 1/120-trade
shapes. Joint XCCY uses five calibrated blocks (domestic discount/forward, foreign
discount/forward, basis), with N=2 or 10 quotes per block and 1/24 trades. Staged
basis portfolios use N=2/5/16 and 1/24 trades. XCCY fixtures use explicit flat
domestic/foreign curves and a deterministic basis quote ladder; the native fixture
generates quotes from sloped curves. XCCY node timing uses the joint calibration
fixture. These are related workloads, not identical native input sets.

Generic joint portfolios use the native three-layered-curve fixture with total
quote widths N=5/10/16 and 100/1,000 IRS. Flat-curve calibration quotes are derived
analytically in Python. Every third position receives fixed on 250,000 notional;
the others pay fixed on 1,000,000. The third curve is a structural-zero trade
dependency. The internal joint-node reference and native preparation/tape counters
are not exposed by Python and are not measured.

Native targets without direct Python kernel timing are listed individually by
`--coverage` and in every report:

| Coverage                          | Native targets                                                                                                                             |
|-----------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| Indirect through another workload | `tape_perf`, `jacobian_perf`, `interp_perf`, `krylov_perf`, `specialfunctions_perf`, `ycinstrument_perf`, `threadpool_perf`, `stacks_perf` |
| No corresponding bound operation  | `matrix_perf`, `pde_perf`, `banded_perf`, `cholesky_perf`, `black_perf`, `iv_brent_perf`                                                   |

Additional gaps within covered families include isolated script preprocessing and
parsing, direct BrownianBridge/IRN RNG fills, XCCY precompute/price/reset-aware and
approximate calibration cases, and internal quote-state probes. Matrix storage
conversion and MC call pricing are not substitutes for native matrix arithmetic
or analytic Black/IV kernels.

## Measurement and correctness

Fixture creation, calibration of risk markets, reference results, and validation
run outside timing. Risk aggregation reuses immutable provenance and markets;
provenance construction has separate cases. Each case performs an untimed checked
call, then warmups, then measured calls. Validation after every call checks output
shapes, finite results, calibration residuals, matrix-retention policy, tree/compiled
PV and Greek agreement, batch/single node-risk agreement, provenance availability,
passive/quote-risk PV agreement and DV01 units. Vanilla MC also checks an independent
Black-Scholes formula. Unavailable or ineligible results fail the run.

`perf_counter_ns` measures one complete public workload per sample. Python argument
and return conversion are included; inspecting results and destroying the previous
result are excluded. GC remains enabled. No native executable is launched or
timed by this runner. Native initialization occurs before measurements.

Full mode defaults to ten samples and two warmups. Smoke mode uses one sample,
zero warmups, 1,024 RNG paths, 2,048 MC paths and at most two portfolio trades;
calibration widths remain intact. Workload metadata records actual sizes, even
when a case's stable name contains its full-profile trade count. Smoke timings
must not be compared with full timings. `DAL_NUM_THREADS` defaults to 4 when
unset and must be set before DAL is imported.

Outputs are `results.json` (schema `dal.python-benchmarks/1`) and `summary.md`.
They retain every raw nanosecond sample, minimum/median/maximum, failures, profile,
coverage, Python/platform/CPU identity, thread settings, native-module path and
SHA-256, suite source hashes, checkout SHA/status, and discoverable CMake flags.
The checkout SHA does not prove an installed wheel's source revision. Interrupted
runs retain `status: running`; failed cases have no successful timing row and the
runner exits nonzero. Reusing an output directory replaces its previous report.

Standalone runner measurements are informational. Python and C++ results have
different boundaries and must not be divided into a claimed language overhead
ratio. The Python CI gate below compares Python base/head runs separately from
the native nine-target gate. For manual comparisons,
retain separate output directories, use equivalent Release builds and workloads,
record both module hashes, fix thread settings, and interleave repeated processes
on the same quiet machine. Shared-host/WSL noise and a single timing run cannot
establish a regression.

The normal Python pytest suite executes every smoke workload and verifies the
runner's failure handling and the complete CMake target mapping. Adding a native
target without a documented coverage entry fails that inventory check. The
repository's Python 3.9 syntax gate includes benchmark sources.

## CI regression gate

The Linux `Benchmarks` job builds both the PR base (or pre-push commit on `master`)
and the tested revision as independent Release builds with Python bindings enabled.
Both use the same CPython 3.13 interpreter, compiler, AAD backend, native architecture
flags and `DAL_NUM_THREADS=4`. The job verifies the head Python correctness suite
against both modules before measuring performance.

The [paired Python gate](../../.github/scripts/check_python_benchmark_regressions.py)
runs the complete head workload suite on **both** native modules. Each fresh process
checks its package and native-extension paths before execution, performs one checked
preflight call and two warmups per case, and records one full-scale sample. Base/head
order alternates on every process pair. Two confirmation rounds each contain ten
pairs: 20 processes per side, with 20 samples for every case on each side. A case
fails only when its head minimum exceeds its base minimum by strictly more than
4% in both rounds. Exactly +4% passes.

The same head test code and workload metadata must be used for both sides, including
when the base predates this benchmark suite. This first introduction still measures
and gates all 70 cases against the base library; it is not an unmeasured adoption
pass. When a base suite exists, its inventory is also checked: removing or renaming
a case fails. New cases must run against both libraries. A missing base API,
incorrect/ineligible result, unexpected package path, changed binary or suite hash,
different workload/configuration, timeout, or incomplete sample set fails the gate.
There is no smoke-mode or case-filter option in the gate.

To reproduce it after creating two independent, equivalently configured workspace
Release builds with `DAL_BUILD_PYTHON=ON` and the same `Python3_EXECUTABLE`, run from
the head source checkout:

```bash
DAL_NUM_THREADS=4 python3 .github/scripts/check_python_benchmark_regressions.py \
  --base-source /path/to/base-source \
  --head-source . \
  --base-root /path/to/base-build \
  --head-root /path/to/head-build \
  --output-dir benchmark-results/python-paired \
  --samples 10 --confirmation-rounds 2 --threshold-percent 4
```

The build-root arguments identify the CMake workspace roots, whose Python packages
are under `dal-python/`; do not pass installed/staged packages. The gate validates
Release configuration, matching compiler/options, and each build's source directory.
Source commit IDs, build configuration, native module paths/hashes, suite hashes,
raw timings, per-round minima and deltas are retained in
`results.json` (`dal.python-performance-gate/1`). Every worker has a `raw/NN-side/`
directory containing its normal runner reports and captured output. Errors still
produce a failing gate report; interrupted runs retain `status: running`.

The job appends the comparison to the GitHub Actions summary and uploads all Python
evidence inside the existing `benchmark-linux-*` artifact for 30 days, including on
failure. Python and native comparison steps can both report failures, and either
failure blocks the existing `Linux CI gate`. Windows continues to run the Python
correctness/smoke workloads through pytest; the paired performance gate is Linux-only.

## Third-party comparison

The Linux `Benchmarks` job also runs the same five common workloads against DAL,
QuantLib-Python and rateslib. All three backends must execute every case and pass
an independent cashflow oracle. Missing dependencies, incorrect results, incomplete
reports, changed workloads/binaries or process timeouts fail the job and therefore
the existing `Linux CI gate`. Relative speed is reported without an absolute
competitor speed threshold; the separate base/head gate still enforces the 4% DAL
regression rule.

The comparison uses CPython 3.13 and a Release DAL build. Benchmark-only dependencies
and their transitive dependencies are version/hash locked in
[`requirements-comparisons.txt`](requirements-comparisons.txt). The
[`QuantLib-Python` compatibility package](https://pypi.org/project/QuantLib-Python/)
is pinned to 1.18, its actual `QuantLib` dependency to 1.43, and `rateslib` to 2.7.1.
These packages are not added to DAL's wheel requirements. Rateslib has its own
[licence terms](https://rateslib.com/licence), including commercial/evaluation
licensing requirements; its licence notice remains visible in worker logs.

Run from the repository root, with the same interpreter used to build DAL:

```bash
uv pip install --require-hashes -r dal-python/benchmarks/requirements-comparisons.txt
PYTHONPATH="$PWD/build/Release-linux/dal-python" \
  python -m pytest dal-python/benchmarks/tests -q
python dal-python/benchmarks/run_comparisons.py \
  --dal-package build/Release-linux/dal-python \
  --output-dir benchmark-results/python-third-party \
  --samples 10 --rounds 2
```

The output directory must be new, so an old successful report cannot satisfy a
failed run. `--smoke` uses four queries/trades and one process per backend for
local correctness checks; CI always uses full sizes and at least two rounds of ten
fresh processes per backend. Backend order rotates for each sample and reverses
on the second round. Each process performs a checked preflight, two warmups, then
one measured invocation per case. Imports, fixture construction, reference values,
validation and result destruction are outside timing. GC stays enabled, DAL uses
four threads, and OMP/OpenBLAS/MKL use one thread.

| Case               | Full workload                                                    |
| ------------------ | ---------------------------------------------------------------- |
| `discount_queries` | 4,096 distinct, permuted dates on a 22-node log-linear DF curve  |
| `irs_pv_32`        | Price 32 forward-starting IRS, returning one PV per trade        |
| `irs_pv_256`       | Price 256 forward-starting IRS, returning one PV per trade       |
| `irs_dv01_32`      | Reprice 32 IRS on two prepared parallel-shifted zero curves      |
| `irs_dv01_256`     | Reprice 256 IRS on two prepared parallel-shifted zero curves     |

All cases use valuation date 2025-01-15, USD, ACT/365F, annual fixed and IBOR legs,
unadjusted dates, no holidays, zero fixing/payment lag, and the same discount and
forecast curve. Trades start in 2026, mature over 2028–2045, and alternate payer
and receiver directions with varying positive notionals and coupons. The common
oracle independently computes the fixed coupon annuity and telescoping floating
leg from log-linear discount factors. Query tolerance is 2e-12 absolute; PV/DV01
tolerance is 2e-7 USD absolute, both with 1e-10 relative tolerance.

DV01 here is `(PV(zero + 1bp) - PV(zero - 1bp)) / 2`, with the same decimal-rate
shift applied to both discounting and forecasting. It measures finite-difference
parallel zero-curve risk, **not** calibrated quote-space risk or native AAD. Curve
construction is excluded. The C++-aligned suite continues to measure DAL's native
node/quote-risk APIs separately; no third-party coverage is claimed for calibration,
MC, XCCY, or other unmatched operations.

DAL uses `PriceRateTrades` batches; QuantLib and rateslib price instrument lists.
All adapters return floats in input order, including conversion cost. QuantLib's
`recalculate()` forces every NPV invocation to run its pricing engine. Rateslib's
public `curve_caching` setting is disabled, so discount queries measure interpolation
rather than cached date lookups, and `ad=0` selects passive pricing. QuantLib keeps
its constructed schedules/coupons; DAL's public batch call owns its internal
preparation. These are comparisons of the available Python APIs, not isolated
identical native kernels.

`results.json` (`dal.python-comparisons/1`) retains raw timings, minimum and median,
per-round ratios, conventions and provenance. Each process also retains its checked
output values, package versions, module paths/hashes, source and dependency-lock
hashes, DAL build flags, CPU/Python/thread settings and log. `summary.md` and the
Actions summary show per-round minima and `third_party / DAL` ratios (>1 means DAL
took less time). Both successful and failed evidence is uploaded in the existing
`benchmark-linux-*` artifact under `python-third-party/`.
