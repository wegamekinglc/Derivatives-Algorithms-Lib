# Numeric implicit-root implementation review

## Findings

No open implementation findings. Verdict: **Approve** for the local numeric
increment; exact published-head platform/Codacy/review and guarded merge remain
required. Review covers the complete new header/unit/four test files, six
sanitizer filter changes, active contracts, current methodology and changelog
on `feature/implicit-root-linearization`, based on verified #500 merge
`f98ef99d6037a1922ef7223d8d23f005292ab53c`.

## Method and design

One const equation evaluation at owned candidate/input copies captures residual,
complete J and K. Validate point/policy/pivot before evaluation and shape/range/
residual admission before one checked J capture. The constructor moves its
snapshots and cache, releases the temporary evaluation J and retains no callback.
Reverse validates all seed columns first, reuses RHS-only transpose solves,
contracts -K^T lambda and returns independent errors. All failed reverse work
is local; the immutable cache remains usable. No native graph is mutated.

The supplied point selects the branch. Nonzero admitted root residuals are
preserved and do not certify root/sensitivity error. Full stationarity J,
non-symmetric transpose and independent complete solves distinguish this map
from finite iteration or Gauss–Newton derivatives. The private zero construction
RHS is not exposed as root convergence. Existing normalized condition range
semantics and relative pivot policy are preserved.

## Tests and retained correction

- Missing public header RED: `implicit-root-red.log`.
- First quadratic owning-point/1/4 risk GREEN: `implicit-root-first-green.log`.
- Twenty new edge/reference cases: `implicit-root-edges-green.log` retains the
  initial 19/20 result. The scalar 1e-310 inverse-range assumption was incorrect:
  normalized scalar J has condition one. The test now retains singular rejection
  and uses a genuine normalized inverse overflow diagonal (1,1e-310) with an
  explicit legal 1e-312 pivot. It also admits uniform tiny scalar J, rejects an
  unrepresentable requested reverse and checks zero-seed recovery. Production
  algorithms and asserted tolerances are unchanged.
- Narrow corrective rerun passes the changed range case and the only other
  caller of the generalized diagonal fixture: 2/2 in
  `implicit-root-range-fixture-correction.log`. Across first/edge/correction,
  all 21 distinct cases pass; unrelated passed cases are not repeated.
- Independent three-step converged coupled/non-symmetric/pivot/stationarity
  references, complete residual Hessian, multiple/zero seed columns, k=0,
  point/source/equation destruction, one evaluation and concurrent const reads.
- Inclusive residual/actual transpose limits, malformed/nonfinite points/
  evaluations/seeds, singular/unsupported normalized inverse, finite large risks,
  overflow recovery and exact/one-byte-short caller capacity with refunds.
- Exact scalar retained/capture peak 76/108 bytes and reverse m=3 result/peak
  48/72 bytes; two-root/two-input retained/capture peak 192/304 and m=3 reverse
  result/peak 72/112. These exclude hidden dense matrix-adjoint allocation.
- Strict new production unit/four test units and direct header compile in OFF
  and combined lifetime/profiling ON: `implicit-root-strict.log` and commands.
- Maximum CCN is 7 in production and 8 in tests; clang-format passes for all
  six new C++ files. All 165 Markdown files pass documentation checks.
- Fresh installed `find_package(dal-cpp)` / `DAL::cpp` consumer passes 1/1,
  including source destruction, detached risks/reports and exact caller budget:
  `implicit-root-installed-consumer.log`.

## Performance and compatibility

All 171 accepted old archive objects and two fresh old caller links remain
byte-identical. Reuse existing accepted timing; four informational root rows
retain two alternating best-of-ten rounds, matching risks/reports and exact
resources. Preserve the initial resource-fixture failure and scope-only repair;
no production code, workload, sampling or threshold is weakened. See the
[performance report](../performance/aad-implicit-root-linearization.md).

The public addition is optional double-valued C++. Existing nonlinear solvers,
weighted effective inverse, numeric/native solves, tape and bindings retain
their contracts. One new source is included by existing CMake source discovery;
the installed header/target works without extra dependencies or generated files.

## Open questions and residual acceptance

No blocking numeric design question. This increment does not deliver native
root events or bindings; they remain in the [whole plan](../plans/aad-implementation.md).
Dense factorization/condition work still costs O(n^3); four small cost rows
do not prove large-system scaling. Add only ImplicitRootTest.* to the existing
six sanitizer filters, require all 21 cases actually execute in each applicable
exact-head profile, inspect all paginated reviews/threads/checks/Codacy and
repeat the final audit before guarded merge. Remote acceptance is pending.

Evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261008/`.
