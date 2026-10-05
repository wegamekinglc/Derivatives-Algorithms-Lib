# F01 discrete Dupire core entry cost

Status: informational opt-in calibration costs after core correctness. These
measurements do not close F01's Hybrid/full-request coverage or P01's independent
production-performance acceptance.

## Configuration and measurement

Measured the uncommitted core increment based on `e3720f9f`, using the final
Release/default-OFF installed static libraries, GCC 15.2.0 and native AAD on the
shared WSL2 host. One worker is configured and each process is pinned to CPU 4.
No DAL build or test process was running during sampling. The retained manifest
includes full source/helper/package/binary hashes, compiler identity, hardware,
affinity and dependency SHAs; every captured identity is unchanged afterward.

Four cases cover flat deterministic-carry and Merton base IVSs, each with a 9x2
or 21x8 spot/time surface and a 3x2 strike/maturity spread matrix. The matrix is
zero in these timing cases; nonzero quotes are covered by the independent
functional oracles. Compare three modes in the same executable:

- Original numeric `AAD::DupireCalib`, including returned surface destruction.
- Checked `CalibrateDupireWithRisk`, including input copies, frozen sample map,
  domain validation, numeric calibration and immutable snapshot construction.
- `PullbackDupireCalibration` from an existing snapshot, including validation,
  active registration/replay, additive all-node seeds, reverse and passive results.

The warm window excludes process startup, initial bitwise/flat references and
three untimed warmups. It includes repeated-call result assertions and result
destruction. Small cases perform 64 calls per process; medium cases perform 16.
Collect two rounds of ten processes per mode/case, rotating all six mode-order
permutations, and reduce each round and the complete sample to its minimum.
All 240 processes preserve numeric surface and repeated VJP bitwise identity;
flat parallel-vol gradients satisfy the predeclared independent analytic bound.

## Results

Times are microseconds per complete operation. `Snapshot extra` compares the
additional snapshot capability with numeric calibration, not two equivalent APIs.

| Case        | Legacy  | Snapshot | VJP     | Snapshot extra | Round 1 extra | Round 2 extra |
|-------------|---------|----------|---------|----------------|---------------|---------------|
| Flat 9x2    | 5.897   | 17.719   | 18.533  | +200.46%       | +201.46%      | +200.46%      |
| Flat 21x8   | 53.156  | 279.506  | 179.557 | +425.83%       | +425.96%      | +421.27%      |
| Merton 9x2  | 85.486  | 96.182   | 18.216  | +12.51%        | +13.54%       | +10.93%       |
| Merton 21x8 | 794.478 | 1019.731 | 173.153 | +28.35%        | +28.70%       | +27.94%       |

The inexpensive flat IVS makes validation and ordered-sample storage visible:
snapshot construction adds about 12 or 226 microseconds at these sizes. Merton
adds about 11 or 225 microseconds. Its pullback uses the frozen numeric IVS
samples rather than rerunning implied-volatility inversions, so a warm VJP costs
about 18 or 173 microseconds in these cases. This is an added feature cost, not
an improvement claim against a preexisting equivalent quote-risk API.

The shared host and small case inventory limit extrapolation. These measurements
do not cover cold calibration, growing quote axes, MC, bindings, whole-request
latency or memory scaling. Complete Hybrid request cost remains required after
its numerical chain passes. Any later optimization must preserve snapshot and
domain semantics and retain this first measurement rather than replace it.

## Existing paths and retained evidence

All nine final OFF gate executables are SHA-256 identical to the previously
measured binaries. Existing simulation/LSM/tape kernels and legacy numeric Dupire
calibration are unchanged. This carries forward evidence for those identical
executables; it does not resolve the production MC cases retained as inconclusive
in the [P01 report](aad-production-profiling.md). No regression threshold, case,
sample count or failed measurement has been removed or relaxed.

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- `aad-dupire-entry-cost-source/` and `aad-dupire-entry-cost-build/` retain helper,
  exact package configuration and executable.
- `run_aad_dupire_entry_cost.py` retains the orchestration and frozen identities.
- `aad-dupire-entry-cost-measurements-01/` retains environment, every raw
  stdout/stderr, all 240 sample rows and reduced `results.json`.
- `aad-dupire-final-off-gate-identity-01.json` retains the nine executable hashes.

Overall existing production no-regression verdict remains **inconclusive** under
P01. This report establishes only the core opt-in cost and stated numeric checks.
