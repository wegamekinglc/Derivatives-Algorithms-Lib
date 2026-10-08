# Native implicit-root scoped performance acceptance

Local verdict: **no regression** in all 22 affected existing caller rows.
New native root costs are informational; they perform additional recording/report work.
Exact-head CI/platform/review and guarded merge remain required.

## Scope and immutable provenance

Baseline: accepted #501 head a4a43f19a8c76bd908b55536a0328bb3ab25f7e7;
merge 299cc8d02caa408409e66ded93fe4f4a0c8e2a51 has the same tree 532010598dbd8e63dd486ae3b58a0934c856c48f.
GNU GCC 15.2 Release C++17, -O3 -DNDEBUG -ffp-contract=fast, Eigen/native-arch/lifetime/profiling OFF.
The accepted isolated baseline archive and source hashes are frozen before changes.
Head archive is copied from the scoped build after correctness passed; no source edits during compilation or timing.
Fresh baseline caller links match five previously accepted executable hashes.

Archive member comparison uses (name, SHA256) multiplicities: 169 unchanged of 172 baseline members;
three native solve/event objects replaced and one native root object added. Duplicate implicitroot.cpp.o basenames
are distinct numeric/native objects. The numeric root member retains accepted bytes.
Changes to generic return deduction and the narrow collector bridge select ordinary (6),
diagnosed (2), dense checked (6), coordinate (4) and checked coordinate (4) caller rows.
No MC, calibration portfolio, tape-wide, PDE or full parameter matrix is newly measured.
Unchanged numeric-root timing is reused from accepted #501 evidence.

## Paired existing-caller gate

CPU affinity is the minimum allowed logical CPU (CPU 0); DAL_NUM_THREADS=4.
Each case retains two rounds of ten alternating base/head process pairs and best-of-ten minima.
Only >+4% in both rounds rejects; no thresholds, repetitions, shapes or gates are relaxed.
Twenty-two rows retain 200 raw process outputs / 880 observations; four optional rows retain
40 final process outputs / 160 observations. All risk checksums and per-side resources are stable.

| Caller             | Case                                  | Base min ns | Head min ns | Round 1 % | Round 2 % | Verdict |
|--------------------|---------------------------------------|-------------|-------------|-----------|-----------|---------|
| checked            | medium-n32-rhs4-width4-full-cached    | 79195.592   | 76604.458   | -0.869    | -3.272    | Pass    |
| checked            | medium-n32-rhs4-width4-full-complete  | 135590.517  | 136034.383  | +0.360    | -1.119    | Pass    |
| checked            | small-n2-rhs1-scalar-full-cached      | 283.946     | 285.032     | +0.382    | -0.166    | Pass    |
| checked            | small-n2-rhs1-scalar-full-complete    | 924.788     | 931.238     | +0.697    | +0.452    | Pass    |
| checked            | tiny-n1-rhs1-scalar-rhs-only-cached   | 177.999     | 178.081     | +3.260    | -5.073    | Pass    |
| checked            | tiny-n1-rhs1-scalar-rhs-only-complete | 662.262     | 687.898     | +4.125    | -0.724    | Pass    |
| ordinary           | n2-rhs1-width0-activity0-cached       | 189.625     | 189.686     | +0.294    | -0.044    | Pass    |
| ordinary           | n2-rhs1-width0-activity0-complete     | 614.698     | 612.976     | -1.120    | +1.210    | Pass    |
| ordinary           | n2-rhs4-width8-activity2-cached       | 2713.628    | 2723.058    | -0.236    | +1.792    | Pass    |
| ordinary           | n2-rhs4-width8-activity2-complete     | 3304.911    | 3248.838    | -2.774    | +1.882    | Pass    |
| ordinary           | n32-rhs4-width4-activity1-cached      | 22659.211   | 22297.850   | -1.595    | +1.789    | Pass    |
| ordinary           | n32-rhs4-width4-activity1-complete    | 36341.633   | 35997.742   | -0.914    | -0.946    | Pass    |
| diagnosed          | n2-rhs1-width0-activity0-cached       | 189.009     | 190.210     | +0.636    | -0.393    | Pass    |
| diagnosed          | n2-rhs1-width0-activity0-complete     | 817.773     | 819.769     | +0.582    | +0.172    | Pass    |
| coordinate         | band-n64-rhs4-width4-cached           | 58849.430   | 58113.385   | -1.251    | -1.019    | Pass    |
| coordinate         | band-n64-rhs4-width4-complete         | 128331.410  | 128553.550  | +1.202    | -0.188    | Pass    |
| coordinate         | symmetric-n2-rhs1-scalar-cached       | 180.805     | 182.144     | +1.351    | -0.090    | Pass    |
| coordinate         | symmetric-n2-rhs1-scalar-complete     | 626.903     | 624.908     | +0.602    | -1.416    | Pass    |
| checked-coordinate | band-n64-rhs4-width4-cached           | 196912.680  | 196038.980  | -1.244    | -0.444    | Pass    |
| checked-coordinate | band-n64-rhs4-width4-complete         | 484303.130  | 483292.660  | +0.792    | -0.940    | Pass    |
| checked-coordinate | symmetric-n2-rhs1-scalar-cached       | 294.344     | 301.148     | +2.312    | +2.484    | Pass    |
| checked-coordinate | symmetric-n2-rhs1-scalar-complete     | 976.348     | 976.322     | -0.223    | +0.004    | Pass    |

