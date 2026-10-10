# European PDE change-scoped cost acceptance

Status: scope selected before measurement; no timing acceptance claimed.
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

## Pending outcome

Raw data, per-round minima/deltas, total measured loop time, numerical and
provenance acceptance, and a scoped verdict will be added after correctness.
