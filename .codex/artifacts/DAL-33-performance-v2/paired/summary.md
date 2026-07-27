## Paired benchmark regression gate

2 independent rounds of 10 interleaved process-level samples; failure requires every round to exceed +4.00%.

| Benchmark | Case | Base min | Head min | Change | Round changes | Result |
|---|---|---:|---:|---:|---:|:---:|
| krylov_perf | BCGSolve (500x500 tridiag) | 0.021477 ms | 0.021079 ms | -1.85% | -3.64%, -1.85% | pass |
| krylov_perf | CGSolve (500x500 tridiag) | 0.016170 ms | 0.017172 ms | +6.20% | +1.52%, +6.20% | pass |

All performance acceptance checks passed.
