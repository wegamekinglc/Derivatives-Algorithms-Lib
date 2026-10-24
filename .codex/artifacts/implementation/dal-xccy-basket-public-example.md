# DAL-430: public XCCY basket example and calling recipes

## Authority and scope

This independent author checkout starts clean at accepted master
`a5da3ecf37bf1dcbfa81097449ba04a63627139f` (PR639), parent
`f5cf78a638806ff682772603f0d744399032bb33` (PR638). Branch:
`agent/dal-implementer/b5ba0300a7c3`, base master. No unmerged PR631 or other
author tree is imported. The controlling DAL-426 note is unchanged, SHA256
`3c5ae6eed21ed0582ef0ba70811f4692efbcead2b69a184bbe5e0b5081a140ad`.
DAL-398, DAL-410, accepted basket review/critique, methodology, merged binding
source/tests and the DAL-429 implementation record remain authoritative.
DAL-396's doc-writer decision `01a16b6c-cd6a-7c48-9762-4d795def6519`
requires this publication; numerical GREEN is not its prerequisite.

Read the current DAL-430/425/396 descriptions and bounded comment threads,
AGENTS, implementer role, component boundaries, code/test conventions and Git/PR
workflow before changes. The family read showed the independent DAL-425 tester
working in its own tests/helpers/validation-record domain, with no second
public documentation writer. This author does not dispatch reviewers or merge.
The coordinator retains the focused code review and original doc-writer check.

Changed paths are only the new
`dal-public/examples/xccy_script_monte_carlo_basket/` C++ and CMake files,
the minimal parent `dal-public/examples/CMakeLists.txt` registration,
`docs/methodology/script_calibration_contract.md`, `CHANGELOG.md`,
`dal-public/README.md`, `docs/README.md`, and this active implementation record.
Product core/public/bindings, generators, old examples, independent tests,
fixtures, RNG, solvers and shared schemas are unchanged.

## Design and behavior

The example links only `DAL::public`, which exposes `DAL::cpp` transitively.
The existing `DAL_PUBLIC_BUILD_EXAMPLES` gate, platform/runtime option loop,
CTest examples label and executable install convention register it. There is
no change to library source discovery or the public component's CMake file.
Existing core directories, targets, names and purpose are retained.

Ordinary public import/model factories construct two immutable MTM instruments
with maturities 2027-11-04 and 2028-05-04, valuation 2026-10-21 and start
2027-05-04. They derive different dated mappings D=30/55. The byte-exact member
IDs `Short` and `short` deliberately demonstrate case sensitivity. Measures
are Price in Price:USD and BasisSpread in decimal spread. The explicit empty
snapshot is authoritative and sufficient for their future fixings.

The required request selects Validation, PseudoRandom/Precise, four batches of
64 paths, seed 1024 and stream `dal.mrg32-normal-paths/1`. None and AAD requests
use the same allocation; AAD selects native `fxspot:EUR/USD`. Both means and
their difference are displayed, with the AAD mean/SE and native partial/SE.
The raw partial holds model curves fixed. Each evaluation consumes 21,760
scalars; the plain/AAD pair consumes 43,520 across the two recorded executions.
This replay provides no extra independent samples. No original acceptance
capture or budget is changed or pooled with these illustrative outputs.

Output is explicitly Estimate. All 64 entries of each 8x8 C_batch and C_mean
and all 16 entries of the 4x4 output covariance are printed with complete
member/label/unit axes. Complete JSON also carries all coefficient rows, output
Jacobian, identities and exact coordinate strings. Shared ExampleFloat keeps
small nonzero differences/covariances visible. Prepare/evaluate/format-JSON-output
timings are informational. Derived-first catches read the normal failure report,
actual validated candidate availability and optional partial JSON, including the
null-partial case. The successful example injects no fault or forced failure.

The existing methodology basket section keeps its core anchor and derivation.
Its new calling subsection supplies C++/Python/Excel recipes, required inputs,
ownership, state-specific Excel getters, exact JSON families, null alternatives
and the executable commands. It removes the stale no-bindings statement but
keeps no product basket validator. The CHANGELOG and two indexes point there;
no new public guide, CLAUDE catalog entry or duplicated core derivation is added.

## RED, GREEN and verification

Commands below run from the repository root. The attached `build/dal430/run.py`
records complete argument arrays, working directories, timestamps, elapsed times,
exit codes and unabridged logs under `build/dal430/evidence/`. JUnit XML is retained.

The focused regression is the new example's actual CTest registration/run:

```bash
ctest --test-dir build/dal430/workspace-make \
  -R '^xccy_script_monte_carlo_basket$' --no-tests=error --output-on-failure
cmake --build build/dal430/workspace-make \
  --target xccy_script_monte_carlo_basket -j 8
```

