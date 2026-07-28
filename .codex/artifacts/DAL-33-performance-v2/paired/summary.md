## Paired benchmark regression gate

2 independent rounds of 10 interleaved process-level samples; failure requires every round to exceed +4.00%.

| Benchmark | Case | Base min | Head min | Change | Round changes | Result |
|---|---|---:|---:|---:|---:|:---:|
| krylov_perf | BCGSolve (500x500 tridiag) | 0.018967 ms | 0.015800 ms | -16.70% | -17.04%, -16.39% | pass |
| krylov_perf | CGSolve (500x500 tridiag) | 0.014924 ms | 0.012419 ms | -16.79% | -16.76%, -16.87% | pass |

All performance acceptance checks passed.