No sustained excess occurs. Small movements inside the calibrated gate support no regression,
not a speedup claim. Measurements are from the shared development host with fixed affinity.

## New optional native cost

Both versions use the same root/candidate/seed checksums and independently checked analytic risks.
Numeric performs owning root capture/reverse. Native additionally registers inputs, publishes output
nodes, clears/seeds the recording, traverses reverse events and returns invocation reports.
Complete native timing includes scope close and full tape clear; cached timing reuses one event.
These are different amounts of work, so their ratio is not an existing-caller regression verdict.

| New case                        | Numeric min ns | Native min ns | Native nodes/events | Retained bytes | Scratch bytes | Caller retained/peak bytes |
|---------------------------------|----------------|---------------|---------------------|----------------|---------------|----------------------------|
| coupled-n2-k2-width4-cached     | 471.341        | 1017.722      | 4/1                 | 880            | 80            | 168/352                    |
| coupled-n2-k2-width4-complete   | 761.887        | 40683.668     | 4/1                 | 880            | 80            | 168/352                    |
| quadratic-n1-k1-scalar-cached   | 92.607         | 279.999       | 2/1                 | 732            | 48            | 112/160                    |
| quadratic-n1-k1-scalar-complete | 281.960        | 39902.995     | 2/1                 | 732            | 48            | 112/160                    |

The full tiny native request minima are about 40–41 microseconds on this host;
cached reverse minima are about 280 ns scalar and 1018 ns for width4.
No universal speedup, sparse factorization or nonlinear convergence claim is made.

## Resource and failure proof

All 26 native root cases have passing local evidence; all 115 affected legacy solve cases pass.
Widths scalar/1/4/8 share owned storage and per-channel scratch. For n=1/k=1 and n=2/k=2,
retained differences equal 116+2*sizeof(Number_); reverse scratch is 48/80 bytes.
Only n output nodes are published. Caller peak includes the same physical scratch also
admitted to the tape; measurements overlap. Exact limits pass and one byte less rejects/refunds.
Cleanup reservation is tape admission headroom and excluded from occupied/peak measurements.

## Evidence and retained failures

Session evidence root: /home/wegamekinglc/.cache/dal-aad-evidence-20261008.
native-implicit-root-performance contains baseline-provenance.json, initial-object-identity.json,
provenance.json, samples.json, results.json and every paired JSONL output.
The initial wrong baseline compiler definition is retained and corrected to the exact accepted command.
The initial optional checksum failure came from six versus twelve printed digits; only the four
optional rows were resampled after fixing their new entry point. The accepted legacy raw rows remain.
The second reduction failed because optional numeric/native resource axes differ; its evidence is retained.
Correct per-side resource parsing reduces existing raw samples without rerunning timing.
The original capacity fixture failures are retained: caller peak omitted scratch overlap and tape
peak incorrectly included reserved cleanup headroom. Corrected tests follow established accounting;
production behavior was unchanged.

## Review correction and scoped reuse

Review 4215890681 identified caller callbacks executing inside event ownership.
A focused RED retains 64 foreign bytes after recording close. The correction
constructs the validated numeric cache and captured bindings in caller context,
then deep-copies into event-owned storage without another factorization.
Three cases verify retained callback buffers, release of a preexisting 4096-double
buffer and throwing callbacks; callback side effects retain caller ownership.
Captured bindings still precede callback evaluation and are revalidated.

