# dal-public Spec Builder API-Shape Deduplication -- Design

> **Artifact status: implemented history.** The accepted design shipped as the
> brace-initialized `Build()` implementations and shared `CurveSolverOptions_`
> vocabulary in `dal-public/src/curvespec.hpp`, `dal-public/src/curvespec.cpp`,
> `dal-public/src/xccycalibration.hpp`, and `dal-public/src/xccycalibration.cpp`.
> The analysis below is retained as design history, not pending work.

## Source

- User request on 2026-06-27: design (do not implement) two API-shape duplications in the
  `dal-public` spec builders, with backward-compatibility analysis and a recommendation.
- Read-only sweep of `dal-public/src/`, `dal-public/tests/`, `dal-python/src/`,
  `dal-python/tests/`, `dal-python/examples/`, `dal-excel/src/`, `dal-excel/auto/`,
  `dal-cpp/examples/`, `dal-cpp/tests/`, and `docs/` on 2026-06-27.

## Problem Statement

Two mechanical duplications sit in the public builder layer:

1. **`Build()` field-by-field copy** -- `CurveCalibrationSpecBuilder_::Build()`
   (`dal-public/src/curvespec.cpp`) and `CrossCurrencyCalibrationSpecBuilder_::Build()`
   (`dal-public/src/xccycalibration.cpp`) both default-construct a fresh `Spec_` and copy
   `this->field_` into it member-by-member. New `Spec_` fields must be added in three places
   (struct, builder, `Build()`); forgetting the `Build()` line silently drops the field.

2. **Shared solver-option fields** -- `CurveCalibrationSpecBuilder_`
   (`dal-public/src/curvespec.hpp:33-39`) and `CrossCurrencyCalibrationSpecBuilder_`
   (`dal-public/src/xccycalibration.hpp:21-27`) both re-declare `smoothingWeight_`,
   `tolerance_`, `fitTolerance_`, `initialGuess_`, `maxEvaluations_`, `maxRestarts_`,
   `solveMode_` verbatim. Defaults mostly agree; `tolerance_` deliberately differs
   (`1e-8` single-curve vs `1e-10` xccy), and `initialGuess_` differs (`0.05` vs `0.0`).

The same two patterns are duplicated one layer deeper in the `Spec_` structs themselves
(`dal-cpp/dal/curve/calibration.hpp:71-95` and `dal-cpp/dal/curve/xccycalibration.hpp:69-85`),
so any dedup that touches the builder has a natural twin in the underlying `Spec_`.

## Goals

- Reduce the maintenance hazard where adding a `Spec_` field requires touching three sites.
- Collapse the duplicated solver-option field list into one shared definition.
- Do not break existing callers in `dal-public/tests/`, `dal-python/`, `dal-excel/`,
  `dal-cpp/examples/`, or `dal-cpp/tests/`.
- Do not invent new vocabulary that contradicts `docs/methodology/yield_curve.md` or
  `docs/methodology/xccy_calibration.md`.

## Non-Goals

