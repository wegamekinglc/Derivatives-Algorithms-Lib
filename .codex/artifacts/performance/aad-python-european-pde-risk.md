# European PDE change-scoped cost acceptance

Status: selected scope measured; comparable caller gate passes. This is local
caller evidence, not execution of the scheduled nine-target benchmark gate.
Baseline is merged #533, `c4c77a3718e2acf5f4f5b647eb680e913375e8aa`.
Correctness must pass before any timing starts.

## Changed callers and selected coverage

- `dal-cpp/dal/math/aad/europeantheta.hpp` extracts and generalizes the accepted
  financial program; `dal-cpp/test-support/europeanaadfd.hpp` keeps its aliases.
  Compare the accepted helper with the extracted helper at 9 nodes/8 ordinary
  intervals and 61 nodes/120 ordinary intervals. Both use the accepted financial
  point/domain, two reverse channels, identical prices/Jacobian and maximum
  forward/transpose error observations. Detailed forward storage remains opt-in,
  so existing example/test callers retain their diagnostic contract.
- `dal-public/src/europeanpderisk.cpp` and the Python binding add owning
  settings/axes/full chronological diagnostics and resource charging. At those
  same two grid cases compare the complete owning native C++ call through a
  minimal ctypes bridge with the owning Python factory. These unequal
  boundary/return contracts are informational and cannot support a 4% regression
  or speedup claim.

No existing nine-target gate covers this complete AAD financial chain:
`pde_perf` uses the ordinary ThetaScheme_ rollback, not this recording/event/
transpose path. Core sampled-step, solve, tape, RNG, calibration, rate and MC/
LSMC implementations are unchanged. Preserve accepted core archive/header/
configuration identities; omit their timing rows rather than claiming new passes.
No adaptive/alternate mesh, callback, higher-order or full parameter matrix is
introduced. Numerical boundaries, resource failures and nondefault settings
are correctness checks, not additional timing cases.

## Evidence requirements

Current-head Codex repair scope: reject error limits above one in the shared
validator and remove repeated linear report lookups from public publication.
Rebuild affected callers and repeat the four selected comparisons. The
reported quadratic-lookup finding identifies one additional size boundary:
compare the original owning C++ request with the repaired identical request at
9 nodes/4096 intervals, using the same calibrated two-round paired sampling
and 4% rule. This fifth comparison is limited to long-schedule publication;
it does not expand into a grid/parameter matrix. Preserve original evidence
as historical evidence, not acceptance of the repaired binary.

Use separate baseline/head source and bridge/build paths, Release/O3,
matching compiler/native core/configuration, one DAL thread and pinned CPU.
Keep accepted prefixes immutable and rebuild each affected caller. Record
source/dependency/object/library/module hashes and actual commands.

Calibrate every selected loop to at least 25ms, retain that repetition count,
then collect two rounds of ten process-level pairs, alternating which side runs
first. Retain every raw sample and reduce each round with min. Apply the sustained
4% gate only to the two comparable extracted-helper rows. Report the two owning
boundary rows separately, even if overhead exceeds 4%.

Verify every price/Jacobian entry and actual solve-error bounds before and
after each measured loop against accepted financial references. Retain machine/
thread/affinity observations and reject inadequate sampling or changed sources.
Repeat only an affected case after a code repair; record the reason before any
expansion. Required exact-head CI remains independent of this local selection.

## Outcome

Both grid sizes and the long-schedule repair complete two rounds of ten alternating pairs:
200 observations, 14.197663285 measured seconds, minimum loop 55.186179ms.
Correctness passes 13 affected C++ tests and 34 new Python cases, with strict
OFF/combined compilation and installed consumer acceptance. Every measured
loop verifies both prices, all six risks and actual solve errors before/after.
Machine: WSL/Linux, GCC 15.2.0, O3/Release, native AAD, one DAL thread, CPU 0
affinity. The machine is shared; a single noisy round is not a sustained verdict.

| Comparison                        | Grid / ordinary intervals | Round 1 minima us, left/right | Round 1 delta | Round 2 minima us, left/right | Round 2 delta | Interpretation                                    |
|-----------------------------------|---------------------------|-------------------------------|---------------|-------------------------------|---------------|---------------------------------------------------|
| Accepted/extracted native helper  | 9 / 8                     | 99.323 / 98.017               | -1.31%        | 99.789 / 100.603              | +0.82%        | Comparable gate passes                            |
| Accepted/extracted native helper  | 61 / 120                  | 7251.267 / 7049.331           | -2.78%        | 7168.199 / 7575.041           | +5.68%        | Comparable gate passes; only one round exceeds 4% |
| Owning C++ ctypes / owning Python | 9 / 8                     | 108.315 / 112.089             | +3.48%        | 105.924 / 111.201             | +4.98%        | Unequal boundary contracts; informational         |
| Owning C++ ctypes / owning Python | 61 / 120                  | 7117.471 / 7546.238           | +6.02%        | 7178.238 / 7278.308           | +1.39%        | Unequal boundary contracts; informational         |
| Original/repaired owning C++      | 9 / 4096                  | 49037.957 / 44721.229         | -8.80%        | 48437.905 / 46215.855         | -4.59%        | Comparable repair gate passes                     |

The scoped verdict is no sustained regression under the declared two-round
4% rule. The second medium-grid round is reported rather than hidden; no
universal speedup, ordinary PDE benchmark pass or full-suite pass is claimed.
Per-request result ownership/resource accounting is included in the owning rows.

Immutable measured implementation:
`35354546a5fb6f7a0301a8b441bb702158132264`.
The long-schedule repair baseline is the original PR head
`a8750ae1e396b3c2bd9e8b79827b3e9f723e557d`; its timed production and bridge
match `ca421c0756c402437b0c326c8d0e411bfef5f296`. Original/repaired owning
contracts are identical. A separate dense complete 4096-interval program
matches both prices within 1e-9 and all six centered derivatives within 2e-6
before freezing the timing reference. The repaired publication uses one
checked reverse-entry pass, avoiding quadratic event lookup. These two measured
improvements are limited to this workload on the shared host.
Baseline/head bridges are separate detached source/build trees; their core
archive hash is unchanged at
`967f5e721f0ba67354b66f8eb14d8be9c4f7e422463bc8c412864491ee06997c`.
The fresh installed Python module hash is
`3b5977055e3235f891096ca9b38286dbf13b281abec8c09766a47927f99ba628`.
Fresh build dependency evidence covers all 52 public/binding translation units.
All 402 existing native headers match; one new production helper is installed.

All 200 repaired-source observations retain complete process stdout/stderr and
structured results. The original 160 observations and their collector-repair
limitations remain historical evidence in
[the original committed report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a8750ae1e396b3c2bd9e8b79827b3e9f723e557d/.codex/artifacts/performance/aad-python-european-pde-risk.md).
They are not acceptance of the repaired module. No unrelated timing matrix
or full local numerical suite was run during the review repair.

Portable structured observations and per-round summaries are in
[the raw result](aad-python-european-pde-risk-results.json). Full commands,
dependency/binary hashes, compiler/CPU observations and process captures remain
under the active evidence root
`/home/wegamekinglc/.cache/dal-aad-evidence-20261010/python-european-pde-risk`.
