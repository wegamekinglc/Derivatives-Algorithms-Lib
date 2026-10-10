# Closed European PDE risk

## Source and problem

Merged audit #533 (`c4c77a3718e2acf5f4f5b647eb680e913375e8aa`) identifies
the missing owning financial projection of #506's accepted fixed-grid program.
The detailed native-only roadmap's H.3 requires complete coefficient, terminal
payoff and external-boundary derivatives, with discrete and continuum checks
kept separate. The sampled theta operator itself is already accepted.

The example/test financial program currently lives in
`dal-cpp/test-support/europeanaadfd.hpp`. Move its implementation to a production
header; preserve that support header's existing two-integer settings calls and
namespace through aliases. Core must not depend on dal-public.

## Goals and exclusions

Deliver one owning C++/Python call/put valuation with a two-row, three-column
rate/volatility/strike Jacobian and actual per-step solve diagnostics. Reuse the
same financial program from the existing example and financial tests.

Mesh, evaluation node, dividend, expiry and time schedule are passive. No Delta,
Gamma, arbitrary objective, equation callback, adaptive mesh, American exercise,
new backend or general higher-order capability is introduced. Excel delivery
uses the same owning result in the subsequent interface increment.

## Requirements

1. Settings describe a uniform physical grid `[0, upper]`, node count, ordinary
   interval count, optional evaluation-node index, positive expiry, finite
   dividend yield and explicit forward/transpose backward-error limits.
   Defaults preserve #506: 61 nodes, 120 intervals, upper 400, expiry 1,
   dividend 0.02, both error limits 1e-12.
   Each error limit must be finite and in [0,1], validated before output
   allocation, budget admission or recording changes.
2. An omitted spot index resolves to the quarter-grid node and requires
   `(nodes-1)%4 == 0`; an explicit index may select any interior node.
   Return the resolved index, physical spot and full passive grid.
3. Require at least five nodes and two ordinary intervals. The actual number
   of steps is `ordinary_steps+2`, representable as int. Grid spacing and
   time increments must be positive, finite and representably distinct.
   Extents and the result-payload formula must be checked before allocation.
   Do not silently clamp an input or substitute a coarser mesh.
4. Parameters are finite absolute decimal rate, strictly positive finite
   volatility and positive finite strike strictly below the upper boundary.
   Negative rates/dividends are supported if the complete calculation remains
   representable and all solve policies pass. Reject strike exactly on a grid
   node: the terminal payoff is nondifferentiable there.
5. Build discount rate r, drift (r-q)S and variance sigma*sigma*S*S with native
   expressions. Terminal states are max(S-K,0), max(K-S,0).
   New-time boundaries are call (0, upper*exp(-q*tau)-K*exp(-r*tau)),
   put (K*exp(-r*tau),0). Use four fully implicit half steps over the first
   two ordinary intervals and Crank-Nicolson thereafter.
6. Register exactly r/sigma/K, record the complete chain and reverse two
   channels with call/put identity seeds. Prices and every Jacobian entry must
   be finite before publishing any result. Keep the native higher-order flag
   false and keep all unrelated interfaces/defaults unchanged.
7. Return an owning request snapshot, prices [Call,Put], Jacobian columns
   [Rate,Volatility,Strike], passive grid and resolved spot. Method is
   NativeAADFixedGridEuropeanTheta. Derivative units are price per absolute
   decimal rate/volatility and price per strike-price unit. No per-bp or
   per-vol-point scaling is applied.
8. Return forward errors as steps-by-2 in chronological order. Return
   transpose errors as steps-by-4, columns (call layer/call seed,
   call layer/put seed, put layer/call seed, put layer/put seed), with explicit
   labels. Resolve report event IDs to chronological steps before detaching;
   do not return live tape handles or recording IDs.
   Publication must take linear time in the number of steps. For this owned
   linear chain, reverse the report-entry traversal and check every event
   against the corresponding chronological step; reject missing/mismatched
   entries rather than silently publishing a different order.
