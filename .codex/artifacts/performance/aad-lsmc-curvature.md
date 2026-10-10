# LSMC curvature scoped cost acceptance

Status: local correctness, strict compilation, installed consumption and
refreshed paired costs pass; publication acceptance pending.

Baseline: merged #525, `f30c7bf4b74d183b071bedc0f51aea3e9864dea8`.

## Changed callers and selected cases

- `dal-cpp/dal/script/lsmc.cpp`: one existing complete Frozen native LSMC request
  and one existing complete RetrainedBump request cover the modified replay and
  optional-constant plumbing. These are baseline/head regression controls.
- `dal-cpp/dal/script/lsmccurvature.hpp` and its LSMC composition: one complete
  new Frozen curvature request and one new RetrainedBump request cover policy
  lifetime and nested training. Report complete latency and checksums; neither
  is a speedup claim over a previously existing adapter.

Use one small two-exercise-date case with a script constant and one explicit
spot direction. Four cases suffice: functional tests cover tree/compiled,
training/validation/RQMC, path/domain and resource boundaries. No Cartesian
matrix of path counts, seeds, model families or diagnostics is needed.

## Sampling and exclusions

Two rounds of ten interleaved process pairs per case, alternating first side,
warmup, fixed thread/configuration, checksum agreement and sustained +4% old
caller gate remain unchanged. Record immutable source, dependency, archive and
executable identities and every raw sample. Retain unchanged accepted evidence;
refresh only executable-identity-applicable pairs after repair.

Rate curves, Dupire, PDE, solve, RNG primitive, tape primitive and segmented
European MC implementations are unchanged. Their matrices are excluded. Required
exact-head CI remains applicable.

## Initial accepted paired measurement

Head `831e196b9b9d9a560f4a7fcf40e3de6190f32a84` completes all four selected cases:
160 observations and 5.063510592 seconds of measured work. The committed
[raw evidence](aad-lsmc-curvature-results.json) contains every sample/checksum,
source and dependency hashes, compiler/CMake/CPU/configuration identity, archive
hashes, executable hashes and exact commands. Every process warms up once;
calibration chooses 128/16/64/8 repetitions respectively for the four cases,
then both sides use identical repetition counts in two interleaved ten-pair rounds.

| Case               | Round 1 delta | Round 2 delta | Verdict                   |
|--------------------|---------------|---------------|---------------------------|
| Existing Frozen    | -16.73%       | -19.10%       | No regression             |
| Existing Retrained | -6.34%        | -14.93%       | No regression             |
| New Frozen         | Informational | Informational | Complete-request coverage |
| New Retrained      | Informational | Informational | Complete-request coverage |

The shared host is noisy; negative deltas are not promoted to speedup claims.
New-case sides execute the same head binary to expose repeatability and complete
latency without inventing an older comparable adapter. Existing-caller checksums
match across baseline and head. Baseline core/public archives are the immutable
#525 accepted artifacts. Only the affected LSMC object is rebuilt in the head
core archive; public code and all other core members retain accepted provenance.

Codacy's complexity repair changes the core archive and affected executable
identities. The same four cases are therefore refreshed with the retained
calibrated repetition counts; no case or parameter matrix is added.

## Accepted complexity-repair refresh

Implementation head `fa16f7bc` passes 22 selected tests, six strict probes and
fresh installed consumption. Entry-point complexity is 7, and every new helper
is at most 7, below the Codacy limit 8. The refreshed paired run contains 160
observations and 2.423908800 seconds of measured work. Its complete raw evidence
and fresh build/executable identities appear under `complexity-repair` in
[the committed evidence](aad-lsmc-curvature-results.json); initial evidence is
retained separately. The baseline remains merged #525.

| Case               | Round 1 delta | Round 2 delta | Verdict                   |
|--------------------|---------------|---------------|---------------------------|
| Existing Frozen    | -14.47%       | -15.37%       | No regression             |
| Existing Retrained | -3.41%        | +2.45%        | No regression             |
| New Frozen         | Informational | Informational | Complete-request coverage |
| New Retrained      | Informational | Informational | Complete-request coverage |

Both existing cases satisfy the sustained +4% gate, and every paired checksum
agrees. Shared-host load changed between the initial run and refresh; neither
cross-run latency differences nor negative round deltas establish a speedup.
Later documentation-only commits reuse this evidence only while source,
dependency, configuration, archive and executable identities remain applicable.

## Selected finite-estimator study

The two-date signed put uses smoothing width 2, 256 training paths and a spot
unit direction. Frozen products agree with independent passive Gaussian-path
price differences at the same declared steps. Retrained products also agree
with independently priced inner policy secants plus passive fixed-policy
partials. Canonical fitted regressions are reused by these price references;
the references do not read native gradients or differentiate the native kernel.

| Policy        | Pricing paths | Outer step | Inner relative step | Replicates | Spot product |
|---------------|---------------|------------|---------------------|------------|--------------|
| Frozen        | 128           | 0.20       | 0.001               | 1          | 0.01595036   |
| Frozen        | 128           | 0.10       | 0.001               | 1          | -0.01094456  |
| Frozen        | 128           | 0.05       | 0.001               | 1          | 0.00427000   |
| Frozen        | 512           | 0.10       | 0.001               | 1          | 0.02682045   |
| RetrainedBump | 128           | 0.10       | 0.001               | 1          | -0.00355374  |
| RetrainedBump | 128           | 0.10       | 0.0005              | 1          | -0.10302571  |
| Frozen        | 512           | 0.10       | 0.001               | 2          | 0.00249757   |
| Frozen        | 512           | 0.10       | 0.001               | 4          | 0.01665673   |

For randomized rows, training seed is 17 and pricing seed is 29. The first
three rows vary only the outer interval on fixed paths; the fourth varies
sample count. The two retrained rows expose inner-policy-secant sensitivity.
The final rows vary randomized replicate count with a common training policy.
They are sensitivity observations, not an estimated confidence interval or
proof of convergence. At these deliberately small sample sizes products even
change sign. This rejects any blanket quadratic convergence or stable-Gamma
claim for this piecewise-smoothed finite-path exercise estimator. Production
users must select steps and sample sizes against their own error requirement.
Expanding this diagnostic into a full parameter matrix would not strengthen
the implementation's finite-estimator identity check.
