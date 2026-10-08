# Recorded sampled PDE theta step

Status: active implementation contract after numeric #504 was accepted and merged.
Native first-order AAD remains the only backend. The owning numeric operator is
current published behavior; this contract controls the new recording wrapper.

## Required public contract

1. `AAD::SampledThetaStepBindings_` groups complete-field active overrides:
   Number vectors `rates_`, `drifts_`, `variances_`; Number matrices `oldValues_`
   and `externalValues_`; optional Number scalars `dt_` and `theta_`. Empty
   containers and absent scalars select the corresponding passive numeric field.
   Nonempty fields are complete active replacements; activity and endpoint
   provenance are explicit, never inferred by equality, pointer or aliases.
   Active oldValues_ determines the resulting layer count; overwritten passive
   field dimensions/values are unused. Resulting coefficients must contain n-2
   entries and any declared external matrix must be two-by-resulting-layers.
2. `SampledThetaStepWithAccuracy(scope, numericInputs, activeBindings, policy,
   tolerance=64*epsilon)` accepts a live owning RECORDING scope and at least one
   declared active field. Passive mesh/provenance and resulting numerical request
   inherit the accepted sampled-step shape, physical coefficient, time/theta,
   pivot, finite-range and accuracy requirements. Each active Number must be a
   valid slot in that recording; source destruction after capture is supported.
3. Active replacements supply their own values. The unused passive field value
   does not need to equal the active replacement. Scalar optional presence and
   container shape are unambiguous. All declared external slots are validated
   and retained even when a side is unused; unused external risks remain zero.
4. `CheckedSampledThetaStepResult_` owns n-by-layers `solution_` Numbers, owning
   `diagnostics_` (forwardBackwardErrors_ and policy_), and a shared opaque
   SolveAccuracyEvent_ token. There is no reciprocal-condition estimator.
   Output slots are separate zero-edge nodes, including copied endpoints.
5. The first native wrapper inherits numeric complete-coordinate reverse
   admission, including range rejection in computed inactive coordinate risks.
   It then scatters only declared active slots. Document/test this restriction;
   selective-range admission is not silently implied by a partial activity set.
   A fully passive step uses the accepted numeric operator.

## Reverse and composition

6. For each scalar/vector channel, collect one n-by-layers output seed matrix,
   invoke the owning numeric reverse, and scatter each selected coefficient,
   old/external state and scalar dt/theta contribution. Each channel's coefficient
   risks aggregate layers. Repeated aliases sum all formal contributions once.
   Consume output seeds after successful propagation. Exact-zero channels may
   omit numerical work only while retaining their exact-zero physical report.
7. Actual transpose report shape is layers-by-channel-width. Report collection,
   full/suffix/prefix provenance, invocation identity and restoration semantics
   reuse existing SolveAccuracyInvocation_ with dense/coordinate/root events.
   Ordinary scope.Reverse also enforces accuracy without a returned collection.
8. Ordinary native expressions, multiple theta steps and direct objective risk
   compose in event order. Serial state outputs may be active overrides of the
   next step. Shared coefficients accumulate across steps/layers/channels.
   Theta endpoints inherit complete one-sided admissible discrete derivatives;
   variances map to volatility through upstream2*sigma, with passive mesh/provider.
9. Returned diagnostics/reports survive copies, source mutation, checkpoints
   and recording close. Output Numbers retain the existing tape lifetime/mode
   contract. Each independent worker owns a scope; no shared mutable tape/cache.

## Failure and resources

10. Use the existing generic checked event wrapper. Stage bindings and numeric
    capture in caller context, then deep-copy once into the tape event without
    a second factorization. Preserve ordinary structured event layouts/hot paths.
11. Validate recording/thread/mode and input slots before output publication.
    Invalid field dimensions/policies or nonfinite values do not publish partial
    outputs. Construction, numerical, range, accumulation or allocation failure
    invalidates the recording and refunds transient/retained charges as supported
    by the existing scoped native operation boundary.
12. Reverse failures return no partial collection and invalidate adjoint reads.
    Checkpoint suffix removal/close releases discarded event caches/bindings.
    A subsequent independent scope must recover. Historical successful passive
    diagnostics/reports remain readable.
13. Retained cache/bindings and reverse scratch obey tape budgets. Returned
    containers/reports and staged numeric construction obey caller budgets.
    Caller peak includes actual overlapping snapshots, diagnostics and outputs;
    reverse scratch can count against both limits. Do not add overlapping
    capacities as independent RSS. Verify exact and one-byte-short peaks,
    refunds, repeated seeds, mixed contexts and suffix removal.

## Executable delivery acceptance

14. After numeric #504 merge, publish controlling SPEC/API/critique and establish
    missing-interface RED with the prepared independent scalar rate fixture.
    First GREEN has total risk163/735 for2*solution(1,0)+rate and actual1x1
    transpose report, with independent primal73/35.
15. Cover each activity field, all-active and partial active overrides, legal
    scalar/vector widths1/4/8, n3 and nonuniform n5/two layers, old/mixed/external
    endpoints, shared aliases and serial rollback. Reuse accepted numeric high-
    precision references and add independent full-step/native-expression oracles
    at the actual composition boundary. Zero outputs/unused lanes remain strict.
    The prepared exact-symbolic n=3/two-layer fixture has left-external and
    right-old provenance, objective257/280, interior values71/35 and41/70, and
    fifteen rational input risks including direct rate/dt/theta terms. Its two
    unused external slots have exactly zero risk. Compare selected activity
    and full activity in scalar/width1/4/8 modes against those frozen values.
16. Test source destruction, repeated seeds and concurrent independent recordings;
    mixed event reports, checkpoints/raw windows; wrong-thread/mode/lifetime,
    invalid values/range/accuracy, failed capture/reverse and successful recovery.
    Actual exact caller/tape capacities protect ownership/staging boundaries.
17. Select performance from actual changed objects/callers. Use the three numeric
    PDE shapes for optional native capture/sweep cost. Fresh same-byte ordinary
    solve/root/PDE callers retain accepted evidence. No full benchmark matrix.
    Preserve two rounds/best-of-ten/+4% for any actually changed legacy path.
18. Include the named AADSampledThetaStepTest suite in the six sanitizer filters;
    inspect actual final-head tests in14 profiles, all required CI/Codacy/review,
    installed consumption and guarded merge/tree verification. Native capability
    is current-state documented only after it exists; bindings remain separate.
