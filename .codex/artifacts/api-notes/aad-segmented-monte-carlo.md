# Segmented Monte Carlo API

Include `dal/script/segmentedmontecarlo.hpp`. The explicit entry point is
`EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, pathCount, settings)`.
The first three arguments are required; settings default to sobol, no bridge,
offset zero and the existing segment defaults. Settings own the optional digital
shift key and existing per-path segment/budget structure.

SegmentedMonteCarloResult_ exposes MeanValue(), MeanGradient(), ParameterLabels()
and Execution(); its data is owned. Execution names maximum per-path resources,
count/offset, fixed batching and generator settings. Errors identify the offending
input/constraint. A positive count is required because an empty mean is undefined.
Without drivers, offsets obey size_t range admission without RNG-specific limits.

The immutable financial kernel exposes const request admission, sharing its
parameter checks and the core's checked checkpoint plan. This does not execute a
path or alter the existing Evaluate request. Recording capacity is admitted on
the executing thread, whose retained native tape can differ from the caller's.

The old SimResults_ raw price sum/normalized-risk convention remains compatible;
the new names explicitly identify means. Per-task ownership and bounded waves
avoid thread-slot aliasing and unbounded coordinator storage.
Inputs and settings are copied before task submission so accepted work owns its
numerical point and generator policy. Public bindings are
a later plan item. Numerical and capability limits are in the
[specification](../specs/aad-segmented-monte-carlo.md).
