# Native solve-coordinate design critique

Verdict: Proceed with caveats.
Reviewed: active native specification/API, numeric coordinate implementation,
accepted native dense event/lifecycle/resource machinery and its nearby tests.

## Blocking issues

None after retaining the following constraints in the implementation contract.

## Significant concerns

1. An internal helper refactor changes the existing native solve object. Source
   similarity does not prove ordinary performance: retain its payload fields and
   execute six ordinary plus two diagnosed-caller boundary comparisons unless
   executable bytes match.
2. Copying active parameters as a dense Number matrix defeats the storage goal
   and creates fixed-zero slots. Capture p bindings and pass numeric expansion
   only to the accepted numeric coordinate cache.
3. A passive coordinate must not run the full parameter reverse: its unused
   contributions can overflow even when the requested RHS gradient is finite.
4. Alias contributions add before propagating scalar producers. Clearing source
   adjoints, averaging symmetric pairs or replacing prior adjoints is incorrect.
5. Caller buffer limits overlap scratch but do not charge retained event caches.
   Generic event sharing must preserve suspension, failure refunds and suffix
   destruction before slot rewind; exact tests are required.
6. Current forward diagnostics do not expose owning transpose residual reports.
   Keep that plan requirement open rather than describing F03 as complete.

## Minor notes and smaller scope

Keep both payload policies in the existing solve translation unit, with shared
private event/output helpers. No general callback framework or extra internal
public header is needed. Omit symmetric-band composition and diagnosed-coordinate
results until their contracts are defined. Dense LU remains O(n^3)/O(n^2), even
when binding and parameter-contraction costs shrink.

## Author questions

None required before implementation.
