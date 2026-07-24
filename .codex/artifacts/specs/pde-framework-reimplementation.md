# PDE Solver Framework Reimplementation - Specification

## Source
- Issue: user request on 2026-07-07 (clean-room reimplementation of the PDE solver framework)
- Related methodology: `docs/methodology/pde.md` (meshers, coordinate maps, boundary-null convention)
- Framework skeleton: `dal-cpp/dal/math/pde/pde.hpp` / `pde.cpp` (namespace `Dal::PDE`)

## Problem Statement

`dal-cpp/dal/math/pde/pde.hpp` sketches an abstract PDE framework — `CoordinateMap_`,
`CoordinateVector_`, the `ScalarCoeff_`/`VectorCoeff_`/`MatrixCoeff_` coefficient
interfaces, and the `Rollback_` scheme interface — but it is dead code: the three
`NewConstCoeff` factories are declared and never defined, no `Rollback_` subclass exists,
and nothing in the codebase includes the header. Meanwhile the *working* finite-difference
stack (`FD1D_`, `Dx`/`Dxx`, the `FDM1DMesher_` family) lives beside it with a mutable-getter
API (`fd.Mu() = ...`) that bypasses the framework entirely. Anyone wanting a second scheme
(ADI, fully implicit with different boundaries) or a second spatial dimension has no seam to
plug into. This project brings the abstract framework to life as the *only* PDE stack and
retires the old one.

## Goals

- Implement the declared-but-missing pieces of `Dal::PDE`: the three `NewConstCoeff`
  factories, callable-backed coefficient adapters, a concentrating `CoordinateMap_`, grid
  construction from `CoordinateVector_`, and a concrete `ThetaScheme_ : Rollback_` for one
  spatial dimension.
- Rewire all PDE consumers (`european_fd` example, `pde_perf` benchmark, PDE unit tests)
  onto the new stack.
- Delete the old stack (`fd1d.{hpp,cpp}`, `finitedifference.{hpp,cpp}`,
  `meshers/*`) and its tests once parity is demonstrated.
- Update `docs/methodology/pde.md` to describe only the new stack; record the change in
  `CHANGELOG.md`.

## Non-Goals

- **No 2-spatial-dimension scheme (ADI).** The `Rollback_` interface and grid/value-container
  conventions must *permit* a future ADI subclass, but no such scheme is implemented,
  tested, or benchmarked in this project.
- **No AAD-aware PDE rolls.** All types are plain `double`; no `Number_`/active-type
  templating of the PDE stack.
- **No changes to `Sparse::TriDiagonal_`** (`dal-cpp/dal/math/matrix/banded.hpp`) beyond
  what the new operator builders consume; it remains the tridiagonal representation.
- **No new boundary-condition framework.** Dirichlet-identity boundary rows are the only
  built-in policy (see FR14); Neumann/Robin/absorbing boundaries are future work.
- **No `ArrayN_`/`Cube_` repairs.** `ArrayN_::Resize` and `ArrayN::Moves` have latent
  bugs (critique B2; see FR11 and API Design); the new stack simply never calls
  `Cube_::Resize`, and fixing `ArrayN_` is out of scope.

## Hard Design Constraints (user-mandated)

- **C1 - Clean room.** The old FD code (`FD1D_`, `Dx`/`Dxx` over `FDM1DMesher_`, the
  `FDM1DMesher_`/`Uniform1DMesher_`/`Concentrating1dMesher_` family) is *numerical
  reference only*. New code must not include, call, or inherit from any of it. At project
  end the old files and their tests are deleted.
- **C2 - Every scheme is a `Rollback_`.** The theta scheme ships as
  `ThetaScheme_ : Rollback_` implementing exactly the existing
  `Rollback_::operator()(double dt, const Vector_<CoordinateVector_>& x_points, const
  Vector_<std::shared_ptr<Cube_<>>>& old_vals, const ScalarCoeff_& discounting, const
  VectorCoeff_& advection, const MatrixCoeff_& diffusion,
  Vector_<std::shared_ptr<Cube_<>>>* new_vals) const` signature. Future schemes (ADI) are
  additional subclasses of `Rollback_`.
- **C3 - `CoordinateMap_` is the mesher.** Grid construction is: uniform y-sample of
  `n` points on `[yLow_, yHigh_]` from a `CoordinateVector_`, mapped pointwise through
  `yToX_` (a `Handle_<CoordinateMap_>`). Uniform grid = identity map; concentrated grid = a
  new `CoordinateMap_` subclass encoding the sinh/asinh concentration stretch with analytic
  `dx/dy`, `d²x/dy²`, and inverse `Y(x)`. No `FDM1DMesher_` anywhere in the new stack.
- **C4 - `Cube_<>` is the value container** (`dal-cpp/dal/math/ndarray.hpp`), covering
  at-most-3-dimensional solving: 1 time dimension plus at most 2 spatial dimensions.
  Value layers are `Vector_<std::shared_ptr<Cube_<>>>` per the `Rollback_` signature;
  1-spatial-dim problems use a degenerate (size-1) second spatial axis.
- **C5 - No `mutable`, no `volatile`** per `.codex/skills/dal-agent-team/references/code-style.md`. Because
  `Rollback_::operator()` is `const`, any decomposition cache must be established in a
  non-const preparation phase or held in caller-owned state — it cannot be a
  `mutable` member updated inside the const call (see FR12 and Open Questions).

## Functional Requirements

### Phase 1 - Coefficient primitives

- **FR1** - `ScalarCoeff_* NewConstCoeff(double val)` is defined: `Value(x, out)` writes
  `val` for any `x`; `XDependence()` returns an all-zero `x_dep_t`.
- **FR2** - `VectorCoeff_* NewConstCoeff(const Vector_<>& val)` is defined: `Value` copies
  `val`; `XDependence()` returns a `Vector_<x_dep_t>` of `val.size()` all-zero bitsets.
- **FR3** - `MatrixCoeff_* NewConstCoeff(const Matrix_<>& val)` is defined: `Value` copies
  `val` into the `SquareMatrix_<>` output (requiring `val` square); `XDependence()` returns
  a `Matrix_<x_dep_t>` of matching shape, all-zero.
- **FR4** - Callable-backed adapters exist for all three coefficient shapes, so that
  spatially varying coefficients are expressible without hand-writing a subclass per use.
  Final factory shapes (declarations in the API Design section): general adapters
  `NewScalarCoeff(f, dep)` / `NewVectorCoeff(f, dep)` / `NewMatrixCoeff(f, dep)` whose
  `dep` argument both supplies the dependence flags (reported verbatim by
  `XDependence()`) and declares the output shape (the adapter sizes the out-parameter
  from `dep` before invoking `f`), plus 1-D convenience overloads
  `NewScalarCoeff(f)` / `NewVectorCoeff(f)` / `NewMatrixCoeff(f)` taking
  `std::function<double(double)>` with axis-0 dependence implied. This is sufficient to
  express the Black-Scholes coefficients μ(x) = (r−d)·x, σ²(x) = vol²·x², r(x) = r in
  one line each.
  Bit convention for `x_dep_t` (critique m1, applies everywhere the type appears): bit
  `i` of the `MAX_DIMENSIONS`-wide bitset means "depends on `x[i]`", the i-th *spatial*
  coordinate. Time dependence is never a dependence bit — `Coeff_::Value` receives
  spatial coordinates only; time variation is expressed out-of-band by re-calling
  `Prepare` (FR12).
- **FR5** - All factories follow the project convention: `New*` free functions returning a
  raw pointer that the caller wraps in `Handle_`/`std::unique_ptr` (as `NewSinhMap` does
  today).

### Phase 2 - Coordinate maps, grids, and difference operators

