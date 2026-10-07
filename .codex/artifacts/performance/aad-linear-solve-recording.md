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

The user's 2026-10-07 project-wide scope rule supersedes the earlier full-matrix
plan. Freeze the stable repair and select cases by changed tape/reset, raw-adjoint,
reverse-event and owned-buffer paths. Cancel the unstarted 81-case portfolio
matrix. Use representative affected scalar/vector and compact/large request
boundaries, reusing accepted hash-identical evidence where applicable. Unchanged
PDE/RNG/interpolation/Krylov/banded/Cholesky algorithms need no new sampling.
Record the final caller-to-case mapping before measurement. Keep the existing
4% two-round gate and relevant portfolio A/A controls/256-request compact windows.
New recorded-solve costs cover complete/cached work and small/larger dimensions;
their protocol includes cache cleanup and separately timed owning destruction,
with oracle scans outside timing. Final platform/Codacy/review acceptance remains open.

| Changed path | Selected acceptance | Scope reason |
|--------------|---------------------|--------------|
| Ordinary tape/reset/event dispatch | `tape_perf` | Direct recording, reset and propagation costs |
| Raw adjoint/scalar/vector harvesting | `jacobian_perf`; eight scalar/eight weighted entry rows | Default reads, dense/blocked harvesting and existing public requests |
| Rate calibration/quote-risk adjoint callers | `rate_risk_perf` | Existing production callers of shared native paths |
| GSR raw adjoint hot loop | Six already measured GSR rows, subject to final binary identity | Reuse the accepted 30-pair confirmation; retain noise/failure evidence |
| Simulation/replay callers | Ordinary MC, compiled BS LSM and compiled daily local-vol LSM | Cover ordinary and replay tape lifecycles; tree replay shares the validated native path |
| Portfolio scalar/vector/request boundaries | Six cases: compact weighted/all; large weighted/sparse; large width-8 Jacobian/sparse; compact width-3 Jacobian/all; compact wide width-3/sparse/one worker; compact empty request | Cover affected widths, selection, reset, worker storage and compact/large costs without the 81-case product |
| New solve cache/reverse/owning results | Eight cost rows: complete/cached, n=2/32, one/four RHS | Dimension endpoints and RHS/cache boundaries; n=8/16 add no distinct implementation branch |

PDE, RNG, interpolation, Krylov, banded and Cholesky need no new measurement.
Curve calibration is covered by the selected Jacobian/rate callers. The existing
scheduled full gate remains applicable. Omitted cases are outside this change's
selected acceptance and are not reported as freshly measured passes.

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

## Final request sweep at `64cdbfa6`

Fourteen final head/baseline binaries and current archives are frozen in
`aad-recorded-solve-perf-build-proof-03/provenance.json`. The exact nine-target
acceptance covers 75/75 rows: fresh rate-risk measurement plus hash-identical
reused tape/Jacobian and six unchanged targets. The assembled identity/raw-data
ledger is `aad-recorded-solve-final-nine-02/results.json`; no threshold changes.
Curve calibration and eight scalar plus eight weighted request rows pass.

The complete MC sweep in `aad-recorded-solve-final-mc-02/` fails one case:
GSR market 48-node price Jacobian, 12 quotes, AAD, at +10.27%/+10.46%.
Baseline minima are 19.298/19.224 ms; head minima are 21.280/21.235 ms.
Other MC cases pass, including all three LSM value/risk comparisons. Retain this
failure and stop queued portfolio/cost sampling on the failing candidate.

The PIC GSR harvest disassembly makes three repeated general-dynamic TLS address
queries for raw adjoint checks. Their call boundaries force register saves even
when the final executable linker relaxes TLS access. The local repair exposes
the thread-stable flag address through a compiler-const accessor, following the
existing buffer-budget slot pattern. It caches an address, never a mutable flag
value; event/failure/reset updates retain their semantics. Focused correctness,
GSR paired acceptance and final fresh binary/platform gates remain required.

The stable-address-only candidate passes 21 focused recording/failure tests,
but GSR still fails +6.93%/+7.18% in `aad-recorded-solve-gsr-pairs-03/`.
An attempted fused read/clear of one validated reference preserves all 25
recording/GSR oracle tests after restoring repository test initialization, but
fails +16.63%/+16.06% in `aad-recorded-solve-gsr-pairs-04/`. Restore the original
GSR implementation; this rejected local change is not part of delivery.

Move default-tape initialization/event validation into a cold out-of-line
function called only when the stable flag is true. Raw read failure/mode/null
semantics remain unchanged. The next candidate requires focused oracle tests
and affected paired measurements before final scoped acceptance.

The cold guard candidate passes 25/25 focused recording/GSR tests. Its first
ten-pair confirmation is borderline and fails +4.09%/+4.22% in
`aad-recorded-solve-gsr-pairs-05/`; retain that failure. Same-binary A/A controls
then pass for both sides. Thirty-pair confirmation on the same source/binary
passes all six GSR rows in `aad-recorded-solve-gsr-pairs-06/`. The previously
failing 48-node AAD row is +3.13%/+3.95%, within the unchanged 4% threshold but
close to its boundary. The final affected build passes 132/132 tests in
`aad-recorded-solve-final-affected-build-06.log`. Final binary identity and
remaining affected-path/platform acceptance remain open.
All eleven affected translation units also pass final combined lifetime/profiling
ON syntax checks in `aad-recorded-solve-cold-combined-syntax-01.log`.
