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
