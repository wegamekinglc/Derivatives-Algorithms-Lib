# Segmented Monte Carlo scoped cost acceptance

Status: selected scope frozen; measurement pending. Correctness precedes timing:
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
Publication and whole-P05 acceptance remain open until those gates pass.