- **FR6** - A new concentrating `CoordinateMap_` subclass with factory
  `CoordinateMap_* NewConcentratingMap(double xLow, double xHigh, double cPoint, double density)`
  (name and parameters confirmed by API design) encodes the stretch
  x(y) = μ + ρ·sinh(c₁(1−y) + c₂y) with c₁ = asinh((xLow−μ)/ρ), c₂ = asinh((xHigh−μ)/ρ),
  ρ = density·(xHigh−xLow), mapping y ∈ [0,1] onto [xLow, xHigh] with nodes concentrated
  around μ ∈ [xLow, xHigh]. It provides analytic `dx/dy` and `d²x/dy²` in
  `operator()(y, dxDy, d2xDy2)` and the analytic inverse `Y(x)`.
  **Endpoint exactness (critique M3 — resolved):** the map stores `xLow`/`xHigh` and is
  *endpoint-exact*: `operator()(0.0, ...)` returns `xLow` and `operator()(1.0, ...)`
  returns `xHigh` **bitwise**. The floating-point sinh/asinh evaluation realizes the
  map's defining boundary constraints x(0) = xLow, x(1) = xHigh only up to
  `sinh(asinh(·))` round-off; when the argument equals a domain endpoint exactly, the
  implementation returns the stored bound verbatim. Symmetrically the inverse is
  endpoint-exact: `Y(xLow) == 0.0` and `Y(xHigh) == 1.0` bitwise (recomputing c₁/c₂
  with the constructor's expression makes this automatic — the quotient becomes
  (c₂−c₁)/(c₂−c₁) — but the contract is the bitwise result, not the mechanism). The
  derivative out-values `dxDy`/`d2xDy2` at the endpoints remain the analytic formulas:
  the snap adjusts the mapped *value* by at most a few ulp and does not redefine the
  derivatives. This is not grid-awareness leaking into the map — the endpoint
  constraints are part of the map's mathematical definition (they are what fix c₁ and
  c₂, per `docs/methodology/pde.md`), so realizing them exactly makes the implementation
  *more* faithful to the definition. The snap fires only on bitwise-exact endpoint
  arguments (never on approximate proximity), so interior behavior and interior
  round-trips are untouched, and the `Y`/`operator()` round-trip criteria are
  strengthened at y ∈ {0, 1} (exact) rather than broken.
  Input validation mirrors
  the old `Concentrating1dMesher_`: μ within `[xLow, xHigh]`, ρ > 0, else `REQUIRE` throws.
  (The old `requireCPoint` node-snapping option is *not* carried over — with the map-based
  design a caller who needs a node exactly on μ chooses `n` and the map so that a uniform
  y-node lands on `Y(μ)`; see Open Questions.) Like the existing maps, the new map's
  `operator()(y, dxDy, d2xDy2)` must accept null derivative out-pointers (the null-safe
  `ASSIGN` convention of `dal-cpp/dal/utilities/algorithms.hpp`).
- **FR7** - A grid-construction function `Vector_<> GridLocations(const CoordinateVector_&)`
  materializes a `CoordinateVector_` into physical nodes.
  **Sampling and pinning semantics (critique M3 — resolved):** with
  `dy = (yHigh_ − yLow_)/(n_ − 1)` computed **once**, the y-sample is
  y₀ = `yLow_` and y_{n−1} = `yHigh_` assigned **verbatim** (never through the
  increment formula, whose value `yLow_ + (n−1)·dy` misses `yHigh_` by round-off) and
  y_i = `yLow_ + i·dy` for interior i = 1..n−2 — the same arithmetic as the old
  meshers' `i*dx`. Each y_i is mapped through `yToX_`, so the end nodes are exactly
  `map(yLow_)`/`map(yHigh_)`; because both `Make*` builders pair verbatim y-bounds with
  endpoint-exact maps (identity trivially; concentrating per FR6), grids built through
  `MakeUniformGrid`/`MakeConcentratingGrid` have first/last node equal to the caller's
  `xLow`/`xHigh` **bitwise** — the same guarantee the old meshers' verbatim
  `locations_.back() = end` pin gave (rationale in `docs/methodology/pde.md`), so
  boundary-value writes keyed to the exact bounds stay exact. Spacings are consistent
  with the pinned endpoints by construction: the new stack stores no
  `DPlus`/`DMinus`-style spacing arrays — `NewDx`/`NewDxx` (FR8) derive Δ⁻/Δ⁺ from the
  final locations vector, so operator spacings always agree with the locations as
  pinned. (Ulp-level nuance vs the old stack, relevant only to the PR-3 cross-check:
  the old `Uniform1DMesher_` stored the closed-form `dx` as the last interval's spacing
  *despite* pinning `locations_.back()`, so its last-interior-row operator coefficients
  can differ from location-derived ones by round-off — absorbed by the 1e-12
  tolerance.) Output is the node locations `x_i`. Two value-returning helpers,
  `MakeUniformGrid(xLow, xHigh, n)` and
  `MakeConcentratingGrid(xLow, xHigh, n, cPoint, density)`, build a correctly paired
  `CoordinateVector_` (map plus its matching y-domain) so callers cannot mismatch the
  concentrating map's y ∈ [0,1] convention (see API Design).
  `REQUIRE`: `n_ >= 3`, `yHigh_ > yLow_`, non-null `yToX_`, mapped locations strictly
  increasing — the monotonicity check also guards the (astronomically unlikely,
  n ≳ 10¹⁵-scale) case where an interior node lands within a few ulp of a pinned
  endpoint and the FR6 snap would invert the last interval: it fails loud, never
  reorders.
- **FR8** - New non-uniform first- and second-derivative operator builders
  `NewDx(const Vector_<>& x)` / `NewDxx(const Vector_<>& x)` return
  `Sparse::TriDiagonal_*` built from node locations (not from `FDM1DMesher_`). Interior
  rows use the standard 3-point non-uniform stencils (identical numerics to the old
  `Dx`/`Dxx`): for spacings Δ⁻ = xᵢ−xᵢ₋₁, Δ⁺ = xᵢ₊₁−xᵢ,
  - Dx row i: (−Δ⁺/(Δ⁻(Δ⁻+Δ⁺)), (Δ⁺−Δ⁻)/(Δ⁻Δ⁺), Δ⁻/(Δ⁺(Δ⁻+Δ⁺)))
  - Dxx row i: (2/(Δ⁻(Δ⁻+Δ⁺)), −2/(Δ⁻Δ⁺), 2/(Δ⁺(Δ⁻+Δ⁺)))
  Boundary rows are zero (the scheme owns the boundary policy, FR14).
- **FR9** - The style wart `CoordinateVector_::n` (missing trailing underscore) is fixed to
  `n_` while the header is being touched. The unused `rescalings_` member
  (`std::map<DateTime_, double>`, no semantics anywhere in the codebase) is removed until
  a real need arrives (API decision, see Open Questions); the remaining members keep
  their names. While the header is touched it is also made self-contained (critique m4):
  `pde.hpp` uses `Cube_<>`, `SquareMatrix_<>`, `Handle_`, `std::bitset`, and
  `std::shared_ptr` yet includes only `dal/time/datetime.hpp` (it compiles today only
  because `pde.cpp` includes `platform.hpp` first), and removing `rescalings_` drops that
  lone include. Adding the missing includes is in scope and is not an interface change
  (see Compatibility).

### Phase 3 - ThetaScheme_

