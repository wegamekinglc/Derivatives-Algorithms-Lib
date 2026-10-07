# Native diagnostic solve review

Findings: no outstanding local correctness, ownership or style finding.

Reviewed implementation: `5bfbaee1d69be781a7d095726510f42b48e5dc89`, compared
with accepted #493 at `c3f93930`. Documentation-only acceptance updates follow
that implementation without changing its compiled runtime.

## Open questions

None needs a user decision. Required exact-head cross-platform CI, Codacy and
all review threads remain publication gates. This is a local review in the
implementing session, not independent external approval.

## Tests

Initial missing-API RED is retained; the analytic composition/repeated-seed and
post-close report case is GREEN. Thirteen new cases pass locally and the
affected solve batch passes 72/72. The three-activity independent Cramer/difference
case additionally passes scalar and width-1/4/8 combinations. Installed CMake
consumption passes, and its runtime object matches the final compiled object.
Formatting, patch and documentation checks pass.

Codacy found the combined activity/difference test above its complexity limit.
The test-only repair extracts sample setup, seeding and independent coordinate
checks, preserving all activity/width/step combinations. Local Lizard checks
all changed C++ functions at complexity eight or below; the repaired oracle
filter passes. Production source is unchanged and scoped timing remains valid.

Numeric tests correctly allow rounding residuals from normalized LU rather than
assuming exact zero. Resource tests separately retain exact healthy-graph peak
admission and one-byte-short rejection, while allowing the existing reserved
cleanup replacement peak during poisoned-context recovery. No production
behavior was relaxed for these corrected test expectations.

## Summary

The optional result copies its report outside the event allocation scope before
output publication. Cached report destruction stays inside the live event
account. Ordinary caches/layouts and public includes are preserved; the reverse
implementation is shared by cache type. All 166 other library objects match the
baseline. Six ordinary boundary comparisons satisfy the unchanged two-round
4% rule; opt-in costs are reported separately. No whole-matrix timing claim is made.

Residual risk is the not-yet-inspected Windows, sanitizer and OFF/ON diagnostic
CI evidence. Native outputs still obey scope lifetime, while the passive report
survives close/restore. Higher-order and structured coordinate support remain
outside this increment.

Verdict: Comment Only, pending exact-head publication gates.
