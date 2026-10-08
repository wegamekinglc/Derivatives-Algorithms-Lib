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

Archive member comparison uses (name, SHA256) multiplicities: 170 unchanged of 172 baseline members;
two native solve objects replaced and one native root object added. Duplicate implicitroot.cpp.o basenames
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
| checked            | medium-n32-rhs4-width4-full-cached    | 76030.675   | 76610.480   | +0.763    | +1.074    | Pass    |
| checked            | medium-n32-rhs4-width4-full-complete  | 132246.212  | 133827.535  | +1.196    | +1.012    | Pass    |
| checked            | small-n2-rhs1-scalar-full-cached      | 284.158     | 288.153     | +1.266    | +1.876    | Pass    |
| checked            | small-n2-rhs1-scalar-full-complete    | 921.082     | 928.372     | -0.083    | +1.627    | Pass    |
| checked            | tiny-n1-rhs1-scalar-rhs-only-cached   | 177.837     | 177.712     | -0.070    | +0.077    | Pass    |
| checked            | tiny-n1-rhs1-scalar-rhs-only-complete | 662.075     | 662.811     | -4.400    | +2.409    | Pass    |
| ordinary           | n2-rhs1-width0-activity0-cached       | 188.832     | 189.286     | -0.978    | +0.817    | Pass    |
| ordinary           | n2-rhs1-width0-activity0-complete     | 612.274     | 608.361     | -1.044    | -0.639    | Pass    |
| ordinary           | n2-rhs4-width8-activity2-cached       | 2677.039    | 2709.844    | +1.453    | -0.417    | Pass    |
| ordinary           | n2-rhs4-width8-activity2-complete     | 3288.444    | 3277.779    | -0.324    | -0.238    | Pass    |
| ordinary           | n32-rhs4-width4-activity1-cached      | 22488.747   | 22469.008   | +2.521    | -0.809    | Pass    |
| ordinary           | n32-rhs4-width4-activity1-complete    | 35380.924   | 35622.088   | -0.320    | +2.340    | Pass    |
| diagnosed          | n2-rhs1-width0-activity0-cached       | 188.695     | 189.696     | +0.042    | +0.532    | Pass    |
| diagnosed          | n2-rhs1-width0-activity0-complete     | 819.678     | 820.489     | -0.029    | +0.394    | Pass    |
| coordinate         | band-n64-rhs4-width4-cached           | 59662.115   | 59297.950   | -1.414    | +0.808    | Pass    |
| coordinate         | band-n64-rhs4-width4-complete         | 126432.530  | 125848.240  | +0.387    | -1.374    | Pass    |
| coordinate         | symmetric-n2-rhs1-scalar-cached       | 181.082     | 180.308     | +0.194    | -0.495    | Pass    |
| coordinate         | symmetric-n2-rhs1-scalar-complete     | 622.098     | 621.698     | -0.031    | -0.064    | Pass    |
| checked-coordinate | band-n64-rhs4-width4-cached           | 197124.595  | 194256.200  | -1.455    | -1.387    | Pass    |
| checked-coordinate | band-n64-rhs4-width4-complete         | 479973.430  | 480959.255  | +0.205    | +0.483    | Pass    |
| checked-coordinate | symmetric-n2-rhs1-scalar-cached       | 297.488     | 295.288     | -0.582    | -0.740    | Pass    |
| checked-coordinate | symmetric-n2-rhs1-scalar-complete     | 964.550     | 969.719     | +1.288    | +0.536    | Pass    |

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
| coupled-n2-k2-width4-cached     | 446.183        | 985.709       | 4/1                 | 880            | 80            | 168/352                    |
| coupled-n2-k2-width4-complete   | 693.994        | 38336.800     | 4/1                 | 880            | 80            | 168/352                    |
| quadratic-n1-k1-scalar-cached   | 89.778         | 265.966       | 2/1                 | 732            | 48            | 112/160                    |
| quadratic-n1-k1-scalar-complete | 275.161        | 37396.641     | 2/1                 | 732            | 48            | 112/160                    |

The full tiny native request costs about 37–39 microseconds on this host;
cached reverse is about 266–267 ns scalar and 986–991 ns for width4.
No universal speedup, sparse factorization or nonlinear convergence claim is made.

## Resource and failure proof

All 25 native root cases pass locally; all 115 affected legacy solve cases pass.
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

## Final production hashes

- dal-cpp/dal/math/aad/implicitroot.cpp: c77cfdf5126b3a52e699405f9c87d59d266c21472a634bcede7aa9eec35bd477
- dal-cpp/dal/math/aad/implicitroot.hpp: c445bbc105b7744a60ac7c60824a6c626af72ba736f670efc637eaec92a9efbd
- dal-cpp/dal/math/aad/linearsolveinternal.hpp: b88d26f4c8e4fbb0290ec3636b0b171969c38f1520ccb23bff4c6b2da0f58c45
- dal-cpp/dal/math/aad/linearsolveaccuracy.cpp: e4dad3818bfb1eaab35564c4f99f6abdcf38905d54974ed67d109dafc883b005
- dal-cpp/dal/math/aad/linearsolveaccuracyinternal.hpp: 33dd76da500cfd71f2a72b1299ddcc7506607e6e0f4d8cce84b9f77c4503d984

Baseline archive SHA256: 9a2a9abff81127422dcf31901503be543c8dabf5127fb778f3e4ecc6f417b398
Head archive SHA256: 7d3227a0290f73efca8bfcda1e9bf55a924fd4526363150b0f328ba4a072e4b9

Publication and final exact-head acceptance must be inspected before merge.
