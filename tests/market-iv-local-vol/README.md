# Independent market-IV/local-vol qualification

This is the DAL-542 / S3 test harness. Production construction now accepts the
frozen grids after the short-time Dupire repair. Numerical acceptance is
determined separately: the current frozen smile grids fail the N5 grid gate,
and the final tau/4 grid also exhausts the permitted PDE reference refinement.
A successful executable exit or a small MC price error does not qualify S3.

The controls are API note section 11 and the corresponding DAL-538 plan.
Production baseline is `53a2b1592993f3a1d354df2d6ec2ae6918e5ef6d`;
the production repair is `b0591270d904d9f4d1a6cf5ce94f121a8acee8f8`.
This directory adds no production policy, public interface or dependency pin.

## A/1 approved execution

The active F2 amendment is
`.codex/artifacts/specs/dal-542-n5-resolution-a.md` (A/1). The original 320 rows
and all original fixtures remain historical evidence. The A/1 current view
contains 160 reused F1 rows plus 320 newly registered F2 rows. Until actual
admission succeeds, absent estimates remain NOT_RUN.

The sealed A/1 execution stopped at its first unavailable old-grid reference:
all eight Black studies passed, but F2_linear at T=91/365 still had spatial
difference/S0=1.146344457336923e-4 after the unique fallback (limit 2.5e-5).
Its time, matched-domain, mass and forward checks passed. The run completed
58 PDE solves in 20.264351 seconds of cumulative stage elapsed, with zero new
MC paths. The remaining seven old-grid studies, new capture/N4, 16 required
P7/P8 studies, 56 diagnostics and two MC families were not run. The checkpoint
is in `a1/checkpoint.json`; `a1/qualification.jsonl` has 160 reused PASS rows
and 320 NOT_RUN rows. Original 184/76/60 historical counts remain unchanged.
These results do not qualify S3 or authorize a different mesh/refinement.
Subsequent Codacy repairs extract helpers and restrict child-process execution;
they preserve the numerical kernel ASTs, canonical request and all 58 solved
meshes. The original execution source/manifest and cumulative budget remain
separate from the published source. No formal solve or path was repeated.

`a1/request.json` contains the canonical 90-digit-to-binary64 P0–P8 axes,
the two families, all four nested timelines and fixed coordinate reservations.
`a1_axes.request` regenerates it from the unchanged original generated request;
the generator does not evaluate candidate prices. The nonuniform reference is
in `a1_pde.py`; the original `pde.py` and continuous oracle fixtures are intact.
`mcpayoff.hpp` extracts the original passive payoff wiring for both runners.

Build the core-only targets using the configuration below, adding
`dal_market_iv_a1_mc`, `dal_market_iv_a1_probe` and
`dal_market_iv_a1_protocol_tests`. Run focused support checks:

```bash
OPENBLAS_NUM_THREADS=1 python3 -m unittest discover \
  -s tests/market-iv-local-vol -p test_a1.py -v
ctest --test-dir build/dal-542-reference \
  -R 'MarketIv(A1|Qualification)Test' --output-on-failure
```

After committing the tested implementation, freeze execution identities once:

```bash
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
python3 tests/market-iv-local-vol/a1_run.py freeze \
  --directory build/dal-542-a1
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
python3 tests/market-iv-local-vol/a1_run.py run \
  --directory build/dal-542-a1
```

The default original input directory is `build/dal-542-formal` from the
accepted evidence, and binary directory is `build/dal-542-reference`.
The freeze records controls, actual sources/binaries/build/dependencies,
request, outputs, tiers, coordinates and the initial cumulative ledger hash.
Results use a separate namespace. Existing manifests and ledgers cannot be
silently reset. Exit 2 reports a numerical non-acceptance/checkpoint.

Execution stops at the first failed required study or grid gate. It first
runs eight Black and eight unchanged old-grid studies under the new method;
then actual capture/N4/probes, 16 required grid studies and 56 diagnostics.
Only complete reference/grid admission permits the new F2 MC. PDE uses at
most four workers; MC runs one family with 16 batch workers at a time.
The opt-in supervisor requires Linux `/proc` and POSIX spawn. It permits only
the fixed qualification executables and validated action-specific arguments;
metadata commands are read-only and use resolved absolute programs.
Budgets and phase elapsed are durably cumulative. An interrupted active phase
requires explicit ledger recovery, never a new budget. Each MC tier reserves
only its new prefix extension before launch; completed paths are recorded
separately. Exact cumulative sums are checkpointed between tiers and checked
before extension. A partially interrupted tier retains its conservative
reservation; it is not evidence of completed paths. No repeated family launch
is silently permitted. Resource stops retain INCOMPLETE and
completed reference evidence; unavailable references and FAIL remain distinct.

## Build and quick checks

Use Python 3.13 and the versions in `requirements.txt`. Initialize the pinned
core submodules, then configure the standalone harness:

