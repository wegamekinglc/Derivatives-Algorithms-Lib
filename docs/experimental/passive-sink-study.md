# Native passive constant sink prototype

The per-tape sink is useful in the synthetic workload with many newly recorded
constants. It does not meet the ordinary Monte Carlo latency adoption threshold
in the selected production cases. Keep it as an isolated research prototype;
retain the production `Number_(double)` and registration contracts. GSR's smaller
tape allocation and process RSS justify considering a future memory-constrained
caller, with an explicit compatibility design before adoption.

## Implementation and scope

The [experiment](../../dal-cpp/experimental/passivesink/README.md) applies
`native-sink.patch` to a detached source tree. Production headers and build options
are unchanged. One lazily allocated leaf receives all passive constant adjoints
for the current tape. Explicit registration creates distinct input leaves;
passive adjoint reads/seeds reject. Checkpoints retain or discard the cached sink
with its prefix/suffix, and full resets invalidate it. Native optional lifetime
diagnostics check stale/foreign bindings. `Number_` remains 16 bytes in both timed
builds. Expression operand edges remain and continue writing to the sink.

The admitted domain is native scalar/vector expression recording, weighted/block
roots and ordinary MC for the selected models. Recorded operators, structural
Jacobian constant outputs, LSMC, calibration and public bindings are not admitted.
The implicit construction contract changes only within the overlay build. This
study does not approve a public migration or production adoption.

## Method and identity

Both sources use master `3be8dab9997646c040db209eaa999a459d556d59` plus the
same experimental harness. The head additionally applies the sink overlay.
Independent out-of-tree builds use GCC 15.2.0, CMake 4.2.3, Release, static
Native/AADET, Eigen enabled, native-arch disabled, profiling disabled and lifetime
diagnostics disabled for timing. A third build enables lifetime diagnostics for
boundary checks. All four initialized dependencies use clean declared gitlinks.
The [machine-readable report](data/passive-sink-summary.json) retains dependency,
source-patch and executable hashes; [CSV](data/passive-sink-results.csv) retains
every selected timing metric.

All heavy builds/tests and the complete sampling cohort hold the same shared
workspace measurement flock. External load is checked without stopping other
work. The host is a shared WSL2 i9-13900HX. Single workers are fixed to CPU 0;
four workers use CPUs 0–3. Each case alternates base/head first across two rounds
of ten independent processes per side. Each round reduces by minimum. Negative
deltas mean faster; improvement/regression requires both rounds to exceed four
percent in that direction. These explicitly requested experimental measurements
are outside the nine-target production gate.

Each chain process uses three warmups followed by twenty timed iterations,
reporting its minimum. Recording includes scope setup, registration and cleanup;
reverse includes adjoint clearing and seeding. Production request timing includes
preparation and uses Default Fast Sobol with no bridge or scramble. Process RSS
includes process startup and is separate from tape payload/capacity.

## Measured latency

Times below are combined minima across the two rounds, in milliseconds. The two
round deltas determine the verdict, rather than the combined minimum alone.

| Case                                                          | Base ms  | Sink ms  | Round 1 delta | Round 2 delta | Verdict     |
|---------------------------------------------------------------|----------|----------|---------------|---------------|-------------|
| Short BS, compiled, 65536 paths, 1 worker                     | 7.139    | 7.167    | -0.74%        | +0.39%        | no-change   |
| Long BS, compiled, 8192 paths, 1 worker                       | 189.015  | 188.843  | +5.54%        | -0.09%        | no-change   |
| LV 8×8, compiled, 2048 paths, 1 worker                        | 77.540   | 78.005   | +1.01%        | +0.60%        | no-change   |
| GSR 3F, compiled, 8192 paths, 1 worker                        | 380.033  | 374.248  | -1.78%        | -1.52%        | no-change   |
| GSR-SLV 3F, compiled, 4096 paths, 1 worker                    | 512.511  | 512.438  | -1.89%        | -0.01%        | no-change   |
| Long BS, tree, 8192 paths, 1 worker                           | 229.244  | 226.248  | +0.40%        | -1.31%        | no-change   |
| Long BS, compiled, 8192 paths, 4 workers                      | 56.290   | 54.322   | -0.86%        | -3.50%        | no-change   |
| 50K Number-constant chain, scalar, recording                  | 0.561259 | 0.423390 | -22.85%       | -24.56%       | improvement |
| 50K Number-constant chain, scalar, clear/seed/reverse         | 0.403821 | 0.320416 | -19.95%       | -20.65%       | improvement |
| 50K Number-constant chain, width 4, recording                 | 0.921182 | 0.565523 | -41.24%       | -38.61%       | improvement |
| 50K Number-constant chain, width 4, clear/seed/reverse        | 0.662373 | 0.481659 | -30.35%       | -23.25%       | improvement |
| 50K double-constant chain, scalar, recording control          | 0.239205 | 0.239148 | -1.70%        | -0.02%        | no-change   |
| 50K double-constant chain, scalar, clear/seed/reverse control | 0.147627 | 0.146746 | +0.06%        | -0.60%        | no-change   |

