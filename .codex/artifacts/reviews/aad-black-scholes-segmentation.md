# P05 Black–Scholes fixed-path implementation review

Verdict: local implementation accepted; CI and external review remain open.

## Findings and dispositions

1. Ordinary expired-product preparation returns before compiling history. The
   opt-in segmented preparation now resolves historical fixings and compiles
   historical-only products. A retained failing test becomes green and proves
   fresh constant risk with zero future model steps. Ordinary preparation keeps
   its existing shortcut; its affected regression tests pass.
2. Trace instrumentation raised complexity in shared compiled dispatch. Small
   inline compile-time helpers now contain the trace-only choices. Default
   execution discards them at compile time. Extrema/comparison dispatch complexity
   falls to five/seven; fuzzy branch complexity falls to eight.
   All new production functions and six new benchmark functions have
   complexity at most eight. Final affected tests and scoped costs pass.
3. The sanitizer selection initially omitted the new suites. All six existing
   sanitizer profiles now include the 28 new model/state/path cases. Actual
   current-head runtime logs must prove execution before accepting publication.

4. Codacy additionally identifies by-value trace-view passing, legacy LSMC/tail
   dispatch complexity and model-test helper complexity. Const-reference copying
   and inline helper splits repair all five annotations. Targeted tests, strict
   checks, installed consumption and two additional existing LSMC cost gates pass.
5. Copilot identifies padded Markdown separators in the two active reports.
   Compact separators preserve aligned widths. Final exact-head re-review remains
   required; the initial Codex body reports no major issues.

6. Copilot identifies the model-provenance gap in accepting generic prepared
   scripts. A two-asset/one-observation fixture produces a compile-time RED under
   the old interface. A private-factory, move-only preparation wrapper now blocks
   generic construction and assignment and exposes only a const ordinary view.
   All ten financial path cases pass, including the new provenance case; six
   affected strict checks and the installed consumer pass. Shared library objects
   and timed request bodies are unchanged, so accepted latency evidence is reused.

## Reviewed boundaries

The immutable model plan shares financial formulas with ordinary Black–Scholes,
uses fresh typed inputs, emits complete samples and preserves raw time-zero spot.
Observation slots retain values through their last consuming sample. The packed
boundary contains all scalar/vector/payment state with proved vector bounds;
restore validates every length before mutation. History is rebuilt once from
fresh script constants. A named policy borrows state only inside one step, and
recursive dispatch retains its exact trace identity and compact observations.
Bounded integer trace slots distinguish smoothing intervals, skipped instructions,
extrema ties and individual vector selections without probabilistic hashes.

The explicit kernel owns its prepared script, rejects unsupported exercise and
incompatible compiled preparation, and returns the core's detached result. The
generic prepared script is not a model identity certificate. The kernel accepts
only the owning wrapper constructed by its exact Black–Scholes factory; the
sample-definition checks are an additional validation boundary.
Nested graphs, modes, budgets, invalid inputs, recovery and shared-plan concurrency
are covered. No existing valuation strategy is automatically changed.

## Validation and unresolved gates

Local evidence: 120 distinct affected tests before the trace-helper refactor,
63 compiler/state/path/model/LSMC cases after Codacy repair, ten strict warning checks, installed
consumer 1/1, and [scoped performance acceptance](../performance/aad-black-scholes-segmentation.md).
The measured long-path total C++ heap reduction is about 6.2%, with warm latency
about 2.6–2.7 times full graph. Short paths have no total-memory advantage.

The provenance repair adds one case, bringing new financial coverage to 29.
Publication still requires complete current-head review bodies, paginated thread
and Codacy inspection, all required CI, actual new-case runtime in the fourteen
sanitizer/extended/MSVC profiles, repeated final audits and guarded merge.
Monte Carlo/RNG acceptance and later whole-plan work remain separate open tasks.
