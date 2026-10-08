# Numeric implicit-root critique

Preimplementation review: no blocking design findings. Runtime acceptance remains open.

| Risk                                                    | Required control                                                                                     |
|---------------------------------------------------------|------------------------------------------------------------------------------------------------------|
| Root risk uses finite iterations or another branch      | Supplied candidate identity, negative branch and independently converged full solves                 |
| Approximate root is advertised as certified sensitivity | Retain nonzero residuals/absolute limits; document captured-point linearization                      |
| Incomplete stationarity derivative                      | Nonzero least-squares residual fixture distinguishes J=7 from Gauss-Newton J=5                       |
| J solve uses the wrong orientation                      | Nonsymmetric and pivoting fixtures with analytic risks and three-step differences                    |
| Equation callback or buffers remain borrowed            | Source/equation destruction, mutation and concurrent const readers                                   |
| Extra cache/inverse or dense matrix gradient is hidden  | Exact retained/capture/reverse capacities and one-byte-short refunds                                 |
| Zero input axes skip requested accuracy checks          | k=0 keeps m report columns and still validates all seeds/transpose solves                            |
| Failed request poisons immutable numeric cache          | Transpose/overflow/capacity failures followed by valid zero/nonzero requests                         |
| Sanitizer green status hides absent new cases           | Add ImplicitRootTest to existing six filters and verify every actual case in fourteen configurations |
| New optional cost is called a legacy regression         | Existing archive/caller identity; four separate new root cost rows                                   |

Use one checked J cache, zero construction RHS and RHS-only transpose requests.
The evaluation's temporary J may be released after cache creation. Avoid exposing
the private LU or widening established solve APIs solely to batch a new optional
operator. Scope expansion must follow actual shared-source changes. Preserve
Underdetermined::Find and its existing weighted effective-inverse contract.