```bash
cmake -S tests/market-iv-local-vol -B build/dal-542-reference \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DDAL_CPP_BUILD_TESTS=ON
cmake --build build/dal-542-reference --target \
  dal_market_iv_capture dal_market_iv_mc dal_market_iv_qualification_tests -j 12
build/dal-542-reference/dal_market_iv_qualification_tests
python3 -m unittest discover -s tests/market-iv-local-vol -p 'test_*.py' -v
```

Public, Python, Excel, portable Excel, examples and benchmarks are forced off.
All executables link only DAL::cpp. Existing product tests and the 57 focused
repair checks have accepted prior evidence; full suites and unrelated
benchmarks need not be rerun for this test-support change.

## Capture, grid PDE and formal MC

All commands below are foreground commands. Preserve logs and collect every
process result before ending an agent turn.

```bash
mkdir -p build/dal-542-formal
python3 tests/market-iv-local-vol/generate.py build/dal-542-formal/request.json
build/dal-542-reference/dal_market_iv_capture \
  build/dal-542-formal/request.json build/dal-542-formal/capture.json \
  --full-precision
python3 tests/market-iv-local-vol/prepare_mc.py \
  build/dal-542-formal/request.json build/dal-542-formal/mc-request.json
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
python3 tests/market-iv-local-vol/grid_study.py \
  build/dal-542-formal/capture.json build/dal-542-formal/grid-pde-qualified.json \
  --jobs 8
for fixture in F1_knots F1_flat F2_linear F2_nonlinear; do
  build/dal-542-reference/dal_market_iv_mc \
    build/dal-542-formal/mc-request.json \
    "build/dal-542-formal/$fixture-formal.json" "$fixture" tau_quarter 20
done
python3 tests/market-iv-local-vol/formal_report.py \
  build/dal-542-formal build/dal-542-formal/formal-qualification.json
```

Audit and freeze completed results separately:

```bash
python3 tests/market-iv-local-vol/verify_sampling.py \
  build/dal-542-formal build/dal-542-formal/sampling-verification.json
python3 tests/market-iv-local-vol/freeze_formal.py \
  build/dal-542-formal tests/market-iv-local-vol/fixtures
```

The retained formal rows are 184 PASS, 76 FAIL and 60 REFERENCE_UNAVAILABLE.
All 320 estimates/SE are populated. Each of four families used 16*2^20 paths;
all reached the SE gate. Both F1 fixtures' 160 rows pass N3. The smile failures
are price/time/grid/reference failures, not insufficient sampling.

All eight unavailable final tau/4 grid references fail the spatial
self-check. The two quarterly cases also fail domain expansion: normalized
differences are 2.849168e-5 (F2_linear) and 2.859273e-5 (F2_nonlinear), both
above 2.5e-5. Their time, mass and forward checks pass. These findings leave
the 320 row statuses and counts unchanged.

Prices from an uncertified grid reference, and the reported grid/stepping
amount decomposition using those prices, are diagnostic only; they cannot
certify the precise allocation of total error. The independently certified
one-year grid failure below and the MC/paired failures remain valid.
The reviewer found 4,032 off-node/first-column/boundary interpolation probes
consistent with production; no new production implementation bug is confirmed.

The attached minimal input reproduces a certified one-year ATM grid failure:

```bash
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
python3 tests/market-iv-local-vol/reproduce_grid_case.py \
  minimal-n5-grid.json minimal-n5-result.json
```

For F2_linear/time_tail, K=100.5 and T=1, the certified grid error/S0 is about
0.00111164 against 0.00025. Expected exit is 2. This focused command retains
the entire original production grid and changes only the payoff selection.

The MC executable returns zero when it successfully collects a run, including
runs with insufficient SE. Only the report's separate gates determine PASS.
Exponents below 20 are explicitly tagged SMOKE_NOT_QUALIFIED and are rejected
by the formal report. The maximum exponent 20 is an upper bound: actual tiers
stop only when every preregistered output has sufficient SE.

The MC runner parses JSON with RapidJSON's full-precision option. The capture
option above matches those actual coordinates. The legacy capture's default
parser can differ by one ULP at some decimal axes; adjacent artificial anchors
cannot support exact two-way subdivision. The request bytes, grid counts,
domains and mathematical axes are unchanged. Keep the exact capture, its hash
and parser mode with the PDE/MC evidence.

## Statistical protocol and actual production execution

Each fixture is a comparison family with seed 1024, MRG32 Precise, 16 fixed
blocks, Pmax=2^20 and Dmax equal to its largest fine timeline dimension.
Scalar positions are `((b*Pmax+p)*Dmax+j)`. Products and offsets are checked
against 64-bit limits, including the entire reservation. SkipNormalTo is used
on a scalar engine; Branch and repeated default-seed valuations are not used
as independent batches. A dedicated test compares every reserved batch's
scalar address with the production vector generator's address.

