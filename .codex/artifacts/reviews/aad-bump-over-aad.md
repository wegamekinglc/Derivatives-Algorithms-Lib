# Native directional curvature implementation review

## Findings

No open local correctness or style findings. Review includes the complete new
driver, public header, thirteen core tests, benchmark harness/dispatch, diagnostic CI
filters, active controls and methodology. All ordinary library bodies are unchanged.

The numbers-only initial callback could not compose recorded operators. A focused
compile RED demonstrated the gap; the scope-taking callback and independent
rational-solve HVP test pass. A post-callback state check precedes root access.
The first tape-budget test incorrectly omitted the documented cleanup reserve;
the corrected test admits peak plus reserve and rejects one byte less. Production
budget semantics were already consistent with the existing capacity contract.

A subsequent allocation-failure audit reproduces a mode leak at allocation index
6: the old entry selected scalar mode before allocating its restoration guard.
The isolated regression test fails on that exact allocation. The driver now
establishes the existing resetter on the stack before switching mode, preserving
both lifecycle checks and allocation-free restoration. The regression GREEN
injects failure at every measured request allocation and verifies recovery. The
private shared allocation probe adds a disabled-by-default, thread-local one-shot
failure seam; its existing callers require focused count checks. No shared native
header or existing library hot body changes.
The isolated regression, three affected mode/failure/nesting cases and four
existing private-probe caller cases pass. Ten affected strict OFF/combined checks
pass. The remaining original cases retain unchanged bodies and arithmetic;
repaired-head installed consumption passes 1/1. The same three selected costs
complete 120 observations in 2.180 seconds; both old-caller rounds pass.

Upstream #516 integration rebuilds nineteen dependency-selected library units,
preserves the other 161 accepted member occurrences and validates all thirteen
new cases plus installed consumption. Review finding `4230078427` is valid:
the manual-secant benchmark retained a heap guard after production moved to the
stack resetter. The baseline-only branch now uses the same stack guard and
lifecycle check. Three affected strict probes and both analytic benchmark smoke
cases pass; the head timing body is unchanged. Corrected new-entry costs are
+11–16% informational overhead. The existing short control passes both +4%
rounds with twenty samples per side, while shared-host noise remains disclosed.
No unrelated local full suite or cost matrix is repeated.

Review finding `4230306027` is valid: a callback can checkpoint, build an output
and restore while the scope remains recording. A focused regression against the
previous archive fails because no rejection occurs. The new internal helper
retains `Begin` validation and rejects nonzero checkpoint generation before
output access. Each new scope starts at zero; a valid restore requires a checkpoint
from that scope. There is no new allocation, scope/tape layout or mode mutation.
The thirteen core cases and isolated fault case pass; the new case covers
checkpoint-only and restore behavior at base/plus/minus with mode restoration
and recovery. Three existing event/solve checkpoint/state cases and eight
affected strict probes pass; installed consumption passes 1/1. The same three
cost comparisons finish 160 observations in 2.447 seconds; both old-caller
rounds pass and new-entry overhead is +13–16%, informational. Current strict
coverage totals 21 applicable probes, retaining unaffected evidence.

## Tests

- Missing-header RED, independent quadratic GREEN and recorded-composition RED/GREEN.
- Eleven unchanged edge cases pass in `edges.xml`; the corrected budget case
  passes in `budget-green.xml`. Together they validate all twelve final test cases.
- Eleven strict C++17 probes pass: header, production, tests and two benchmark
  translation units under OFF/combined modes, plus the baseline-only harness.
- Formatting passes. Production/benchmark cyclomatic complexity is at most 6/7.
- Documentation integrity passes for 167 Markdown files.
- Installed-only recorded-solve consumption passes 1/1 with independent rational HVP checks.
- Three selected comparisons finish 120 observations in 1.844 seconds. The old
  caller passes both +4% rounds; new capability overhead is disclosed separately.

Local evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261008`, with
`bump-over-aad-first/` RED/GREEN records and `bump-over-aad-strict.json`.

## Open questions

None for the deterministic native primitive. Financial common paths/smoothing,
full quote recalibration, policy response and mixed-mode capabilities remain
separate plan requirements. Reference captures are caller-owned fixed state;
copying a function object cannot deep-freeze them.

## Summary and verdict

Approve for publication; current-head CI and external review remain merge gates.
Residual risks are platform/sanitizer runtime and external findings;
exact-head CI must actually execute this suite before merge. No stochastic or
exact higher-order guarantee is inferred from smooth-kernel tests.
