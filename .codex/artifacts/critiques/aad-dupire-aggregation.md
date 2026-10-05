# F01 portfolio aggregation oracle review

Verdict: Proceed with caveats. Use the controlling full-recalibration protocol
for mathematical acceptance and retain separate-reverse roundoff as evidence.

## Finding and diagnosis

The first distinct-trade test compared `VJP(g1 + g2)` with
`VJP(g1) + VJP(g2)` at absolute 1e-10. Its first difference was
5.9226294979453087e-9; all six differences are retained. The maximum is
6.5083440858870745e-9. A standalone executable linked to the unchanged,
already published core archive reproduces all six differences bitwise, without
the new Hybrid adapter. The archive SHA-256 remains
`97b4b2d7cfba2ed45b48b597b98c94fc05a6adaef8245a594d0df5062b3e9325`.

That comparison assumes real-arithmetic distributivity across differently
rounded reverse accumulations. The existing small discrete Dupire stencil has
large intermediate derivatives, so it does not provide an exact floating
reference for distinct seeds. The independent power-of-two control is bitwise
equal at every quote. No production calibration, interpolation, native reverse
or simulation arithmetic changes in response to this diagnostic.

## Oracle repair and preserved cases

- Keep the strict 1e-10 sum assertion for two identical actual valuations,
  requiring every extracted seed to agree first. The doubled-seed control
  tests exact scaling and compatibility through the public boundary.
- Retain both distinct trades and all their original numeric seeds. Compare
  their aggregated pullback against independently recalibrated portfolio
  prices for each of six quote buckets and a direction, using the already
  declared steps 2e-4/1e-4/5e-5 and abs 1e-3 plus rel 1e-3. Keep all rows.
- Freeze the original trade's direct script constant during these portfolio
  bumps; no direct quote seed was supplied to the aggregate pullback.
- Keep the original failed source, executable control, full six-row diagnostic
  and input hashes. Do not present this test-reference repair as a production
  RED/GREEN fix or as proof of exact distributivity for general floating seeds.

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.
Files: `aad-dupire-hybrid-aggregation-diagnostic-01.log`,
`aad-dupire-hybrid-aggregation-diagnostic-source-01.cpp`,
`aad-dupire-aggregation-control-source/control.cpp`,
`aad-dupire-aggregation-control-02.log`,
`aad-dupire-aggregation-control-inputs-01.sha256` and
`aad-dupire-hybrid-portfolio-off-01.log`.

## Acceptance and limitations

The repaired suite passes 18 tests. All 21 new portfolio finite-difference
rows pass; all original 300 Hybrid node/quote rows remain bitwise identical.
The complete 321 rows pass without changing finite-difference steps, tolerances,
paths, calibration references or valuation kernels. Instrumented/cross-platform
verification and full F01 binding/curve/performance acceptance remain separate.
This evidence supports the declared discrete derivative accuracy, not an
unconditional 1e-10 commutation claim for separately rounded reverses.
