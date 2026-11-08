# PDE risk qualification tools

This driver executes the frozen DAL-497 fixtures. It captures failed and
unavailable records as evidence. It never raises a product budget or changes
a numerical tolerance. The qualification outcome is separate from process
exit status: a successful capture can contain FAIL or INCOMPLETE.

From the repository root, initialize the pinned submodules, then build core:

```bash
git submodule update --init --recursive
cmake --preset Release-linux -S . -B build/dal502-make \
  -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_PYTHON=OFF -DDAL_BUILD_EXCEL=OFF \
  -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DDAL_CPP_BUILD_BENCHMARKS=OFF
cmake --build build/dal502-make --target dal_cpp_tests -j 6
g++ -std=c++17 -O3 -DNDEBUG -I dal-cpp \
  -I dal-cpp/externals/rapidjson/include \
  dal-cpp/test-support/pde-risk-qualification/driver.cpp \
  build/dal502-make/dal-cpp/libdal_cpp.a -pthread -o build/dal502-driver
```

The scripts use Python's standard library. Run each command to completion:

```bash
python3 dal-cpp/test-support/pde-risk-qualification/qualify.py product \
  --driver build/dal502-driver --output build/dal502-evidence --jobs 4
python3 dal-cpp/test-support/pde-risk-qualification/qualify.py reference \
  --driver build/dal502-driver --output build/dal502-evidence \
  --select N1,N2,N3,N4,N5b,N10d,N11 --jobs 4
python3 dal-cpp/test-support/pde-risk-qualification/observe.py \
  --build build/dal502-make --evidence build/dal502-evidence
python3 dal-cpp/test-support/pde-risk-qualification/supplement.py \
  --driver build/dal502-driver --output build/dal502-evidence
python3 dal-cpp/test-support/pde-risk-qualification/qualify.py report \
  --driver build/dal502-driver --output build/dal502-evidence
python3 dal-cpp/test-support/pde-risk-qualification/audit.py build/dal502-evidence
DAL_NUM_THREADS=4 build/dal502-make/dal-cpp/dal_cpp_tests \
  --gtest_filter='EarlyExercisePdeRiskQualificationTest.*'
```

Use a fresh output directory for a changed source or executable. Reference
results already present in `references.json` are reused within an unchanged
run; remove that file or use a fresh directory to recompute them. `--select`
accepts comma-separated fixture ID prefixes for targeted reproduction.

Each original input, full owning output and reference node price is saved
under its fixture ID. The output includes original/resolved settings, complete
plan geometry, every risk/stencil/scenario, original residual maxima, base
step diagnostics and work/capacity counters. `summary.json` contains numerical
comparisons; `audits.json` contains policy and separated-refinement checks.
Read these together: a smooth estimate can pass a hard error limit while its
directional sequence fails, and a contract-only case requires its audit.
Reference self-qualification uses strict quarter limits; product comparisons
use inclusive hard limits. A failed oracle never supplies a product PASS.

`reference.hpp` has no DAL dependency. It implements scalar erfc/exp BS, paired
CRR and daily-anchored log-space implicit stepping, with Thomas elimination
for Bermudan and original-residual PSOR for American. Log-space interpolation
uses three neighboring spot nodes. The fixed reference levels include their
specified domain changes; product space/time/domain/penalty/bump sequences
are recorded separately. Scalar Python finite differences check analytic
formulas independently. Core tests also check reference European prices.

`observe.py` adds an observation statement to a build-directory copy of the
production rollback. It asserts that removing the observation exactly restores
the source, links that object ahead of the unchanged library, and checks that
the complete N9a result equals the ordinary executable's result. It independently
assembles all 40 boundary-eliminated systems, enumerates 32 active sets per
system and recomputes final iteration change, LCP and linear residuals.
It does not edit the production source or add a public API.

`supplement.py` inspects scenario exercise regions by replaying the fixed plan,
checks the prescribed boundary bracket midpoint and offsets, and audits
individual carry partial sums against parallel bumps. Its LOG-LCP parallel
reference uses the same frozen three levels and an extra bump halving.
These diagnostic replays are distinct from the risk evaluator's own reported
solve/work counters and are not performance measurements.
