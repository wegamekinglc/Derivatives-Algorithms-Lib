# Worker reuse and block-policy critique

Verdict: **Proceed with caveats**, for the active
[P02/P03 specification](../specs/aad-worker-reuse-block-selection.md).

## Blocking issues

No unresolved scope blocker. Fresh batches and owning results provide independent
references. All six families remain full-stage acceptance obligations.

## Significant concerns

- Reused allocations can contain stale active numbers. Prove reset of model
  coefficients, historical aliases, scenario slots, compiled stacks and tail
  lanes; construct and close guards on the executing thread.
- Worker scheduling can change reduction/concurrency. Keep original batch slots,
  offsets and per-batch prefix reversal initially; admit actual futures and
  retained replacement capacity before history.
- Selected extraction must preserve caller order and owner/private identities.
  Complete axes do not require unwanted numeric columns. Selected nonfinite
  risks and report overflow remain errors.
- Full-entry timing does not isolate setup. Measure resource/phase counts first
  and require complete-request benefit without weakening low-path or legacy gates.
- Capacity-safe width and fastest width differ. Preserve defaults; hidden trial
  pricing/history and unmeasured heuristics are unacceptable.

## Minor notes

Keep changes within existing simulation/replay and capacity abstractions. Avoid
a general cache framework or speculative public controls. Run focused tests per
increment and complete platform acceptance at a stable head.

## Counter-proposal

Begin with request-local buffers and per-batch recording. Prove native reset
before retaining active models across recordings. Implement selected extraction
before introducing a performance-driven width policy.

## Author questions

No user input is needed. Establish observer support for setup/allocation counts
and required native refresh fields. Missing reset proof requires fresh construction.
