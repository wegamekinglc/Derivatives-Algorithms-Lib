# Pre-implementation critique

Verdict: Proceed with caveats.

Blocking issues: none after the following boundaries were made explicit in the
specification and API note.

Significant concerns and required disposition:

- MC cannot run inside the generic independent recording callback. Compose
  full first-order requests using shared numeric helpers instead.
- All perturbed model domains must be admitted before the base submits work;
  otherwise a negative-volatility minus bump wastes an entire base/plus run.
- A main-thread tape cap would miss worker tapes. Intersect both supplied
  recording limits in the per-path settings and test worker execution.
- `1+2*M` counts gradient requests, not reverse sweeps. Preserve per-path
  resource semantics and avoid aggregate-memory claims.
- Fuzzy condition kernels are piecewise linear/triangular and extrema can be
  hard. A smoothing width does not prove C2; finite-step Gamma must disclose
  its estimator and mathematical limits. Convergence tests use smooth products.
- Retaining just random settings would omit contract/smoothing provenance.
  Hold immutable prepared ownership in the result and test detached lifetime.
- Moving accepted quotient code affects the generic driver. Its existing
  correctness cases and one cost control are in scope; unrelated full suites
  and Cartesian performance matrices are not.

Minor notes: native model axes use continuous rates, steps remain explicit,
empty direction rows are supported, and duplicate directions are intentionally
not optimized. Existing recording lifetime rules still apply.

Counter-proposal: use the smallest reusable internal template rather than a
new public abstract gradient-provider interface. It avoids callback allocation
and keeps binding design independent of implementation machinery.

Author questions: none remain before the first missing-entry RED.
