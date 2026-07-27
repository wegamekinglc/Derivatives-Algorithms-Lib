## Paired benchmark regression gate

2 independent rounds of 10 interleaved process-level samples; failure requires every round to exceed +4.00%.

| Benchmark | Case | Base min | Head min | Change | Round changes | Result |
|---|---|---:|---:|---:|---:|:---:|
| krylov_perf | BCGSolve (500x500 tridiag) | 0.019025 ms | 0.018890 ms | -0.71% | -0.71%, -0.13% | pass |
| krylov_perf | CGSolve (500x500 tridiag) | 0.014978 ms | 0.015231 ms | +1.69% | +1.69%, +1.88% | pass |

All performance acceptance checks passed.
