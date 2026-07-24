# C++ Example Style Guide for Methodology Docs

This is the internal style guide every doc-writer follows when adding C++ examples to a
methodology note under `docs/methodology/`. It is not itself a methodology note. Read it before
editing any doc in this directory.

Its goal is consistency: a reader moving from `aad.md` to `yield_curve.md` to `script_engine.md`
should see the same include style, the same naming, the same way of pointing at a runnable program,
and the same rules about what may be invented.

## Scope

Apply this guide to every C++ snippet added to every file under `docs/methodology/`:

`aad.md`, `black_scholes.md`, `dates.md`, `dupire.md`, `index_parsing.md`, `interpolation.md`,
`log_discount_curve.md`, `matrix.md`, `pde.md`, `quadrature.md`, `random.md`, `script_engine.md`,
`underdetermined_search.md`, `xccy_calibration.md`, `yield_curve.md`, `yield_curve_jacobian.md`.

## Source of truth

Snippets compile against the real public headers in `dal-cpp/dal/` (the `dal_cpp` target) and,
where relevant, the public wrappers in `dal-public/src/`. The runnable programs under
`dal-cpp/examples/` are the canonical reference for include sets, namespace usage, and factory
patterns. When a snippet disagrees with a header or an example program, the header wins and the
snippet is wrong.

## Include style

- Fenced code blocks only: open with ` ```cpp ` and close with ` ``` `.
- Show a minimal-but-compilable include set at the top of each snippet. Order follows
  `.claude/rules/code-style.md`: standard/system headers first, then `<dal/...>` headers, then
  local headers. The example programs place `<dal/platform/platform.hpp>` before other `<dal/>`
  headers; match that.
- Draw includes from the real headers. Do not invent headers. The per-topic include sets observed
  in `dal-cpp/examples/` are:

| Topic                            | Minimal include set                                                                                                                                    |
|----------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------|
| AAD                              | `<dal/platform/platform.hpp>`, `<dal/math/aad/aad.hpp>`, `<dal/math/operators.hpp>`, `<dal/math/vectors.hpp>`                                          |
| Random / Sobol                   | `<dal/math/random/sobol.hpp>`, `<dal/math/random/quasirandom.hpp>`, `<dal/math/vectors.hpp>`                                                           |
| Interpolation                    | `<dal/math/interp/interp.hpp>`, `<dal/math/interp/interplinear.hpp>`                                                                                   |
| Yield curve / log-discount curve | `<dal/curve/calibration.hpp>`, `<dal/curve/curveblock.hpp>`, `<dal/curve/ycinstrument.hpp>`, `<dal/curve/yclogdf.hpp>`, `<dal/math/interp/interp.hpp>` |
| Yield-curve Jacobian             | `<dal/curve/calibration.hpp>`, `<dal/curve/curveblock.hpp>`, `<dal/math/matrix/matrixs.hpp>`, `<dal/math/matrix/matrixarithmetic.hpp>`                 |
| Cross-currency calibration       | `<dal/curve/calibration.hpp>`, `<dal/curve/curveblock.hpp>`, plus the xccy public wrapper when relevant                                                |
| Script engine                    | `<dal/platform/platform.hpp>`, `<dal/script/event.hpp>`, `<dal/script/simulation.hpp>`, `<dal/storage/globals.hpp>`                                    |
| Black / Dupire / MC / FD         | `<dal/model/blackscholes.hpp>`, `<dal/math/distribution/black.hpp>`, `<dal/storage/globals.hpp>`                                                       |
| Dates                            | `<dal/time/date.hpp>`, `<dal/time/dateincrement.hpp>`, `<dal/time/daybasis.hpp>`, `<dal/time/holidays.hpp>`, `<dal/time/periodlength.hpp>`             |
| Index parsing                    | the indice headers under `<dal/indice/...>`                                                                                                            |
| Quadrature                       | the quadrature headers under `<dal/math/...>`                                                                                                          |
| Globals / init                   | `<dal/platform/initall.hpp>` (`RegisterAll_::Init`), `<dal/storage/globals.hpp>` (`Global::Dates_::SetEvaluationDate`)                                 |

- Platform header: most `.cpp` files include `<dal/platform/platform.hpp>` first among `<dal/>`
  headers. Show it once at the top of a multi-line snippet; omit it from one-liners.
- `using` declarations: the example programs put `using namespace Dal;` (and where relevant
  `using Dal::AAD::Number_;`) at file scope after the includes. Snippets may show the same.
- Every snippet must be a plausible excerpt from a program that links against `dal_cpp`. Happy
  path only: no `try`/`catch` ceremony, no error-handling boilerplate. The library's `REQUIRE`
  is the project's precondition pattern; show it only where the example is specifically about
  validating input.

## Naming

