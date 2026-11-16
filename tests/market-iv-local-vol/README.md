# Independent market-IV/local-vol reference checkpoint

DAL-542 / S3 is **BLOCKED**, not numerically qualified. On baseline
`53a2b1592993f3a1d354df2d6ec2ae6918e5ef6d`, all four frozen fits succeed,
but every frozen Dupire grid is rejected before model construction. The small
reproducer below fails at `S=20`, `t=1/36500` even with fitted `rho=eta=0`.
The independent local variance is `0.04`; the production point reports
`NUMERIC_RANGE` because its extremely small put price is unrepresentable.

This directory contains independent Black/F/D, 90-digit quote and derivative
generators, monotone PCHIP, continuous and discrete-coefficient log-spot PDE
references, a core-only production capture, reference checks and frozen data.
It changes no production code or PDE admission rule. Its executable returns
2 for rejected production grids. The report entry point also returns 2 because
S3 is incomplete, including when the independent reference self-checks pass.

## Reproduce

Use Python 3.13 with the versions in `requirements.txt` (the captured environment
used Python 3.13.9). Initialize the repository's pinned Eigen, Google Test,
Machinist and RapidJSON submodules, then configure the standalone test harness:

```bash
cmake -S tests/market-iv-local-vol -B build/dal-542-reference \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DDAL_CPP_BUILD_TESTS=ON
cmake --build build/dal-542-reference --target dal_market_iv_capture dal_cpp_tests -j 12
python3 -m unittest discover -s tests/market-iv-local-vol -p 'test_*.py' -v
ctest --test-dir build/dal-542-reference/workspace --output-on-failure \
  -R 'MarketIvLocalVolTest|MarketIvSurfaceTest'
```

The harness forces public, Python, Excel, examples and benchmarks off; only
`DAL::cpp` and its private RapidJSON headers are used. Ordinary core/public
build files and CI test selection are unchanged. No facade examples changed.

Run the minimal failing production reproducer independently of PDE or MC:

```bash
mkdir -p build/dal-542-evidence
python3 tests/market-iv-local-vol/generate.py \
  build/dal-542-evidence/minimal-request.json --minimal
build/dal-542-reference/dal_market_iv_capture \
  build/dal-542-evidence/minimal-request.json \
  build/dal-542-evidence/minimal-capture.json
# Expected exit 2; inspect fit_success, snapshot and grids[0].invalid_examples.
```

Run the independent reference study and capture the whole frozen grid matrix:

```bash
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
python3 tests/market-iv-local-vol/run.py \
  --capture build/dal-542-reference/dal_market_iv_capture \
  --output build/dal-542-evidence --jobs 8
# Exit 2 preserves BLOCKED, FAIL and NOT_RUN; it is not an infrastructure error.
```

Outputs are `request.json`, `capture.json`, `qualification.json`,
`provenance.json` and `capture.log`. They record actual fitted snapshots, all
grids' failure counts/examples, every PDE level, derivative comparisons, and
per-K/T/carry unexecuted product rows. Oracle values are separate from absent
MC estimates and SE. The capture makes no RNG draws.

Freeze a completed capture explicitly, without silently changing fixtures:

```bash
python3 tests/market-iv-local-vol/freeze.py \
  build/dal-542-evidence tests/market-iv-local-vol/fixtures
```

`fixtures/manifest.json` contains SHA-256 checksums for the quoted inputs,
actual fitted snapshots, 90-digit derivatives and complete PDE self-check
levels. Full production failure/qualification evidence is attached to the
DAL-542 issue. Runtime build paths are working copies, not delivered artifacts.

## Mathematics and reference admission

`reference.py` interpolates log F and log D independently, extends their last
slopes and evaluates Black using `mpmath.erfc`. F1 uses the exact S0 fit
settings and mixed PRICE/DECIMAL_IV, call/put quotes, one quote per economic
K/T. F2 uses rho=-0.35, eta=0.60 with linear or the prescribed nonlinear theta.
The reference reconstructs the **actual fitted** theta and its PCHIP slopes;
it never assumes the optimizer recovers the generating parameters.

