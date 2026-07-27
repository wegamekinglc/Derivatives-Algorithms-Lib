## Paired benchmark regression gate

2 independent rounds of 10 interleaved process-level samples; failure requires every round to exceed +4.00%.

| Benchmark | Case | Base min | Head min | Change | Round changes | Result |
|---|---|---:|---:|---:|---:|:---:|
| krylov_perf | BCGSolve (500x500 tridiag) | 0.019112 ms | 0.018478 ms | -3.32% | -2.80%, -4.08% | pass |
| krylov_perf | CGSolve (500x500 tridiag) | 0.014973 ms | 0.015092 ms | +0.79% | +0.79%, +0.75% | pass |

All performance acceptance checks passed.
