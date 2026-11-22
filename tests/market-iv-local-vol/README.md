# Independent market-IV/local-vol qualification

DAL-542 / S3 has **480/480 PASS**: 160 reused F1 rows and 320 new F2 rows.
The numerical execution used source `39338e26de8ab11ca1e3a9e24e0630d215e38421`
and the frozen A/3 control,
SHA-256 `acf410567786916601584b71a954991b0a2c81f180aab734cd4be0091ed96ec7`.
The original 320-row history remains 184 PASS / 76 FAIL /
60 REFERENCE_UNAVAILABLE. Publication cleanup preserves the completed results;
it does not claim a new numerical execution.

The full report, raw block means, exact checkpoints, inputs, executed source,
binary/build identities, references and the cumulative ledger are attached to
DAL-542 as `dal-542-a3-evidence.tar.xz`. Its sealed A/2 dependency is
`dal-542-a2-evidence.tar.xz`. Old execution/recovery controllers and generated
run snapshots are retained in those archives and Git history.

The current [qualification method](../../docs/methodology/monte-carlo/market-iv-local-vol-validation.md)
describes the gates, canonical grids, fixed Brownian coordinates and evidence
reuse. Model admission and request `/2` compatibility are documented in the
[model guide](../../docs/models/local-volatility.md#request-identity-and-execution-reports).
These guides do not replace or rehash the executed control in the sealed evidence.

Two families each used 16,777,216 fine fields, seed 1024, MRG32 Precise,
16 blocks and no bridge. Both used the five fixed SE-only tiers. Final maximum
price SE/S0 is 5.977259e-5 / 7.194086e-5; maximum paired SE/S0 is below 7.51e-7.
The final view uses h=1/3072 and its pair with h=1/1536. No threshold changed.
The same ledger totals 290 solves and 15715.218988 numerical seconds;
no numerical process remains running. Reviewer/CI acceptance is separate.

## Build and quick checks

Use the Python packages pinned in `requirements.txt`. Generate the canonical
input into the build directory; the generator refuses to overwrite different
existing input. Its bytes match the executed request exactly (SHA-256
`2818bfc153f3ae8b12980bcdf2adc5d400deb47a2abee474d013ec58cc7721db`).

```bash
python3 tests/market-iv-local-vol/a1_axes.py build/dal-542-reference/request.json
cmake -S tests/market-iv-local-vol -B build/dal-542-reference \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DDAL_CPP_BUILD_TESTS=ON
cmake --build build/dal-542-reference --target \
  dal_market_iv_capture dal_market_iv_a1_mc dal_market_iv_a1_probe \
  dal_market_iv_a1_protocol_tests -j 12
ctest --test-dir build/dal-542-reference -R MarketIvA1Test --output-on-failure
OPENBLAS_NUM_THREADS=1 python3 -m unittest discover \
  -s tests/market-iv-local-vol -p 'test_*.py' -v
```

The standalone build links DAL::cpp; public/language components, examples and
benchmarks are off. `DAL_A1_REQUEST` can select an existing canonical input
for the protocol tests. Quick checks exercise axes, strict nesting, scalar
seeking, df=15, nonuniform differences, reference admission and SE guards.
They do not launch the formal qualification matrix.

## Numerical reproduction entry points

The standalone quote-refit check rebuilds each side from the original quotes
using the core fit, Dupire and model APIs. It compares the complete quote
axis, selected branch, grid values and identities. Its fixtures include a
one-sided call/put parity failure and a successful pair with different
branches.

```bash
cmake -S tests/market-iv-local-vol -B build/p22 -DCMAKE_BUILD_TYPE=Release
cmake --build build/p22 --target dal_market_iv_refit_pair
build/p22/dal_market_iv_refit_pair
```

### Script PV refit risk

```bash
cmake -S tests/market-iv-local-vol -B build/p24 -DCMAKE_BUILD_TYPE=Release -DDAL_CPP_BUILD_TESTS=ON
cmake --build build/p24 --target dal_market_iv_refit_pv
ctest --test-dir build/p24 -R MarketIvRefitPvTest -V
```

The fixture prices a one-year European call in USD on 2026-11-16 with spot
100, forward 105, discount .96 and strike 105. The three call quotes at
strikes 90, 105 and 120 have decimal IV .20. Dupire spot nodes are 80, 100
and 120, with times 1/8, 1/2 and 1. Each compiled valuation uses factor
`W_EQ`, MRG32 Precise seed 1024, and three chronological normals per path
with the bridge off. For path `p` in block `b` of size `N`, interval `j`
uses scalar draw `3*(b*N+p)+j`, indexed from zero.

The independent driver rebuilds each bumped quote set and Dupire grid, reads
the scalar stream in order, evolves log spot at the left-endpoint grid vol,
and discounts the terminal payoff. The coordinate check compares individual
path PVs for base, plus and minus. The single-bucket check bumps `atm` by
`s=1e-4` and `5e-5` with 16 blocks of 257 paths. For each step, the side
means must agree to `1e-12*L` and the paired slope and SE to `1e-12*L/s`,
where `L=max(1,abs(PV+),abs(PV-))`. The output includes both estimates,
the slope change from full to half step, and each fit's branch relative to base.

The Black check shifts all three IVs together by ±1e-4, refits each side,
and compares the directly sampled slope with the discounted Black call's
central difference. Its fixed budget is 16 blocks of 131072 paths per scenario.
Both `SE/S0 <= 1e-3` and `abs(estimate-Black)/S0 <= 1e-3 + 4.5*SE/S0`
must pass. CTest reports a failure if the budget falls short.

The installed consumer `dal_installed_market_iv_script_refit`,
`dal-python/tests/test_market_iv_script_refit.py` and the Windows runner below
exercise the same single bucket and Black direction through each binding.
The Black calculation pairs the 16 base block means from the two jointly
shifted snapshots. The premium fixture moves the ATM call by ±0.01 with a
put parity band and checks the surviving side and the following IV bucket.
The worksheet runner saves the recalculated workbook alongside its JSON
capture, then erases the request handles and reads the result rows again.

```powershell
python dal-excel/tests/windows/run-market-iv-script-refit.py --xll build/Release-windows/dal-excel/dal_excel.xll --output script-refit.json
```

Capture the installed consumer's stdout and the Python test's `pytest -sq`
output as UTF-8. Use binaries from the same build for exact row comparison.

```sh
python tests/market-iv-local-vol/compare_script_refit.py --sdk sdk.txt --python pytest.txt --excel script-refit.json
```

### PDE and Monte Carlo qualification

Long calculations are explicit. The completed qualification budget is sealed;
these commands describe reproduction and must not overwrite the archived run
or create a fresh official budget.

The native runner uses production CreateModel/Allocate/Init/GeneratePath and
the compiled passive payoff evaluator. Its tiny invocation checks all eight
models, 64-path default-stream coordinate/path/price alignment and Brownian
nesting. The first formal tier restores those exact tiny sums:

```bash
b=build/dal-542-reference
"$b/dal_market_iv_a1_mc" "$b/request.json" "$b/F2_linear-alignment.json" F2_linear 0
"$b/dal_market_iv_a1_mc" "$b/request.json" "$b/F2_linear-tier-16.json" F2_linear 16 \
  "$b/F2_linear-alignment.json.checkpoint.json"
```

For either F2 family, later exponents are 17..20 and require the previous
checkpoint. Continue only for insufficient registered SE; the native runner
rejects extending sufficient formal SE or a mismatched prefix. Each fine field
drives P7/P8 × four h values; coarse Brownian increments aggregate the same
fine increments. Dimensions are 1092/2184/4368/8736 with all 438 anchors.
Only segment 60 receives n=4, guarded by its exact endpoints. Scalar addresses
are `(((f*16+b)*2^20+p)*8736+j)`; the final address is 293131517951.
Old Dmax1584/8728 raw is not used for new statistics.

Capture and high-precision node/probe checks have separate direct entries:

```bash
"$b/dal_market_iv_capture" "$b/request.json" "$b/capture.json" --full-precision
python3 tests/market-iv-local-vol/a1_n4.py nodes "$b/capture.json" "$b/n4"
python3 tests/market-iv-local-vol/a1_n4.py inputs "$b/capture.json" "$b/n4"
"$b/dal_market_iv_a1_probe" "$b/n4/probe-input.txt" > "$b/n4/probe-actual.txt"
python3 tests/market-iv-local-vol/a1_n4.py probes "$b/capture.json" "$b/n4"
```

`a1_study.certify` and `a1_study.task` retain the frozen nonuniform reference
method and its six base / unique four fallback solves. `a1_pde.py` retains
central 601/1201/2401/4801 and matched expanded meshes. `reference.py`,
`pde.py` and `qualify.py` retain independent Black/F/D/SSVI/PCHIP mathematics
and continuous reference checks. Grid coefficients interpolate sigma linearly
in log spot/time with flat boundaries. Production code is not an oracle.
Only `fixtures/snapshots.json` and `fixtures/pde.jsonl` remain as small
continuous-reference fixtures; runtime products belong in build/evidence.

## Replay completed evidence without simulation

After verifying and extracting both attached archives:

```bash
python3 verification/verify_a3_archive.py dal-542-a3-evidence.tar.xz dal-542-a2-evidence.tar.xz
python3 tests/market-iv-local-vol/a1_verify.py numeric/a3/F2_linear-mc.json
python3 tests/market-iv-local-vol/a1_verify.py numeric/a3/F2_nonlinear-mc.json
python3 tests/market-iv-local-vol/a1_report.py numeric/a3 > replayed-qualification.json
```

The verifier recomputes all 16-block means/SE and SE-only stopping decisions.
The report preserves all 480 rows and refuses missing reference/admission or
insufficient family SE. These are read-only checks, not MC/PDE reruns.
Uncertified old-grid prices and their grid/stepping decomposition remain
diagnostic. The archived 10 P3 expanded revalidations, 48 central layers,
8 Black studies, 16 required grid studies, 393782 N4 nodes and 1984 probes
retain their original execution identities. The 8 old unavailable studies
and 56 single-layer INCOMPLETE diagnostics retain their true statuses.
