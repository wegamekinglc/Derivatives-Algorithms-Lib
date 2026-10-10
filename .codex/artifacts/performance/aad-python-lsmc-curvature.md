# Python LSMC curvature boundary acceptance

## Selected scope before measurement

| Changed paths and actual callers                                             | Selected evidence                                                                                                           | Exclusions                                                                                                                       |
|------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------|
| `dal-public/src/lsmccurvature.*`, new Python binding and module registration | Complete compiled Frozen and RetrainedBump Python calls; N=5, M=1, 128 training paths, 35 pricing paths, two exercise dates | Tree, history, bridge/precision, validation/RQMC, M=0/M=2 and budgets have focused correctness tests; no Cartesian timing matrix |
| Public installed headers and exported symbols                                | Installed consumer and script consumer                                                                                      | No additional financial algorithm timing                                                                                         |
| Existing native LSMC, AAD, RNG, calibration and prior bindings               | Verify immutable source/dependency/configuration and old object identities against merged #531                              | No changed native algorithm; do not repeat unrelated benchmark targets or claim new passes                                       |

Baseline: merged #531, `b047d0b37be388b83b173720acfbfdef7cf533f2`.
The baseline bridge invokes its accepted native LSMC evaluator and copies numeric
outputs through ctypes. The head invokes the new owning public/Python result
and copies its numeric getters. Preparation is outside timing on both sides;
policy training, pricing and all three gradient evaluations are inside.
These ownership and conversion contracts differ: costs are informational, with
no +4% regression verdict or speedup claim.

Two rounds of ten alternating interleaved process pairs per mode: 80 samples
total, each calibrated to at least 25 ms. Pin processes to one available CPU,
retain commands, compiler/platform, affinity, load, hashes and raw samples.
Compare round minima. Export only bridge C entry points to isolate native DAL
registries from the Python extension.

Before and after each timed loop, validate every numeric output: Frozen uses an
independent finite-path price replay over the retained fixed policy and finite
differences of that same policy; RetrainedBump uses the existing declared
first-order estimator at base and actual outer points. The untimed reference
may read the head's passive policy but never supplies an objective callback to
the timed native work. Record the actual measured totals after acceptance.

## Accepted local results

All 80 samples pass their untimed numerical checks. Timed loops total
8.17993001 seconds; the shortest sample is 88.781788 ms. Each mode has two
rounds of ten alternating pairs under `DAL_NUM_THREADS=1`, verified from
runtime startup output, and one pinned CPU. Raw evidence is retained in
`aad-python-lsmc-curvature-results.json`.

| Mode          | Round | Native bridge minimum (us) | Owning Python minimum (us) | Difference |
|---------------|-------|----------------------------|----------------------------|------------|
| Frozen        | 1     | 101.319                    | 109.586                    | +8.16%     |
| Frozen        | 2     | 101.654                    | 110.508                    | +8.71%     |
| RetrainedBump | 1     | 693.608                    | 706.952                    | +1.92%     |
| RetrainedBump | 2     | 694.136                    | 711.007                    | +2.43%     |

These are informational unequal-contract costs, not a regression or speedup
verdict. No additional sample set is justified by these figures.

The native archive remains SHA-256
`967f5e721f0ba67354b66f8eb14d8be9c4f7e422463bc8c412864491ee06997c`;
all 402 accepted native headers and native source/configuration remain unchanged.
All 27 prior public archive members are byte-identical. Of 21 prior Python
objects, 17 are byte-identical, the module adds registration, and three differ
only in installed-header diagnostic path strings/alignment and symbol sizes.
Read-only ELF comparison proves their code and relocations identical; no object
is rewritten for inspection. Accepted old timing therefore remains applicable
to those unchanged computations, without claiming omitted cases ran again.

Fresh head public archive: `236fe9d9833210b2ea8b8dcd6fc0272ca5e54e0fab775e6159f8eede07e22d62`.
Fresh Python module: `700762bc8509071d5e330ce70874bb322e3bb88191e1eca94b589cf5d29d3252`.
Bridge: `839786eb3298154a11e3c7a2219b0447c25d0da9e6311c2278ef28ec03c19f57`.
