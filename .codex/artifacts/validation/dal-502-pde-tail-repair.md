# DAL-502 diffusion-tail repair

This record covers the N5b/N7 production repair on draft PR #685. S3 still
requires affected qualification, the TREE reference decision and independent
review. The original qualification record and its FAIL/INCOMPLETE results are
preserved unchanged.

## Scope and provenance

The production baseline is merged S2 `1b8c92e8f23320b2723628fd37b399455c918e23`;
the qualification/reproducer baseline is
`14e8d92c3393a43d9c64e0537de7e9c9b6cf0281`.
[identity.json](dal502-repair/identity.json) records the implementation commit,
source hashes, exact dependencies, configuration, compiler/CPU and executable
hashes. A subsequent evidence-only commit does not change those tested sources.

Changed implementation: `dal-cpp/dal/math/pde/earlyexercisepde.cpp`.
Direct regressions: `dal-cpp/tests/math/pde/test_early_exercise_pde.cpp`.
All other changes are this repair's validation records. The two frozen risk
tests, fixtures, independent references, existing result files, public APIs,
defaults and Q/C/L caps remain byte-identical to the qualification baseline.
No shared matrix implementation or legacy `ThetaScheme_` path changed.

## Root cause and minimal design

The Thomas factors remain positive and usable. The single common binary scale
is insufficient for the span of a diffusion tail: after scaling by
`5.2093862756873862e238`, the original N5b PUT row 4737 retains only
`2.3758628777213864e-319`; its componentwise error is
`1.0397496282895078e-5`. N7 European PUT row 3196 has scaled value
`1.2499860839783538e-321` and error `0.0019801980198019802`.
Both are real significand losses during triangular substitution, rather than
bad pivots or a tolerance-calibration problem. Normalizing the already rounded
solution's residual cannot recover the missing digits.

There is also a measurement-range trap. In the first short-expiry CALL solve,
the ordinary diagnostic can report `1.82057e-16` while an independent decimal
row check finds error 1 in a truncated tail. Some residuals and denominators
have both rounded to zero. A repair triggered only by an exceeded error ratio
is therefore insufficient; the decimal audit caught and rejected that variant.

The private scratch representation now retains binary64 significands and
separate integer exponents for the triangular RHS, iterate and previous iterate.
The ordinary residual pass also detects tiny rows. Such a row completes the
same factored system with extended-exponent triangular sweeps, then computes
the original physical componentwise residual at a common per-row exponent.
Subsequent penalty iterations in that step retain the representation, signs
and iteration differences. The next assembled step resets the exponent state.
Original LCP checks still use the original unpenalized physical system.

The internal `2^-500` switch and packing headroom protect arithmetic; they are
not acceptance tolerances. Every completed solution is still checked against
the supplied linear limit. The new `1e-30` error-path control still returns
`LINEAR_SOLVE_FAILURE` with the measured maximum and no price. Normal-path
arithmetic and loops retain their original specialization.

The smallest observed saved exponent is -20213. This also exceeds the useful
range of common extended hardware types, so replacing `double` with a native
`long double` would not supply a portable repair. The significand remains
binary64 throughout.

The repair reuses the same factors, RHS and penalty mask; its extra triangular
sweeps complete the same linear-system operation. No scenario, PDE step or
penalty iteration is added. Contract work accounting remains one interior-node
charge per system/iteration, with the extra CPU work covered by the scoped
timings below. Scratch adds three integer vectors; their actual capacities and
bookkeeping enter `Scratch_::Bytes()`. They fit the existing workspace envelope,
without changing any budget or preflight allowance.

## RED and GREEN

Before editing production, both exact commands below exited 1 with
`BASE_FAILED/LINEAR_SOLVE_FAILURE`. The newly added one-layer work-limit test
was built and confirmed RED for the same cause. Linking the final new tests
against the original static library independently gives two expected failures
and preserves the strict-limit control's expected failure status.

```bash
DAL_NUM_THREADS=4 build/dal502-make/dal-cpp/dal_cpp_tests \
  --gtest_filter=EarlyExercisePdeRiskQualificationTest.TestFrozenN5bShortExpiryRequiresAvailableGamma
DAL_NUM_THREADS=4 build/dal502-make/dal-cpp/dal_cpp_tests \
  --gtest_filter=EarlyExercisePdeRiskQualificationTest.TestFrozenN7FixedSpacingDomainRefinementRetainsAllRisks
```

Final source verification uses only the affected target and direct callers:

```bash
cmake --build build/dal502-make --target dal_cpp_tests -j4
DAL_NUM_THREADS=4 build/dal502-make/dal-cpp/dal_cpp_tests \
  --gtest_filter='EarlyExercisePdeTest.*:EarlyExercisePdeRiskTest.*:EarlyExercisePlanTest.*'
```

