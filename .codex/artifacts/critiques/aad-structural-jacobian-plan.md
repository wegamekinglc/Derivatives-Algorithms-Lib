# Structural Jacobian plan critique

Verdict: Proceed with caveats.

## Blocking issues

None in the explicit numeric boundary. It cannot establish mathematical supports
from raw indices and must not be enabled automatically in production callers.

## Significant concerns resolved in the specification

- Matrix accepts int dimensions without checked byte multiplication. Validate
  extents and payload arithmetic before constructing result matrices.
- A sparse descriptor can declare a very wide unused axis. Incidence storage
  must scale with used columns; accept a 0-by-INT_MAX plan without a large array.
- An empty support means proven independence, not missing metadata. Unknown
  support is full. Numerical zeros and maturity prefixes are insufficient proof.
- Coloring is deterministic greedy, not optimal. Conservative supersets remain
  correct but may remove savings; no production speedup is promised here.
- Validate every direction gradient's finiteness before recovery. Structural
  masking must not hide invalid backend output.
- Numeric payload budgeting includes directions plus result, excludes metadata
  and process memory, and admits explicit zero/empty cases.
- Native input-slot identity, aliases, repeated reverse and dynamic structure
  invalidation are subsequent acceptance boundaries. The plan alone closes none
  of those requirements and does not complete P04.

## Minor notes and counter-proposals

Optional row colors avoid leaking an implementation sentinel. Keep owning plan
getters const and reuse this recovery in the future native adapter rather than
copying coloring/reconstruction logic into curve or script callers.

## Author questions

None for the current numeric API. First freeze the independent matrix/shape/
budget references, capture missing-interface RED, then implement and test the
small scope. Preserve ordinary object/caller identity and time only new costs.