No selected ordinary MC case improves by more than four percent in both rounds,
and none regresses by more than four percent in both rounds. Long BS has a +5.54%
first-round signal and -0.09% second round; both are retained. Shared-host absolute
minima are not portable forecasts, and sub-threshold changes are not proof of zero
benefit. The artificial chain has 100,000 new constant constructions per recording;
ordinary MC mostly reuses prefix constants.

## Memory and graph effects

The Number-constant chain shrinks from 150001 to 50002 nodes while retaining all
150000 edges. Scalar occupied tape decreases from 8,400,072 to 4,400,112 bytes;
width-four occupied tape decreases from 13,200,104 to 6,000,176 bytes. Scalar tape
capacity decreases from 9,961,472 to 6,029,312 bytes, and width-four capacity from
14,680,064 to 7,602,176 bytes. The double-constant control remains at 50001 nodes,
50000 edges and 2,800,040 occupied bytes. Its lower cost illustrates the extra
edges/writes that a shared sink still retains.

Eight-path production structure probes retain one sink candidate in the prefix;
they eliminate the previously observed extra constant leaves without merging
registered model/script inputs. Their prices and every named risk match by double
bits. Maximum occupied/allocated tape per worker and median process peak RSS
across the twenty timed processes per side are:

| Model      | Base occupied B | Sink occupied B | Base capacity B | Sink capacity B | Base RSS KiB | Sink RSS KiB |
|------------|-----------------|-----------------|-----------------|-----------------|--------------|--------------|
| Short BS   | 1216            | 856             | 1966080         | 1966080         | 10672        | 10674        |
| Long BS    | 246648          | 173248          | 1966080         | 1966080         | 12718        | 12762        |
| LV         | 503368          | 429928          | 1966080         | 1966080         | 12370        | 12350        |
| GSR 3F     | 1556704         | 1404184         | 2621440         | 1966080         | 12702        | 11566        |
| GSR-SLV 3F | 1822464         | 1659824         | 2621440         | 2621440         | 12876        | 12824        |

GSR crosses a node-block boundary: allocated tape decreases 25%, and median peak
process RSS decreases about 8.94%. Other cases reduce occupied payload without
the same allocated-block reduction. The structure probes and process RSS use
different path counts and measurement boundaries; they are not added together.
No production-memory threshold is inferred from the synthetic graph reductions.

## Validation and limitations

- Base and sink native contract binaries each pass five tests. The diagnostic
  sink build passes seven, including rejected-reset preservation, discarded
  suffix/clear and exited-thread bindings, plus re-registration.
- The existing production-profile CLI checks pass nine tests on each build.
  All 280 timed MC requests preserve PV and every complete named risk axis by
  double bits, including signed zero. Ten eight-path structure recordings also
  preserve the corresponding result fingerprints. Three chain workloads preserve
  their value and gradient fingerprints and independently check the analytic
  chain result.
- The collector passes three meaningful rejection/reduction tests in ordinary
  Python and with `-O`: adjacent doubles/signed zero, missing processes/rounds,
  nonfinite evidence, and sustained two-round verdicts. Preparation is smoke-tested
  by creating fresh worktrees and applying the overlay.
- A diagnostic-only reset-cache ordering correction is included after sampling.
  Both timed head executables are rebuilt and remain byte-identical to the sampled
  executables. The original sampled overlay and final overlay hashes, plus the
  executable identity proof, are retained. The valid timing cohort is reused.

The final cohort has 414 successful commands and thirteen paired timing metrics.
Raw stdout/stderr, resource records, load snapshots, patches, source/configuration
and hashes are retained in the issue attachment. Full regression, Windows,
sanitizers, all vector widths, structured operators, LSMC and calibration are not
claimed as validated. The shared host is not an exclusive measurement machine.

The conclusion is **limited experimental value, no general ordinary-MC adoption**.
An explicitly passive representation for an actual high-constant or constrained
GSR caller may merit further work. Production adoption still needs the original
public compatibility/migration design, complete price/risk and lifetime coverage,
and accepted benefit on that actual caller. Existing double coefficient paths
remain preferable where the coefficient is already known to be fixed.