- **FR10** - `ThetaScheme_ : Rollback_` implements one backward time step of
  ∂V/∂t + μ(x)·∂V/∂x + ½σ²(x)·∂²V/∂x² − r(x)·V = 0 via the theta scheme:
  explicit sub-step for weight (1−θ), implicit solve for weight θ. θ = 0 is fully
  explicit, θ = 0.5 Crank-Nicolson, θ = 1 fully implicit. θ is a constructor parameter,
  `REQUIRE`d in [0, 1]. The implementation supports exactly 1 spatial dimension:
  `REQUIRE` that `x_points.size() == 1` (a 2-dim request must throw, not silently
  degrade). Convention: the `MatrixCoeff_` (diffusion) supplies the *variance* σ²(x); the
  scheme applies the factor ½ itself, matching the old `0.5 * var_(i)` in `CalcAx`.
  Whether θ = 1 skips the (identity) explicit multiply and θ = 0 skips the solve is an
  implementer's choice — the two are observationally equivalent.
- **FR11** - `operator()` performs *one* step of size `dt`; the caller owns the time loop
  (stepping, boundary-value updates between steps, terminal condition). Multiple value
  layers in `old_vals` are all rolled with the same operator in a single call.
  `old_vals` must be non-empty and every layer non-null (`REQUIRE`, fail-loud).
  `new_vals` must be non-null; the scheme brings it to `old_vals.size()` layers, each of
  shape (1, 1, n), **without ever calling `Cube_::Resize`** — a null or mis-shaped
  target layer is *replaced* with a freshly constructed
  `std::make_shared<Cube_<>>(1, 1, n)` (`ArrayN_::Resize` has a latent buffer-sizing bug
  and `ArrayN::Moves` never copies element (0,0,0); the new code path must not exercise
  them — see Non-Goals). Aliasing (resolved, see API Design): whole-vector aliasing
  (`new_vals == &old_vals`, the recommended idiom `roll(..., vals, ..., &vals)`) and
  same-index layer aliasing (`(*new_vals)[l]` pointing at the very `Cube_` object of
  `old_vals[l]`) are permitted and tested; when the vectors alias, the scheme must not
  clear or reallocate the vector, and layer replacement must not release a source cube
  before its data has been consumed. Aliasing across *different* layer indices (e.g.
  `(*new_vals)[0]` aliasing `old_vals[1]`) is forbidden and its behavior undefined.
