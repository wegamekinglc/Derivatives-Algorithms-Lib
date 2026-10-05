# D04 structured risk-entry cost

Status: informational cost of the new opt-in API, measured after final local
correctness checks. This is not an extra target in the nine-executable gate and
does not resolve P01's original-baseline production-performance acceptance.

## Configuration and protocol

Source: `8886c083dcf80f00f09ea8e4c4c83c619507a786` plus the frozen D04
working-tree patch, captured in the sample environment's full source SHA-256
manifest. GCC 15.2, CMake 4.2.3, portable Eigen, Release/static native AAD,
profiling/lifetime diagnostics OFF, CPython bindings enabled in the library build.
The standalone C++ driver links installed targets rebuilt from that source.

Pin the caller to CPU 4 on the shared WSL2 host; initialize one DAL thread.
Compare legacy and structured calls in the same executable, with two rounds of
ten process pairs and alternating first-side order. Each process first runs an
untimed legacy/structured oracle, then measures repeated full public requests:
64 repetitions for 64 paths/one event; eight for 2,048 paths/twelve events.
Report best-of-N per-request minima for each round and across both rounds.
No builds or tests run during sampling.

The timed request includes parsing, preparation, simulation and returned state.
The new API additionally retains axes, model/product/history snapshots, execution
provenance and a numeric matrix. Legacy native calls extract all risks; selected
or empty structured requests still use the current complete native execution.
Passive omission selects no columns. Matrix/report getter costs beyond the
initial result are excluded. This is warm full-entry cost, not cold process
startup or a phase-separated estimate.

All 640 processes pass strict single-worker legacy PV/every-selected-risk
bitwise checks. Full source, helper, executable and supporting archive hashes
remain unchanged throughout sampling. Raw stdout/stderr, every duration and
source/environment identity are retained.

## Results

| Case                          | Legacy μs | Structured μs | Delta   | Round 1 | Round 2 |
|-------------------------------|-----------|---------------|---------|---------|---------|
| 64x1-tree-native-all          | 15.875    | 20.225        | +27.40% | +23.50% | +27.67% |
| 64x1-tree-native-one          | 16.087    | 20.386        | +26.73% | +26.73% | +22.96% |
| 64x1-tree-native-empty        | 16.131    | 20.016        | +24.08% | +23.78% | +26.05% |
| 64x1-tree-passive-all         | 9.471     | 13.931        | +47.09% | +45.64% | +47.09% |
| 64x1-compiled-native-all      | 16.337    | 20.248        | +23.94% | +26.47% | +23.72% |
| 64x1-compiled-native-one      | 16.147    | 20.135        | +24.70% | +24.70% | +26.07% |
| 64x1-compiled-native-empty    | 16.449    | 20.315        | +23.51% | +24.50% | +23.51% |
| 64x1-compiled-passive-all     | 9.469     | 13.896        | +46.75% | +46.55% | +46.75% |
| 2048x12-tree-native-all       | 2263.086  | 2257.070      | -0.27%  | -0.27%  | +0.79%  |
| 2048x12-tree-native-one       | 2285.183  | 2315.431      | +1.32%  | -0.97%  | +1.33%  |
| 2048x12-tree-native-empty     | 2331.756  | 2364.836      | +1.42%  | +0.92%  | +2.47%  |
| 2048x12-tree-passive-all      | 749.356   | 780.842       | +4.20%  | +3.33%  | +5.10%  |
| 2048x12-compiled-native-all   | 2090.307  | 2125.900      | +1.70%  | +1.70%  | +1.29%  |
| 2048x12-compiled-native-one   | 2086.965  | 2108.028      | +1.01%  | +0.88%  | +1.57%  |
| 2048x12-compiled-native-empty | 2043.553  | 2051.745      | +0.40%  | +1.39%  | +0.40%  |
| 2048x12-compiled-passive-all  | 498.689   | 518.625       | +4.00%  | +4.00%  | +3.83%  |

Short native requests add about 3.9–4.4 microseconds (23.5–27.4%); short passive
requests add about 4.4 microseconds (46.7–47.1%). Metadata and planning are visible
when path work is very small. Longer native requests range from -0.27% to +1.70%
in overall minima; longer passive requests add about 20–31 microseconds, with
+4.00%/+4.20% overall movement and mixed confirmation rounds on this host.

The negative and small long-case differences are not evidence that snapshot
construction speeds simulation up. These compare different returned capabilities
on a noisy shared host. Preserve every row; do not apply the old regression gate
to the new feature's additional work or label these figures an old-entry pass.

## Existing-path evidence and limits

The final D04 OFF executables for all nine existing gate targets have identical
SHA-256 hashes to the retained measured binaries. Gate scripts/configuration and
simulation/LSM/tape hot-loop sources are unchanged by D04. The earlier 65-case
formal gate therefore remains evidence for those identical executables; it does
not establish a new public-facade timing result.

The [P01 report](aad-production-profiling.md) retains all original MC failures,
same-binary controls and matched-prefix confirmation, with an overall
inconclusive production verdict. Selected/empty inputs currently save returned
storage rather than native forward/reverse work. P02/P03 must measure any future
workspace/extraction optimization without changing estimator or normalization.

## Durable evidence

Home-cache root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`;
`/tmp/dal-aad-evidence` is only its alias.

- `aad-risk-entry-cost-source/`, `aad-risk-entry-cost-build/` and
  `run_aad_risk_entry_cost.py` retain the exact standalone helper.
- `aad-risk-entry-cost-measurements-01/environment.json`,
  `samples.jsonl`, `results.json` and per-process stdout/stderr retain raw data.
- `aad-risk-final-off-gate-binary-identity-01.json` proves nine-target identity.
- Final local OFF/combined builds, focused/legacy/binding tests and fully
  instrumented ASan/UBSan logs are recorded in the
  [risk-result review](../reviews/aad-risk-results.md).

Exact new-head CI remains separate from these local results. No merge or complete
Stage A/B/C/D acceptance is claimed.
