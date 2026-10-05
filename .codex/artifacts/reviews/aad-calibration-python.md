# F01 common calibration Python review

Verdict: Comment Only. No unresolved local findings. Full local correctness,
installed-package parity and incremental performance verification pass. The
Python publication head still requires its own CI; accepted common C++ head
`12b3d7d1a9e43ac008c257c24b6365e424b458e5` passes all 35 checks.

## Findings and design

Read the complete new binding and tests, the config overload/getter changes,
registration/CMake changes, public common header/implementation, controlling
spec/API/critique, current-state guides and performance protocol/raw results.
No native math, inverse, recording, currency, path or report policy changes.

Source, seed and result classes expose readonly owning projections. Variant
sources use explicit copied pybind projections; matrices detach on every getter.
Shared seed registration avoids duplicate wrappers. Required arguments precede
optional direct input; direct is keyword-only. Common values support copying
and explicitly reject pickle/native archive serialization.

Factories accept typed matrices or rectangular lists/tuples. Raw cells reject
Boolean/enum/string coercion, nonfinite doubles and unrepresentable integers.
Dimensions are checked against the validated native boundary before allocation;
row/column context survives raw and native validation. The old generic matrix
constructor's normalization is unchanged. Mapping copies checked source/seed/
direct arguments before releasing the GIL, calls only the existing native
provider, and reacquires the GIL on both result and exception paths. Frozen
IVS inputs do not invoke or retain Python callbacks during mapping.

The old two-argument provenance config constructor remains first and uses its
original assembly body. A separate keyword-only overload adds strict bool
capture; omitted/explicit false keep native defaults. Canonical record projection
returns exact UTF-8 content without parsing or modifying fingerprints/inverse.

## Verification

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.
All filenames below have prefix `aad-calibration-python-`.

- Keep common/capture/matrix RED logs before implementation and their build/
  GREEN logs. No original test tolerance, quote step, path count or CI policy
  changes. Extended suites pass 50 then 62 cases in `extended-{01,02}.log`.
- The 62 cases cover all four curve providers and ANALYTIC/BUMPED, plain/layered
  generic axes, canonical hash bytes, owning source lifetime, typed/raw seeds,
  detached getters, copy/readonly/serialization behavior, complete case-sensitive
  curve identity, Dupire quote-only direct identity, missing/unavailable sources,
  coercion/shape/domain errors, zero/direct terms, finite-input overflow and
  recovery. Actual single and plain-generic trade gradients match legacy quote
  risk exactly. Manual supplied-gradient maps are binding projection checks;
  coupled/layered actual pricing and independent recalibration remain covered
  by the accepted native tests and complete existing language suite.
- Python callbacks can be collected after freezing. Twenty-four concurrent
  mappings preserve both domains' results. A real larger Dupire mapping allows
  the Python heartbeat to progress with no native test barrier enabled.
- Workspace OFF full Python passes 919 in `off-full-01.log`.
  Fresh standalone builds against frozen installed OFF/combined public libraries
  each pass 918 with one workspace-only opaque fixture skipped in
  `{off,combined}-standalone-full-01.log`. That skipped fixture is exercised
  by the workspace suite; no new common case skips.
- Workspace and both standalone modules match the separately linked installed
  C++ consumer exactly: 48 numeric cells and three metadata rows per module,
  in `{joint,off-standalone,combined-standalone}-parity-01.json`. The consumer
  covers frozen Dupire and plain/layered generic curves.
- Actual CPython 3.9.25 compiles all 111 governed sources in `syntax-01.log`.
  Binding and test CCN-eight checks pass (8 C++ / 30 Python functions), formats
  and patch whitespace pass; documentation checks cover 111 Markdown files.
- Both native archives and all nine accepted C++ gate executables remain
  byte-identical. The [performance report](../performance/aad-calibration-python.md)
  retains every old-entry row, raw processes, fixed protocol and new-entry costs.

## Remaining acceptance

Exact Python publication-head Linux/MSVC/ARM/wheel/runtime CI remains required;
earlier C++ CI does not accept changed Python bindings. Excel capture/common
wrappers and F01 request/budget integration remain required. P01's production
MC verdict and all remaining whole-plan stages stay open. No user decision is needed.