Follow `.claude/rules/code-style.md` exactly. The rows that matter most for snippets:

| Element           | Convention                | Example                                                         |
|-------------------|---------------------------|-----------------------------------------------------------------|
| Classes/Structs   | PascalCase + trailing `_` | `Date_`, `Vector_<>`, `DiscountCurve_`, `CurveCalibrationSpec_` |
| Functions/Methods | PascalCase                | `CalibrateYieldCurve()`, `NewSobol()`, `Date::AddMonths()`      |
| Local variables   | camelCase                 | `numPaths`, `knotDates`, `today`                                |
| Template params   | single letter + `_`       | `T_`, `E_`                                                      |
| Constants/Macros  | UPPER_SNAKE_CASE          | `M_SQRT_2`, `THROW_REQUIRE`                                     |

Enum access uses the generated-class form, never a bare `enum class` literal:

```cpp
spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
spec.knotPolicy_       = CurveKnotPolicy_::Value_::INPUT;
spec.solveMode_        = CurveSolveMode_::Value_::EXACT;
```

Factory functions follow `NewXxx()` (returns a handle or `std::unique_ptr`), e.g. `NewSobol(...)`.
Calibration entry points such as `CalibrateYieldCurve(...)` are not factories and keep their
verb-first name.

## Pointers and ownership

Match the header's return type exactly. The example programs show both shapes:

- `Handle_<T_>` for shared, const ownership (`Handle_<YCInstrument_>(new Swap_(...))`).
- `std::unique_ptr<T_>` for exclusive ownership, e.g.
  `std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, ccy, instruments, knotDates));`.

Do not rewrite one form as the other to make a snippet tidier.

## Example-program reference pattern

A methodology doc that has a matching `dal-cpp/examples/<name>/` program must do both of the
following:

1. Link to the program directory with a project-relative markdown link. The canonical phrasing is:

   ```markdown
   See [`dal-cpp/examples/aad/`](../../dal-cpp/examples/aad/) for a runnable version.
   ```

   Use exactly the relative form `../../dal-cpp/examples/<name>/` from any file in
   `docs/methodology/` (the lint resolves markdown links, and directory links with no fragment
   pass). Keep the backtick path inside the link text so the project-relative path is visible.

2. Show a one-line excerpt pointer at the top of any snippet copied from or mirroring an example
   program, using a `// from dal-cpp/examples/<name>/<file>.cpp` comment:

   ```cpp
   // from dal-cpp/examples/sobol/sobol.cpp
   #include <dal/math/random/sobol.hpp>
   #include <dal/math/random/quasirandom.hpp>

   auto rsg = Dal::NewSobol(numDims, 1000);
   ```

Notes on example filenames:

- Example programs are named `<name>/<name>.cpp`, not `<name>/main.cpp`. Cite the real filename
  (e.g. `dal-cpp/examples/aad/aad.cpp`, `dal-cpp/examples/interpolate_curve/interpolate_curve.cpp`).
- The `european_mc` target's source file is misspelled on disk as
  `dal-cpp/examples/european_mc/euorpean_mc.cpp`. Cite it as it is spelled; do not "fix" it in
  prose without renaming the file.

## Comment density

Sparse, "why" not "what", per `.claude/rules/code-style.md`. A one-line `// why` pointer is the
ceiling for inline comments. Do not paste multi-paragraph derivations into a snippet; methodology
prose belongs in the doc text, not in the code block. If a snippet needs setup context, put it in
the doc paragraph immediately before the fence.

## Output illustration

When a snippet shows what a program prints, mark the output with a `// ->` comment and use
plausible, illustrative values. Do not claim exact benchmark timings, not even rounded ones; say
"elapsed in the order of milliseconds on a workstation" in prose if timing matters, and keep
numbers in code blocks clearly illustrative:

```cpp
std::cout << "PV = " << pv << "\n";
// -> PV = 8.521...
```

## Hard rules

- Never invent functions, types, enums, headers, or factory names. Every symbol must exist in
  the real public header it comes from. Open the header and match the signature exactly,
  including argument order and `const`/reference qualifiers. If unsure, grep the header rather
  than guessing.
- Use the project's enum access form `EnumName_::Value_::VALUE_NAME`. Never write
  `EnumName_::VALUE_NAME` or a C-style cast.
- `RegisterAll_::Init();` is called once at the start of every example program that touches
  globals, curves, or the script engine. Show it at the top of `main` in any snippet that sets
  the evaluation date or calibrates a curve.
- The evaluation date is set via `Global::Dates_::SetEvaluationDate(Date_(...));`. Curves and
  schedules depend on it; show the call when the snippet builds curve or schedule inputs.
- Math notation uses only macros GitHub renders. Do not use the `operatorname` macro (GitHub's
  renderer drops it); write `\mathrm{}` instead, or `\mathbb{}` for number sets. This is enforced
  by `.github/scripts/check_docs.py` for every file under `docs/`.