The native-only root object changes. Five fresh legacy caller links retain the
exact measured hashes, so all 22 raw legacy rows/gate results are reused, not
resampled. Only four optional root rows are resampled under the same protocol.
Retained tape and per-channel scratch stay 732/880 and 48/80 bytes. Caller capture
peak includes the staged numeric/binding/value storage: 132/352 bytes for n=k=1/2.
The coupled caller peak is now 352 rather than 248. Exact/one-byte-short tests
cover both scalar reverse overlap and coupled capture admission, with refunds.
The original now-stale caller-peak assertion failure is retained and replaced
with the explicit maximum of staged capture and actual reverse overlap.

native-implicit-root-pre-callback-performance retains the previous complete
measurement/provenance, and native-implicit-root-callback-caller-identity.json
records fresh identity. New raw optional rows and final hashes are under
native-implicit-root-performance. No generic buffer-accounting change occurs.

The later diagnostic-layout CI resource test correction changes tests/docs
only. It includes the staging/publication phase, whose coupled peak is 480
bytes with larger diagnostic Number handles versus 448 for construction.
OFF remains at 352. Actual tape/caller budgets, resource thresholds and all
production/archive/binary timing hashes are unchanged; no timing is repeated.
The two affected OFF resource tests and strict OFF/combined checks pass.

## Final shared-error review correction

The final review body identified misleading LinearSolve prefixes and two mutable
fixture members. Three focused RED cases reproduced null/wrong-phase, stale-input
and alias-overflow diagnostics. Shared error labels are now RecordedOperation;
the success-path algorithms and admission rules are unchanged. Test state is
caller-managed. All 25 native and 115 existing solve cases pass, with 12 affected
OFF/combined strict checks and a refreshed installed consumer.

This changes shared native objects, so the final 22 affected legacy rows and four
optional rows are freshly measured under the original paired protocol. No other
performance families are sampled. All 22 legacy rows pass: the tiny complete
row moves +4.125% in round one and -0.724% in round two, so it does not violate
the predeclared sustained-both-rounds gate. Failed/intermediate evidence remains
in native-implicit-root-pre-neutral-performance and the earlier retained folders.
The tables above and hashes below describe this final shared-error correction.

## Post-callback phase correction and final scoped reuse

A focused RED reproduces root publication after the equation calls FinishRecording.
Moving AccuracyRecording after CaptureRoot preserves one phase/mode check while
rejecting before event identity/publication. The new case and six affected cases
pass, plus four affected OFF/combined strict checks. No existing numeric/shared
object changes in this repair. Five fresh legacy links retain the final neutral-error
hashes, so their 22 raw rows are reused. Only four optional costs are resampled;
new process count is 40. The prior full evidence remains in
native-implicit-root-pre-phase-performance; caller identity is retained in
native-implicit-root-callback-phase-caller-identity.json. The new optional table
and final hashes describe this correction. No unmeasured family is claimed as a pass.

## Final production hashes

- dal-cpp/dal/math/aad/implicitroot.cpp: 32692ad92dcd9384e819749b7d70a674d28e55f0e2508f35e71f6b5be0c0f5ac
- dal-cpp/dal/math/aad/implicitroot.hpp: c445bbc105b7744a60ac7c60824a6c626af72ba736f670efc637eaec92a9efbd
- dal-cpp/dal/math/aad/linearsolveinternal.hpp: 0e6721ae129cd91970a3fceab7f4d0e2aa6619bebf8f3b720b4334b25bdcb8a1
- dal-cpp/dal/math/aad/linearsolveaccuracy.cpp: e4dad3818bfb1eaab35564c4f99f6abdcf38905d54974ed67d109dafc883b005
- dal-cpp/dal/math/aad/reverseevent.cpp: be5d191b9c34800cdd55d12f39408edf501efc0e9486f3f91b4d80ef75029424
- dal-cpp/dal/math/aad/linearsolveaccuracyinternal.hpp: 33dd76da500cfd71f2a72b1299ddcc7506607e6e0f4d8cce84b9f77c4503d984

Baseline archive SHA256: 9a2a9abff81127422dcf31901503be543c8dabf5127fb778f3e4ecc6f417b398
Head archive SHA256: 02f6a3538170358ab7654d8bfa71ed1e06ce6717ca8d9d11f0fdf972e5064fb2

Publication and final exact-head acceptance must be inspected before merge.
