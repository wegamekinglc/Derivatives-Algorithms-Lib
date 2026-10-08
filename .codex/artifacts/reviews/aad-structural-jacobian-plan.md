# Structural Jacobian plan review

Verdict: Comment Only — local gates pass; publication remains open.

## Findings

No unresolved correctness or style findings in the reviewed numeric boundary.
Read the complete three new C++ files, controlling spec/API/critique, workflow
filter additions, ledger and published changes. Normalization/coloring uses
owning metadata and only used-column incidence. Empty rows consume no direction;
full matrix shape and exact zeros remain intact. Byte multiplication/addition,
lower-bound and final budgets precede publication. Recovery rejects all
non-finite gradients before result assembly and leaves source state untouched.

The API deliberately cannot prove caller-provided mathematical support. Published
documentation and the active controls make this conditional boundary explicit.
Numerical zeros never build the pattern. Copies and detached results own their
data; moved-from plans require reassignment before use. Existing dense/weighted
and native kernels remain unchanged. P04 is not marked complete.

## Open questions

None for the numeric plan. Native independent-slot/current binding identity,
seeding/clearing, financial proof, dynamic invalidation and strategy selection
remain subsequent separately controlled requirements.

## Tests and evidence

- Missing-header RED and an intermediate test iterator compile failure are
  retained. The repair changes test mutation to indexed writes without weakening
  assertions. The formatted final batch passes 10/10 in 1 ms.
- Independent D.4 recovery and all 512 small linear support patterns compare
  every coordinate, with same-color disjointness/group coverage checks.
- Zero-to-nonzero dependencies, conservative supersets, empty/wide axes, canonical
  snapshots, exact budgets, invalid shapes/indices/non-finite data, recovery after
  failure and four concurrent const requests pass.
- Six strict OFF/combined-diagnostic source/header checks pass. Clang-format and
  patch integrity pass; all 34 functions meet complexity <= 8.
- Core target compiles only the new cold source. Installed-only consumer passes
  1/1; compile commands contain installed headers and no source-tree DAL include.
- All 176 old archive members/object files and seven fresh legacy callers retain
  accepted bytes. Two new costs pass 40 observations in 0.3613 seconds; zero old
  timing rows repeat. No full local suite or benchmark matrix is justified.

## Summary and delivery

The focused published guide explains actual numeric recovery, proof, ownership
and budget semantics. Its new numerical capability qualifies for a changelog
entry. Protected CLAUDE guidance is unchanged under the user's explicit rule.
The completed #506 controls are archived through pinned Git links in the ledger.
New suite selectors are appended to all six sanitizer filters.

Exact-head CI/Codacy, complete current review bodies/threads, all ten new cases
in fourteen actual runtime profiles and guarded tested/merged tree equality are
required before merging this PR or starting the native execution increment.
