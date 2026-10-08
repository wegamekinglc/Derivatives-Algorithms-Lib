# Native structural Jacobian review

Verdict: CommentOnly — local implementation has no remaining actionable finding;
publication CI/Codacy, actual runtime profiles and external review are pending.

## Scope and findings

Reviewed the complete new header, implementation and thirteen-case test suite,
then the focused axis-context validation change. Scope identity is checked
before retained addresses; fresh slots are validated before adjoint access.
Binding precedes graph construction and excludes recorded-event leaves.
Validation completes before clearing; per-block clearing and additive seeds
handle aliases, direct inputs and fixed-width partial blocks. Recovery checks
every harvested gradient and returns a complete owning matrix.

No existing production header, layout, hot path or CMake input changes. All 177
old objects and seven fresh accepted callers preserve bytes. Financial support
proof and structural invalidation remain explicit subsequent requirements;
slot identity alone does not discharge them. Ordinary Number lifetime rules
remain required outside generation-diagnostic builds.

## Local acceptance

- RED fails for the absent native API; final affected suite passes 13/13.
- Analytic full matrices and individual dense reverse references cover zero to
  nonzero dependence, aliases, constants, direct/intermediate outputs and
  recorded linear-solve composition.
- Invalid state/count/order/slot/mode and foreign/closed/thread bindings reject
  before clearing; healthy requests recover after validation/non-finite harvest.
- Empty axes and zero-color requests preserve exact shape without reverse;
  concurrent scopes share only immutable plans and own detached results.
- Six strict implementation/header/test OFF/combined checks pass with zero
  warnings. Formatting passes; 36 functions have CCN at most eight.
- The installed-package consumer passes 1/1 using installed headers only.
- Four new complete-request cost rows pass eighty observations in 1.55 seconds;
  zero existing rows are retimed. Costs are informational, with no financial
  speedup or default-mode promotion claim.

Evidence and cost limits are in the active
[performance report](../performance/aad-native-structural-jacobian.md).
Full current-head review bodies, paginated threads/checks, fourteen actual
runtime profiles, Codacy annotations and tested/merged tree equality must pass
before guarded merge.