9. Reject active independent recordings and nonempty legacy graphs before any
   mode, graph or adjoint mutation. Own the native two-channel mode and restore
   the caller's prior mode on success and failure. After a request's recording
   starts, cleanup invalidates only its own active values. Failed preflight
   must preserve an existing caller graph, seeds and mode.
10. Provide optional numeric-payload and recording-capacity byte limits.
    Zero is a real limit. The exact retained floating-point result payload is
    `8*(17 + nodes + 6*(ordinary_steps+2))`: request point 3, request physical
    settings/error limits 5, prices 2, Jacobian 6, spot 1, grid nodes and
    six errors per step. This excludes integers, optional byte counters,
    labels, allocator/container overhead, temporary binding copies and peak
    process memory. Check this limit before recording or allocating outputs.
11. Reuse TapeCapacityBudget_/Scope_ with cleanup reservation. Its limit
    covers admitted retained block capacities, event-owned payloads and event
    reverse scratch; unused retained tape blocks also count. Report actual
    peak charged tape bytes, cleanup reserve, reverse scratch peak and the
    caller's optional limits separately. Do not describe the payload limit
    as a full heap quota or derive a universal tape requirement from nodes
    alone. Admission can fail after partial forward/reverse work.
12. Python accepts three checked real parameters and a copied typed settings
    object, plus optional nonnegative size_t budgets. Reject bool, enum,
    string, coercion-only numeric objects, nonfinite reals and wrong settings
    types with field/constraint context. Integer settings/budgets use the
    established checked integer helpers. Release the GIL only after copying
    and validating Python inputs. Result properties return passive detached
    values/copies and survive subsequent requests and GC.

## Executable acceptance

- First RED: a focused public test requiring the missing owning API and #506
  9-node/8-interval prices/Jacobian fails to compile for the missing header.
- GREEN: the public test matches both prices and all six risks at accepted
  1e-10/1e-9 tolerances and validates every actual error under its policy.
- Compare all prices and risks with a separate dense same-grid complete
  implementation and three centered bump sizes; this reference must not call
  DAL stencils, sampled-step pullbacks or the financial helper.
- Exercise nondefault physical upper/expiry/dividend/evaluation node, negative
  rate, nonzero boundary rate/strike contributions, minimum schedule and
  volatility variance-chain contribution.
- Run the existing affected financial tests once after extraction. Their
  independent small-grid reference and frozen three-level continuum/parity
  checks remain distinct from linear-solve residual acceptance.
- Test invalid dimensions/index/spot/expiry/volatility/strike/kink, NaN/Inf,
  unrepresentable spacing/schedule/coefficient/discount and error limits.
  Test payload limits zero/exact/one byte short. For tape limits obtain an
  actual fresh-thread peak plus cleanup reserve, then test exact and short on
  matching fresh threads with the same configuration.
- Test active and legacy caller-graph rejection without graph/seed/mode loss;
  follow request failures with successful requests. Test result detachment and
  immutable settings, two concurrent callers and a deterministic GIL-release
  synchronization check that does not depend on a long timing workload.
- Compile changed translation units under strict OFF and combined
  lifetime/profiling configurations; run focused numerical tests and compile/
  run the installed C++ consumer. Required exact-head platform CI remains.
- Preselect only complete financial programs at 9 nodes/8 intervals and
  61 nodes/120 intervals. Compare the extracted native caller with the accepted
  caller using equivalent outputs/diagnostics where possible; separately
  disclose owning Python boundary cost. Preserve >=25ms calibrated loops and
  2 rounds of 10 alternating interleaved pairs per selected case. A 4% gate
  applies only to equivalent caller contracts. No unrelated benchmark matrix.
- Local read-first review, docs/changelog, full remote review bodies/threads,
  Codacy/CI repair, two final audits and SHA-guarded merge close this PR before
  subsequent implementation.

## Compatibility and open questions

Keep aggregate `{nodes, intervals}` example settings and its namespace working.
Installed public consumers must receive the new production helper through the
normal core header installation and link against matching public/core packages.
Do not modify the read-only original checkout or accepted binary prefixes.

No blocking user decision remains. Development estimate: 6–10 hours excluding
CI queue/build time; resource/report formulas are verified by tests rather than
treated as estimates.
