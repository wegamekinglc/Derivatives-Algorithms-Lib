# LSMC curvature review

Verdict: Comment Only; publication gates pending.

## Findings

No unresolved correctness finding in the current local implementation.

The first numerical RED distinguished repeated legacy policy training from
curvature conditional on a single baseline policy. Additional RED/GREEN covers
retrained semantics, capacity admission, bumped historical constants and an
unrepresentable lower inner step. Independent passive prices check the complete
frozen gradient/product and the nested retrained partial-plus-policy secant.

## Open questions and residual limits

No author question requires user input. The small estimator study exposes
substantial outer-step, sample-size and inner-policy-step sensitivity. It
certifies the declared finite estimator, not stable production Gamma,
unbiasedness, symmetry or a convergence order. The public method labels and
current-state methodology preserve that boundary.

The API is a native Black–Scholes C++ entry. Language bindings and mixed mode
remain separate work. Tape caps are per replay batch; retained preparation,
policy and working storage are outside the numeric-output budget.

## Validation

Twenty-one local selected tests cover tree/compiled constants and history, common RQMC
blocks and adaptive fitting, signed/empty directions, model-domain and inner
step preflight before submission, caller mode/nested recording, deterministic
multi-batch reduction and drained submission failure. Existing retrained model
and constant callers remain selected regression controls. OFF/combined strict
probes and installed package consumption pass. The worker-mode restoration test
also passes after capacity failure and recovery. The existing model-allocation
guard rejects foreign future observations before submission. Initial paired
costs pass with 160 observations; raw evidence is committed. Codacy's complexity
repair splits validation, policy training and gradient accumulation into private
helpers, with local Lizard complexity at most 7 (limit 8). Its selected numerical
tests pass. Refreshed costs and complete current-head CI/Codacy/review/runtime
logs remain acceptance gates before merge.
