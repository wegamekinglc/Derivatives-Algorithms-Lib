## Paired benchmark regression gate

2 independent rounds of 10 interleaved process-level samples; failure requires every round to exceed +4.00%.

| Benchmark | Case | Base min | Head min | Change | Round changes | Result |
|---|---|---:|---:|---:|---:|:---:|
| krylov_perf | BCGSolve (500x500 tridiag) | 0.019317 ms | 0.018709 ms | -3.15% | -2.94%, -3.15% | pass |
| krylov_perf | CGSolve (500x500 tridiag) | 0.014979 ms | 0.015353 ms | +2.50% | +2.50%, +3.51% | pass |

All performance acceptance checks passed.
