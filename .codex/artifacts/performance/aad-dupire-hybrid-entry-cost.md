# F01 public Hybrid quote entry costs

Status: informational opt-in capability costs after correctness acceptance.
These comparisons do not establish an old-entry production regression verdict.

## Configuration and timing boundary

Release GCC 15 static native libraries on the shared Linux/WSL2 host, native
architecture flags disabled, lifetime/profiling OFF, one worker. The retained
core is `e1ff0dd6` plus the new public Hybrid adapter; compiled inputs and the
installed package are identified by the measurement manifest.

Sixteen cases combine flat/Merton bases, 9×2/21×8 surfaces, tree/compiled
evaluation and 257/2057 paths. Each uses six spread quotes, deterministic carry,
a one-year smooth quadratic payoff with an affine direct quote term and a
0.25-year simulation step. Every timed request uses an already frozen
calibration; snapshot creation is separately measured in the
[core cost report](aad-dupire-entry-cost.md).

Modes:
- `legacy`: existing `ValueByMonteCarlo` price/model-risk dictionary.
- `structured`: existing scalar `ValueByMonteCarloWithRisk`.
- `quote`: that valuation followed by the new public quote pullback/wrapper.
- `pullback`: the new public operation on an already completed passive valuation,
  including model restoration, complete-axis/surface validation, scalar VJP,
  direct contribution and retained-result copying.

The standalone helper performs three warmups and ten timed requests for short
cases or three for long cases. Two rounds use ten independent process samples
per case/mode with deterministic shuffled case/mode ordering. All 1,280
processes finish. Builds and functional tests are closed before timing.

## Results

Microseconds per warm request; best-of-20 process measurements. Each round's
best-of-ten values and every raw sample remain in the JSON evidence.

| Base | Surface | Evaluator | Paths | Legacy | Structured | Complete quote | Public pullback |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| flat | 9×2 | tree | 257 | 224.759 | 246.882 | 298.318 | 40.347 |
| flat | 9×2 | tree | 2057 | 1526.082 | 1543.155 | 1599.034 | 41.457 |
| flat | 9×2 | compiled | 257 | 222.697 | 240.937 | 296.300 | 40.251 |
| flat | 9×2 | compiled | 2057 | 1505.383 | 1526.219 | 1577.065 | 41.131 |
| flat | 21×8 | tree | 257 | 412.452 | 445.462 | 763.288 | 273.439 |
| flat | 21×8 | tree | 2057 | 1784.974 | 1819.457 | 2153.703 | 274.395 |
| flat | 21×8 | compiled | 257 | 406.486 | 435.540 | 758.252 | 271.366 |
| flat | 21×8 | compiled | 2057 | 1749.339 | 1783.788 | 2118.780 | 271.548 |
| merton | 9×2 | tree | 257 | 223.270 | 247.214 | 298.315 | 39.895 |
| merton | 9×2 | tree | 2057 | 1520.718 | 1545.836 | 1597.025 | 40.959 |
| merton | 9×2 | compiled | 257 | 221.748 | 244.698 | 295.460 | 39.957 |
| merton | 9×2 | compiled | 2057 | 1506.582 | 1521.151 | 1584.919 | 41.044 |
| merton | 21×8 | tree | 257 | 412.595 | 442.876 | 767.081 | 269.614 |
| merton | 21×8 | tree | 2057 | 1776.846 | 1823.072 | 2140.883 | 271.142 |
| merton | 21×8 | compiled | 257 | 407.910 | 440.579 | 761.673 | 269.932 |
| merton | 21×8 | compiled | 2057 | 1749.582 | 1792.533 | 2119.819 | 270.281 |

The public operation costs about 40–42 microseconds on 9×2 surfaces and
270–275 microseconds on 21×8 surfaces. Complete quote requests add about
51–64/318–335 microseconds over the corresponding structured requests.
Model restoration/validation and result retention are included; these are
additional capabilities with different outputs. Applying the old 4% regression
threshold between these modes would compare different operations.

## Identity and acceptance limits

Before timing, the structured result's complete legacy price/every-risk map
must match the old entry bitwise. The direct input agrees with its analytic
discounted PV derivative at 1e-10. Each timed request preserves its price;
each quote/pullback request preserves every quote adjoint bitwise. Prices,
quote sums and geometry remain bitwise identical across every process/mode
for each case.

All 1,040 source/helper/package/binary hashes are unchanged before/after
measurement. The default core archive remains SHA-256
`97b4b2d7cfba2ed45b48b597b98c94fc05a6adaef8245a594d0df5062b3e9325`,
and all nine existing gate executables remain identical to the retained
accepted measurement binaries. This establishes artifact identity for those
unchanged paths, not the missing production performance verdict. P01 remains
inconclusive on this host; its failed timings and original policy are unchanged.

Correctness precedes this measurement: 18 focused public checks, all 321
predeclared node/quote/portfolio difference rows, full OFF/combined CTest
2,382/2,395, 28 instrumented ASan/UBSan checks and both installed consumers
in each configuration pass. New-head CI, Python/Excel bindings, curve
adaptation and complete F01 acceptance remain required.

## Evidence

Root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- `aad-dupire-hybrid-cost-source/{cost.cpp,CMakeLists.txt}` and
  `run_aad_dupire_hybrid_cost.py`.
- `aad-dupire-hybrid-cost-measurements-01/`: protocol, input manifests,
  every process's stdout/stderr, samples and two-round summary.
- `aad-dupire-hybrid-cost-run-01.log`: terminal 1,280-process result.
- `aad-dupire-hybrid-off-gate-identity-01.json`: all nine unchanged binaries.
