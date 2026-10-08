# Fixed-grid financial PDE acceptance review

Verdict: Comment Only — local verification passes; publication gates remain open.

## Findings

No unresolved local correctness or style findings. The installed consumer
compiles the real example with only the installed DAL public include directory;
its repository-local support headers do not replace installed library headers.
The accepted production kernels, headers, caller paths and library archive are
unchanged. The new CMake target is registered for the existing example smoke
test flow and installation.

Reviewed all four new C++ source/header files, example CMake registration,
published guides and active controls. Checked physical variance and its sigma
chain, discounted boundary and terminal-strike dependencies, exact one-year
damping schedule, layer/channel axes, current slots for zeros, owning reports
and scope-before-mode cleanup. Independent dense execution constructs full
matrices and terminal/boundary state at every bump; it shares no DAL numerical
kernel. The refinement fixture keeps strike at the cell midpoint before any
native execution; all original proposal outputs and failure evidence remain in
session evidence. No numerical assertion or predeclared ceiling was weakened.

## Open questions

None for this fixed-fixture example/test boundary. Parameter-dependent meshes,
exercise decisions and higher-order derivatives remain outside its contract.

## Tests and evidence

- Missing-support-header RED is retained. Initial test seed constness is
  repaired; green small-mesh prices/Greeks pass without changing assertions.
- Six new named cases pass in focused batches; the final formatted batch passes
  6/6 in 89 ms. Three refinements execute once each inside one case, receiving
  frozen-reference, continuum convergence and parity assertions.
- Complete independent differences use nine declared coordinate/bump points,
  both price layers and a weighted objective with direct parameter terms.
  Frozen masks separately prove nonzero terminal/boundary contributions.
- Scalar/vector-4 repeated sweeps and zero lane, four-half-step minimum and
  rejected invalid/overflowing configurations pass. Every step returns actual
  two-layer accuracy reports with the requested channel count.
- Eight strict OFF/combined-diagnostic source/header checks pass. All 26 new
  functions satisfy complexity <= 8; clang-format and patch integrity pass.
- Normal CMake example smoke test passes 1/1. Independent installed example
  consumer passes 1/1. No old local suite or performance matrix is repeated.
- Core archive retains all 176 object bytes and seven accepted caller hashes;
  two new warm-thread complete-request cost rows pass 40 samples in 5.59 s.

Key session evidence: `pde-financial-final-tests.log`,
`pde-financial-strict.json`, `pde-financial-example-target-test.log`,
`pde-financial-installed-test.log`, `pde-financial-performance/identity-proof.json`
and `pde-financial-performance/results.json`.

## Documentation and delivery

The new published guide describes current C++ behavior, units and separate
derivative/discretization tolerances. The completed #505 controls move to
immutable history, with ledger links updated. No new changelog entry is needed:
this increment validates and demonstrates the already shipped sampled-step
capability; it adds no library algorithm/API. Protected CLAUDE guidance remains
unchanged under the repository's explicit instructions.

Require exact-head CI/Codacy/full review-body audits, actual execution of the
new named cases in fourteen profiles, repeated final merge gates and tested/
merged tree equality before accepting F03 or beginning P04 implementation.
