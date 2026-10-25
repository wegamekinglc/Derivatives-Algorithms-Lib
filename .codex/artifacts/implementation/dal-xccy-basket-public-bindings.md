# DAL-429: public XCCY basket bindings and owned partials

## Authority and scope

Started in a new, clean author checkout on accepted master
`7b2501603d69d4fa3ba008be1d026515ed9fe32a`, containing DAL-426 design
`197e33df` and DAL-424 M5 `ccec714f`. Branch:
`agent/dal-implementer/6deec7486ee8`. The controlling basket note has SHA256
`3c5ae6eed21ed0582ef0ba70811f4692efbcead2b69a184bbe5e0b5081a140ad`;
DAL-410 remains
`c2ff68e1a696b620682b896ca9b292ec6d577bf92cadb3597991b427267733d5`.
DAL-398, the methodology, accepted review/critique conclusions, M5/core
implementation records and current component/style/test contracts apply.

This is the authorized additive facade/binding window. No core, old single
API/JSON/validator/translator, generic dispatch, shared schema, example, independent
acceptance file/helper/fixture/record, public guide or CHANGELOG is changed.
DAL-428 owns the two shared documents; DAL-425 retains independent V1.
No unmerged sibling source or author tree was imported. The delivery is one
base-master PR for coordinator review and native CI acceptance, without self-merge
or downstream dispatch.

During verification, master accepted DAL-428's documentation-only PR638 as
`f5cf78a6`. The author branch fast-forwarded to that accepted master without
conflict or history rewrite. The incoming delta is exactly CHANGELOG and the
methodology guide; those upstream bytes are retained unchanged and are excluded
from this PR's implementation delta. Product sources and configurations are
unchanged by this integration.

## Design and changed paths

`dal-public/src/xccymontecarlobasket.hpp/.cpp` adds the required request,
prepared facade, Prepare/Evaluate and separate complete/partial JSON dumps.
The public member and complete result reuse the owning core types; the facade
adds no estimator or mutable ledger. Allocation inspection is a const view
whose C++ lifetime is the prepared owner. Returned complete results and shared
partials own their records independently. Python allocation inspection copies
the value instead of borrowing this view.

Phase, sampling and a nonnull explicit snapshot are required. An empty typed
snapshot is valid. Context V, pair, collateral and routes come from the explicit
immutable model. The existing PseudoRandom, Precise, zero smoothing, no bridge,
None/AAD, native-label and phase-capacity rules are retained. All member admission,
geometry/native-map preflight, allocation, execution, covariance and candidate
validation delegate to the accepted core. The facade has no exception-rewriting
catch; a derived core error crosses it unchanged, retaining the identical report
and partial owner. Early facade input errors remain ordinary owning calibration
errors with no partial.

Complete JSON uses `dal.xccy-gaussian-public-basket-result/1`; partial JSON uses
`dal.xccy-gaussian-mc-basket-partial/1`. Both retain actual header, allocation,
member order, source/binding/context/mapping/native-map identities, source uses,
dated slots, native axes/units and exact integer strings. Complete JSON publishes
all coefficient rows, C_batch including cross blocks, C_mean, output Jacobian,
output covariance and qualified member summaries. None has null gradient fields.
A failed partial publishes only committed rows and actual progress/consumption;
unfinished member summaries, joint covariance/SE and numerical acceptance remain
null. Planned reservations never become consumed spans. Report `/1` is nested
without changing its existing field/number conventions.

`dal-python/src/bindings/xccymontecarlobasket.cpp` adds explicit required
constructor signatures, strict nested input conversion, read-only record
projections, Prepare and prepared.evaluate. Inputs are copied before releasing
the GIL. Local derived-first/base catches run after GIL restoration and raise
`XccyBasketCalibrationError(CalibrationError)`, preserving the original base
properties and adding read-only partial. Candidate remains the actual typed
validated model or None. No global translator or old Candidate helper changes.
Nested getters copy containers/records or share normal immutable owners.
Observation uses, slots, source locations and frozen context are also available
as owned read-only projections. The explicit Python source list and one initializer
after M5 in `calibrationinstrument.cpp` are the only shared registration seams.

`dal-excel/src/__xccymontecarlobasket.hpp/.cpp` adds the complete three-column
member factory, required phase/sampling/snapshot request, typed prepared/result/
partial/outcome session values and all required getters. Prepare and Evaluate
are separate. Prepared, Estimate and Failed have mutually exclusive prepared,
result and partial alternatives. Failed candidates come only from the core error;
there is no fallback to the prepared model. Independent partial wrappers survive
outcome destruction. Absent getters raise owning Input errors at the exact
`/outcome/prepared`, `/outcome/result`, `/outcome/model` or `/outcome/partial` path.
Session Write retains `CalibrationPersistenceUnsupported`.

