# Python Dupire curvature performance scope

Status: premeasurement scope; correctness/build gates precede sampling.

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