- **FR12** - Decomposition reuse, driven by `XDependence()` and constrained by C5
  (mechanism resolved, see API Design):
  - `ThetaScheme_` has a non-const method
    `Prepare(dt, xPoints, discounting, advection, diffusion)` that builds the grid,
    samples the coefficients, assembles the explicit and implicit operators with `dt`
    baked in, and (for θ > 0) factors the implicit operator **exactly once**. The const
    `operator()` is a pure consumer — it never assembles or factors; it applies the
    prepared explicit multiply and implicit solve to every value layer.
  - `operator()` `REQUIRE`s consistency with the prepared state: exact `dt` match
    (bitwise — callers should compute `dt` once, not re-derive it per step, e.g.
    `t·(n+1)/N − t·n/N` differs by round-off and trips the check spuriously), matching
    grid (`n_`, `yLow_`, `yHigh_`, same `yToX_` handle), and a two-tier coefficient
    check: (a) *object identity* of the three coefficient arguments (same addresses as
    passed to `Prepare`) as the fast path, and (b) *probe-value revalidation* — at
    `Prepare` time the scheme stores the sampled values of all three coefficients at 3
    probe nodes (first, middle, and last grid node); every `operator()` call re-samples
    the coefficients at those probe nodes and `REQUIRE`s bitwise equality with the
    stored values (message: "coefficient values differ from those prepared - call
    Prepare again"). The probe check exists because address identity alone passes
    silently under heap-address reuse (a freed-and-reallocated coefficient landing at
    the same address) and under in-place mutation of captured state (e.g. a lambda
    capturing a time variable by reference) — both would otherwise roll with stale
    `Prepare`-time samples and produce quietly wrong numbers, a failure the old
    `FD1D_::CacheHit` value-comparison was immune to. Cost: ~9 virtual calls (3 probes ×
    3 coefficients) per step, O(1) and noise next to the O(n) solve; this is stated as a
    performance commitment and covered by the FR17 benchmark gate. The probe check
    detects any change visible at a probe node (deterministic for address reuse with
    different values); a mutation that leaves all three probe nodes bitwise unchanged is
    not detected — callers whose coefficients change over time must still call `Prepare`
    again before the next roll; the probes are a guard-rail, not the contract.
  - Reuse across time steps is therefore caller-managed and explicit: for a
    time-homogeneous problem, `Prepare` is called once before the loop and the single
    factorization serves all N steps; for time-dependent coefficients, `Prepare` is
    called per step. There is no hidden cache-invalidation logic.
  - All-zero `XDependence()` on every coefficient additionally permits `Prepare` to
    sample each coefficient once instead of per-node during assembly.
- **FR13** - Observability of reuse: `ThetaScheme_` exposes
  `[[nodiscard]] int Decompositions() const` — the number of factorizations performed
  since construction (analogous to the old `FD1D_::DecompositionsSinceInit()`). Each
  `Prepare` with θ > 0 increments it by exactly one; `Prepare` with θ = 0 performs no
  factorization and leaves it unchanged; `operator()` never changes it. It is a plain
  `int` with no saturation semantics (rollover is not a realistic concern at one
  increment per `Prepare`). Tests can assert
  "one `Prepare` + N time-homogeneous steps ⇒ exactly 1 factorization".
- **FR14** - Boundary policy: default Dirichlet-identity rows. **Both** the explicit-part
  operator and the implicit-part operator carry identity rows at both spatial boundaries
  (A(0,0) = A(n−1,n−1) = 1, off-diagonals zero); in particular the −r·I term of L must
  not touch the boundary diagonals of *either* operator. This matches the old `CalcAx`,
  which applied identity rows unconditionally in both the explicit and implicit
  assemblies — an implicit-only reading would let the explicit half-step spuriously
  discount boundary values (diagonal 1 − dt(1−θ)r ≠ 1) and break the 1e-12 parity
  cross-check. Net effect: boundary values pass through the whole roll unchanged; the
  caller overwrites boundary nodes between steps if desired (as `european_fd` does
  today).
- **FR15** - Value-container convention (per C4): each `Cube_<>` layer holds the spatial
  field at a single time level. Axis convention (critique m1): I = reserved, always
  size 1 per layer; J = future second spatial dimension (size 1 in this project); K =
  first spatial dimension. Time is *not* a cube axis of a layer — the time dimension
  lives in the caller's loop over layers/steps. For 1-spatial-dim problems the shape is
  (1, 1, nX) — the spatial axis is the *last* (K) axis, so a layer's data is contiguous
  via `Cube_::SliceBegin(0, 0)`/`SliceEnd(0, 0)` and tridiagonal solves run over
  contiguous memory. The reserved first two axes leave room for a future second spatial
  dimension without changing the `Rollback_` signature. `ThetaScheme_` `REQUIRE`s
  `SizeI() == 1 && SizeJ() == 1` and `SizeK() == n_` of the single `CoordinateVector_`.

### Phase 4 - Consumer rewrite

- **FR16** - `dal-cpp/examples/european_fd/european_fd.cpp` is rewritten on the new stack:
  Black-Scholes European call, Crank-Nicolson (θ = 0.5), uniform grid via
  `MakeUniformGrid`/`GridLocations`, coefficients via `NewConstCoeff`/callable adapters
  (μ = (r−d)x, σ² = vol²x², r const), caller-side time loop with the same
  boundary-value updates, cubic-interpolated read-out at spot, convergence table against
  `Distribution::BlackOpt` — same financial scenario and output format as today.
- **FR17** - `dal-cpp/benchmarks/pde_perf/pde_perf.cpp` is rewritten to time the equivalent
  workload on the new stack (200 space × 200 time CN roll of the same European call,
  rebuild-grid-and-prepare included in the timed body, same `Bench::Run` harness and
  repeat counts).
- **FR18** - PDE unit tests are rewritten under `dal-cpp/tests/math/pde/` as
  `test_<name>.cpp` files covering: coordinate maps (identity, sinh, concentrating),
  coefficient factories/adapters, grid construction, operator builders, and
  `ThetaScheme_` (see Acceptance Criteria). Old test files
  (`test_fd1d.cpp`, `meshers/test_uniform1dmesher.cpp`,
  `meshers/test_concentrating1dmesher.cpp`) are deleted with the old code.

### Phase 5 - Deletion and documentation

- **FR19** - Deleted at project end: `dal-cpp/dal/math/pde/fd1d.{hpp,cpp}`,
  `dal-cpp/dal/math/pde/finitedifference.{hpp,cpp}`, the whole
  `dal-cpp/dal/math/pde/meshers/` directory, the three old test files, and all build-system
  references to them. A grep for `FD1D_`, `FDM1DMesher_`, `Uniform1DMesher_`,
  `Concentrating1dMesher_`, `finitedifference.hpp` scoped to `dal-cpp/` (sources, tests,
  examples, benchmarks, build files) and `docs/methodology/` must come back empty — i.e.
  no *code* or methodology-doc references remain. Prose references elsewhere (this spec,
  preserved `.claude/**` references and `CHANGELOG.md` history) are out of the grep's scope.
  The preserved Claude references remain historical provenance and are not update targets;
  only the `docs/README.md` index reference requires updating per FR20.
- **FR20** - `docs/methodology/pde.md` is rewritten current-state-only for the new stack
  (coordinate maps including the new concentrating map and its FR6 endpoint-exactness
  contract, grid construction including the FR7 pinned y-sampling — together replacing
  the old mesher-pinning rationale — operator
  stencils, theta scheme, boundary policy, decomposition reuse and the probe-value
  guard), no line-number citations, project-relative file paths per the markdown rules.
  The same PR updates the `docs/README.md` index entry for `pde.md` (currently indexing the old
  mesher content). The historical Claude guidance references that described deleted classes are
  retained as provenance; their current Codex counterparts were modernized during migration and
  are not future update targets. A dated `CHANGELOG.md` entry records
  the replacement (old mesher/FD1D stack removed, `Rollback_` framework implemented).

## Non-Functional Requirements

- **Performance** - Rewritten `pde_perf` shows no regression vs the merge-base baseline on
  the equivalent 200×200 CN workload: paired best-of-N (N ≥ 10, interleaved, Release,
  same machine), gate on min-of-N; a delta must exceed ~2× the ~1% noise floor to count
  as a regression. Decomposition reuse must make the time-homogeneous roll cost one
  factorization plus per-step triangular solves, matching the old `FD1D_` asymptotics.
  The FR12 probe-value revalidation adds O(1) work per step (~9 virtual coefficient
  calls, independent of n) and is included in the benchmarked path.
- **Differentiability** - None. Plain `double` throughout; no AAD requirement.
- **Compatibility** - `pde.hpp` has zero external consumers today, so its abstract
  interfaces (`CoordinateMap_`, `Coeff_` hierarchy, `Rollback_` signature,
  `MAX_DIMENSIONS`) are kept as-is except the FR9 rename, the FR9 removal of the
  unused `rescalings_` member, and the FR9 addition of the includes `pde.hpp` itself
  needs to be self-contained (not an interface change); no serialized state, no public
  (`dal-public`/Excel/Python) surface is touched. Everything else in the workspace must
  keep building and passing: full build + `ctest` green at every PR boundary.
- **Style** - `.codex/skills/dal-agent-team/references/code-style.md` (trailing-underscore classes, camelCase
  params, lowercase-no-separator filenames, `#pragma once`, no `mutable`/`volatile`) and
  `.codex/skills/dal-agent-team/references/unit-test-style.md` (`TEST` only, no fixtures, `ASSERT_*` preferred,
  suite names PascalCase).

## Inputs and Outputs

Key surface of the new stack (final, resolved by `dal-api-designer` — declarations in
the API Design section below):

| Name                                                   | Type                                                    | Units         | Range / Constraints                                                          |
|--------------------------------------------------------|---------------------------------------------------------|---------------|------------------------------------------------------------------------------|
| `NewConstCoeff(val)`                                   | `double` / `Vector_<>` / `Matrix_<>` in, `*Coeff_*` out | model units   | matrix must be square                                                        |
| `NewScalarCoeff` / `NewVectorCoeff` / `NewMatrixCoeff` | callable + dependence flags in, `*Coeff_*` out          | model units   | callable non-empty; matrix `dep` square; 1-D overloads fix axis-0 dependence |
| `NewConcentratingMap(...)`                             | `double xLow, xHigh, cPoint, density` in, map out       | x-space       | `xHigh > xLow`, `cPoint` in `[xLow, xHigh]`, `density > 0`; endpoint-exact (FR6)       |
| `CoordinateVector_`                                    | `{yLow_, yHigh_, n_, yToX_}`                            | y-space       | `yHigh_ > yLow_`, `n_ >= 3`, `yToX_` non-null                                |
| `MakeUniformGrid` / `MakeConcentratingGrid`            | bounds + `n` in, `CoordinateVector_` out                | x-space       | map and `CoordinateVector_` constraints above                                |
| `GridLocations(points)`                                | `CoordinateVector_` in, `Vector_<>` locations out       | x-space       | strictly increasing locations; end nodes pinned per FR7 (bitwise `xLow`/`xHigh` for `Make*` grids) |
| `NewDx` / `NewDxx`                                     | node locations in, `Sparse::TriDiagonal_*` out          | 1/x, 1/x²     | `n >= 3`; zero boundary rows                                                 |
| `ThetaScheme_(theta)`                                  | `double theta`                                          | dimensionless | `0 <= theta <= 1`                                                            |
| `ThetaScheme_::Prepare(...)`                           | `dt`, grid, coefficients in                             | time in years | `dt > 0`; `xPoints.size() == 1`                                              |
| `ThetaScheme_::operator()`                             | per `Rollback_` signature                               | time in years | prepared state matches (incl. probe values, FR12); cube shapes per FR15      |
| `ThetaScheme_::Decompositions()`                       | `int` accessor                                          | count         | factorizations since construction                                            |

## API Design

Resolved by `dal-api-designer` on 2026-07-07; M3 endpoint-pinning follow-up resolved
2026-07-07 (see "Endpoint pinning" under Grid construction, and Open Questions).
Audiences: C++ quants only — this project
touches no `dal-public`/Excel/Python surface (see Compatibility), so ergonomics are
judged on the C++ call sites in `dal-cpp/examples/`, `dal-cpp/benchmarks/`, and tests.

### File layout (Q4 — confirmed)

- `dal-cpp/dal/math/pde/pde.{hpp,cpp}` — abstract framework (unchanged interfaces),
  coordinate maps (`NewSinhMap`, `NewIdentityMap`, new `NewConcentratingMap`), and all
  six coefficient factories. Everything a coefficient author needs is one include.
- `dal-cpp/dal/math/pde/pdegrid.{hpp,cpp}` — `GridLocations`, `MakeUniformGrid`,
  `MakeConcentratingGrid`.
- `dal-cpp/dal/math/pde/pdeoperators.{hpp,cpp}` — `NewDx`, `NewDxx`.
- `dal-cpp/dal/math/pde/thetascheme.{hpp,cpp}` — `ThetaScheme_`.
- Tests: `test_pde.cpp`, `test_pdegrid.cpp`, `test_pdeoperators.cpp`,
  `test_thetascheme.cpp` under `dal-cpp/tests/math/pde/`.

All names are lowercase with no separators per `.codex/skills/dal-agent-team/references/code-style.md`.

### Coefficient factories (Q1)

In `dal-cpp/dal/math/pde/pde.hpp`:

```cpp
namespace Dal::PDE {
    // constant coefficients (declared today, now defined)
    ScalarCoeff_* NewConstCoeff(double val);
    VectorCoeff_* NewConstCoeff(const Vector_<>& val);
    MatrixCoeff_* NewConstCoeff(const Matrix_<>& val);

    // callable-backed adapters; dep declares both the dependence flags and the output
    // shape (the adapter sizes the out-parameter from dep before invoking f)
    ScalarCoeff_* NewScalarCoeff(std::function<double(const Vector_<>&)> f, Coeff_::x_dep_t dep);
    VectorCoeff_* NewVectorCoeff(std::function<void(const Vector_<>&, Vector_<>*)> f, const Vector_<Coeff_::x_dep_t>& dep);
    MatrixCoeff_* NewMatrixCoeff(std::function<void(const Vector_<>&, SquareMatrix_<>*)> f, const Matrix_<Coeff_::x_dep_t>& dep);

    // 1-D conveniences: single spatial coordinate, axis-0 dependence implied;
    // NewVectorCoeff yields a length-1 vector, NewMatrixCoeff a 1x1 matrix
    ScalarCoeff_* NewScalarCoeff(std::function<double(double)> f);
    VectorCoeff_* NewVectorCoeff(std::function<double(double)> f);
    MatrixCoeff_* NewMatrixCoeff(std::function<double(double)> f);
} // namespace Dal::PDE
```

Rationale:
- **Distinct names per shape, not an overloaded `NewCoeff`.** The produced type is in
  the name (matching `ScalarCoeff_`/`VectorCoeff_`/`MatrixCoeff_`, so the name is
  guessable from the interface), and generic lambdas can never make an overload set
  ambiguous across shapes. Within each name the two overloads differ in factory arity
  (one argument vs two), so a lambda binds to exactly one — including generic lambdas.
- **1-D conveniences carry their own dependence.** The only scheme shipped is 1-D; the
  flagship consumers write `NewVectorCoeff([=](double s) { return (rate - div) * s; })`
  — μ(x) = (r−d)x in one line, no bitset spelling, no wrong-shape `dep` possible. The
  general adapters remain the seam for a future ADI scheme.
- **`dep` doubles as shape declaration** for the general vector/matrix adapters, so the
  callable only fills values and there is no separate size argument to get wrong;
  `REQUIRE`s: callable non-empty, matrix `dep` square.
- **`NewConstCoeff` overload safety (verified):** `Vector_<>`, `Matrix_<>`, and
  `SquareMatrix_<>` have no implicit conversions from `double` or from each other (all
  single-argument constructors are `explicit`), so `double`/`Vector_<>`/`Matrix_<>`
  arguments each bind exactly one overload and integer literals promote to the `double`
  overload. The one case needing care: a single-element braced list
  (`NewConstCoeff({1.0})`) binds the `double` overload, not a length-1 vector — callers
  wanting the vector write `NewConstCoeff(Vector_<>{1.0})`. Documented, not "fixed":
  removing the `double` overload would hurt the dominant scalar use.

### Grid construction (Q3, Q6)

Map factory in `dal-cpp/dal/math/pde/pde.hpp` (FR6, confirmed):

```cpp
namespace Dal::PDE {
    // x(y) = mu + rho*sinh(c1*(1-y) + c2*y) on y in [0, 1], nodes concentrated at cPoint.
    // Endpoint-exact (FR6): operator()(0.0, ...) returns xLow and operator()(1.0, ...)
    // returns xHigh bitwise; Y(xLow) == 0.0 and Y(xHigh) == 1.0 bitwise. Derivative
    // out-values at the endpoints remain the analytic formulas.
    CoordinateMap_* NewConcentratingMap(double xLow, double xHigh, double cPoint, double density);
} // namespace Dal::PDE
```

Grid surface in `dal-cpp/dal/math/pde/pdegrid.hpp`:

```cpp
namespace Dal::PDE {
    // Uniform y-sample mapped through points.yToX_ (FR7): y_0 = yLow_ and y_{n-1} = yHigh_
    // are assigned verbatim; interior y_i = yLow_ + i*dy with dy = (yHigh_ - yLow_)/(n_ - 1)
    // computed once. End nodes are exactly map(yLow_)/map(yHigh_) — for Make*Grid-built
    // grids, the caller's xLow/xHigh bitwise (FR6 endpoint exactness).
    Vector_<> GridLocations(const CoordinateVector_& points);

    CoordinateVector_ MakeUniformGrid(double xLow, double xHigh, int n);
    CoordinateVector_ MakeConcentratingGrid(double xLow, double xHigh, int n, double cPoint, double density);
} // namespace Dal::PDE
```

Rationale:
- **`NewConcentratingMap(xLow, xHigh, cPoint, density)`** keeps the positional-doubles
  style of `NewSinhMap(xWidth, dxdyRange)`: domain first, then concentration point, then
  the dimensionless density budget (the methodology-doc vocabulary: `cPoint`,
  `density`). All four are required — there is no principled default density.
- **No `Grid1D_` class.** A 1-D grid *is* its node locations; `GridLocations` returns a
  plain `Vector_<>` that feeds payoff evaluation, boundary writes, `NewDx`/`NewDxx`, and
  interpolated readout directly. Spacings are derived downstream (FR8).
- **`Make*` helpers close a real footgun.** The concentrating map is defined on
  y ∈ [0,1] while the identity map wants y = x; building `CoordinateVector_` by
  aggregate requires the caller to know which y-domain pairs with which map, and a
  mismatch is *silently* wrong (the stretch is monotone on all of ℝ). `MakeUniformGrid`
  and `MakeConcentratingGrid` (value-returning builders, `Make*` per the `MakeSchedule`
  precedent, vs heap-factory `New*`) construct the pair correctly; the aggregate stays
  public as the primitive for hand-rolled maps such as `NewSinhMap`.
- **Endpoint pinning (critique M3 — resolved): pin at both levels, each where it owns
  the information.** The builder pins the *y*-endpoints (y₀/y_{n−1} assigned verbatim —
  the grid-level analogue of the old meshers' `locations_.back() = end`), and the
  concentrating map is *endpoint-exact* (returns its stored bounds bitwise at
  y = 0.0/1.0 — it is the only component that knows `xLow`/`xHigh`, and those bounds
  are its own defining constraints, FR6). Composition gives `Make*` grids verbatim
  user bounds. Alternatives rejected: (a) *builder-only verbatim assignment*
  (`locations.front() = xLow` inside `GridLocations`) is unimplementable at that seam —
  the builder sees only `{yLow_, yHigh_, n_, yToX_}`, and adding x-bound members to
  `CoordinateVector_` would duplicate state the map already owns (a silent-disagreement
  footgun); (b) *map-only pinning* leaves the uniform path broken —
  `yLow + (n−1)·dy ≠ yHigh` in floating point, so the y-sample itself needs the verbatim
  endpoint assignment; (c) *documenting the ulp drift* forfeits verbatim bounds, makes
  the grid-endpoint acceptance criterion tolerance-shaped, breaks boundary writes keyed
  to exact bounds, and leaves "identical grids" for the old-vs-new cross-check
  unconstructible; (d) *snapping on approximate proximity* (`IsClose`) inside the map
  would be genuinely impure — the snap fires only on bitwise-exact endpoint arguments.
  No new error case: the existing "strictly increasing" `REQUIRE` in `GridLocations`
  already converts a snap-induced last-interval inversion (possible only at
  n ≳ 10¹⁵-scale) into a loud throw, so the error-message table is unchanged.
- **Boundary-value overwriting** stays caller-side and index-explicit:
  `(*vals[0])(0, 0, 0) = ...;` and `(*vals[0])(0, 0, n - 1) = ...;` between steps. With
  the FR15 layout the spatial axis is the last cube index, so no helper wrapper is
  needed; the worked example below shows the idiom.

### `ThetaScheme_` and the `Prepare` mechanism (Q2)

In `dal-cpp/dal/math/pde/thetascheme.hpp`:

```cpp
namespace Dal::PDE {
    class ThetaScheme_ : public Rollback_ {
    public:
        explicit ThetaScheme_(double theta);   // REQUIRE 0 <= theta <= 1

        // assembles operators with dt baked in; factors the implicit operator (theta > 0)
        void Prepare(double dt,
                     const Vector_<CoordinateVector_>& xPoints,
                     const ScalarCoeff_& discounting,
                     const VectorCoeff_& advection,
                     const MatrixCoeff_& diffusion);

        // pure consumer of prepared state; REQUIREs dt/grid consistency, coefficient
        // address identity, and probe-value agreement (FR12)
        void operator()(double dt,
                        const Vector_<CoordinateVector_>& xPoints,
                        const Vector_<std::shared_ptr<Cube_<>>>& oldVals,
                        const ScalarCoeff_& discounting,
                        const VectorCoeff_& advection,
                        const MatrixCoeff_& diffusion,
                        Vector_<std::shared_ptr<Cube_<>>>* newVals) const override;

        [[nodiscard]] double Theta() const { return theta_; }
        [[nodiscard]] int Decompositions() const { return decompositions_; }

    private:
        double theta_;
        int decompositions_ = 0;
        double preparedDt_ = 0.0;                                    // 0.0 means "not prepared"
        CoordinateVector_ points_;                                   // prepared grid spec
        Vector_<> x_;                                                // prepared node locations
        const ScalarCoeff_* discounting_ = nullptr;                  // identity of prepared coefficients
        const VectorCoeff_* advection_ = nullptr;
        const MatrixCoeff_* diffusion_ = nullptr;
        Vector_<> probeSamples_;                                     // Prepare-time coefficient values at the
                                                                     // 3 probe nodes (first/middle/last), FR12
        std::unique_ptr<Sparse::TriDiagonal_> explicitOp_;           // I + dt*(1-theta)*L, identity boundary rows (FR14)
        std::unique_ptr<SquareMatrixDecomposition_> implicitSolve_;  // factored I - dt*theta*L, identity boundary rows (FR14)
    };
} // namespace Dal::PDE
```

Rationale — why `Prepare` beats the alternatives under the `mutable` ban:
- **No hidden cache, no invalidation heuristics.** `Prepare` = assemble + factor
  (exactly one factorization when θ > 0); `operator()` = apply. The caller manages
  reuse by *placement*: `Prepare` outside the loop for time-homogeneous problems, per
  step for time-dependent ones. This is precisely the code-style rule's prescription —
  "make the caller manage the state explicitly" — and makes `Decompositions()` trivially
  predictable for FR13 tests.
- **Constructor-injected workspace rejected**: shared mutable state behind a `const`
  call reintroduces `mutable` semantics by the back door, plus aliasing questions when
  two schemes share a workspace. A per-call workspace argument cannot be added because
  the `Rollback_::operator()` signature is fixed (C2).
- **Consistency is checked, not trusted**: `operator()` `REQUIRE`s prepared state
  exists, exact `dt` match, grid match, and a two-tier coefficient check (FR12):
  *address identity* of the three coefficient arguments as the fast documentation of
  intent, plus *probe-value revalidation* — re-sampling the coefficients at the 3
  stored probe nodes and requiring bitwise agreement with the `Prepare`-time samples.
  Address identity alone was rejected as the sole guard (critique B1): it passes
  silently under heap-address reuse and under in-place mutation of captured state
  (e.g. a lambda capturing time by reference), exactly the cases where the roll would
  use stale samples and produce quietly wrong numbers — a regression vs the old
  `FD1D_::CacheHit` value comparison. Full per-node value revalidation was rejected
  as it re-samples three coefficients per node per step for no benefit in the dominant
  time-homogeneous case; the O(1) probe check buys loud failure for both silent modes
  at ~9 virtual calls per step, noise next to the O(n) solve.
- Scratch buffers for the explicit multiply / implicit solve are locals of `operator()`
  (one `Vector_<>` per call), keeping the method genuinely const; the O(n) allocation
  per step is noise next to the O(n) solve (validated by the FR17 benchmark gate).

### `new_vals` aliasing contract (Q5)

- Whole-vector aliasing is **permitted, required, and tested**: `newVals == &oldVals`
  (the very same `Vector_` passed as both arguments) is the recommended time-loop idiom
  `roll(dt, x, vals, ..., &vals)`. When the vectors alias (or whenever
  `newVals->size()` already equals `oldVals.size()`), the scheme must not `clear()`,
  `assign()`, or reallocate the vector — doing so would destroy the source layers
  mid-roll.
- Same-index layer aliasing is **permitted and tested**: `(*newVals)[l]` may be the same
  `Cube_` object as `oldVals[l]`; the scheme copies the source slice into local scratch
  before writing.
- Cross-index aliasing (`(*newVals)[l]` aliasing `oldVals[m]`, `l != m`) is **forbidden**
  and documented as undefined; the scheme does not check it (an O(L²) pointer sweep for
  an error nobody has a reason to write).
- `oldVals` must be non-empty and all its layers non-null (`REQUIRE`, see FR11 and the
  error table).
- `newVals` must be non-null; the scheme brings it to `oldVals.size()` layers of shape
  `(1, 1, n)`. A null or mis-shaped target layer is **replaced** with a freshly
  constructed `std::make_shared<Cube_<>>(1, 1, n)` — never reshaped in place:
  `Cube_::Resize` is forbidden in the new code path (critique B2: `ArrayN_::Resize`
  sizes the new buffer from the *old* extents, so growing a cube corrupts the heap, and
  `ArrayN::Moves` never copies element (0,0,0); nothing else in the workspace calls
  `Resize`, and fixing `ArrayN_` is a Non-Goal). Replacement must not release a source
  cube before its data has been consumed (relevant when the vectors alias).

### Error messages

| Input violation                        | Message text                                                          |
|----------------------------------------|-----------------------------------------------------------------------|
| `NewConstCoeff` matrix not square      | "constant matrix coefficient must be square"                          |
| adapter callable empty                 | "coefficient callable must be non-empty"                              |
| `NewMatrixCoeff` `dep` not square      | "matrix coefficient dependence must be square"                        |
| `NewConcentratingMap` `xHigh <= xLow`  | "concentrating map requires xHigh > xLow"                             |
| `cPoint` outside `[xLow, xHigh]`       | "concentrating map requires cPoint in [xLow, xHigh]"                  |
| `density <= 0`                         | "concentrating map requires density > 0"                              |
| grid `n_ < 3`                          | "grid requires at least 3 points"                                     |
| grid `yHigh_ <= yLow_`                 | "grid requires yHigh > yLow"                                          |
| grid `yToX_` empty                     | "grid requires a coordinate map (yToX_ is empty)"                     |
| mapped locations not increasing        | "grid locations must be strictly increasing"                          |
| `NewDx`/`NewDxx` bad input             | "operator builder requires at least 3 strictly increasing locations"  |
| `theta` out of range                   | "theta must be in [0, 1]"                                             |
| `Prepare` with `dt <= 0`               | "Prepare requires dt > 0"                                             |
| 2-spatial-dim request                  | "ThetaScheme_ supports exactly one spatial dimension"                 |
| coefficient shape wrong in `Prepare`   | "advection/diffusion shape must match one spatial dimension"          |
| roll before `Prepare`                  | "ThetaScheme_ must be Prepared before rolling"                        |
| `dt` mismatch at roll                  | "dt differs from the prepared dt - call Prepare again"                |
| grid mismatch at roll                  | "grid differs from the prepared grid - call Prepare again"            |
| coefficient object mismatch            | "coefficients differ from those prepared - call Prepare again"        |
| coefficient probe-value mismatch       | "coefficient values differ from those prepared - call Prepare again"  |
| `old_vals` empty                       | "old_vals must contain at least one value layer"                      |
| `old_vals` layer null                  | "old_vals layers must be non-null"                                    |
| value layer shape wrong                | "value layer must have shape (1, 1, n)"                               |

The `dt` check is bitwise: callers must compute `dt` once and reuse it across the loop —
re-deriving it per step (e.g. `t·(n+1)/N − t·n/N`) differs by round-off and trips the
check spuriously.

### Worked example — rewritten `european_fd` happy path

Seed for `dal-cpp/examples/european_fd/european_fd.cpp` (one convergence-table round):

```cpp
using namespace Dal;
using namespace Dal::PDE;

const CoordinateVector_ x = MakeUniformGrid(0.0, 500.0, numX);
const Vector_<> loc = GridLocations(x);
const Vector_<CoordinateVector_> grids(1, x);

const Handle_<ScalarCoeff_> disc(NewConstCoeff(rate));
const Handle_<VectorCoeff_> mu(NewVectorCoeff([=](double s) { return (rate - div) * s; }));
const Handle_<MatrixCoeff_> var(NewMatrixCoeff([=](double s) { return vol * vol * s * s; }));

Vector_<std::shared_ptr<Cube_<>>> vals(1, std::make_shared<Cube_<>>(1, 1, numX));
for (int k = 0; k < numX; ++k)
    (*vals[0])(0, 0, k) = std::max(loc[k] - strike, 0.0);

ThetaScheme_ scheme(0.5);
const double dt = t / numT;
scheme.Prepare(dt, grids, *disc, *mu, *var);
for (int n = 0; n < numT; ++n) {
    scheme(dt, grids, vals, *disc, *mu, *var, &vals);   // in-place roll (whole-vector aliasing, FR11)
    (*vals[0])(0, 0, 0) = 0.0;
    (*vals[0])(0, 0, numX - 1) = 500.0 * std::exp(-div * (n + 1) * dt) - std::exp(-rate * (n + 1) * dt) * strike;
}

const Cube_<>& v = *vals[0];
const Vector_<> res(v.SliceBegin(0, 0), v.SliceEnd(0, 0));
std::unique_ptr<Interp1_> interp(Interp::NewCubic("cubic", loc, res, Interp::Boundary_(2, 0.0), Interp::Boundary_(2, 0.0)));
const double price = (*interp)(spot);
```

The seed is illustrative of the API, not a complete replacement: FR16's "same output
format as today" governs — in particular the convergence table's elapsed-time column
(current `european_fd.cpp` prints `Elapsed (ms)`), which the seed omits.

## Acceptance Criteria

Framework primitives:
- [ ] For each map (identity, sinh, concentrating): `map.Y(map(y, nullptr, nullptr)) == y`
      to `ASSERT_NEAR` 1e-10 across a sweep of y values spanning the domain, with
      concentrating-map parameters drawn from density ∈ [0.01, 10] (outside this range
      the round-trip error near μ can exceed the stated tolerance — critique m2; the
      test pins the tested range).
- [ ] Concentrating-map endpoint exactness (FR6): `map(0.0, nullptr, nullptr) == xLow`,
      `map(1.0, nullptr, nullptr) == xHigh`, `Y(xLow) == 0.0`, and `Y(xHigh) == 1.0`,
      all bitwise (`ASSERT_EQ`, no tolerance), across the same parameter sweep —
      including `cPoint` coincident with a bound.
- [ ] For each map: analytic `dxDy` and `d2xDy2` match central finite differences of
      `operator()` to relative tolerance appropriate to the h used (documented in the
      test), at interior sweep points.
- [ ] `NewConstCoeff` (scalar/vector/matrix): `Value` returns the constant regardless of
      `x`; `XDependence()` is all-zero and of the right shape; non-square matrix input
      throws `Dal::Exception_`.
- [ ] Callable adapters: `Value` matches the wrapped callable on sample points;
      `XDependence()` echoes the constructor flags.
- [ ] Grid builder (endpoint clause final per the M3 resolution): grids built via
      `MakeUniformGrid` and `MakeConcentratingGrid` have `locations.front() == xLow` and
      `locations.back() == xHigh` **bitwise** (`ASSERT_EQ`, no tolerance); the identity
      map on `[yLow, yHigh]` reproduces a uniform grid with interior nodes
      `yLow + i·dy` (`dy` computed once, FR7); the concentrating grid's nodes are
      strictly increasing on `[xLow, xHigh]` with spacing minimized nearest `cPoint`;
      invalid inputs (`n < 3`, reversed bounds, null map) throw.

Operator exactness:
- [ ] New Dx applied to nodal samples of f(x) = a + bx reproduces b exactly
      (`ASSERT_NEAR` 1e-12) at all interior points, on both a uniform and a concentrated
      grid.
- [ ] New Dxx applied to nodal samples of f(x) = a + bx + cx² reproduces 2c exactly
      (`ASSERT_NEAR` 1e-12) at all interior points, on both grid types; Dx on the
      quadratic reproduces b + 2cx at interior points.

ThetaScheme_ pricing (external validation, not old-code-shaped):
- [ ] Crank-Nicolson (θ = 0.5) European call (spot 100, strike 120, r 5%, d 3%, vol 15%,
      T 3y — the `european_fd` scenario) converges to the
      `Distribution::BlackOpt` analytic price as grids refine; the test asserts the error
      at a fixed fine grid is below a stated bps tolerance and decreases under refinement
      consistent with second-order behavior.
- [ ] θ = 1 (implicit) and θ = 0 (explicit, with a stable dt) also converge to the same
      analytic benchmark at coarser tolerance.
- [ ] Whole-vector aliased roll: an N-step in-place roll using the recommended idiom
      `scheme(dt, grids, vals, ..., &vals)` (same `Vector_` as both `old_vals` and
      `new_vals`) produces bitwise the same result as an out-of-place roll into a
      separate, freshly allocated vector; a roll where a `new_vals` layer is null or
      mis-shaped replaces the layer (no `Cube_::Resize` call anywhere in
      `dal-cpp/dal/math/pde/`) and still produces the correct values.
- [ ] One-off pre-deletion cross-check (performed and reported in the PR-3 description,
      *not* committed as a test): new `ThetaScheme_` and old `FD1D_` agree to ~1e-12 on
      identical grids, coefficients, dt, θ, and boundary handling — boundary rows
      identity in both the explicit and implicit operators per FR14. Grid choice (final
      per the M3 resolution): the check runs on **both** a uniform grid and a
      concentrating grid — the concentrating case is where map-vs-mesher parity is
      actually at risk. "Identical grids" recipe (constructible thanks to FR6/FR7
      pinning): build the old mesher (`Uniform1DMesher_`; `Concentrating1dMesher_` with
      `requireCPoint = false`) and the new grid (`GridLocations(Make*Grid(...))`) from
      the same `(xLow, xHigh, n[, cPoint, density])`, and first **assert the two
      location vectors are bitwise identical (0 ulp)** — expected because endpoints are
      verbatim-pinned on both sides and interior concentrating nodes evaluate the
      identical expression `μ + ρ·sinh(c₁(1−y_i) + c₂·y_i)` at bitwise-identical
      `y_i = i·dy` (uniform interiors are `xLow + i·dx` on both sides). Then roll both
      stacks on their own, verified-identical grids. Fallback if a platform quirk (e.g.
      different FP contraction across the two translation units) ever breaks the 0-ulp
      grid assertion: drive both stacks from the old mesher's exact `Locations()` — the
      new stack via a harness-local one-off `CoordinateMap_` returning the tabulated
      locations at the uniform y-nodes (the uncommitted harness already links old code
      by necessity). Expected ulp-level artifact, not a blocker: old
      `Uniform1DMesher_` stored the closed-form `dx` as the last interval's spacing
      while the new `NewDx`/`NewDxx` derive spacings from the final locations, so
      last-interior-row operator coefficients may differ by round-off — well inside
      the 1e-12 tolerance. No permanent test or code depends on old files.

Decomposition reuse:
- [ ] With time-independent coefficients and constant dt, an N-step roll (one `Prepare`
      before the loop + N `operator()` calls) performs exactly 1 factorization
      (`Decompositions()` per FR13), and rolling multiple value layers in one call
      performs no additional factorizations.
- [ ] Re-preparing (changed dt or coefficients) triggers exactly one new factorization
      per `Prepare` call.
- [ ] `Prepare` with θ = 0 (fully explicit) performs no factorization —
      `Decompositions()` stays 0.
- [ ] Rolling with a `dt`, grid, or coefficient object different from the prepared ones
      throws `Dal::Exception_` (no silent stale-state reuse).
- [ ] Probe-value guard (FR12): mutating captured coefficient state in place after
      `Prepare` (e.g. a lambda capturing a time variable by reference, then advancing
      it) and rolling without re-`Prepare` throws `Dal::Exception_` with the
      "coefficient values differ" message; destroying a prepared coefficient and
      constructing a *different-valued* replacement that happens to reuse its address
      likewise throws rather than rolling with stale samples.

Project hygiene:
- [ ] Rewritten `pde_perf` shows no regression vs merge-base baseline (best-of-N
      methodology per NFR Performance).
- [ ] All old PDE files and their tests are removed (FR19 grep clean over `dal-cpp/` and
      `docs/methodology/`); the `docs/README.md` index update listed in FR20 is made; full
      workspace build succeeds and `ctest --output-on-failure` is
      green.
- [ ] `docs/methodology/pde.md` describes only the new stack; `CHANGELOG.md` has a dated
      entry; all changed files pass `dal-reviewer`.

## Delivery Sequencing

- **PR-1** - Phases 1-2: coefficient factories/adapters, concentrating map, grid builder,
  operator builders, plus their unit tests. Old code untouched and still green.
  The M3 endpoint-pinning gate is **lifted**: FR6/FR7's endpoint and y-sampling
  semantics are final (M3 resolved — see Open Questions), so PR-1 is ready to delegate.
- **PR-2** - Phase 3: `ThetaScheme_` (+ `Prepare` mechanism with probe-value guard,
  `Decompositions()` counter), pricing, reuse, and aliasing tests. Old code still
  present; cross-check groundwork.
- **PR-3** - Phases 4-5: rewrite `european_fd`, `pde_perf`, run and report the one-off
  old-vs-new 1e-12 cross-check, delete old files/tests/build references, rewrite
  `docs/methodology/pde.md`, update the `docs/README.md` index entry listed in FR20,
  and add a `CHANGELOG.md` entry.

## Open Questions

- **`CoordinateVector_::rescalings_`** — **Resolved (API design)**: removed. No semantic
  exists anywhere in the codebase or methodology docs, the header has zero consumers, and
  a dead member in a public aggregate invites cargo-cult use. Removing it also drops
  `pde.hpp`'s only dependency on `dal/time/datetime.hpp`. Re-add with real semantics if
  time-dependent grid rescaling ever lands (see FR9).
- **Decomposition-cache mechanism under the `mutable` ban** (FR12) — **Resolved (API
  design, amended per critique B1)**: non-const `ThetaScheme_::Prepare(...)` assembles
  and factors; const `operator()` is a pure consumer that `REQUIRE`s dt/grid
  consistency, coefficient address identity, and probe-value agreement at 3 grid nodes
  (converting heap-address reuse and in-place coefficient mutation from silent wrong
  numbers into loud throws). No hidden cache or invalidation logic; reuse is
  caller-managed by placing `Prepare` outside the time loop. See the API Design section
  for the class shape and the rejected alternatives.
- **File naming and layout** — **Resolved (API design)**: as suggested — `pde.{hpp,cpp}`
  (framework + maps + coefficient factories), `pdegrid.{hpp,cpp}`,
  `pdeoperators.{hpp,cpp}`, `thetascheme.{hpp,cpp}`; tests `test_pde.cpp`,
  `test_pdegrid.cpp`, `test_pdeoperators.cpp`, `test_thetascheme.cpp`.
- **Loss of `requireCPoint`** (FR6): the old concentrating mesher could force a node
  exactly onto the concentration point; the pure-map design does not reproduce that
  without an extra mechanism. Confirm no consumer needs it (none does today); if a need
  emerges, it becomes a follow-up feature, not part of this project.
- **`new_vals` aliasing** (FR11) — **Resolved (API design, amended per critique B2)**:
  whole-vector aliasing (`new_vals == &old_vals`) and same-index layer aliasing
  permitted and tested (in-place roll is the recommended idiom); cross-index aliasing
  forbidden and unchecked; mis-shaped/null target layers replaced, never
  `Cube_::Resize`d. See the API Design section.
- **Concentrating-grid endpoint exactness and y-sampling arithmetic (critique M3)** —
  **Resolved (`dal-api-designer` follow-up, 2026-07-07): pin at both levels, each where
  it owns the information.** (i) `NewConcentratingMap` stores its bounds and is
  *endpoint-exact* — `map(0.0) == xLow`, `map(1.0) == xHigh`, `Y(xLow) == 0.0`,
  `Y(xHigh) == 1.0`, all bitwise (FR6). The endpoint constraints are the map's own
  defining conditions (they are what fix c₁/c₂), so exact realization makes the map
  *more* faithful to its definition, not grid-aware; the snap fires only on
  bitwise-exact endpoint arguments, derivatives and interior round-trips are untouched,
  and the round-trip criteria are strengthened (exact at y ∈ {0, 1}). (ii)
  `GridLocations` assigns y₀ = `yLow_` and y_{n−1} = `yHigh_` verbatim and samples the
  interior as `yLow_ + i·dy` with `dy` computed once (FR7, matching the old meshers'
  `i*dx` arithmetic). Composition gives `Make*` grids first/last nodes bitwise equal to
  the caller's `xLow`/`xHigh` — the old meshers' verbatim-pin guarantee — with no
  stored spacings to go stale (`NewDx`/`NewDxx` derive Δ from the final locations).
  Builder-only pinning was rejected as unimplementable at the `GridLocations` seam (the
  builder never sees x-bounds, and duplicating them into `CoordinateVector_` is a
  disagreement footgun); map-only pinning leaves the uniform y-sample's own endpoint
  drift unfixed; documenting the drift was rejected because it forfeits exact
  boundary-value keys and makes the old-vs-new cross-check's "identical grids"
  unconstructible. The 1e-12 cross-check now runs on both a uniform and a concentrating
  grid with a 0-ulp grid-equality precondition and a spelled-out fallback (see
  Acceptance Criteria). No new error case (the strictly-increasing `REQUIRE` covers the
  pathological snap-inversion). PR-1 is unblocked.
- **Identity-map factory quirk**: `NewIdentityMap()` currently returns a `SinhMap_`-family
  object via `NewSinhMap(1.0, 1.0)`'s degenerate branch. Keep the factory as the public
  spelling of "uniform"; whether the degenerate branch stays inside `NewSinhMap` is an
  implementation detail.