For `phi=eta/sqrt(theta*(1+theta))`, the SSVI variance is
`w=theta/2*(1+rho*phi*k+sqrt((phi*k+rho)^2+1-rho^2))`.
The 90-digit reference differentiates this expression with `mpmath.diff`.
With `g=(1-k*w_k/(2*w))^2-w_k^2/4*(1/w+1/4)+w_kk/2`,
the local variance is `w_t/g`. This ratio can be regular even when a Black
price or density underflows in double precision. The separate vector PDE
coefficient is checked against these derivatives, including knots, tails and
the short first-column times.

`pde.py` solves `V_t + v/2*V_xx + (L'-v/2)*V_x - r*V=0` for
`x=log(S/100)`, `L=log(F/100)` and `r=-d(log D)/dt`. It uses midpoint
Crank–Nicolson, two half implicit-Euler solves replacing the first step from
the payoff, exact carry-knot anchors and independent asymptotic discounted
forward/payoff boundaries. It solves all five strikes' calls/puts and the
constant/spot terminal functions together. Constant/D and spot/(D*S0) test
the probability and normalized forward moments; no production MC is used
to validate the reference. Linear-solve residuals are diagnostic raw output.

Spatial {801,1601,3201}, time {512,1024,2048} and domain [-3,3] to [-4,4]
studies change one setting at a time. Each final difference must be at most
`2.5e-5*S0`; the Black limits and mass/forward checks also have the frozen
`2.5e-5` gate. Only the fixed {6401,4096} fallback is permitted. Failure after
that level is `REFERENCE_UNAVAILABLE`. Midpoint coefficients and the initial
implicit steps use no t=0 singular derivative evaluation. Discrete
coefficients independently interpolate **vol**, linearly in log spot/time,
with flat boundaries. The discrete interpolation has a fast synthetic check;
production-grid PDE comparisons are NOT_RUN because grid construction fails.

## Remaining S3 work

The production repair belongs to the coordinator and the S1/S2 authors.
Do not move tau, narrow the spot domain, omit failing nodes, loosen thresholds,
or reinterpret warnings as exemptions. Regenerate only directly affected
captures after the repair; the continuous reference evidence remains reusable
when its source, fitted parameters, dependencies and configuration match.

The formal production-path statistical runner is **not implemented in this
blocked checkpoint**. No 16-batch result, Brownian pairing or default-stream
alignment is claimed. It must use seed 1024, MRG32 Precise, b=0..15,
Pmax=2^20, fixed scalar slots `((b*Pmax+p)*Dmax+j)`, checked 64-bit offsets,
and fine-increment aggregation for coarse paths. Per-batch tiers are
{2^16,2^17,2^18,2^19,2^20}, upgraded only for SE; df=15 and 4.5*SE are fixed.
Do not use `Branch`, repeated default-seed calls or Sobol IID assumptions.

N3 still needs production call/put/forward/parity/fixed and delayed PAYS/DF,
fit price/S0 <=1e-6, |local vol-.20| <=1e-7, statistical error/S0
<=5e-5+4.5*SE/S0 and SE/S0 <=1e-4. N5 still needs separate grid/continuous
PDE error <=2.5e-4*S0, paired h/h2 error <=2.5e-4*S0+4.5*paired-SE with
paired-SE/S0 <=5e-5, and total error <=6e-4*S0+4.5*SE with SE/S0 <=1e-4.
Use h={1/48,1/96,1/192}, plus the paired 1/384 fine level, preserving every
anchor. The time-tail case with the grid ending at two years remains required.
DF payment error <=2e-12 is deterministic. Every absent value stays NOT_RUN;
insufficient sampling is INCOMPLETE. This checkpoint is not S3 acceptance.
