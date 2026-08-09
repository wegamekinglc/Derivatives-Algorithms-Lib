# PDE Solver Framework Reimplementation - Critic Critique

> **Artifact status: implemented history.** The reviewed PDE reimplementation has shipped.
> Current supported behavior is documented in `docs/methodology/pde.md`.
> References to the retired PDE stack below describe the critique baseline and are intentionally historical.

## Target
- Spec: `.claude/specs/pde-framework-reimplementation.md` (including the API Design section added by `dal-api-designer`)

## Verdict
**Revise** — the overall shape is sound (the theta-scheme math, the FR8 stencils, and the Cube_
layout all check out against the old code), but two Blockers and one internal contradiction sit
squarely on the PR-2 path, and one acceptance criterion is unsatisfiable as literally written.
All fixes are one-paragraph spec amendments. PR-1 (Phases 1-2) is close to ready; see the verdict
detail at the end.

## What was verified and found OK

- **FR8 stencils are algebraically identical to the old `Dx`/`Dxx`.** Old middle Dx coefficient
  `(dxu/dxl − dxl/dxu)/dxm` simplifies exactly to the spec's `(Δ⁺−Δ⁻)/(Δ⁻Δ⁺)`; sub/super diagonals
  and all three Dxx entries match (`dal-cpp/dal/math/pde/finitedifference.cpp:19-27, 39-45`).
- **Theta-scheme sign conventions match.** Old `CalcAx(1.0, dt(1−θ))` builds `I + dt(1−θ)L` with
  `L = μDx + ½σ²Dxx − rI` and `CalcAx(1.0, −dtθ)` builds `I − dtθL`
  (`dal-cpp/dal/math/pde/fd1d.cpp:31-43, 55-75`); the spec's `explicitOp_`/`implicitSolve_`
  comments encode the same operators. Backward roll with positive `dt` matches old usage.
- **FR15 cube layout is real.** `Cube_` strides are row-major with K fastest
  (`dal-cpp/dal/math/ndarray.cpp` `Strides`), so a `(1, 1, nX)` layer is fully contiguous and
  `SliceBegin(0,0)`/`SliceEnd(0,0)` (`dal-cpp/dal/math/ndarray.hpp:102-112`) span exactly `nX`
  doubles. `Cube_<>` compiles via the `template <class = double> class Cube_;` forward declaration
  in `dal-cpp/dal/platform/platform.hpp:52`.
- **"Zero external consumers of `pde.hpp`" is true** — only `pde.cpp` includes it — so the FR9
  rename and `rescalings_` removal break nothing.
- **The factory-overload analysis holds**: the 1-D conveniences take one factory argument, the
  general adapters two, so even generic lambdas bind unambiguously.
- **Benchmark comparability (FR17/NFR) is measurable**: merge-base binary times the old stack,
  new binary times the equivalent workload; note the new stack assembles the explicit operator
  once in `Prepare` while old `RollBwd` re-ran `CalcAx` every step, so the gate should pass with
  headroom despite the per-step scratch allocations.

## Findings

### Blocking Issues

