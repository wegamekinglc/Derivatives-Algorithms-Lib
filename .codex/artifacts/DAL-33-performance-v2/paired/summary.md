## Paired benchmark regression gate

2 independent rounds of 10 interleaved process-level samples; failure requires every round to exceed +4.00%.

| Benchmark | Case | Base min | Head min | Change | Round changes | Result |
|---|---|---:|---:|---:|---:|:---:|
| krylov_perf | BCGSolve (500x500 tridiag) | 0.018975 ms | 0.015874 ms | -16.34% | -14.93%, -17.29% | pass |
| krylov_perf | CGSolve (500x500 tridiag) | 0.014870 ms | 0.012630 ms | -15.06% | -15.02%, -16.49% | pass |

All performance acceptance checks passed.
