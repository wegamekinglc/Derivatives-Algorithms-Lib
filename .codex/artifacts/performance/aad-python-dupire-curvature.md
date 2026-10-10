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