The Excel raw tables reuse M5's complete-dimension validation/conversion before
coercion. Phase has a new local scalar converter that validates raw type before
ToCell. Unknown/normalized duplicate headers/settings, wrong handles, booleans,
numeric types and original cell locations are preserved. The new outcome envelope
is `dal.xccy-gaussian-public-basket-outcome/1`; absent alternatives are null and
existing diagnostic chunking preserves the exported content. Only this new
domain's Machinist blocks and 24 corresponding inc/htm outputs are added.

Focused tests are exclusively in each owning component's new
`test_xccy_monte_carlo_basket` files, a separate Excel raw test and a Python
native test bridge. Faults use the accepted private core TestAccess seam or the
existing high-annuity-floor failure. They add no product fault setting, worker
callback or global error store. Test-only Excel catch access is compiled only
under the existing test definitions. No financial/API design deviation was needed.

## RED, GREEN and evidence conventions

The attached evidence contains exact command arrays, times, working directories,
exit codes, logs/XML, generated probe sources and artifact/source/config hashes.
Commands below run from the repository root; `build/dal429/run.py` records them.

- Public RED: focused syntax compile exited 1 on the absent new public header.
  The first minimal linked parity/JSON test then passed, exit 0. Subsequent
  focused public tests cover strict inputs, phase/order/duplicate economics,
  numeric refresh, rejected geometry and actual/synthetic owned failures.
- Python RED: after building the actual existing module, the new callable test
  failed with AttributeError for absent `XccyMcBasketMember_`, exit 1. The new
  registration makes the same test pass. Required omissions name the missing
  parameter in TypeError; typed invalid/null cases preserve owning errors.
- Excel RED: focused syntax compile exited 1 on the absent new basket header.
  The new typed implementation passes complete Prepare/Evaluate and ownership
  tests. The generated raw negative control disables full-range validation and
  truncates conversion only in the private probe source; its new error-cell
  test must fail. Production sources and assertions remain intact.
- A strict GCC14 warning check caught a false positive lifetime warning caused
  by a temporary String_ diagnostic argument alongside a returned reference.
  The new helper now takes the literal field as const char*, passing the unchanged
  repository warning policy; no warning suppression was added.

Initial missing Python import paths, native bridge include/conversion mistakes,
one verification attempted during module relinking, generator markup placement
and portable return-adapter compilation errors are retained separately as harness/
authoring failures. They are not product RED evidence. Tests were not weakened.
Only completed rebuilds and matching final artifacts qualify final verification.

## Verification and component closure

The primary Release workspace uses GCC14.3, C++17, native AAD, Eigen, static
PIC libraries, native architecture disabled, real tests enabled, Python and
portable Excel. Existing CONFIGURE_DEPENDS globs discover new public/Excel files;
their CMake files need no edits. Python explicitly links DAL::public, which
transitively links DAL::cpp. No reverse dependency or core example move is added.

Primary commands:

```bash
cmake --preset=Release-linux -S . -B build/dal429/workspace \
  -DCMAKE_CXX_COMPILER=g++-14 -DDAL_BUILD_PYTHON=ON \
  -DDAL_CPP_BUILD_EXAMPLES=ON -DDAL_PUBLIC_BUILD_EXAMPLES=OFF \
  -DCMAKE_INSTALL_PREFIX=build/dal429/stage
cmake --build build/dal429/workspace --target dal_generate -j 4
cmake --build build/dal429/workspace \
  --target dal_public_tests _dal dal_excel_portable_tests -j 8
build/dal429/workspace/dal-public/dal_public_tests \
  --gtest_filter='PublicXccyMonteCarloBasketTest.*:PublicXccyMonteCarloTest.*'
build/dal429/workspace/dal-excel/dal_excel_portable_tests \
  --gtest_filter='ExcelXccyMonteCarloBasketTest.*:ExcelXccyMonteCarloTest.*'
env PYTHONPATH=build/dal429/workspace/dal-python python3 -m pytest \
  dal-python/tests/test_xccy_monte_carlo_basket.py -q
python3 build/dal429/generated_probe.py final
```

The public suite has six new cases and three existing single controls. Portable
Excel has six new cases and seven existing single controls. Python has fourteen
new cases, with three selected existing translator/sampling/validation controls.
No skip or omitted target is counted as a pass. Direct core/public parity uses
each caller's actual input; no claim of a shared cross-language archive fixture
or independent financial oracle is made.

The two native raw test bodies run through the actual generated includes,
verbatim source-extracted raw validators/converters and rebuilt real Excel factory/
runtime objects using a Linux return/storage ABI adapter. Adapter precedent is
the accepted M5 R1 attachment `01a169da-51e1-76fe-9241-cb735cf1c90a`, ZIP SHA256
`231512d6b6b86181e219e4896b6d2c69dae41d96360d9626ce1b7a09dbd2df0f`.
This is Linux portable/ELF evidence, not MSVC, native XLL, 32-bit or workbook
execution. The new native raw cases retain DAL-before-Windows header order and
both process/XLL initialization calls; exact-head Windows CI remains required.

