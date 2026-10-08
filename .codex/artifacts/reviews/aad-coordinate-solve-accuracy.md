# Checked coordinate accuracy local review

Verdict: Approve. Local implementation scope only; remote completion gates are open.

## Findings

No remaining local finding. Reviewed the full new public header/implementation,
private contraction helper, ordinary coordinate implementation and all new tests
against the [specification](../specs/aad-coordinate-solve-accuracy.md),
[API decision](../api-notes/aad-coordinate-solve-accuracy.md),
[critique](../critiques/aad-coordinate-solve-accuracy.md) and methodology.

The cache owns one checked physical solve. Both reverse methods call checked
RHS-only reverse once; full requests contract directly into physical packed
coordinates. Symmetric contributions add both entries, zero parameters retain
risk, and RHS-only omits unused overflow. Policy/report axes and conditioning
remain physical. Source mutation, const concurrency and unpublished failure
cleanup are covered. Ordinary payload and Number/node/tape layouts are unchanged.

## Tests and quality

- Initial harness configuration retains its missing-Ninja environment failure.
  Using the available Unix Makefiles generator then confirms the intended
  missing-header RED; no production behavior or assertion was weakened.
- One analytic symmetric GREEN plus twelve new edge cases pass; the first
  unchanged case is not repeated in the edge batch. Forty-four affected numeric/
  native coordinate cases pass, giving 57 distinct accepted cases.
- Independent Cramer/differences, asymmetric chain contributions, exact rounding
  thresholds, zero seed columns, paired/unused overflow, legal small pivots,
  ownership, concurrent readers and malformed inputs/policies/seeds are covered.
- Four-by-four exact budgets prove one cache/expansion, RHS-only 80-byte reverse
  and packed 112-byte reverse; one-byte-short failures refund completely.
- Strict primary units/direct headers pass OFF and combined lifetime/profiling
  ON. Maximum production function CCN is 5; test maximum is 7.
- Fresh installed `find_package(dal-cpp)`/`DAL::cpp` consumption passes 1/1,
  including detached owning risk and RHS-only behavior.
- [Scoped performance](../performance/aad-coordinate-solve-accuracy.md) accepts
  four changed existing caller rows and reports four optional costs separately.
  The banded cached +8.07% single-round fluctuation is explicitly retained.

## Open questions and merge readiness

No local API question remains. Residual risk is platform behavior until the exact
published head passes CI/Codacy/review and actual logs confirm all thirteen new
cases in fourteen sanitizer, extended and MSVC configurations. Those gates must
finish before merge. This increment closes numerical coordinate accuracy only;
native coordinate reporting, implicit roots and PDE remain separate deliveries.
