## Paired benchmark regression gate

2 independent rounds of 10 interleaved process-level samples; failure requires every round to exceed +4.00%.

| Benchmark | Case | Base min | Head min | Change | Round changes | Result |
|---|---|---:|---:|---:|---:|:---:|
| krylov_perf | BCGSolve (500x500 tridiag) | 0.019813 ms | 0.019060 ms | -3.80% | -4.02%, -3.78% | pass |
| krylov_perf | CGSolve (500x500 tridiag) | 0.015291 ms | 0.015617 ms | +2.13% | +6.29%, +2.13% | pass |

All performance acceptance checks passed.