The two exact frozen filters pass on the final executable. The direct selection
passes **51/51** tests, including three new tests for PUT/CALL tails at both
sizes, American original residuals/work exhaustion, and strict linear failure.
Both original frozen risk test bodies are unchanged. Their analytic price and
all-risk assertions retain the original hard limits.

Two additional unchanged saved inputs were replayed with the newly linked
capture driver. [Additional products](dal502-repair/additional-products.json)
retain all scenario and risk rows:

- `N7-1-domain-800`: COMPLETE, 17/17 scenarios, all five risks AVAILABLE;
  maximum linear error `2.396311588173404e-16`, risk work 1,337,277,970.
- `N7-3-domain-800`: COMPLETE, 25/25 scenarios, all seven risks AVAILABLE;
  maximum linear error `2.427772432094626e-16`, risk work 639,879,975.

These establish successful product execution. The American continuous-model
accuracy claim still requires the qualified TREE reference.

## Independent row audit

The raw archive contains `audit.py`, the inserted observation and its exact
source/executable hashes. It compiles a source copy, captures one full original
matrix/RHS and the saved significands/exponents, and stops the separate
diagnostic request at one system's work cap. This one-layer diagnostic cap is
not a change to a frozen qualification fixture. No production observer is added.

An independent 80-digit decimal Thomas solve uses the captured binary64 inputs
exactly, and independently recomputes every physical row residual. Both PUT and
CALL at N=3201 and N=16001 pass. Maximum relative solution difference is
`1.3311e-14`; maximum decimal backward error is `1.9097e-16`.
[All four results](dal502-repair/decimal-audit.json) include the extended range.
This validates the actual solution as well as the new residual measurement.

After unpacking the raw archive at the repository root:

```bash
python3 build/dal502-repair/audit.py
python3 build/dal502-repair/capture.py
```

The first command rebuilds its observation executable against the current
static library. The capture command uses the matching `product-driver`; its
complete compile command is in the identity record. Original saved inputs
and their SHA-256 values are retained in the archive.

## Scoped performance

The changed solver also serves the old owning price interface. Therefore the
existing `pde_perf --early-exercise` selector is measured: Bermudan, American,
daily projections and near-zero diffusion/CN. The unrelated European
`ThetaScheme_`, shared linear algebra, bindings and other benchmark families
are excluded by caller analysis.

Both standalone benchmark consumers use the same unchanged benchmark source,
compiler/flags and their matching Release static libraries. The baseline
library hash equals the original qualification hash. No full benchmark build
or qualification matrix is repeated. `perf.py` uses the repository parser and
round comparison: two rounds of ten alternating process pairs, each retaining
the benchmark's original ten repeats, minima, and sustained strict +4% rule.
Raw samples and all trial results are preserved in
[performance.json](dal502-repair/performance.json) and the raw archive.

| Case     | Baseline minimum | Final minimum | Round 1 | Round 2 | Verdict |
|----------|------------------|---------------|---------|---------|---------|
| Bermudan | 4.025 ms         | 4.050 ms      | +0.942% | +0.621% | PASS    |
| American | 11.249 ms        | 11.402 ms     | +1.360% | +1.305% | PASS    |
| Daily    | 37.590 ms        | 37.710 ms     | +0.391% | -0.214% | PASS    |
| NearZero | 0.618402 ms      | 0.626554 ms   | +1.327% | +1.084% | PASS    |

An intermediate implementation regressed American by 5.60%/5.96%. Moving the
range branch outside the iteration-change loop removed that regression. The
subsequent proactive tiny-row correction was independently measured again;
the table above is that final source's result. No samples or thresholds were
dropped. A compatibility relink overlapped the beginning of final round 1;
there were no concurrent numerical workloads, and round 2 independently
confirmed the result.

## Evidence reuse and remaining qualification

Unchanged independent reference self-convergence, frozen inputs, S2 interface/
ownership contracts and unrelated code gates retain their original provenance.
Old product captures remain observations of the original library; they do not
become captures of this changed executable. This repair supplies the two exact
regressions, direct contracts, the two additional failing N7 inputs and scoped
price performance. The coordinator/tester must consolidate affected numerical
qualification and the reviewer must review this production diff.

No full N1–N12 rerun, TREE layer extension, replacement oracle, Windows/XLL run,
binding release, merge or auto-merge is claimed. The TREE Gamma self-proof gap
and outstanding Codacy findings remain with their assigned follow-up. Required
CI for the pushed head is reported separately; historical S2 green checks do
not certify this changed implementation.

Raw archive identity and per-file hashes are in
[raw-archive.json](dal502-repair/raw-archive.json). It is delivered as a Multica
attachment, including commands, RED/GREEN logs, XML, observations, saved inputs,
full new product captures and all performance trials.