- Redesigning the internal `dal-cpp/dal/curve/` API (architect's call).
- Renaming the underlying `CurveCalibrationSpec_` / `CrossCurrencyCalibrationSpec_` types.
- Touching the Excel public-function names (`CALIBRATE.SINGLECURVE`, `CALIBRATE.XCCYMARKET`)
  or their `(today, ccy, instruments, knotDates, settings)` positional shape.
- Restructuring `dal-python/src/dal/api.py`'s `calibrate_curve()` helper signature.

## Call-Site Survey (load-bearing for the compatibility analysis)

There are ~28 distinct builder construction sites across the repo. Every one of them
default-constructs then assigns public fields. **No caller uses aggregate init
(`Builder_{...}`)** and **no caller uses fluent chaining**. `.Build()` is always a trailing
statement; the returned `Spec_` is consumed immediately by `CalibrateSingleCurve` /
`CalibrateXccyMarket` / `CalibrateMultiCurveBundle`, or pushed into
`MultiCurveCalibrationSpec_::stages_`.

### Per-surface exposure

| Surface                     | Notices field reshuffle?    | Why                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
|-----------------------------|-----------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `dal-public/tests`          | No                          | All callers use `builder.field_ = x;` then `builder.Build()`. Field names unchanged -> tests compile and pass.                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| `dal-excel/src`             | No                          | Glue (`__curvespec.cpp`, `__xccycalibration.cpp`) default-constructs the builder, applies a `(key,value)` settings matrix via `Apply*Settings` dispatch, calls `.Build()`, and consumes the opaque `Spec_`. The Excel UDF signature is flat positional + settings matrix; the builder layout is invisible to Excel.                                                                                                                                                                                                                                                         |
| `dal-python`                | **Yes (compile + runtime)** | `dal-python/src/bindings/curve.cpp` pins every solver field by member-pointer: `def_readwrite("tolerance_", &CurveCalibrationSpecBuilder_::tolerance_)` etc. (lines 269-274, 440-445). Nesting these into a sub-struct makes every `&Builder_::tolerance_` fail to compile. `dal-python/src/dal/api.py` also sets `spec.tolerance_ = 1e-8` by name (lines 47-58); if the bindings compiled but exposed `solver_.tolerance_` instead, `setattr` would silently create a phantom instance attribute and the real field would keep its default -- a runtime behavioural break. |
| `dal-cpp` (tests, examples) | No                          | dal-cpp bypasses the builders entirely and populates `Spec_` structs directly (default-construct then assign, same idiom). The builders are a `dal-public` + bindings layer over the top.                                                                                                                                                                                                                                                                                                                                                                                   |

The `Spec_` result types (`CurveCalibrationSpec_`, `CrossCurrencyCalibrationSpec_`) are
**opaque** in both binding layers (registered as bare `py::class_` with no exposed members,
curve.cpp:33-34). Reshuffling the `Spec_`'s internal layout is therefore invisible to every
binding -- only the builder's field shape is observable.

### Already-extant precedent: `CurveCalibrationOptions_`

The codebase already separates "what to calibrate" (the `Spec_`) from "how to solve"
(`CurveCalibrationOptions_`, `dal-cpp/dal/curve/calibration.hpp:101-103`). The split is
documented in `dal-cpp/dal/curve/calibration.hpp:97-100` and reinforced by
`dal-cpp/tests/curve/test_curve_jacobian_mode.cpp:40`. Today's `CurveCalibrationOptions_`
only holds `jacobianMode_`; the solver-tuning knobs (`tolerance_`, `maxEvaluations_`, etc.)
live inside the `Spec_`. A future direction (out of scope here) could migrate those knobs
into `CurveCalibrationOptions_`, but doing so as part of this dedup would be a breaking
change to the `Spec_` layout that ALL dal-cpp tests populate directly.

---

## Duplication 1: `Build()` field-by-field copy

### Options assessed

**(a) Aggregate constructor on each `Spec_` taking the builder fields.**
Add `CurveCalibrationSpec_(const CurveCalibrationSpecBuilder_&)` (and the xccy twin). The
builder's `Build()` becomes a one-liner: `return CurveCalibrationSpec_(*this);`.

- Pro: `Build()` body shrinks to one line; adding a `Spec_` field only requires updating the
  constructor's member-init list, not a 20-line copy block.
- Con: Couples `dal-cpp/dal/curve/calibration.hpp` (the internal layer) to
  `dal-public/src/curvespec.hpp` (the public layer). That inverts the dependency direction
  (`dal-cpp` must not depend on `dal-public`). Would require either moving the constructor
  into a `dal-public` header as a free function, or having `dal-cpp` forward-declare the
  builder (fragile).
- Verdict: rejected on dependency-direction grounds.

**(b) Builder holds the `Spec_` directly; `Build()` is a const-ref accessor.**
Replace `CurveCalibrationSpecBuilder_::today_` etc. with `CurveCalibrationSpecBuilder_::spec_.today_`,
expose `const CurveCalibrationSpec_& Build() const { return spec_; }`.

- Pro: Zero-copy; no field-list to keep in sync; `Build()` cannot silently drop a field.
- Con: Forces every caller to write `builder.spec_.today_ = ...` instead of
  `builder.today_ = ...`. That is a breaking change to every caller in dal-public tests,
  dal-python (`def_readwrite(&Builder_::today_)`), and dal-excel (`builder.today_ = today`).
  Approximately 60+ line-level edits across the repo, plus Python binding rewrites.
- Verdict: rejected -- caller ergonomics cost exceeds the dedup benefit.

**(c) `Build()` returns brace-init of the `Spec_` from the builder, validated.**
Keep the builder field layout identical (so all callers and bindings are untouched).
Replace the 23-line `Build()` body with a single aggregate-init:

```cpp
CurveCalibrationSpec_ CurveCalibrationSpecBuilder_::Build() const {
    CurveCalibrationSpec_ spec{
        today_, ccy_, curveName_, instruments_, knotDates_,
        discountCurves_, forwardCurves_, baseCurve_, targetCollateral_,
        targetTenor_, calibrateDiscountCurve_, liborBasis_,
        smoothingWeight_, tolerance_, fitTolerance_, maxEvaluations_,
        maxRestarts_, initialGuess_, solveMode_, parameterization_,
        knotPolicy_, initialGuessPerNode_, logDfScheme_
    };
    ValidateCurveCalibrationSpec(spec);
    return spec;
}
```

- Pro: Caller- and binding-invisible. Eliminates the per-field assignment typo risk
  (`spec.foo_ = foo_` written/not-written correctly). Compiler enforces field-count
  agreement between builder and `Spec_` (too few or too many initializers is a hard error).
- Con: Aggregate init is order-dependent on the `Spec_`'s declaration order, so a future
  reorder of `Spec_` members silently re-orders the init. Mitigation: a static_assert or
  comment pinning the order, and a unit test that round-trips a sentinel value through
  each field.
- Verdict: **recommended** for Duplication 1. Pure mechanical dedup with the smallest
  possible public-surface impact (zero).

### Compatibility

Option (c) is **fully backward-compatible**:

- `dal-public/tests/test_curvespec.cpp`, `test_xccy_calibration.cpp`: unchanged, still pass.
- `dal-python/` (bindings, tests, examples, `api.py`): unchanged, still compile and pass.
- `dal-excel/src/__curvespec.cpp`, `__xccycalibration.cpp`: unchanged.
- `dal-cpp/`: untouched (it doesn't use the builders).

No deprecation, no migration. Drop-in.

---

## Duplication 2: Shared solver-option fields

### Options assessed

**(a) Extract a `CurveSolverOptions_` sub-struct embedded in both builders.**
Define a new public struct holding the seven shared fields, embed it by value in each
builder (and optionally in each `Spec_`), with per-builder default overrides where they
differ.

```cpp
namespace Dal {
    struct CurveSolverOptions_ {
        double smoothingWeight_ = 1.0;
        double tolerance_ = 1.0e-8;       // single-curve default; xccy overrides to 1e-10
        double fitTolerance_ = 1.0e-6;
        double initialGuess_ = 0.05;      // single-curve default; xccy overrides to 0.0
        int maxEvaluations_ = 200;
        int maxRestarts_ = 20;
        CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;
    };
}
```

Then:

```cpp
struct CurveCalibrationSpecBuilder_ {
    // ... calibration-target fields unchanged ...
    CurveSolverOptions_ solver_;          // single-curve defaults apply directly
    CurveCalibrationSpec_ Build() const;
};

struct CrossCurrencyCalibrationSpecBuilder_ {
    // ... calibration-target fields unchanged ...
    CurveSolverOptions_ solver_ = CurveSolverOptions_{
        1.0, 1.0e-10, 1.0e-6, 0.0, 200, 20, CurveSolveMode_::Value_::EXACT
    };
    CrossCurrencyCalibrationSpec_ Build() const;
};
```

- Pro: Single source of truth for the solver-knob list and default values. The
  single-curve-vs-xccy differences are localized to the xccy builder's `solver_`
  default-member-init.
- Con: **Breaks the Python binding at compile time**. Every
  `def_readwrite("tolerance_", &CurveCalibrationSpecBuilder_::tolerance_)` (curve.cpp:269-274)
  and the mirrored xccy lines (curve.cpp:440-445) fails to compile because
  `Builder_::tolerance_` no longer exists at that class scope. Also breaks `api.py`'s
  `spec.tolerance_ = 1e-8` at runtime (silent phantom-attribute bug). To keep Python
  source-compatible, the bindings would need accessor shims that read/write
  `b.solver_.tolerance_` behind a top-level `tolerance_` property
  (~7 shims per builder x 2 builders = 14 lambdas). The Excel glue would need updating
  too (it writes `b.tolerance_ = d;` in `ApplyDoubleSettings`), but that is internal to
  `dal-excel/src/` and not observable to Excel users.
- Verdict: rejected as-is. Caller cost too high relative to benefit.

**(b) Extract `CurveSolverOptions_` but keep flat accessor fields on the builder via
private inheritance or composition + `using` declarations.**
Same `CurveSolverOptions_` struct, but the builders expose the fields at their own scope so
existing call sites and bindings compile unchanged.

```cpp
struct CurveCalibrationSpecBuilder_ : private CurveSolverOptions_ {
    // ... calibration-target fields unchanged ...
    using CurveSolverOptions_::smoothingWeight_;
    using CurveSolverOptions_::tolerance_;
    using CurveSolverOptions_::fitTolerance_;
    using CurveSolverOptions_::initialGuess_;
    using CurveSolverOptions_::maxEvaluations_;
    using CurveSolverOptions_::maxRestarts_;
    using CurveSolverOptions_::solveMode_;
    CurveCalibrationSpec_ Build() const;
};
```

- Pro: Caller source-compatible -- `builder.tolerance_ = 1e-8` still resolves to
  `CurveSolverOptions_::tolerance_` via the `using` declaration. The Python
  `def_readwrite(&CurveCalibrationSpecBuilder_::tolerance_)` member-pointer still resolves
  (it points into the base subobject). Excel glue unchanged.
- Con: Private inheritance is an unusual idiom for this codebase; `using` declarations for
  data members are legal but uncommon and read oddly to newcomers. Defaults still need
  per-builder override (the xccy builder must re-default `tolerance_` and `initialGuess_`),
  which private inheritance doesn't directly support for data members -- the xccy builder's
  constructor body or a default-member-init of the base would have to do it. Subtle.
- Verdict: clever but fragile; the contortion to override two defaults under private
  inheritance eats most of the readability win. Rejected.

**(c) Extract `CurveSolverOptions_` as a free-standing public type, but DO NOT embed it in
the builders.** Use it only inside `Build()` as an implementation detail to centralize the
default values, while the builders continue to declare the seven flat fields with their
existing per-builder defaults.

```cpp
namespace Dal {
    // Centralizes the *names and default values* of the shared solver knobs.
    // Builders still declare flat fields (for caller-source-compatibility with
    // dal-public tests, dal-python bindings, and dal-excel glue); Build() reads
    // them and constructs the Spec_ directly.
    struct CurveSolverOptions_ {
        double smoothingWeight_ = 1.0;
        double tolerance_ = 1.0e-8;
        double fitTolerance_ = 1.0e-6;
        double initialGuess_ = 0.05;
        int maxEvaluations_ = 200;
        int maxRestarts_ = 20;
        CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;
    };
}
```

- Pro: Source-compatible everywhere. Documents the shared vocabulary in one place. Gives
  the Excel `Apply*Settings` dispatchers and the Python `api.py` helper a stable name to
  reference. Provides the seed for a future (breaking) migration where the knobs move
  properly into the builder.
- Con: Does not actually eliminate the duplicated field declarations in the two builders.
  Only eliminates the "what is the canonical set and default" duplication. The dedup is
  documentation-level, not code-level.
- Verdict: weak -- it's a vocabulary anchor without structural dedup. Useful only as
  preparation for a future breaking change.

**(d) Skip the structural dedup entirely; document the shared fields in a comment.**
Add a comment in both builder headers pointing at each other and at the underlying `Spec_`
structs, so a maintainer adding a new solver knob remembers to update all four sites (two
builders, two `Spec_`s).

- Pro: Zero risk. Zero code change.
- Con: The duplication remains; future drift is still possible.
- Verdict: acceptable fallback if the user prefers compatibility over dedup.

### Compatibility

Only options (c) and (d) are non-breaking. Option (a) breaks Python at compile time and
runtime; option (b) is source-compatible but semantically awkward.

### Recommendation

**Implement option (c) as the structural anchor, AND combine it with Duplication 1 option
(c) (brace-init in `Build()`).** Then **defer the breaking migration (option a)** to a
future major-version change. Specifically:

1. Add `CurveSolverOptions_` to `dal-public/src/curvespec.hpp` as a documented public type
   centralizing the seven knob names and their single-curve defaults. Do not embed it in
   either builder yet.
2. Rewrite both `Build()` bodies using brace-init (Duplication 1 option (c)) -- this is
   the immediate, no-risk win that makes future field additions safe.
3. In each builder header, add a one-line comment above the seven solver fields:
   `// keep in sync with CurveSolverOptions_ (curvespec.hpp) and the Spec_ struct`.
4. Leave the per-builder `tolerance_` and `initialGuess_` defaults as they are
   (`1e-8`/`0.05` for single-curve, `1e-10`/`0.0` for xccy). The xccy defaults are
   documented as deliberate divergences in `docs/methodology/xccy_calibration.md`.

This delivers the field-copy safety win immediately, establishes the vocabulary anchor for
the shared knobs, and leaves the door open for a later structural collapse without forcing
a Python-binding rewrite in this PR.

**Skip option (a) for now.** The marginal benefit (eliminating seven duplicated field
declarations across two builders) does not justify the Python-binding rewrite (14 accessor
shims, ~30 lines of `api.py` rework, and a non-trivial risk of silent runtime breakage in
`setattr`-based code paths). The structural dedup can be revisited when the project next
takes a breaking-change window.

---

## Proposed Concrete Shape (what an implementer codes after approval)

### `dal-public/src/curvespec.hpp`

Add a new public struct near the top of `namespace Dal`:

```cpp
namespace Dal {

    // Shared solver-tuning knobs for curve and cross-currency calibration.
    // Builders and Spec_ structs declare these as flat fields for source-compatibility
    // with dal-public tests, dal-python bindings (which pin fields by member-pointer),
    // and dal-excel glue. This struct centralizes the canonical names and the
    // single-curve defaults; the xccy builder/spec override tolerance_ to 1e-10 and
    // initialGuess_ to 0.0.
    struct CurveSolverOptions_ {
        double smoothingWeight_ = 1.0;
        double tolerance_ = 1.0e-8;
        double fitTolerance_ = 1.0e-6;
        double initialGuess_ = 0.05;
        int maxEvaluations_ = 200;
        int maxRestarts_ = 20;
        CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;
    };

    // ... existing CalibrationResult_, CurveCalibrationSpecBuilder_ unchanged in layout ...
}
```

In `CurveCalibrationSpecBuilder_`, add the sync-reminder comment above the solver fields
(no structural change):

```cpp
        // --- Solver-tuning knobs; keep in sync with CurveSolverOptions_ above and with
        // --- CurveCalibrationSpec_ in dal-cpp/dal/curve/calibration.hpp.
        double smoothingWeight_ = 1.0;
        double tolerance_ = 1.0e-8;
        // ... unchanged ...
```

### `dal-public/src/curvespec.cpp`

Rewrite `Build()` as brace-init (Duplication 1 option (c)):

```cpp
CurveCalibrationSpec_ CurveCalibrationSpecBuilder_::Build() const {
    CurveCalibrationSpec_ spec{
        today_, ccy_, curveName_, instruments_, knotDates_,
        discountCurves_, forwardCurves_, baseCurve_, targetCollateral_,
        targetTenor_, calibrateDiscountCurve_, liborBasis_,
        smoothingWeight_, tolerance_, fitTolerance_, maxEvaluations_,
        maxRestarts_, initialGuess_, solveMode_, parameterization_,
        knotPolicy_, initialGuessPerNode_, logDfScheme_
    };
    ValidateCurveCalibrationSpec(spec);
    return spec;
}
```

### `dal-public/src/xccycalibration.hpp`

Add the same sync-reminder comment above the xccy builder's solver fields. Optionally
`#include <dal-public/src/curvespec.hpp>` so the xccy header sees `CurveSolverOptions_`
(for the comment reference and for future structural collapse). No layout change.

### `dal-public/src/xccycalibration.cpp`

Rewrite `Build()` as brace-init:

```cpp
CrossCurrencyCalibrationSpec_ CrossCurrencyCalibrationSpecBuilder_::Build() const {
    CrossCurrencyCalibrationSpec_ spec{
        today_, basisPair_, domesticCurveBlock_, foreignCurveBlock_,
        fxSpot_, fxForwardCollateral_, instruments_, knotDates_,
        smoothingWeight_, tolerance_, fitTolerance_, initialGuess_,
        maxEvaluations_, maxRestarts_, solveMode_
    };
    return spec;
}
```

(xccy `Build()` does not currently call a validate function; leave that behavior unchanged.
The xccy calibration has no `ValidateXccyCalibrationSpec` analog today -- adding one is out
of scope for this dedup.)

### Test additions (`dal-public/tests/test_curvespec.cpp`)

Add one round-trip test per builder asserting that every builder field flows through
`Build()` into the corresponding `Spec_` member with the expected value. This guards
against the order-dependence risk of brace-init: if a future `Spec_` reorder breaks the
positional init, the round-trip test fails on the first sentinel-mismatched field.

```cpp
TEST(CurveCalibrationSpecBuilderTest, TestBuildRoundTripsEveryField) {
    CurveCalibrationSpecBuilder_ b;
    b.today_ = Date_(2026, 6, 27);
    b.ccy_ = "USD";
    b.curveName_ = "rt";
    b.tolerance_ = 1.0e-10;
    b.initialGuess_ = 0.04;
    b.maxRestarts_ = 7;
    b.knotPolicy_ = CurveKnotPolicy_::Value_::INSTRUMENTS;
    // ... set every other field to a sentinel ...
    auto spec = b.Build();
    ASSERT_EQ(spec.today_, b.today_);
    ASSERT_EQ(spec.tolerance_, b.tolerance_);
    ASSERT_EQ(spec.maxRestarts_, 7);
    ASSERT_EQ(spec.knotPolicy_, CurveKnotPolicy_::Value_::INSTRUMENTS);
    // ... assert every other field ...
}
```

Plus a `TestCurveSolverOptionsDefaults` asserting the new struct's defaults match the
single-curve builder's defaults (`1.0`, `1e-8`, `1e-6`, `0.05`, `200`, `20`, `EXACT`).

### Files touched (summary)

| File                                         | Change                                                  |
|----------------------------------------------|---------------------------------------------------------|
| `dal-public/src/curvespec.hpp`               | Add `CurveSolverOptions_`; add sync comment in builder. |
| `dal-public/src/curvespec.cpp`               | Brace-init `Build()`.                                   |
| `dal-public/src/xccycalibration.hpp`         | Add sync comment; optional include.                     |
| `dal-public/src/xccycalibration.cpp`         | Brace-init `Build()`.                                   |
| `dal-public/tests/test_curvespec.cpp`        | Add round-trip test + defaults test.                    |
| `dal-public/tests/test_xccy_calibration.cpp` | Add round-trip test.                                    |
| `dal-python/`, `dal-excel/`, `dal-cpp/`      | Unchanged.                                              |

## Risk Assessment

- **Brace-init `Build()` is order-dependent.** The round-trip tests are the mitigation.
  Without them, a future reorder of `Spec_` members would silently re-bind initializer
  values to the wrong fields as long as types are compatible. With them, the first
  mismatched sentinel fails the test.
- **`CurveSolverOptions_` is a new public type.** Adding it is non-breaking, but it
  becomes part of the ABI. Once shipped, removing it would be a breaking change. That is
  acceptable: it documents a real concept ("solver-tuning knobs") and matches vocabulary
  already used informally in the Excel `Apply*Settings` dispatchers and `api.py`'s
  `_apply_optional_setting`.
- **No deprecation needed.** Nothing is removed; no caller is forced to migrate.

## Documentation

No methodology doc needs updating: the dedup is a structural cleanup of the builder layer,
not a methodology change. The `docs/methodology/yield_curve.md` and
`docs/methodology/xccy_calibration.md` code snippets reference the `Spec_` structs
directly (which are unchanged), so they remain accurate.

A CHANGELOG.md entry is borderline: this is a refactor with no behaviour change and no
public-surface removal, which the changelog guide says to omit. Recommend **no changelog
entry** unless the team considers "new public type `CurveSolverOptions_`" to clear the
"significant capability" bar.

## Open Questions

- Should `CurveSolverOptions_` live in `dal-public/src/curvespec.hpp` (where the
  calibration-target audience is) or in `dal-cpp/dal/curve/calibration.hpp` (where the
  `Spec_` types live)? The recommendation above puts it in `dal-public` to avoid any
  `dal-cpp`-depends-on-`dal-public` inversion; the architect may prefer it elsewhere.
- Should the round-trip test cover the `Spec_` structs too, or only the builders? dal-cpp
  tests already cover `Spec_` round-trips implicitly via `MakeValidSpec()` helpers; adding
  explicit field-by-field round-trip tests for the `Spec_` types is in scope but would
  double the test additions.
- Does the team want to take the breaking migration (option a) in a near-term major
  version? If yes, this PR should land option (c) first, then a follow-up PR does the
  structural collapse with the Python-binding rewrite as a single coordinated change.
