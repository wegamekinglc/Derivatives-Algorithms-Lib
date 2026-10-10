# Python Dupire curvature performance scope

Status: scoped informational costs accepted; publication gates remain.

Changed paths: `dal-python/src/bindings/dupirecurvature.cpp` adds passive request
conversion, owned getters and two financial factories. `bindings.h` adds one
declaration; `module.cpp` registers the new classes; `dal-python/CMakeLists.txt`
adds the new unit. Existing binding function bodies, `dal-public` and `dal-cpp`
sources are unchanged from accepted #528 (`2f1f6f9008b6a0d9e800e718b5eaca5a2d9d4019`).
The declaration has no runtime callers beyond module initialization. Rebuild the
extension and prove native installed-library/header provenance.

Selected cases: one complete base-only request (Q=6, M=0) and one signed
multi-direction request (Q=6, M=3), seventeen common paths in compiled mode.
Include Python request-boundary model/product creation, native plan/admission,
valuation and all returned gradient/product extraction. Reusable request/source
construction and interpreter import are outside each measured loop. Validate
the direct quadratic's analytic gradient/products before and after every sample.

Compare the new financial factory with independently composed existing Python
first-order plans/calibrations on equivalent quote points. Native curvature
planning additionally validates/rebuilds all perturbed points before valuation;
manual composition lacks that admission contract. These are informational complete
entry costs, not comparable old-versus-new implementations or a regression gate.
No general speedup, exact-Hessian or reduced-calibration-work claim is allowed.

Use two rounds of ten alternating interleaved process pairs for each selected
case, calibrated to at least 25 ms per timed loop, warm-up and fixed affinity,
one native worker and identical compiler/library/configuration provenance.
Retain all eighty observations, calibration attempts, source/module/library hashes,
environment and both round minima. The sustained +4% policy remains unchanged for
comparable affected old callers; this additive binding has none.

Exclude unchanged native tape, calibration, PDE, rate, segmentation, LSMC and
portfolio benchmark matrices by caller analysis. The one-time PIC library build
is needed to link a Python shared extension, not grounds to repeat those matrices.
Remote required exact-head CI still applies. Do not label excluded cases as newly
measured passes. Documentation-only repairs need no repeated timing; actual
binding changes repeat only these two cases with fresh executable identity.

## Results and provenance

Implementation source: `b2832b6b3cbcdbad4573b0f3187585c2f4efcded`; native baseline
source: `2f1f6f9008b6a0d9e800e718b5eaca5a2d9d4019`. Both modes use separate
package copies of the same freshly built/installed extension, hash
`932a9aa5b0519f35e7fcabf9aee77641e3294818313d2bbee41b0e3ecdcd5a27`.
This is an entry-cost comparison inside one implementation, not a branch-baseline
regression verdict. The existing first-order binding bodies are unchanged.
Fresh PIC support is necessary for the shared extension; all installed public
header bytes match the source, apart from the explicitly non-installed private
`eigenbridge.hpp`. Full library/source/configuration hashes are retained in
[the raw evidence](aad-python-dupire-curvature-results.json).

All eighty observations pass analytic gradient/product checks. Total measured
loop time is 3.465331108 seconds. Base-only requests calibrate to 128 repetitions
per process; three-direction requests use 16. Each round retains ten samples per
mode, alternating first position, one worker and fixed caller affinity.

Base-only native minima are 0.4731 and 0.4659 ms/request, versus composition
0.2208 and 0.2219 ms (informational overhead 114.3% and 109.9%). Three-direction
native minima are 2.6264 and 2.7274 ms/request, versus 2.0782 and 2.1041 ms
(26.4% and 29.6%). Native planning includes perturbation admission and rebuilds
before execution. Composition instead constructs independent first-order plans
and resamples the same fixed BS IVS; it has no whole-request admission contract.
The new surface provides owned financial semantics, not a demonstrated speedup.

The first calibration output includes DAL's startup banner; the initial JSON-only
parser rejected it. The original output is retained, and the orchestration parser
now reads the single JSON result line while retaining full stdout/stderr. No
production, cost-driver or sampling contract changed; the complete paired run
then succeeds. No old native benchmark cases were repeated or claimed as new passes.

## Codacy repair scope

Codacy flags three `assert` statements in the Python cost driver's validation.
Replace them with explicit failure checks so optimization cannot remove numeric
oracles. The request operations, timing interval and tolerances remain unchanged.
Repeat only these same two affected entry-cost cases with fresh driver provenance;
retain all original observations. Binding code, native libraries and the selected
test executable are unchanged, so no correctness suite or old timing repeats.

