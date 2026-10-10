# Python rate curvature design critique

Verdict: Proceed with caveats.

## Blocking issues

None. The accepted native adapter covers the financial computation and resource
contract. This increment only projects passive native objects.

## Significant concerns

- A finite-step HVP must not be compared with an exact analytic Hessian at an
  unjustified tolerance. Use the analytic gradient at the actual plus/minus
  points as the primary oracle, then a separate convergence check.
- Joint/staged axes must come directly from provenance. Repeated display names
  are not unique coordinates; staged fixed roots are not free quote columns.
- Typed calibration specs include handles and nested data. Copy before GIL
  release and preserve the native sealing path. Detached result getters must
  not expose mutable native vectors/matrices by reference.
- Additional fixing snapshots must retain native saved-history conflict and
  zero-weight trade validation. Do not shortcut zero weights or expired rows.
- Empty matrices retain the full quote width, and optional zero budgets cannot
  become an absent/unlimited budget through Python truthiness.

The specification explicitly addresses these concerns and requires independent
numerical, ownership, boundary and error-recovery tests.

## Minor notes and counter-proposals

Use one additive binding unit, template the four calibration factory wrappers,
and reuse shared request conversion/copy helpers. Keep the native scalar callback
and smooth prototype outside the Python API. The smaller base-only subset alone
would omit the requested financial Hessian products, so retain both empty and
multi-direction requests with a small acceptance set.

## Author questions

None requiring user clarification.