A 64-path check first compares the ordinary configured production stream and
scalar stream coordinate by coordinate, then compares generated paths and the
ordinary passive MC price. It uses the same fitted/grid model, seed, Precise
normal mode, bridge setting and compiled payoff evaluator.

The harness uses production CreateModel/Allocate/Init/GeneratePath. Its
timeline contains every original carry/grid anchor and T={91/365,1,2,2.25}.
Each anchor interval has n=ceil(48*dt) coarse steps, exactly 2n/4n/8n steps
at subsequent levels, and unchanged endpoints. The model verifies that it
introduced no additional internal step. Requested h is {1/48,1/96,1/192,1/384};
the full actual timelines are retained. Coarse normals are weighted sums of
the identical fine Brownian increments, divided by the coarse root dt.

The production compiled passive evaluator supplies five calls, five puts,
discount, discounted forward, five parities, forward mean, fixed payment and
delayed payment at every T. This retains all 320 registered outputs.
Time-independent payoff programs are prepared at a valid canonical calendar
event/payment pair to obtain the production observation/discount slots.
Actual sampled fields and discount-to-2.25 are generated by the model on the
qualification time axis, including the subday 2.25 endpoint. This tests the
payoff algebra and model/sample semantics; calendar request admission remains
covered by the accepted production integration tests. The separate ordinary
MC alignment uses an actual one-year calendar product without remapping.

Per-batch path counts are {2^16,2^17,2^18,2^19,2^20}. Each extension consumes
only the remaining common prefix, preserving all fine coordinate slots.
Every family upgrades solely for preregistered SE: SE/S0<=1e-4 for values and
paired-SE/S0<=5e-5 for adjacent-level differences. No oracle price is supplied
to the C++ stopping decision. Batch means yield df=15; 4.5*SE is the fixed
statistical band, without an unconditional coverage claim. Kahan summation
keeps constant-payment accumulation below its 2e-12 numerical gate.
All tier means, SE, 16 batch means, wall costs and coordinates are retained.

## Independent references and admission

`reference.py` implements F/D log interpolation and last-slope carry, erfc
Black, monotone PCHIP and SSVI without importing DAL. Quotes and arbitrary
precision derivatives use 90 digits and mpmath.diff. The vector PDE
coefficient is checked against that independently differentiated expression.
Actual fitted parameters are reconstructed; generating parameters are not
assumed to be recovered by optimization.

For phi=eta/sqrt(theta*(1+theta)), SSVI has
`w=theta/2*(1+rho*phi*k+sqrt((phi*k+rho)^2+1-rho^2))`.
With
`g=(1-k*w_k/(2*w))^2-w_k^2/4*(1/w+1/4)+w_kk/2`,
local variance is w_t/g. That ratio may remain regular when prices or absolute
Black densities underflow. The production repair preserves unavailable prices.

`pde.py` solves the independent log-spot backward equation with drift
L'-v/2 and killing -r, midpoint CN, two initial half implicit-Euler solves,
exact carry anchors and independent F/D boundaries. Calls/puts at all five
strikes and constant/spot terminal functions are solved together. Constant/D
and spot/(D*S0) check probability and the normalized forward moment.
Discrete coefficients interpolate vol linearly in log spot/time, with flat
boundaries, independently of production interpolation.

Continuous reference fixtures preserve the accepted 24 studies and 144
solves. They remain valid for the unchanged fitted snapshots and mathematics.
`grid_study.py` adds actual discrete-grid comparisons and separate reference
self-checks for tau/4 and the two-year time-tail grids. It refines space
{801,1601,3201}, time {512,1024,2048} and domain [-3,3] to [-4,4], changing one
setting at a time. Only the prescribed {6401,4096} fallback is allowed.
Every fallback retains the actual discrete coefficient. Failure of the
2.5e-5*S0 reference gate is REFERENCE_UNAVAILABLE; diagnostic differences from
such a reference cannot certify the grid. Other grid configurations retain
diagnostic levels without claiming a separately certified reference.

The formal report keeps price, SE, fit, grid, paired time and reference gates
separate. N3 fit/S0<=1e-6, |local vol-.20|<=1e-7; MC error/S0<=5e-5+4.5*SE/S0,
SE/S0<=1e-4; deterministic payment error<=2e-12. N5 grid difference/S0<=2.5e-4;
paired difference/S0<=2.5e-4+4.5*paired-SE/S0 with paired-SE/S0<=5e-5;
MC/continuous difference/S0<=6e-4+4.5*SE/S0 with SE/S0<=1e-4.
Missing runs, insufficient SE, failed grids and unavailable references remain
NOT_RUN, INCOMPLETE, FAIL or REFERENCE_UNAVAILABLE.

The original oracle fixtures and their checksums are retained. Production
status fields in the original derivative fixtures describe that historical
capture; current production comparisons come from the repaired capture.
Raw qualification evidence is attached to DAL-542. No additional resolution,
grid policy, density floor, seed, threshold or production PDE admission may be
changed just to obtain acceptance.