Before registration, the first command exits **8 RED**, no matching test.
After adding the program and registration, rebuilding and running the same
focused CTest exits **0 GREEN**, one actual test. The final same test also runs
inside each six-example public CTest set. The output checker validates case/order,
D=30/55, exact consumption, four complete eight-column coefficient rows,
matrix dimensions/finiteness/symmetry, C_mean scaling and all 144 printed cells.
It is a publication smoke check, not an independent financial oracle.

One intermediate example run failed its own invented bit-equality assertion
between double and AAD means. IEEE arithmetic need not be bit-identical across
these evaluators. The example now checks matching allocation identity and
reports both means and their difference; no library, existing test, numerical
tolerance or statistical gate was changed. In the primary capture the differences
are about 1.78e-15 and 2.78e-17. The original failure log remains in evidence.
This is an example correction, with no financial/API design deviation.

Actual final verification, all exit 0:

- Workspace GCC14.3 Release/static/PIC/native-AAD/Eigen build of the new example,
  all six public examples and `dal_public_tests`; **9/9** selected existing
  public basket/single CTests and **6/6** public example CTests. The selected
  existing tests cover required inputs, complete JSON, preflight/null candidate,
  late failure and independent partial ownership. No test source is changed.
- Workspace public-subdirectory install succeeds, including the new executable;
  the newly installed executable runs successfully.
- Fresh public-disabled workspace configure/build of `xccy_mtm_calibration`,
  followed by its real CTest: **1/1**. No public target/source/link is present.
- Fresh standalone `dal-cpp` configure, full default build and install, with
  examples ON, unit tests/benchmarks OFF and no public project; the actual built
  and installed MTM example each run successfully. Only these core executions
  are claimed; compiling other default core examples does not count as running them.
- Fresh standalone `dal-public` against that installed core, examples ON,
  unit tests OFF: full build, **6/6** real example CTests and install succeed;
  its newly installed basket executable runs successfully.
- A new out-of-tree consumer uses `find_package(dal-public)` and `DAL::public`:
  fresh configure/build and **1/1** real CTest succeed. It compiles the example
  with copied ordinary construction/format helpers; all DAL includes/libraries
  resolve exclusively from the installation, with no repo include directories.
- A separate examples-OFF configuration omits the new target. Source/include,
  core-only and public transitive link closure, install registration and consumer
  include closure are checked against actual generated files.
- GCC14 and GCC15 syntax checks pass with the repository CI's unchanged
  Wall/Wextra/Wpedantic/Werror and existing legacy warning exclusions;
  clang-format dry-run, eight functions with CCN<=8, documentation links/anchors,
  Excel names versus generated help, Python recipe syntax and diff checks pass.

Harness mistakes are retained but not counted as passes: initial Ninja configure
failed because Ninja is absent, then Unix Makefiles was used; a bare strict
warning probe omitted CI's existing legacy exclusions; standalone core with unit
tests OFF has no root CTest inventory, so its attempted CTest exited 8 and its
actual executable was run instead. A workspace examples-label invocation also
selected unbuilt core targets after a preset reconfigure enabled core examples;
that failed invocation is retained. Restricting discovery to the public
subdirectory produced the actual six-test result above. No no-tests/unbuilt
result is reported as successful, and no assertions were suppressed in tests.

The attached source, dependency, config, runtime, object/library/executable
manifests and compile/link commands identify each execution separately. The
complete scoped Git patch and merge base are retained. New configurations and
callers are freshly built and run; no previous binary is substituted. Unchanged
accepted core mathematics, single APIs and binding acceptance are background
evidence only, not newly executed platform or numerical tests.

## Performance and acceptance limits

The changed C++ path is an example caller/output layer; CMake changes registration,
and documentation changes describe existing APIs. Scope therefore selects the
tiny caller, existing public failure controls and component/install closure.
No kernel, estimator, solver or generator change justifies long MC, eighteen
old reports, six basket profiles, full platform/performance/install matrices or
enum regeneration. None was rerun. Static binding recipe checks do not claim
new Python/native Windows/XLL/workbook/32-bit execution.

One primary capture measured prepare 20.00 ms, evaluate 384.64 ms and formatting/
JSON/output 2.036 ms on the shared host. No isolated paired baseline or nine-gate
performance comparison is available: performance remains **inconclusive**.

This publication does not qualify numerical V1 or calibrated quote risk.
All original eighteen reports, six basket overall FAIL outcomes, parity/
precision/combined pass counts 288/216/214, FXsigma and five binary64 FD RED/full
eFD and unsampled gaps remain. The Precise ideal-Gaussian tail diagnostic
`6.04063e-10 > 5e-13` and `financial_bias_bound_qualified=false` remain.
M4 x4 Price 3SE `0.05917492649405597 > 0.0025 USD` and par
`0.001072359542351817 > 0.000025` remain. All original batches, models/fixtures,
identities, budgets/reservations/counts, seeds/scales/thresholds and observed
old HeldOut status are preserved. No paths/VR/RNG/estimator changes, prefix
pooling or P4 expansion are authorized. DAL-425/396 remain in_progress; this
issue's delivery awaits focused code and documentation review, with done human-owned.