- **B1 — The coefficient *object-identity* check cannot deliver the "checked, not trusted" claim;
  the design has a silent-wrong-numbers path the old stack did not have.**
  FR12 and the API Design rationale sell address identity as the consistency guard, with values
  "sampled at `Prepare` time only". Two failure modes pass the check silently:
  1. *Heap-address reuse.* A caller who re-creates a coefficient per step
     (`Handle_<VectorCoeff_> mu(NewVectorCoeff(...))` inside the loop after one `Prepare`) frees
     and reallocates a same-sized object each iteration; allocators overwhelmingly return the same
     address, so the identity `REQUIRE` **passes** while the roll silently uses the stale
     `Prepare`-time samples.
  2. *In-place mutation.* The natural idiom for time-dependent coefficients — a lambda capturing a
     time variable by reference (exactly how a Dupire local-vol roll would be written) — never
     changes the object address. Forgetting the per-step `Prepare` produces silently frozen
     coefficients and wrong prices, with the check green.
  The old stack was immune by construction: `FD1D_::CacheHit` value-compared `mu_`/`var_`/`r_`
  every roll (`dal-cpp/dal/math/pde/fd1d.cpp:45-53`), and
  `test_fd1d.cpp` `TestChangingCoefficientsBustsCache` pinned that behavior. The migration
  replaces automatic detection with a manual contract policed by a check that passes precisely in
  the dangerous cases. For a quant library this is the highest-cost class of bug (quietly wrong
  numbers downstream).
  - **Suggested fix:** keep the address check as the fast path, and add cheap *probe-value
    revalidation*: `Prepare` stores the sampled values of all three coefficients at k ≈ 3 probe
    nodes (first/middle/last); `operator()` re-samples the coefficients at those probes and
    `REQUIRE`s bitwise equality (message: "coefficient values differ from those prepared - call
    Prepare again"). Cost is ~9 virtual calls per step — noise next to the O(n) solve — and it
    converts both silent modes into loud throws (deterministically for address reuse with
    different values, and for any mutation visible at a probe node). Amend FR12, the error-message
    table, and add an acceptance criterion: "mutating captured coefficient state and rolling
    without re-`Prepare` throws".

- **B2 — FR11's "allocates (or reshapes)" clause walks into a latent heap-corruption bug in
  `ArrayN_::Resize`, and whole-vector aliasing is under-specified.**
  1. `ArrayN_::Resize` sizes the new buffer from the **old** extents
     (`Vector_<E_> new_values(sizes_[0] * strides_[0], E_())` at `dal-cpp/dal/math/ndarray.hpp:60`,
     evaluated before `sizes_` is updated). Growing a cube leaves `vals_` undersized while
     `sizes_`/`strides_` claim the new shape — every subsequent write through `operator()(i,j,k)`
     is an out-of-bounds heap write. Additionally `ArrayN::Moves` increments its counter before
     recording the first move (`dal-cpp/dal/math/ndarray.cpp`, the `for(;;)` loop), so element
     `(0,0,0)` is never copied. Both bugs are invisible today because **nothing in the workspace
     calls `Cube_::Resize`** and `test_ndarray.cpp` has no Resize test. The spec's instruction to
     "reshape" mis-shaped layers to `(1, 1, n)` invites the implementer straight into this.
  2. The recommended idiom `scheme(dt, grids, vals, ..., &vals)` aliases the **whole vector**
     (`new_vals == &old_vals`), not merely per-layer cubes. The Q5 text ("the scheme resizes it to
     `oldVals.size()`... allocates any layer that is null or mis-shaped") is written as if
     `new_vals` were independent; a naive `newVals->clear(); newVals->resize(...)` or
     `assign(...)` destroys `old_vals`' layers mid-roll.
  - **Suggested fix:** amend the Q5 section to (a) forbid `Cube_::Resize` — null or mis-shaped
    layers are **replaced** with freshly constructed `std::make_shared<Cube_<>>(1, 1, n)` (or,
    alternatively, explicitly pull "fix `ArrayN_::Resize`/`Moves` + ndarray Resize tests" into
    project scope); and (b) state that `new_vals == &old_vals` must be supported: when
    `new_vals->size()` already equals `old_vals.size()` the vector must not be cleared or
    reallocated, and layer replacement must not release a source cube before its data is consumed.
    Add a test criterion for the whole-vector-aliased roll (the worked example already depends on
    it).

### Major Concerns

- **M1 — FR14 contradicts the API Design on the *explicit* operator's boundary rows; the naive
  reading breaks the 1e-12 parity cross-check.**
  FR14 assigns Dirichlet-identity rows to "the implicit operator" only. The API Design member
  comment says `explicitOp_` is "I + dt*(1-theta)*L, **identity boundary rows**". These must both
  be identity: `NewDx`/`NewDxx` boundary rows are zero, but the `−r·I` term of `L` contributes at
  boundary diagonals, so an implementer following FR14 literally assembles an explicit boundary
  diagonal of `1 − dt(1−θ)r(x_bdy) ≠ 1` and the boundary values get spuriously discounted in the
  explicit half-step. The old code sets identity rows in **both** assemblies (the unconditional
  `else A_->Set(i, i, 1.0)` in `CalcAx`, `dal-cpp/dal/math/pde/fd1d.cpp:40-41`), so the naive
  reading fails the PR-3 cross-check at bps level, discovered only after PR-2 is built.
  - **Suggested fix:** one sentence in FR14: "Both the explicit and the implicit operators carry
    identity boundary rows (`A(0,0) = A(n−1,n−1) = 1`); in particular the `−r` term must not
    touch the boundary diagonals — matching old `CalcAx` which applied identity rows in both the
    explicit and implicit assemblies."

- **M2 — The FR19 grep-clean acceptance criterion is unsatisfiable as written, and three guidance
  docs will go stale.**
  "A workspace-wide grep for `FD1D_`, `FDM1DMesher_`, ... must come back empty (excluding
  `CHANGELOG.md` history)" can never pass: the spec file itself contains the names, as do
  `.claude/skills/dal-unit-test-write/SKILL.md` (uses `Uniform1DMesher_` in its worked examples,
  lines 74, 182-184), `.claude/agents/dal-performancer.md:85` (describes `pde_perf` as "European
  call via `FD1D_`" — factually wrong after PR-3), and `docs/README.md:70-75` (indexes the old
  mesher content of `pde.md`). FR20 rewrites `pde.md` but not its index entry.
  - **Suggested fix:** scope the grep to `dal-cpp/**` (sources, tests, build files) plus
    `docs/methodology/**`. Add to FR20: update the `docs/README.md` index entry for `pde.md`, the
    `dal-performancer.md` benchmark description, and the `dal-unit-test-write` SKILL.md examples
    that reference deleted classes.

- **M3 — Concentrating-grid endpoint exactness is silently weaker than the old mesher, making two
  acceptance criteria formally unfalsifiable/flaky, and "identical grids" for the parity check is
  not pinned.**
  FR7 pins the first/last node "exactly to the *mapped* endpoints", i.e. `map(yLow)`/`map(yHigh)`.
  For the concentrating map, `map(1.0) = μ + ρ·sinh(asinh((xHigh−μ)/ρ))`, which equals `xHigh`
  only up to round-off — whereas the old mesher pinned `locations_.back() = end` **verbatim**
  (`concentrating1dmesher.cpp:48-49`; `docs/methodology/pde.md` calls this out as deliberate).
  Consequences: (a) the acceptance criterion "concentrating map yields strictly increasing nodes
  **on [xLow, xHigh]**" can fail by one ulp; (b) "identical grids" for the old-vs-new 1e-12
  cross-check is not bitwise achievable on concentrating grids; (c) the exact y-sampling
  arithmetic in `GridLocations` is unspecified, so even uniform grids are only
  ulp-reproducible by luck.
  - **Suggested fix:** (i) have `NewConcentratingMap` store `xLow`/`xHigh` and return them exactly
    at `y == 0.0` / `y == 1.0` (the analytic inverse stays exact: `Y(xHigh)` recomputes `c2`
    bitwise and yields exactly 1.0); (ii) pin the sampling formula
    `y_i = yLow + i·(yHigh−yLow)/(n−1)` in FR7 (matching old `i*dx` arithmetic); (iii) state that
    the 1e-12 cross-check runs on **both** a uniform and a concentrating grid — the concentrating
    case is where map-vs-mesher parity is actually at risk, and the spec currently doesn't say
    which grid the check uses.

### Minor

- **m1 — `x_dep_t` bit semantics and cube-axis convention are never defined.** C4 says the three
  cube dimensions are "1 time + 2 spatial"; FR15 says a layer is a *single time level* with the
  first two axes "reserved for a future second spatial dimension"; the 1-D convenience implies
  "axis-0 dependence". Which bit of the `MAX_DIMENSIONS = 3` bitset indexes what? Since
  `Coeff_::Value` receives spatial coordinates only, time-dependence cannot be an `x_dep_t` bit —
  but FR12's sampling optimization keys off these bits, so the convention must be written down.
  **Fix:** one paragraph: bit `i` of `x_dep_t` = dependence on `x[i]` (the i-th *spatial*
  coordinate); cube axes = (I: reserved, always 1 per layer; J: future second spatial; K: first
  spatial); time-dependence is expressed out-of-band by re-`Prepare`, never by a bit.
- **m2 — The 1e-10 round-trip criterion doesn't hold for extreme densities.** Near `μ` the y-error
  of `Y(map(y))` is ≈ `ulp(x)/(ρ(c2−c1))`; on a [0, 500] domain with density ≲ 1e-9 this exceeds
  1e-10. **Fix:** pin the tested parameter range in the criterion (e.g. density ∈ [0.01, 10]) or
  scale the tolerance.
- **m3 — `old_vals` validation gaps.** A null `shared_ptr` layer in `old_vals` is a dereference
  waiting to happen (the error table only covers `new_vals` allocation and layer *shape*); empty
  `old_vals` (zero layers) is unspecified. **Fix:** `REQUIRE` non-null old layers with a message;
  define empty `old_vals` (suggest: `REQUIRE` non-empty, consistent with fail-loud).
- **m4 — `pde.hpp` is not self-contained**, using `Cube_<>`, `SquareMatrix_<>`, `Handle_`,
  `std::bitset`, `std::shared_ptr` with only `dal/time/datetime.hpp` included (it compiles today
  only because `pde.cpp` includes `platform.hpp` first). New headers will include it, and FR9's
  `rescalings_` removal drops the datetime include. **Fix:** note in FR9/Compatibility that adding
  the missing includes is in scope and not an "interface change".

### Nits

- FR10/FR16 imply the `MatrixCoeff_` supplies σ² (variance) with the ½ applied by the scheme
  (matching old `0.5 * var_(i)`); say so in one sentence — a misreading is caught by the
  convergence test, but cheaply prevented here.
- The `dt` consistency `REQUIRE` is bitwise; a caller computing per-step `dt` as
  `t·(n+1)/N − t·n/N` will trip it spuriously. Document "compute `dt` once" next to the error
  message.
- The map acceptance criterion calls `map(y, nullptr, nullptr)`; the existing maps are null-safe
  via `ASSIGN` (`dal-cpp/dal/utilities/algorithms.hpp:12`) — require the new concentrating map to
  follow the same convention.
- The worked example omits the elapsed-time column that FR16's "same output format as today"
  requires (current `european_fd.cpp:81` prints `Elapsed (ms)`); the seed is illustrative, but the
  implementer should follow FR16, not the seed.
- θ = 1 / θ = 0 fast paths (skip the identity explicit multiply / skip the solve) are unspecified;
  either choice is observationally equivalent — fine to leave to the implementer, worth a
  half-sentence saying so.

## Counter-Proposals

None structural. The `Prepare`/`operator()` split is the right answer under the `mutable` ban, and
the map-based grid is a genuine simplification over the mesher family. The only design-level push:
if B1's probe-value check is rejected, the fallback should be *removing* the false sense of
security — document the identity check as a debugging aid only, in FR12 and the methodology doc,
and add a "sharp edge" paragraph about time-dependent coefficients. Checked-but-wrong is worse
than documented-unchecked.

## Questions for the Author

1. B1: probe-value revalidation, or explicit downgrade of the identity check to "aid, not
   guarantee"? One of the two must land in FR12.
2. B2: replace-don't-reshape, or is fixing `ArrayN_::Resize`/`Moves` (with tests) intended to be
   in scope? The spec currently implies Resize works.
3. M3: which grid(s) does the one-off 1e-12 cross-check run on? If concentrating grids are
   included, the endpoint-pinning fix is required for "identical grids" to be constructible.
4. Should `Decompositions()` saturate or is `int` rollover acceptable? (Trivial; asking only
   because FR13 makes it a tested contract.)

## Verdict Detail

- **PR-1 (Phases 1-2)** may be delegated after resolving **M3** (endpoint pinning + y-sampling
  formula live in FR6/FR7) and the cheap m1/m2/m4 amendments — everything else is Phase-3+.
- **PR-2** must not start before **B1**, **B2**, and **M1** are resolved in the spec; all three
  sit directly on `ThetaScheme_`'s contract and would otherwise be discovered as rework in PR-3's
  parity check.
- **PR-3** needs **M2**'s grep re-scoping and doc-update list.
