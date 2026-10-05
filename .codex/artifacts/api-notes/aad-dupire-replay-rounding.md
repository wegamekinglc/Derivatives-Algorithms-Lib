# Dupire replay with contracted floating arithmetic

Status: controlling correction for F01's ARM wheel failure. No new public API.

## Evidence and audience

Python wheel users on AppleClang ARM64 receive `InvalidDupirePullback` for both
flat and Merton snapshots. The retained `12dcd09a` wheel log has seven failures;
ordinary functionality otherwise passes. A local Clang `-O3 -mfma
-ffp-contract=on` consumer reproduces the failure against the unchanged core:
call differences of several ULP become local-vol differences up to 2.37e-9.
Clang/GCC `fast` controls pass. The stencil's subtractive cancellation explains
why a strict final surface check alone cannot accommodate different contraction.

## Decision

Preserve numeric calibration, frozen samples and all public signatures. First
run the original active replay and compare its surface at the unchanged
relative/absolute 1e-12. When it agrees, retain that original graph. Otherwise,
record the checked replay below on the same registered quotes; the unseeded
original intermediates remain at exact zero. Always finish and perform the
strict final check before seeding either graph. Share
the existing five-call local-vol formula in a callable-based internal template;
the legacy IVS method supplies its unchanged `Call` implementation. The new
checked replay supplies a scalar call primitive: compute the numeric call with
the same frozen IVS and passive quote values, compute the existing active call
expression, require both finite and their difference no greater than
`8 * epsilon * discount * (forward + strike)`, and retain the scalar price as
the primitive's primal with the active expression's unchanged local derivative.
An identical call needs no extra node. This bound comes from rounding at the
two discounted Black legs; it is not a quote-gradient or surface tolerance.

The initial unconditional checked replay passes correctness but the retained
480-process paired measurement shows four VJP cases slower by 72–98% in both
rounds on an unaffected x86 configuration. Reject that implementation. Keep the
original agreeing path to preserve its cost and measure the final version with
the same fixed paired protocol, including legacy calibration and snapshots.

Replay uses freshly computed scalar calls, not retained output values. The
original relative/absolute 1e-12 surface check remains before reverse. Shared
formula, frozen band and alias copies preserve the actual discrete function.
Zero/negative/direct seeds, lifecycle and readonly snapshots are unchanged.

## Alternatives and compatibility

Reject a looser surface check: stencil cancellation can magnify call rounding,
and a relaxed output check would hide unrelated replay errors. Reject global
floating compiler changes: they affect existing pricing and performance.
Reject replacing the snapshot with active output: it changes the established
numeric calibration and makes the check self-referential. An analytic Black or
Dupire atomic adjoint would require a separate derivation and acceptance scope;
the existing active expression already supplies the required derivative.

## Errors and acceptance

An excessive/nonfinite call disagreement fails explicitly as a replay call
mismatch. Final grid/vol mismatches still fail at the existing check. No callback
survives and no primitive escapes the scoped recording. The legacy exact-price
and independent quote oracles retain their assertions, steps and tolerances.
Require the retained local FMA RED to pass, native OFF/combined and sanitizers,
installed bindings/consumers and exact corrective CI before acceptance. Inspect
old performance binaries after rebuilding; changed bytes require corresponding
performance investigation. No open public API questions.
