# Segmented Monte Carlo scoped cost acceptance

Verdict: no regression in the four selected ordinary MC callers. Segmented MC is
an explicit memory/latency tradeoff, with no automatic promotion. Correctness precedes timing:
eleven new cases pass, all eleven strict checks pass, and an installed-only
consumer checks mean value and every gradient column.

## Selection and exclusions

The changed hot path is the new explicit segmented MC request: RNG lane creation
and seeking, one fixed-path replay per draw, bounded task waves, ordered reduction
and cleanup. Extend the existing `tape_perf` executable with its explicit
`--financial-segmented-mc` command. Keep 128 common Sobol paths, no bridge, offset
zero, h=64 and the same prepared running-sum product and model point on each side.
Select 16 and 2048 daily fixing steps, each with one and four pool threads: four
complete-request shapes, not a generator/size Cartesian matrix.

Compare ordinary full MC on the immutable parent with ordinary full MC on the
branch for those four caller controls. Reuse the branch full-MC samples as the
reference for four explicit segmented/full strategy comparisons. Old ordinary
MC templates and all 179 existing archive members are unchanged; this focused
control covers the rebuilt executable and public caller without repeating prior
fixed-path, tape, RNG, LSMC, calibration, PDE or solver matrices. Prior #513 cost
evidence remains accepted through byte-for-byte provenance, not newly measured.

## Frozen protocol

Baseline: merge parent `201339fd305e677d455e127e74bd6a5dbd65ba63`.
Capture the implementation commit before creating detached baseline/head sources
and separate Release builds. Reuse the verified old 179-member archive on the
baseline; the head archive preserves those bytes and adds the new MC object.
Compile benchmark sources against their corresponding isolated headers and
archives. The identical new command harness is overlaid on the baseline with
`DAL_SEGMENTED_MC_BASELINE`, which only excludes the unavailable new entry point.
Retain all hashes, commands, compiler/CPU/configuration and overlay identity.
Explicitly enable benchmarks; do not use installed timing binaries.

Each process performs one warmup and three timed complete requests, including RNG,
replay, reduction, result materialization and cleanup. Use two rounds of ten
interleaved process samples per build/strategy/shape, rotate first position, and
reduce each round to its minimum. Ordinary caller regression requires more than
+4% in both rounds. New strategy ratios describe a memory/latency tradeoff, not
an automatic-selection recommendation. Numerical preflight checks mean value and
all four model gradients on every selected shape. Timing is read-only.

Separate untimed ordinary/aligned C++ allocation probes measure simultaneous
process payload peaks and retained payload, in separate full/segmented processes
with the same initial caller-tape floor. Include preparation, the segmented
adapter when present, model/inputs, all RNG lanes and coordinator storage, retained
worker/caller tapes and the consumed final result. Record cold and warm requests;
do not sum unrelated high-water marks or mistake per-path metadata for aggregate
memory. Exclude allocator metadata, direct C allocations, stacks and RSS, and name
those limits in the result. Fixed-path savings do not establish MC savings.

## Evidence and verdict

Evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261008`.
`segmented-mc-library/verification.json` proves 179 unchanged members and the
unchanged legacy fixed-path request body. Focused RED/GREEN, strict compilation
and installed-consumer logs use the `segmented-mc-` prefix. Record paired raw
outputs, numerical preflight, resource rows and final verdict here after sampling.
Implementation and local cost acceptance pass; current-head publication remains open.

The first 240-sample pass is retained but inconclusive: concurrent Rust build
load changed during sampling, and second-round minima improved by roughly 2–3x.
Do not accept those unstable ratios. Repeat only the same four selected shapes
after the competing load subsides, with the same frozen executables, two rounds,
ten samples and reduction/threshold. Keep the initial raw records separately;
do not expand to an unrelated target or parameter matrix.

An unpinned confirmation still shows 8–15% ordinary-caller minima drift, and
another multi-core Rust compiler is active. Preserve it as inconclusive. Mitigate
CPU migration/competition by pinning both sides identically: CPU 1 for one-thread
requests and CPUs 1,4,22,28 (four distinct physical cores) for four-thread requests.
Repeat the same four shapes and unchanged sampling policy; retain CPU topology,
affinity and the shared-host caveat. No universal quiet-host claim is justified.

Affinity confirmation stabilizes paired ordinary-caller deltas for short one/four
threads and long four threads. Long one-thread remains noisy (+6.0%/-5.8% across
rounds). Repeat only that one shape, preserving the same executable, affinity and
two-by-ten protocol; reuse the other three rows and all resource/numerical evidence.

## Accepted results

Frozen implementation: `bb7492ba2d9959903e3e33a5d7b30f21ba19be3b`.
GCC 15.2.0, CMake 4.2.3, Intel Core i9-13900HX, native AAD, Release/O3/NDEBUG,
ffp-contract=fast, PIC library archive, Eigen enabled without Eigen parallelism,
native arch off.
Separate detached `base-source`/`head-source` and build roots are under
`segmented-mc-performance/`. The focused CMake driver explicitly enables benchmarks
and imports the verified archives instead of rebuilding unchanged objects.
Archive SHA-256s are `4637807865a8c9b49601b84abb78e9bde74afa4779b2b6a033a7117e52e60205`
(base) and `08d7a68b83caafb86db7370d8bbd3cc9084bb21713e93385c5bab9612845b9be`
(head); executable, overlay and probe identities are in `environment.json`.

Each row retains twenty samples per side in two best-of-ten rounds. All four
ordinary caller controls pass the sustained +4% gate. Paired short-case deltas
stabilize despite common absolute drift; the final long/one-thread repeat has
about 1–2% minima drift. Shared WSL/background compilation remains a limitation
on generalizing absolute times. Do not claim a universally quiet machine.

| Shape      | Threads | Parent full ms | Head full ms | Combined delta | Round 1 delta | Round 2 delta |
|------------|---------|----------------|--------------|----------------|---------------|---------------|
| 16 steps   | 1       | 0.1600         | 0.1526       | -4.66%         | -4.66%        | -4.26%        |
| 16 steps   | 4       | 0.1176         | 0.1116       | -5.09%         | -5.09%        | -4.77%        |
| 2048 steps | 1       | 16.0574        | 16.0796      | +0.14%         | +1.23%        | -1.26%        |
| 2048 steps | 4       | 5.5142         | 5.7198       | +3.73%         | +0.99%        | +3.73%        |

The new strategy comparisons are informational; they share the accepted head
full samples above and use the same numerical outputs and consumer payload.

| Shape      | Threads | Full ms | Segmented ms | Combined ratio | Round 1 ratio | Round 2 ratio |
|------------|---------|---------|--------------|----------------|---------------|---------------|
| 16 steps   | 1       | 0.1526  | 1.3161       | 8.63x          | 8.63x         | 8.11x         |
| 16 steps   | 4       | 0.1116  | 0.3829       | 3.43x          | 3.43x         | 3.34x         |
| 2048 steps | 1       | 16.0796 | 130.5103     | 8.12x          | 8.05x         | 8.12x         |
| 2048 steps | 4       | 5.7198  | 41.8571      | 7.32x          | 7.48x         | 7.32x         |

Warm simultaneous C++ allocation payload peaks, including the initial 1,966,144
byte caller-tape floor, complete passive preparation, adapter, request owners,
RNG lanes, task storage, worker tapes and consumed result:

| Shape      | Threads | Full bytes | Segmented bytes | Change  |
|------------|---------|------------|-----------------|---------|
| 16 steps   | 1       | 2,005,807  | 2,004,947       | -0.04%  |
| 16 steps   | 4       | 2,007,943  | 2,012,795       | +0.24%  |
| 2048 steps | 1       | 7,119,391  | 6,440,755       | -9.53%  |
| 2048 steps | 4       | 16,896,567 | 13,258,387      | -21.53% |

These are observed peaks, not process bounds. Worker scheduling changes retained
tape allocations: cold four-thread short probes show 2,007,911 full bytes versus
3,979,971 segmented bytes (+98.2%), while the warm rows above are nearly equal.
Cold long four-thread peaks are 17,568,359/13,258,355 bytes. Keep cold/warm records,
not only favorable long-path rows. The probe excludes allocator metadata, direct
C allocations, stacks, common registration/thread-pool infrastructure and RSS.
It includes all request-owned payload and actual retained tapes simultaneously;
the initial caller tape is added explicitly. Metadata remains per-path only.

`accepted-results.json` merges the three accepted affinity rows with the final
long/one-thread row. Its SHA-256 is
`f65cfff767177d5984285dc0e086cc77fd2945d0e04cb2668c1fa59a2f748e92`.
Raw accepted outputs are in `affinity-confirmation/raw/` and
`running-one-confirmation/raw/`; those directories contain per-round results and
numerical preflight. `heap-results.json` and sixteen `heap-*.json` retain resource
records. Initial/unpinned and superseded single-shape samples remain diagnostic:
240 accepted process samples plus 540 retained noise diagnostics, about 165 seconds
total sampling. No unrelated matrix ran. Documentation-only publication changes
reuse these immutable bodies, archives and executable identities without new timing.
