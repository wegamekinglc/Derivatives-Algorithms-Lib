# Rate structural provider scoped cost acceptance

Status: local final-source evidence accepted; exact-head publication gates pending.

Baseline is accepted #508, merge `5f41297f550aa327a2eb04a43c9d8342f5e06461`.
The active branch is `feature/rate-structural-jacobian-provider`; its final commit
is pinned in session evidence after publication. The immutable final-source
manifest was captured before sampling and verified afterward.

## Selection and identity

The only changed existing archive member is `ratecashflowpricing.cpp.o`, containing
the additive cold capture bridge. All 177 other members match the accepted
baseline; the sole new member is `ratestructuraljacobian.cpp.o`. Existing pricing
function bodies are unchanged. Select the complete passive three-trade request
and existing joint native curve matrix request that exercise the affected source.
Keep all unrelated tape, MC, solve, PDE, interpolation and portfolio matrices
outside this change; their prior evidence remains retained rather than reported
as newly measured passes.

New informational rows cover capture plus cold numeric planning, and capture plus
complete equality against an owning reused plan at a fresh numerical point.
They include validation and release; immutable trade/market fixtures are outside
timing. They do not record or execute financial Jacobians, include full dense
fallback work or justify any default strategy selection.

## Method and results

Release GCC 15.2.0, C++17, `-O3 -DNDEBUG -ffp-contract=fast`, native AAD,
diagnostics OFF, CPU 0, `DAL_NUM_THREADS=4`, zero workers. Independent processes
alternate baseline/head first, ten samples per side in each of two rounds.
Each row performs eight warmups and 128 complete requests; every PV and supported
matrix entry is checked against the frozen independent analytic fixture.
Reduce each round with `min`; use the calibrated sustained +4% two-round policy.
This custom fixture is supplementary affected-path evidence, not the scheduled
nine-executable regression gate.

| Row                                   | Baseline minima, ms/request | Head minima, ms/request | Round deltas    | Result                      |
|---------------------------------------|-----------------------------|-------------------------|-----------------|-----------------------------|
| Existing passive portfolio            | 0.00164308 / 0.00163773     | 0.00164098 / 0.00164054 | −0.13% / +0.17% | No sustained material delta |
| Existing joint native matrix          | 0.00697195 / 0.00702692     | 0.00702101 / 0.00712170 | +0.70% / +1.35% | No sustained material delta |
| New provider cold plan                | —                           | 0.01208934 / 0.01201553 | —               | Informational               |
| New provider reused plan, fresh point | —                           | 0.01179674 / 0.01158775 | —               | Informational               |

Final sampling completes 120 row observations in 0.2087 seconds: forty affected
existing comparisons and forty new-provider observations. No wider set runs.
The preliminary pre-review-fix sample remains preserved; the final sample repeats
only these four selected rows because admission fixes changed the captured source.

## Evidence

Session evidence root is
`/home/wegamekinglc/.cache/dal-aad-evidence-20261008/rate-structural-performance`.
It retains `identity-proof.json`, `selection.json`, `results.json`, each process's
raw stdout/stderr, and preliminary evidence under `before-review-fixes`.
Baseline archive SHA-256 is
`54fcab902c22ca97222474abc3701a5e0fef361c9ca4df846ac130967adf8a96`;
final archive is
`2f6a787fb180b3a2de554184a9ce7dc337f4310945badd471fe50dddf70d9d9c`.
Sources, binaries, archives and independent reference hashes are verified across
sampling. No compilation or source editing overlaps pure timing.
