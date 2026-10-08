# Native solve accuracy local review

Verdict: Comment Only; local acceptance passes, remote gates remain open.

## Findings

No remaining actionable local findings.

The empty-graph wrapper initially accepted an invalid raw native width. A
focused RED reproduces that boundary; the wrapper now invokes the native mode
validator before creating invocation metadata. Report RHS/channel extents also
check the supported buffer range before allocation. Eight affected mode,
report/scratch, late-failure, copy and thread cases pass after this repair.

The first tape-budget fixture attached a capacity scope after entering recording,
violating the existing lifecycle. The corrected fixture admits before scope
creation. Its global peak accounts for the independent entry-replacement peak
as well as the declared 24-byte reverse scratch. Exact and one-byte-short tape
and caller budgets retain successful/refund/failure assertions. Original failed
fixtures and the corrected evidence remain outside the repository.

## Open questions

None requiring user input. Checked coordinate APIs, implicit-root derivatives,
PDE and later stages remain separate work; this delivery does not complete F03.

## Tests and limitations

- 25 new cases and 78 related existing cases (103 distinct) pass through the
  original, formatted and affected-only repair logs, with an explicit manifest.
- Analytic asymmetric/alias/channel and multi-RHS cases, independent rational
  precision thresholds and three-step Cramer differences cover actual reverse.
- Owning copies/windows/restore/threads, failed-graph reads, late failure,
  representable large risks and exact tape/caller resources are covered.
- Strict primary units and directly parsed headers pass OFF/combined ON; shared
  legacy includes are system headers. The primary-header-only pragma warning
  is excluded. Format, function complexity <=8 and documentation checks pass.
- Fresh installed DAL::cpp consumer passes. Twelve existing caller rows pass
  the frozen two-round rule; optional costs and the single-round narrow-band
  fluctuation remain explicitly disclosed in the performance report.

## Summary

One immutable checked numeric cache supplies actual channel risks/errors. Only
new checked payloads consult the collector. Caller-owned report storage is
prepared before tape-owned scratch; no owned vector crosses that boundary.
Return occurs after complete reverse success, and every detached entry retains
its own provenance. Full paginated exact-head CI/Codacy/review audits and actual
new-case execution in all fourteen configurations remain required before merge.
