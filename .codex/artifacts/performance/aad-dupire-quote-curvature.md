# Dupire quote-curvature cost acceptance

Status: scoped costs accepted; exact-head publication pending.

Base: merged #519 at `d2bd2ad22142929c30eb21ea25870ebbd2123b57`.
New implementation uses the accepted 182-member native archive, replacing only
the Dupire risk object and adding the quote-curvature object. Dependency and
member identity evidence must establish retained generic/MC applicability.

## Selected paths

- New flat-base parallel request: six quotes, eighteen local-vol nodes, one
  direction and three full calibration/gradient chains.
- New nonflat Merton-base request: the same grid, a parallel and a signed mixed
  direction, five complete chains.
- Existing first-order Dupire pullback: one small nonflat surface and complete
  node seeds. The changed Dupire risk translation unit contains this old caller,
  so its affected cost receives one paired control.

The two new cases compare the public driver with explicit manual composition of
the same passive recalibration, fresh objective AAD and fresh calibration AAD.
Both use the candidate primitive and precision, complete preflight, scalar mode
restoration, capacity scopes, owning outputs and result verification. This is
an informational new-method overhead comparison, not a historical speedup or
regression gate. The first-order control uses the immutable base archive.

Reuse the accepted generic and MC costs only with unchanged dependency/member
and executable proof. Exclude unrelated tape, solver, rate, PDE and portfolio
matrices: their algorithms and recording headers are unchanged. Required CI
continues to use its existing platform profiles.

## Sampling

For each selected pair: two rounds of ten process samples per side, alternating
base/head order within each round; one warmup and three timed repetitions per
process, best-of-ten reduction per round. The first-order control retains the
two-round +4% threshold. Save commands, configuration, CPU/compiler/environment,
source/dependency/object/executable hashes, all raw samples and verification.
Do not edit C++ or Git/PR state during sampling. Expand only for an identified
failure, unstable minima or new affected caller.

## Results

Measured implementation: `d54be6daecc20ef236673d3ccf489f805cabbcb5`.
Separate detached head/base sources and executables use native Release C++17,
GCC 15.2.0, `-O3 -DNDEBUG -fPIC -ffp-contract=fast`, Eigen enabled and native
architecture tuning disabled. The host is an Intel Core i9-13900HX, shared and
not claimed exclusive; measured load was approximately 0.52/1.75/1.61.

- Flat parallel: head/manual minima 122,544/122,536 ns and 122,353/122,365 ns;
  overhead +0.0065% / −0.0098%, informational.
- Nonflat mixed: head/manual minima 202,535/203,192 ns and 203,849/202,186 ns;
  overhead −0.3233% / +0.8225%, informational.
- Existing pullback: head/base minima 18,028/18,178 ns and 17,943/17,937 ns;
  changes −0.8252% / +0.0335%. Both selected +4% control rounds pass.

All 120 process samples verify their numeric results; sampling takes 0.2875
seconds, excluding configuration/compilation. Each new complete request uses
the candidate recalibration primitive on both sides; this comparison measures
composition overhead and does not claim a historical speedup. The old caller
uses the immutable merged baseline archive on its baseline side.

The combined archive retains 181 accepted members byte for byte and adds one
curvature object while replacing one Dupire object. Fresh base/head MC test
links are identical. Generic and MC implementations/recording dependencies are
unchanged, so their accepted evidence remains applicable; they are not newly
timed passes. The selected old Dupire path is accepted and the two new costs are
informational. No broader regression sweep is claimed or needed for this scope.

Evidence directory: `dal-aad-evidence-20261010/dupire-quote-curvature`.
Keep `cost-samples.json`, `cost-results.json`, `cost-environment.json`,
`isolated-cost-builds.json`, all 120 `cost-raw` logs and `library.json`.

The scheduled-registration review repair changes benchmark entry dispatch only.
The complete timed `Run` source region and production archive are identical to
the measured version. Default and both selected entry smokes, generated CTest
name/executable contract and five affected strict probes pass. Paired samples
remain applicable; `benchmark-repair.json` records the retained identities.