- No source line numbers in docs. Cite the struct, function, or file name (e.g. "the
  `CurveCalibrationSpec_` struct in `dal-cpp/dal/curve/calibration.hpp`"), never
  `calibration.hpp:70` or `dal/curve/yc.hpp:142-150`.
- No trailing whitespace on any line. The lint fails on it.
- Backtick paths in files under `docs/` are not existence-checked by the docs lint (path
  verification runs only for the agent-facing guides listed in `check_docs.py`). Copy every
  `dal-cpp/examples/<name>/` path verbatim from the mapping table below; do not retype it from
  memory.

## Canonical example-program to doc mapping

Use exactly these paths. Every directory was verified against `dal-cpp/examples/` on this branch.

| Methodology doc             | Example program(s)                                                                                                                                                                |
|-----------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `aad.md`                    | `dal-cpp/examples/aad/`                                                                                                                                                           |
| `black_scholes.md`          | `dal-cpp/examples/vanilla/`, `dal-cpp/examples/european_mc/`, `dal-cpp/examples/european_fd/`, `dal-cpp/examples/digital/`, `dal-cpp/examples/uoc/`, `dal-cpp/examples/snowball/` |
| `dupire.md`                 | `dal-cpp/examples/vanilla/`, `dal-cpp/examples/european_mc/`, `dal-cpp/examples/european_fd/`, `dal-cpp/examples/uoc/`                                                            |
| `yield_curve.md`            | `dal-cpp/examples/curve_calibration/`, `dal-cpp/examples/euribor3m_curve/`, `dal-cpp/examples/interpolate_curve/`, `dal-cpp/examples/joint_multi_curve_calibration/`              |
| `log_discount_curve.md`     | `dal-cpp/examples/curve_calibration/`, `dal-cpp/examples/interpolate_curve/`                                                                                                      |
| `interpolation.md`          | `dal-cpp/examples/interpolate_curve/`                                                                                                                                             |
| `yield_curve_jacobian.md`   | `dal-cpp/examples/yield_curve_jacobian/`                                                                                                                                          |
| `random.md`                 | `dal-cpp/examples/sobol/`                                                                                                                                                         |
| `script_engine.md`          | `dal-cpp/examples/script/`                                                                                                                                                        |
| `underdetermined_search.md` | `dal-cpp/examples/underdetermined/`                                                                                                                                               |
| `xccy_calibration.md`       | `dal-cpp/examples/xccy_curve_calibration/`, `dal-cpp/examples/xccy_mtm_calibration/`, `dal-cpp/examples/xccy_reset_pricing/`                                                      |
| `pde.md`                    | no new program needed; verify the existing C++ blocks stay consistent with this guide                                                                                             |
| `matrix.md`                 | `dal-cpp/examples/concurrency/` where relevant, else an inline snippet from `dal-cpp/dal/math/matrix/` headers                                                                    |
| `dates.md`                  | inline snippet from `dal-cpp/dal/time/` headers; no dedicated example program exists                                                                                              |
| `index_parsing.md`          | inline snippet from `dal-cpp/dal/indice/` headers; no dedicated example program exists                                                                                            |
| `quadrature.md`             | inline snippet from the real quadrature headers under `dal-cpp/dal/math/`; no dedicated example program exists                                                                    |

Corrections to the mapping that was circulated during planning (later agents: use the table above,
not the earlier draft):

- The cross-currency mark-to-market example is `dal-cpp/examples/xccy_mtm_calibration/` (MTM =
  mark-to-market), not `xccy_mitm_calibration/`.
- Example sources are `<name>/<name>.cpp`, for example `dal-cpp/examples/aad/aad.cpp`. Earlier
  drafts referenced a `main.cpp` that does not exist in this tree.

## Docs-lint notes for doc-writers

`.github/scripts/check_docs.py` runs on every `*.md` under `docs/` and fails the build on:

- Markdown links that do not resolve (including missing `#anchor` targets). Use the
  `../../dal-cpp/examples/<name>/` form for example-program links from any file in this directory.
- Pipe tables whose header, delimiter, or body rows have differing cell counts.
- Trailing whitespace on any line.
- Stale command strings anywhere in the file, including inside fenced code blocks, such as
  references to staged `bin/` test binaries. When illustrating a test invocation, point at the
  build-tree binary (e.g. `./build/Release-linux/dal-cpp/dal_cpp_tests`) and a real
  `--gtest_filter=SuiteName.*`, never the retired ones.
- The forbidden `operatorname` macro; write `\mathrm` instead.

Path-existence checks for `dal-cpp/...` tokens run only on the agent-facing guides, not on files
under `docs/`. That is why the mapping table above is authoritative: nothing else checks it for
you.
