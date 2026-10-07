# Recorded solve performance acceptance

Status: resource-head regressions repaired locally; complete acceptance is open.
Do not merge or reuse earlier #490 timing acceptance for this implementation.

## Frozen evidence and protocol

Baseline is accepted #490 merge `a08d20e02396ef89629df3edb869f81bca0033f9`.
Fourteen immutable fresh-linked baseline binaries retain their accepted hashes.
The resource head targets and request helpers are rebuilt/recompiled with the
matching current headers and frozen separately before sampling. Full commands,
source/archive/helper hashes and binary pairs are in session evidence
`aad-recorded-solve-perf-build-proof-01/provenance.json`.

Release GCC 15, native AAD, Eigen ON, lifetime/profiling OFF, native architecture
OFF, DAL_NUM_THREADS=4 and CPU affinity 0/2/4/6. The existing executable gate
uses two independent rounds of ten alternating process pairs and minimum
duration. Failure requires greater than 4% in both rounds. All 75 case rows in
the nine-target gate have complete samples. Raw outputs, samples, comparisons
and failures are retained in `aad-recorded-solve-legacy-nine-01/`.

## First resource-head results

| Target        | Case                                        | Round 1 | Round 2 | Verdict    |
|---------------|---------------------------------------------|---------|---------|------------|
| tape_perf     | Clear and re-record, 100K nodes              | +4.28%  | +4.76%  | Regression |
| tape_perf     | Rewind and re-record, 100K nodes             | +6.13%  | +6.41%  | Regression |
| jacobian_perf | Analytic dense harvest, 24 by 23             | +44.02% | +44.05% | Regression |
| jacobian_perf | Analytic row-width harvest, 24 by 23         | +22.94% | +22.94% | Regression |
| jacobian_perf | Production dense harvest, 95 by 96           | +4.25%  | +4.53%  | Regression |
| jacobian_perf | Production prefix harvest, 95 by 96          | +4.93%  | +5.30%  | Regression |

The other seven targets and remaining cases pass the unchanged gate. The head
Sobol precise/fast ratio is 9.11, below the existing 10.00 ceiling. These results
accept neither the overall implementation nor any later local repair.

Stop the queued full request/portfolio/cost stages after this finding; retain
the complete nine-target output rather than spending those samples on a known
failing candidate. Investigate ordinary raw adjoint reads, default tape
initialization and shared reset paths. Preserve failed-graph/mode validation,
Number/TapNode layout and ordinary node allocation/propagation behavior.

## Open acceptance

First repeat the affected tape/Jacobian pair after a correctness-preserving
repair. Then freeze the stable accepted source and run the complete nine-target,
curve, scalar/weighted, MC and 81-case default/selected portfolio workload matrix.
Portfolio A/A controls and the calibrated 256-request compact windows remain
required. Complete recorded-solve cost/storage measurements are informational
new coverage and do not replace old-path regression acceptance. Their frozen
protocol includes cache cleanup and separately timed owning result destruction;
oracle scans are excluded. Final platform/Codacy/review acceptance remains open.

The first repair candidate keeps a constant-initialized thread-local event-check
flag for raw adjoint access, updated only at event/failure/reset boundaries of
the default tape. Caller-owned tape resets cannot clear default-tape failure
checks. A private delegating constructor identifies the default tape without
changing node allocation; the ordinary constructor is inline again. Focused
failure/mode tests plus affected existing native/resource contracts pass 128/128
in `aad-recorded-solve-fast-guard-affected-01.log`. Header formatting unrelated
to the repair is restored before the next benchmark build. Its affected two-target
sampling in `aad-recorded-solve-fast-guard-pairs-01/` completes 17 case rows:
all six original failing cases pass, including analytic dense harvest at
+2.10%/+2.08%. Production dense harvest at 95 by 96 improves
26.27%/26.22%. However passive-constant recording now fails at +8.06%/+7.66%,
so that candidate remains unaccepted. Explicitly inlining both constructor
layers also fails passive recording at +6.81%/+8.15% in
`aad-recorded-solve-inline-guard-pairs-01/`; those hints are removed.

Disassembly identifies a concrete spill: adjacent false/true constructor flags
occupy a saved general register and move the repeatedly recorded value to the
stack. Separate the flags with the existing storage counters and fit the cold
event state into the original padding. The final two-target candidate retains
the original 64-bit Tape size (368 bytes); Number/TapNode are unchanged. Its
128-test predecessor already protects graph failure/mode semantics; the final
layout requires fresh functional/ON/platform evidence.

`aad-recorded-solve-separated-guard-pairs-01/` passes all 17 two-round case rows.
Passive recording is +0.97%/+0.98%; analytic dense harvest is +0.34%/+0.43%;
all six original failing cases pass. Production dense harvest at 95 by 96 is
-25.88%/-25.95%. Disassembly restores the recorded value to a saved register.
The platform-derived padding expression preserves the same 64-bit layout.
The final build passes 128/128 affected cases in
`aad-recorded-solve-repaired-final-affected-01.log`. Both freshly built targets
match the accepted repair hashes exactly, recorded in
`aad-recorded-solve-final-guard-binary-identity-01.json`; the 17-case results
remain valid without repeated timing.
Eleven final affected translation units also pass combined lifetime/profiling
ON syntax checks in `aad-recorded-solve-repaired-combined-syntax-01.log`.
Complete remaining request/portfolio/cost and exact-head CI gates stay open.