The repaired driver at `a2dd66d0870098de2210e718279ef41701b178ae` completes eighty
fresh observations in 3.568148566 measured seconds, with the same repeats,
sampling, analytic bounds and extension identity. Both original and repaired
sets remain in the raw evidence; `codacy_repair` is the applicable final set.
Base-only native minima are 0.4644/0.4571 ms, versus 0.2174/0.2250 ms composition.
Three-direction native minima are 2.6296/2.7457 ms, versus 2.0853/2.0928 ms.
The informational admission-overhead conclusion is unchanged. Optimized Python
execution also rejects an incorrect analytic gradient with an explicit error.

## Cancellation repair scope

Synchronized-head GCC 13 CI and a focused local repetition expose a prior RNG
lifetime race in `dal-cpp/dal/script/simulation.hpp`: cancelled closures retain
their RNG past future readiness. The repair releases that capture in the cancelled
branch. It changes no valuation, random stream or normal work operation, but
dependent objects must be rebuilt and compiler/layout effects need scoped checks.

Dependency files identify eight core, six public and twelve binding translation
units that include the shared header. Rebuild those dependencies; compare object
identities to exclude units with no generated-code change. The helper has only two
production call sites: passive and AAD Monte Carlo batching. Select successful
IRN/tree/compiled financial cases covering both call sites, plus a non-positioned
Sobol control. Add weighted/blocked/portfolio entry cases only where the rebuilt
object/caller analysis establishes changed generated code. Cover multi-batch and
tail work with four workers; the focused correctness tests retain one/four-worker,
empty-run, cancellation-source, bridge and precision boundaries.

The two new Dupire curvature entry-cost cases also receive fresh installed-module
evidence. Preserve both prior result sets. Use the same two rounds of ten
alternating interleaved process pairs, calibrated loops and unchanged sustained
+4% gate for comparable old callers. Unchanged tape, curve calibration, solve,
PDE, rate and full parameter matrices remain excluded. Final selection and object
identity findings are recorded before sampling; no omitted case is a fresh pass.

The completed incremental rebuild changes four core objects (LSMC, diagnostics,
portfolio admission and portfolio batches), five public objects (scalar/weighted
risk, value, Dupire risk/curvature and portfolio replay), and eleven bindings.
The core scalar simulation, blocked Jacobian and segmented replay objects remain
byte-identical. Inclusion and line-number/layout changes alone do not establish
execution of the modified helper: portfolio replay uses its own `RunBatches`,
and blocked/segmented Jacobian replay uses a separate dispatcher. These are
excluded from repair timing, together with unchanged LSMC policy operations.

The final six comparable successful-call cases are IRN passive/tree,
passive/compiled, AAD/tree, AAD/compiled, weighted-AAD/compiled, and a passive
compiled Sobol control. Scalar cases enter through `MonteCarlo_ValueWithSettings`;
weighted replay covers the separate objective instantiation in `riskvalue.cpp`.
Each runs 32,785 paths (four full 8,192-path batches and a tail), four workers,
and checks an analytic value/gradient before and after timing. The baseline is
the preserved pre-repair installed module at `70d5285d4b`; native sources match
the merge base `27afc72e5b` there. Both sides use the same driver and configuration.
This is scoped caller evidence, separate from the scheduled nine-target gate.
The existing M=0/M=3 Dupire driver additionally runs against the rebuilt module,
with its original one-worker inputs and informational composition comparison.

The first repair run completes 320 observations in 12.139067637 measured seconds.
No comparable caller exceeds +4% in both rounds. The passive/tree case has
discordant -8.55% and +5.73% minima, so repeat only that case with two further
ten-pair rounds to distinguish drift from a repeatable change. Retain the initial
observations and all other accepted cases; do not repeat the eight-case set.

The focused confirmation adds forty observations in 1.936624215 measured seconds,
with +6.39/+2.96% round minima: no sustained +4% slowdown under the same rule.
All six comparable cases pass that scoped rule, with visible passive/tree noise
and no acceleration claim. The two refreshed informational Dupire cases retain
the admission-overhead finding (M=0: +111/+117%; M=3: +28/+28%). Total current
repair evidence is 360 observations and 14.075691852 measured seconds, stored in
`cancellation_repair`; the original and Codacy/test repair records remain intact.
The fresh installed module SHA-256 is
`430083a2d77c4fdebda54b66ddf70eb9a38c5593270477571305a300e297b5b8`.
Its 402 installed headers match source; the only changed header is simulation.
Native acceptance passes 1,000 cancellation repetitions and all 15 batch cases;
ten affected installed Python cases and six strict OFF/combined probes pass.
