# Interpolation profiles

`interp_perf` without arguments retains its three existing rows and workloads:
cubic and virtual linear at 50 knots/10K queries, and the inline linear LV-style
load at 200 knots/20M queries. The LV-style label describes a load, not an
established direct model caller. The nine-target regression gate still invokes
the default executable; the following profiles are optional informational rows.

```bash
build/core-bench/dal-cpp/benchmarks/interp_perf/interp_perf --active-profile interval
build/core-bench/dal-cpp/benchmarks/interp_perf/interp_perf --active-profile flat
build/core-bench/dal-cpp/benchmarks/interp_perf/interp_perf --active-profile interval --preflight
```

Both profiles register 50 active ordinates `y[i] = 2 + 2*i` on passive knots
`x[i] = i`, and 10K active queries. `interval` uses `49*(i+1)/10001`, strictly
inside segments; `flat` uses `-1`, below the first knot. Preflight checks the
total value, every query derivative (2 or 0), and every ordinate derivative
against independently accumulated linear weights. Exact-knot and right-flat
branch conventions remain unit-test coverage, not additional timed cases.

Three spans are reported from the same actions:

- `record`: interpolation and accumulation through FinishRecording; allocation,
  input registration and StartRecording are outside this span.
- `reverse`: ClearAdjoints, unit seed and Reverse.
- `lifecycle`: allocation, registration, recording, reverse, Close and destruction
  of all local inputs/results. Analytical verification is outside measured calls.

After correctness preflight, batch size doubles to at least 10ms of lifecycle
time, capped at 128 actions. Two untimed batches precede ten measured batches.
`PROFILE_SAMPLES` records retain each batch's nanoseconds, inner loop count and
per-action values for each span. CTest names `interp_active_interval` and
`interp_active_flat` carry the `benchmark` label and retain their arguments in
the existing smoke runner's inventory.

Use the current correct implementation as the future active-interval baseline.
Earlier code with zero interval query derivatives performs different work and
cannot establish an equivalent-work regression budget. Earlier external probes
also checked gradients inside their lifecycle span and stopped before vector
destruction; their lifecycle timings are not directly interchangeable with this
profile. Compare two rounds of ten alternating fresh processes per side using
minimum reductions and the repository's noise rules; do not infer a regression
from one process or extend these rows to whole-request performance.