Closure additionally checks a public-disabled workspace and the existing core
`xccy_mtm_calibration` CTest; standalone core build/tests/install; standalone
public build/CTest/install against that core package; and an installed-only basket
consumer. The consumer executes Prepare/Evaluate/serialize with ordinary owning
model/instrument inputs and links only DAL::public. Test inventories, compile/link
commands, caches and actual binaries are retained. No no-tests configuration or
partial target is represented as passing tests.

Static checks cover GCC14/15 using the repository's unchanged Wall/Wextra/
Wpedantic/Werror policy, CCN<=8 in new production files, formatting, generated
drift, documentation and diff integrity. Final counts, identities and command
results are recorded in the evidence package and delivery comment.

Final local results (all corresponding commands exit 0): **22/22** discovered
workspace CTests; **17/17** Python cases (14 new plus three existing controls);
**2/2** generated-entry Linux probe bodies; **4/4** selected standalone core
cases plus the separately executed **1/1** old mixed-mapping/overlap/CRN guard;
**1/1** public-disabled core example; **6/6** standalone public basket CTests;
and **1/1** installed-only consumer. Core/public configure, complete target builds
and installs succeed. The first core filter contained one nonexistent spelling;
it actually ran four cases, and the intended old guard was subsequently run by
its exact name. No nonexistent case is counted. The range mutant builds with
exit 0 and its raw regression exits 1 as required; the second raw control passes.

GCC14 and GCC15 checks pass on all three new production translation units,
with the original CI warning policy. CCN<=8, formatting, generated drift, Markdown
and diff checks pass. The source/control manifest has 1,560 entries, excluding
generated Python bytecode. Final production hashes match the pre-execution
snapshot. The dedicated public test was subsequently strengthened to inspect
every serialized covariance/Jacobian entry, label and unit, then both public
targets were rebuilt and their CTests rerun; the final manifest captures that
delivered test source. The provenance capture records 1,395 actual dependency/
runtime files, build caches, compile/link settings and 19 library/object/caller
artifacts; expanded packaging also preserves the raw commands and support sources.

Workspace, public-disabled, standalone and installed core archives are byte-identical,
SHA256 `3c7979d410a3daac7fc2d889ec46ea2a500740fc600bc85224b5fcff38134745`.
Workspace public archive:
`b2af8e9d581039d6dfd796b0a87021da3ed106d4c33349820e176cb0037fcf47`.
The actual Python module:
`abf8b8c57371c8c2eada09fa6e88296041ab8599e83de0b2d48845c09ddccf58`.
Standalone public uses its separate default static-library configuration and
system GTest 1.16 rather than the workspace's submodule GTest; it has its own
executed evidence and archive identity. No binary equivalence is assumed there.

## Reuse, numerical and performance limits

Core source, native mathematics, RNG/allocation, single APIs and their fixtures
are byte-identical to accepted master. Their accepted mathematical/coordinate
review remains applicable; none of the old long-run outputs is claimed as a new
execution. New callers and artifacts are actually rebuilt and run. Different
build paths/configurations/binaries have separate recorded identities. Only the
small directly affected core controls are repeated for the standalone closure.

Changes map to public Prepare/Evaluate/serialization, Python GIL/conversion/
ownership, Excel factories/handles/generated calls and install closure. No hot
kernel or solver changes require the formal nine-benchmark gate. Tiny new-caller
prepare/evaluate/serialize timings are informational on a shared host, without
an isolated paired baseline; performance remains inconclusive. No old full-repo,
20/46-case matrix, long MC, 40-axis, statistical or performance matrix is rerun.

The installed two-member, 2x3 AAD caller consumed 360 scalars and exported a
33,385-byte complete JSON. One informational run measured preparation about
10.21 ms, evaluation 9.57 ms and serialization 1.18 ms. These are caller smoke
observations on the shared host, not a speedup or formal performance verdict.

This is a callable Estimate checkpoint, not numerical V1 or quote risk acceptance.
All original eighteen reports, precision/combined/numerics failures, FX-sigma and
five binary64 FD REDs/full eFD gaps, budgets/reservations/scales/thresholds and
source/config/binary identities remain. Frozen M4 x4 Price 3SE
0.05917492649405597 exceeds 0.0025 USD; par 0.001072359542351817 exceeds 0.000025.
No paths/VR, fixture/model/estimator, threshold change or prefix pooling is used.
Old HeldOut was observed; this family does not create an untouched holdout.
DAL-425/396 remain in_progress pending real full capability and independent
evidence; done remains human-owned.
