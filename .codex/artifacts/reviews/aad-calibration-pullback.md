# F01 common C++ calibration pullback review

Verdict: Comment Only. No unresolved local findings. Full local correctness,
installed-package and incremental performance checks pass; exact publication-head
CI remains required. Python/Excel and complete F01 integration remain unfinished.

## Findings

No unresolved correctness, methodology, compatibility or style findings in this
increment. Read the complete new public implementation/header and tests, shared
mapping helper, legacy rate-pricing file, public CMake, changed current-state
guides/changelog, controlling spec/API/critique and measured performance report.
The reviewed local source is the uncommitted increment on accepted native capture
`a3833be93e64a2e427debbcf787fd99a7ff37c22`; that parent's 35 successful CI checks
accept capture only and do not accept these new common entry points.

The boundary owns a closed variant of existing immutable typed sources. Tagged
seed aliases share one checked implementation and copy numeric inputs. Results
own the same source plus calibration/direct/total matrices. No active values,
borrowed market inputs, automatic currency conversion or path/report scaling
enter the common result. All getters expose stored const passive data.

Source matching first rejects different domains or fingerprints, then compares
complete case-sensitive curve ID, axes/ranges, coordinates, state components,
bindings and exact canonical std::string bytes. Same immutable source ownership
is a safe fast path. Dupire delegates its accepted full-content comparison.
Parameter seeds require full identity. Dupire direct seeds retain the existing
ordered-quote/value identity across different bases; curve direct seeds require
the complete captured source. Direct PV adjoints are added exactly once.

Curve construction rejects unavailable sources with their original reason and
available sources without a retained record with the declared missing-record
token. Positive shape/storage/iterator bounds, exact seed shape and finite
numeric cells are checked before copying/allocating the new result. The retained
source's checked inverse is used without another solve or market lookup.
Nonfinite mapped or accumulated outputs fail before result publication.
Success/failure/success and unrelated native recordings retain their behavior.

The shared callback mapping primitive preserves the original Matrix::Multiply,
per-quote division, DV01 finite guard, error tokens and one output loop.
Legacy metadata assembly stays in its callback, with no new source validation,
common metadata allocation or extra mapping pass in the old hot path.
The two other legacy-file edits are clang-format line wraps only.
The public test fixture source remains PRIVATE to its test target; production
installed consumers link only DAL::public. No enum/markup/generated file changes
are needed in this increment.

## Open questions

No user decision is needed. The implementation uses the existing retained curve
effective inverse and conservative method/boundary labels; it does not claim a
new implicit solver, actual requested ANALYTIC mode, nonlinear selected-solution
derivative or complete market Greek. Those limits are stated in the guide.

Residual risk is exact new-head Linux/MSVC/shared-DLL/ARM wheel/sanitizer CI.
Actual local MSVC syntax checks and earlier accepted heads are insufficient for
that acceptance. Keep this review at Comment Only until the new head passes.
Python strict Boolean capture, common immutable/copy projections, Excel capture
options/handles/registrations, serialization errors, numeric budgets and market
request integration remain required. None of the full F01 boxes is closed here.

## Tests

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- Missing-header RED: `aad-calibration-pullback-common-red-01.log`.
  Keep initial missing FlatIVS test-class build failure and later setup failures
  in `common-green-build-01.log` and `extended-red-01.log` (all with the same
  `aad-calibration-pullback-` prefix). Correct only fixtures: the reused curve
  direct rule, overwritten XCCY market route and genuine nondependent cells.
  Reference gradients admit only the original structural-zero reason; all
  other node-risk failures reject. No tolerance, step or production rule changes.
- Twelve common cases pass: `aad-calibration-pullback-providers-green-01.log`.
  Coverage includes bitwise typed Dupire numeric parity and copied seeds,
  direct identity, all four providers, ANALYTIC/BUMPED with all four native
  representations and layered/plain generic sources, actual legacy signed
  portfolios, mixed representations, separate actual USD/EUR PV currency
  groups, full identity including case-only IDs, unavailable/missing content,
  malformed/nonfinite/domain inputs, zero/direct/signed seeds, overflow recovery,
  passive curve mapping inside a live native graph and Dupire nested recovery.
- Twelve independent smooth-square recalibration rows pass the originally
  declared 2e-6/1e-6/5e-7 steps and abs-or-rel 1e-6 criterion, with adjacent
  passes required. Both inverse modes and plain/layered cases pass; no square
  oracle is applied to the underdetermined PWLF chart. Existing actual full
  repricing/recalibration +/-1e-6 and +/-1e-4 N=5/10/16 tests also run in the
  broad suites and retain their published limits.
- Full OFF/combined CTest passes 2,412/2,426:
  `aad-calibration-pullback-{off,combined}-full-01.log`. OFF includes 857
  Python cases and 33 regular examples. Existing benchmark/slow labels are
  excluded as specified; performance is run independently.
- Fully instrumented ASan/UBSan passes 186 relevant cases in
  `aad-calibration-pullback-sanitized-01.log`; twelve common cases also pass
  with leak detection ON in `aad-calibration-pullback-sanitized-leaks-01.log`.
- Fresh OFF/combined installed consumers pass; sixteen numeric and three
  metadata rows are identical in `aad-calibration-pullback-installed-parity-01.json`.
  Their explicit old-prefix RED fails only on the missing new public header.
- Actual MSVC 14.51.36231/SDK 10.0.26100.0 C++17 syntax checks pass all six
  common/test/legacy OFF/combined combinations:
  `aad-calibration-pullback-msvc-syntax-01.json`. Preserve raw and decoded logs.
- Clang-format, CCN-eight, 109 documentation checks and patch whitespace pass.
  Keep initial SameCurve CCN-nine evidence; extracting SameBindings preserves
  every comparison and removes the warning without changing the threshold.

## Summary

The [performance report](../performance/aad-calibration-pullback.md) retains all
75 nine-target cases under the unchanged two-round/ten-process/4% gate; every
case passes, and 677 measured input hashes remain unchanged. Eight executables
and the provenance-constructor object are byte-identical to accepted capture.
Twenty separate informational processes retain all twelve numeric checks and
408 input hashes. These prepared-entry costs and bounded incremental acceptance
do not resolve P01's inconclusive production MC verdict.

The common C++ operation is ready for publication and exact-head CI review.
The overall AAD goal remains active; complete language/request/budget integration
and the remaining Stage B/C/D work still require implementation and acceptance.
