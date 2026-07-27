## Paired benchmark regression gate

2 independent rounds of 10 interleaved process-level samples; failure requires every round to exceed +4.00%.

| Benchmark | Case | Base min | Head min | Change | Round changes | Result |
|---|---|---:|---:|---:|---:|:---:|
| krylov_perf | BCGSolve (500x500 tridiag) | 0.019277 ms | 0.018633 ms | -3.34% | -3.38%, -3.21% | pass |
| krylov_perf | CGSolve (500x500 tridiag) | 0.014918 ms | 0.015321 ms | +2.70% | +2.23%, +2.70% | pass |

All performance acceptance checks passed.
