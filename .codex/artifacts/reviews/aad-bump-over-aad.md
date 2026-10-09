# Native directional curvature implementation review

## Findings

No open local correctness or style findings. Review includes the complete new
driver, public header, twelve tests, benchmark harness/dispatch, diagnostic CI
filters, active controls and methodology. All ordinary library bodies are unchanged.

The numbers-only initial callback could not compose recorded operators. A focused
compile RED demonstrated the gap; the scope-taking callback and independent
rational-solve HVP test pass. A post-callback state check precedes root access.
The first tape-budget test incorrectly omitted the documented cleanup reserve;
the corrected test admits peak plus reserve and rejects one byte less. Production
budget semantics were already consistent with the existing capacity contract.

## Tests

- Missing-header RED, independent quadratic GREEN and recorded-composition RED/GREEN.
- Eleven unchanged edge cases pass in `edges.xml`; the corrected budget case
  passes in `budget-green.xml`. Together they validate all twelve final test cases.
- Eleven strict C++17 probes pass: header, production, tests and two benchmark
  translation units under OFF/combined modes, plus the baseline-only harness.
- Formatting passes. Production/benchmark cyclomatic complexity is at most 6/7.
- Documentation integrity passes for 166 Markdown files.
- Installed-only consumption and three selected cost comparisons are pending.

Local evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261008`, with
`bump-over-aad-first/` RED/GREEN records and `bump-over-aad-strict.json`.

## Open questions

None for the deterministic native primitive. Financial common paths/smoothing,
full quote recalibration, policy response and mixed-mode capabilities remain
separate plan requirements. Reference captures are caller-owned fixed state;
copying a function object cannot deep-freeze them.

## Summary and verdict

Comment Only until installed consumption, selected costs and publication gates
complete. Residual risks are platform/sanitizer runtime and external findings;
exact-head CI must actually execute this suite before merge. No stochastic or
exact higher-order guarantee is inferred from smooth-kernel tests.
