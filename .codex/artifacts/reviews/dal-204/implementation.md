# DAL-204 F7 implementation

Current status: the approved R1 correction passes a fresh pybind11 2.11.1 binding build and the full Python suite. Draft PR: [#375](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/375). Ready for parent acceptance and the existing independent tester, documentation decision, and mandatory reviewer sequence. Earlier results and the initial R1 scope request below remain historical evidence. No merge or final closing intent has been added.

## 2026-09-15 R1 correction after approved scope expansion

### Change and source identity

Foreign `str, enum.Enum` values now raise contextual TypeError in the constructor and setter paths for `today_fixing`, `default_index`, and `method`. A failed setter retains its previous valid value. The policy errors include both `Model` and `RequireHistorical`; all errors include the class, field, offending value, identifier, and non-enum string constraint.

- Product/test commit: `27de3544d4497a77ee2c6d67743f147bbeb65d53`; tree `23d36a169849f8cd5144b0c87ab0ded94f9392a3`.
- Base: `f2125ea96b668fc32b66061241cec67213d0ea28`; tree `c635e713783d3c7116cad22d356ad43ab8875429`. The subsequent report commit changes only this file; its final SHA/tree is in the delivery comment and `delivery-identity.json`.
- Continued `feature/dal-204-python-fix-settings` and PR #375. The old working tree's test/report diff matched the authenticated recovery patch byte for byte. It was preserved, and that patch was restored into the new clean checkout before production edits.
- Downloaded scope evidence SHA256 `2fb93b48d2acd772084caffb69cbf3cfac2c8bcfd402278d3a3d2dc2b65b0021` and production proposal SHA256 `7f1327774757b00ac2f4c9c2045f564667c6b76562e82062d5c88b7b62c4f75b` matched the approved handoff. The proposal was implemented with `apply_patch`, with only directly necessary indentation adjustment.

Changed files are exactly the approved five:

- `dal-python/src/bindings/scriptsettings.hpp`: private `SettingStringInput` rejects enums, then delegates to the existing text/NUL converter.
- `dal-python/src/bindings/script.cpp`: only the default-index constructor/setter use that helper.
- `dal-python/src/bindings/value.cpp`: today-policy text and method use that helper; the actual native policy branch remains first.
- `dal-python/tests/test_script_settings.py`: preserves the eight original R1 regressions; adds 28 legal constructor/setter controls and eight high/low shared-text pricing cases, for 161 settings tests total.
- This implementation report: current R1 result and retained history.

All `StringInput` callers were reviewed. Event text and model-binding keys/values continue to use the unchanged generic helper. Their `str`, ordinary str subclass, DAL String_, and foreign string-enum conversions produce independent expected PV100 in both layers. Existing NUL, case, numeric/date, unknown-keyword, legacy Value, and GIL/native-copy behavior remain covered. Conversion completes before any field assignment. No API/core/public/Excel/generated/build/CI/docs/CHANGELOG changes were needed. No substantive design deviation or further scope expansion was introduced.

### Fresh RED, GREEN, and verification

The eight restored regressions were rerun before the production patch: **8 failed / 117 deselected**, exit 1, all `DID NOT RAISE TypeError`. This fresh RED used the verified S5 module; its hash is `0b03aaf10280ff4b810510f06c9f421cffc92ef68a1d26e42d6fd9e3d4285c9e`. Original prior-run RED logs/XML and the recovery patch are also retained separately in the evidence input archive.

After all nine binding translation units were freshly compiled, the same focused tests were **8 passed / 117 deselected**, exit 0. Legal/compatibility controls were then added while green; no production refactor or test weakening was needed. The new extension hash is `b310233dc9f9e2bc7ade8f6591c8d4f3fd67cd8a4a102b5b58982eaa10a1cdc9`.

Actual commands are captured as argv with cwd, UTC start/end, stdout/stderr and return code in the attached `r1-evidence`. In the abbreviated commands below, `PY` is the S3 `evidence/venv313/bin/python`, `S5_BUILD` is its reviewer's `review-evidence/build-python`, `STAGE` is the S3 `build/stage/Release-linux`, and `PYBIND` is that interpreter's `pybind11/share/cmake/pybind11`. Every expanded absolute path is retained in the corresponding JSON record.

```bash
env PYTHONPATH="$S5_BUILD" "$PY" -m pytest dal-python/tests/test_script_settings.py -k foreign_string_enums -q --junitxml=../r1-evidence/focused-red.xml
cmake -S dal-python -B ../r1-build-python -DCMAKE_BUILD_TYPE=Release -DDAL_INSTALL_PREFIX="$STAGE" -DPython3_EXECUTABLE="$PY" -Dpybind11_DIR="$PYBIND"
cmake --build ../r1-build-python -j 4
"$PY" ../r1-evidence/run_python.py pytest dal-python/tests/test_script_settings.py -k foreign_string_enums -q --junitxml=../r1-evidence/focused-green.xml
"$PY" ../r1-evidence/run_python.py pytest dal-python/tests/test_script_settings.py -q --junitxml=../r1-evidence/settings-green.xml
"$PY" ../r1-evidence/run_python.py pytest dal-python/tests -q --junitxml=../r1-evidence/python-full.xml
"$PY" ../r1-evidence/run_python.py probe ../r1-review/dal-242-review-evidence/probe_foreign_enums.py
"$PY" ../r1-evidence/run_python.py probe ../r1-evidence/probe_strenum.py
"$PY" ../r1-evidence/run_python.py example
"$PY" -O ../r1-evidence/run_python.py example
cmake --build ../r1-build-consumer -j 2
ctest --test-dir ../r1-build-consumer --output-on-failure --output-junit ../r1-evidence/consumer-tests.xml
```

- Full Python: **640 passed / 1 skipped**, 9.04 seconds. The skip is the unpublished `_dal_quote_risk_test` fixture in `test_joint_quote_risk`; no skipped R1 case. This is 239 F7 cases, including all 44 added R1/control cases.
- The same full run includes **299 related cases**: settings161, FIX valuation78, script13, value21, API8, curve pricing14, resettable/snapshot4. These counts are derived from the full-run XML, not an additional test invocation.
- The unchanged reviewer probe rejects all **8** foreign enums and passes its **6** legal controls. A supplemental replay with Python 3.13 `enum.StrEnum` also rejects all eight and passes six controls. Repository tests retain Python-3.9-compatible syntax.
- The complete FIX example succeeds in both ordinary and optimized Python: PV `260.00000000000006`, d_SCALE `80.0`, original key/schema constraints active. Both runs load the fresh extension and verify identities using explicit exceptions under `-O`.
- Native proportional verification: two freshly compiled installed-package consumers pass **2/2**, covering legacy/typed public signatures and historical compiled AAD PV160/d_SCALE80. They link the verified unchanged S3 libraries. Full native CTests were not repeated for this Python-only conversion change.
- C++ clang-format, Black targeting Python3.9, and patch whitespace checks pass. Initial Black checking requested two call-layout changes, which were applied; initial commit failed for missing local Git identity, then succeeded with the same dal-implementer identity used by the earlier implementation. Neither failure was a product test failure.

### Build provenance, inherited coverage, and handoff

Fresh binding environment: Linux/WSL2 x86_64, kernel5.15.167.4, glibc2.43; GCC15.2.0, CMake4.2.3, Unix Makefiles, Release/C++17, native AADET, Python3.13.9, **pybind112.11.1**. Module: `../r1-build-python/dal/_dal.cpython-313-x86_64-linux-gnu.so`. Python's distribution reports GCC11.2 for the interpreter itself; the extension compiler is GCC15.2.

Before restoration, all 771 inherited S3/S5 source/test/build-input hashes matched the reviewed baseline. Subsequent verification confirms that only the four authorized product/test inputs differ. All 266 installed headers match current native sources. Both installed and build-tree static libraries match the S3 recorded hashes: core `b32de330ef6adbf8b90a93c49c169e83a1377a7d806b8643e8e547176f39d35e`, public `cbd032efe1dce67c5cf3d5c888cdd60a2cd788bcc5c335b0cafed20c6315ee8f`. All 31 installed pybind11 headers match pinned commit `8a099e44b3d5f85b20f05828d919d2332a8de841`. The same-process runner checks the fresh module path/hash, all 771 current input hashes, and the two Python helper files before each GREEN/probe/example invocation. Evidence includes compile/link flags and library dependencies.

S3 at `607584aaeba0596edf8e384a8137e5e518a3ddaa` remains inherited evidence for native1778, sanitizer255, and Python3.9.25/3.13.9 wheels (each596/1skip). S5 at the base above remains inherited evidence for its Python596/1skip and prior review findings. S2 CoDiPack597 at `fd9c42d07b54506366652f6559ddf1e0fff093e8` used pybind113.0.4. None of those runs is relabeled as current R1 verification. This run did not rebuild core, wheels or sanitizer modules, run alternate AAD, Windows/XLL, macOS, Python3.9–3.12, manylinux repair, or performance benchmarks. The old sanitizer run disabled host-Python leak detection.

The final handoff updates the same draft PR, records final SHA/tree and a single post-push CI snapshot, and attaches this report with fresh evidence. Independent DAL-240 testing, DAL-241 documentation/CHANGELOG decision, and DAL-242 review remain for parent-controlled serial acceptance. No final Closes, merge, successor launch, F8, or sidecar is part of this delivery.

## Historical R1 scope request: shared-helper compatibility requires two script call sites

The current DAL-239 R1 contract permits only `scriptsettings.hpp`, necessary `value.cpp`, `test_script_settings.py`, and this report. It also requires preserving existing conversions and escalating any necessary scope expansion to the parent. The safe implementation needs **two call-site substitutions in `dal-python/src/bindings/script.cpp`**, which is absent from that write scope. No production file has been edited, committed, or pushed during this run.

### Reproduction and proposed design

All `StringInput` callers were inspected. In addition to the three constrained settings fields, it converts `model_bindings` keys/values in `value.cpp` and event text in `script.cpp`. The frozen API explicitly excludes enums for `default_index`, `today_fixing`, and `method`; it describes the other text inputs as str/String_ without that exclusion.

Fresh execution against the hash-verified S5 module confirms both high and low layers currently accept `class Foreign(str, enum.Enum)` event text and model-binding keys/values. Constructor and setter binding conversion produce `{"spot": "EQ[A]"}`, and the resulting same-day FIX product prices at exactly PV=100. Ordinary str, a normal str subclass, and DAL String_ controls also produce PV=100 in both layers. A global `IsEnum` rejection in `StringInput` would reject those existing event/binding inputs as well. That collateral change is unnecessary for R1.

The proposed patch leaves `StringInput` unchanged and adds `SettingStringInput`, which rejects enums with field/identifier/constraint context before delegating to the existing full-length text and NUL checks. Only default-index constructor/setter, today-policy text, and method conversion use it. The dedicated valid native today-policy branch remains first. All settings continue to finish conversion before assignment, preserving their prior values on failure. No GIL/native-copy boundary, numeric/date check, legacy Value conversion, core, generated file, or public API changes are proposed.

The attached `proposed-production.patch` contains the complete proposed production change and passes `git apply --check`. It is a reviewable proposal, **not an applied or compiled fix**. The only additional write permission needed is the two default-index calls in `script.cpp`; event text stays on `StringInput`.

### Fresh RED and evidence identity

- Reviewed/source HEAD remains `f2125ea96b668fc32b66061241cec67213d0ea28`; tree `c635e713783d3c7116cad22d356ad43ab8875429`. Target remains `feature/dal-204-python-fix-settings` and draft PR #375. Multica created the local checkout branch `agent/dal-implementer/4154535fddff` at that exact revision; no remote branch was created.
- Downloaded S5 report and evidence hashes match the assigned `0f7b8737bede1be2ca04cbd600e3d36d82e831743fec1d87bf83a2ee8f4c47ab` and `a0a541cd7d6cc6a2e211fd3f0ecc4cb9fbe4b157d07ee2e0d4dddf4569c40ca3`. The report was read in full.
- Before adding the regression, all 771 recorded S3 source/test/build-input hashes matched this checkout. Both executing Python helper files matched it. The reused S5 extension SHA256 is `0b03aaf10280ff4b810510f06c9f421cffc92ef68a1d26e42d6fd9e3d4285c9e`; its exact module path, interpreter, platform, and hash are in `probe.stdout`.
- Runtime: Python 3.13.9, pybind11 2.11.1, Linux/WSL2; S5 build provenance is GCC 15.2, Release, AADET. This run **reuses** that S5 extension; it does not claim a fresh binding/core build or a GREEN result.
- `probe.py` freshly repeats the eight reviewer constructor/setter inputs: all eight are wrongly accepted, including MODEL-to-REQUIREHISTORICAL assignment. The six reviewer positive controls pass. Its intentional RED exit is 1. The additional eight high/low compatibility scenarios above all pass before the probe signals R1.
- Added `test_settings_reject_foreign_string_enums` to the allowed test file, with eight independently collected cases. Each requires contextual TypeError; setter cases start from a different valid value and require it to remain unchanged. The class uses Python-3.9-compatible `str, enum.Enum` syntax.
- Focused RED command: `env PYTHONPATH=<verified S5 build-python> <S3 venv313>/bin/python -m pytest dal-python/tests/test_script_settings.py -k foreign_string_enums -q --junitxml=../r1-scope-evidence/focused-red.xml`. Exact expanded command, timestamps, stderr/stdout, and exit code are in `focused-red.json`. Result: **8 failed, 117 deselected, exit 1**, each failing at `DID NOT RAISE TypeError`.
- `python3 -m black --check --target-version py39 dal-python/tests/test_script_settings.py`, `git diff --check`, and `git apply --check ../r1-scope-evidence/proposed-production.patch` pass.

Only the regression file and this report are locally modified. `work-in-progress.patch` in the attachment preserves both for the next run. They are intentionally not pushed as a completed fix while the regression is RED. No full pytest, example smoke, sanitizer, native tests, wheel, or new CI run was performed; all previously reported counts retain their original source identities.

Next action belongs to the parent: add those two `script.cpp` calls to this same task's R1 write scope and resume DAL-239. Then apply the scoped fix, rebuild the real binding, collect GREEN/compatibility/full-Python/example evidence, and publish to the existing draft PR before the unchanged independent testing → documentation decision → mandatory review chain. This is a source-write coordination boundary, not a user-facing API decision.

## 2026-09-15 example validation correction

The resumed DAL-239 scope is limited to `dal-python/examples/009.fix_settings.py` and this report. The five original Codacy annotations on check run `104279592641`, at head `88f218b1557e83913bdda4f34ed87c1b38844fb7`, were fetched in full and confirmed to flag the five assertions removed by optimized Python. `original-codacy-annotations.log` preserves that historical failure; the final delivery records one new-head CI snapshot after push.

The example now uses explicit conditions and field-specific `RuntimeError` messages containing the expected and actual values. All five constraints are preserved: PV260 with `rel_tol=0.0, abs_tol=2.6e-10`, d_SCALE80 with `rel_tol=0.0, abs_tol=1e-10`, only PV/d_ result keys, `dal.script-product/2` and `dal.script-valuation/1`. There is no suppression or tolerance change. No refactoring beyond these five guards was needed.

- Example fix commit: `44e46349eedf34f4bd3b133f355df99999a4c5ff`, tree `adc4eeea15d17e1e187ce8c183144459118a26c8`. It contains exactly the example change. The subsequent commit updates only this report; the final comment and evidence identify the final head/tree.
- Tested example SHA256: `f88259a2542b10a95c03b4fe3116894f01e680be2ff131975ea8c9c9a55488d2`.
- Reused the existing F7 native AADET extension built from `fd9c42d07b54506366652f6559ddf1e0fff093e8`. Its SHA256 still equals `37d08004c8699811763f8842bcb0ae939a9d3638e6985e8514b684d3cabcc3c4`. Both checkouts initially matched the handed-off head/tree and were clean. The old checkout remains unchanged; binding/core/public/build source has no diff from the build's source commit, and the loaded Python helper matches current source bytes.
- Fresh identity logs record absolute `dal.__file__`, `dal._dal.__file__`, Python executable/version, pybind11 version, extension hash/ldd, source diff and example hash. The module is the prior task's `build/Release-linux/dal-python/dal/_dal.cpython-313-x86_64-linux-gnu.so`, loaded by an explicit absolute PYTHONPATH. Runtime remains Python3.13.9/pybind11 3.0.4, Linux/WSL2; the reused build is GCC15.2/AADET.

### Fresh RED/GREEN and smoke evidence

Local `probe_example.py` calls the real binding, corrupts one returned field, then executes the full example with `runpy`. It fails unless the expected field-specific RuntimeError is raised. The probe itself uses no assertions, so optimization cannot disable its expectation. It and `run_check.py` are delivered only in the evidence attachment, outside repository tests.

Below, `F7_PYTHONPATH` is the existing native build's absolute Python package directory recorded verbatim in every log. Commands run from the current repository:

```bash
env PYTHONPATH="$F7_PYTHONPATH" python -O ../probe_example.py dal-python/examples/009.fix_settings.py pv
env PYTHONPATH="$F7_PYTHONPATH" python -O ../probe_example.py dal-python/examples/009.fix_settings.py risk
env PYTHONPATH="$F7_PYTHONPATH" python -O ../probe_example.py dal-python/examples/009.fix_settings.py keys
env PYTHONPATH="$F7_PYTHONPATH" python -O ../probe_example.py dal-python/examples/009.fix_settings.py product-schema
env PYTHONPATH="$F7_PYTHONPATH" python -O ../probe_example.py dal-python/examples/009.fix_settings.py valuation-schema
env PYTHONPATH="$F7_PYTHONPATH" python dal-python/examples/009.fix_settings.py
env PYTHONPATH="$F7_PYTHONPATH" python -O dal-python/examples/009.fix_settings.py
python -m black --check --target-version py39 dal-python/examples/009.fix_settings.py
git diff --check
```

- RED: `red-pv.log` exits1 because `PV=260+3e-10` was accepted under `-O`. After fixing only its guard, `green-pv.log` exits0. The remaining four RED probes then each exit1 because `d_SCALE=80+2e-10`, an extra `unexpected` key, product schema `/1`, or valuation schema `/2` was accepted. Each numeric mutation is outside the required absolute tolerance while still catching accidental default relative tolerance.
- GREEN: after the remaining guards, all five unchanged probes exit0 and show their corresponding RuntimeError messages (`green-*.log`). They test the actual example's rejection behavior, not a duplicate validation implementation.
- Full normal and optimized smoke both exit0 with PV `260.00000000000006` and d_SCALE `80.0` (`smoke-normal.log`, `smoke-optimized.log`). All real pricing and both diagnostic calls run. Black and diff checks also exit0.

No bindings/core/public, other tests, API note, public docs/CHANGELOG, CI/Codacy settings, or ignore rules changed. No substantive API deviation was introduced. No native build, full pytest/CTest, alternate backend or sanitizer suite was rerun for this example-only correction. The original evidence below is **inherited from the initial S2 run at `fd9c42d07b54506366652f6559ddf1e0fff093e8`**, including native1778/Python597/CoDi597/sanitizer255 and consumer2; references to fresh results or "this run" below describe that original run. Its report attachment is `01a0a3ca-aa84-77ec-8208-1c95fe568548`; evidence attachment `01a0a3ca-b6f3-791f-be39-9b2b51139497` has SHA256 `85521c457a904adedaa1ff3ca322446873698c2c4023bf042bbc3d6b1386fef3`.

Independent DAL-240 testing must rebuild at the final new head and cover the wheel's pinned pybind11 2.11.1 on supported Python. That compatibility remains unverified here. The parent's serial acceptance/testing/docs/review sequence is unchanged; F8 and performance sidecars were not started.

## Original S2 evidence (inherited)

## Source identity and scope

- Branch: `feature/dal-204-python-fix-settings`.
- Baseline: `a98bf9b07e9bd0fee23faa4cc9edca737f709654`, tree `4bc1d06508fef4d918f587ce189b4c7899731176`.
- Original tested implementation: `fd9c42d07b54506366652f6559ddf1e0fff093e8`, tree `8918bf9be0f70c41662e3edb242654878f1e8e7e`. Builds/tests used these exact source bytes before this commit was recorded; post-commit identity captures show a clean tree. The initial report-only commit was `88f218b1557e83913bdda4f34ed87c1b38844fb7`, tree `d28b97dca2d22f378839ad6393fbe3556bce373f`. The example correction above supersedes that initial delivery head.
- Accepted F7 note copied byte for byte to `.codex/artifacts/api-notes/dal-204-python-fix-settings.md`; SHA256 `d158902e5fae1def90ae844ab1e70eaec0a9d62b80499a06c135f11c6f21b605`.
- `source-files.json` records SHA256 and staged Git blob identities for 1,278 source files/gitlinks at the implementation commit. Submodule identities are separately captured; no submodule pointer changed.

Changed files:

- `dal-python/src/bindings/script.cpp`: product settings, native input copies and raw Describe JSON.
- `dal-python/src/bindings/value.cpp`: valuation/simulation settings, original policy enum projection, checked conversions, new Value/Explain and legacy Value forwarding.
- `dal-python/src/bindings/scriptsettings.hpp`: private binding helpers for strings, field context, native settings copies and copy/deepcopy registration.
- `dal-python/src/dal/api.py`: keyword-only product settings, Cell passthrough/context and high-level JSON-to-dict diagnostics.
- `dal-python/tests/test_script_settings.py`: 117 collected conversion/signature/settings cases.
- `dal-python/tests/test_fix_valuation.py`: 78 collected pricing, diagnostics and lifecycle cases.
- `dal-python/examples/009.fix_settings.py`: directly executable approved FIX example.
- Accepted API note and this report.

Core/public/Excel, build registration, generated enums, docs and CHANGELOG were not edited. Existing Machinist-generated `TodayFixingPolicy_::Value_` is reused; no new enum markup or regeneration is required.

## Design and behavior

The binding owns conversion and Python error context; the existing C++ public entry points own contract preparation, history resolution and pricing. `Product_New` preserves high-level `events_dates` and native `dates`, accepts Cell values without double wrapping and adds only keyword-only `settings`. The original Value retains its 3–8 positional arguments and keywords; new valuation/simulation settings are accepted only by `MonteCarlo_ValueWithSettings`.

All three settings classes are the original native value types. Constructors and setters share validation. A failed setter leaves the prior value intact. Copy/deepcopy creates independent settings; model binding getters return detached dicts and the immutable snapshot handle is shared. Policy strings are exactly `Model` and `RequireHistorical`; enum members map to the existing MODEL/REQUIREHISTORICAL values. New flags require Python bool, while legacy valid flag and float conversions remain usable.

Both pricing entries accept only Python integer/index protocol path counts in `1..INT_MAX`, excluding booleans and enums. Type/range errors retain `InvalidPathCount`, `num_path`/`numPath` and the positive bound. Huge integers also retain context when Python refuses their decimal repr. Smoothing rejects non-finite/non-positive values and overflow with `InvalidSmoothing` and field/constraint context; the legacy entry still accepts valid `__float__` conversions. Date settings require a valid DAL Date, without formatting an invalid date. Text conversion preserves UTF-8 length and rejects embedded NUL. Model binding keys are copied in dict order to native bindings; case-equivalent `spot`/`SPOT` entries reach native duplicate validation. A repeated identical Python dict key is already lost before binding and is not claimed as detectable.

Product events, settings, binding vectors and owning native handles are copied while the GIL is held. Only the shared C++ call executes with the GIL released; worker code has no dependency on Python containers or callbacks. `None` fixings and an explicit empty snapshot remain distinct. Describe and Explain preserve native schemas; high-level helpers return dicts, low-level helpers return raw JSON. Pricing results contain only PV/risk entries. Explain performs a fresh native preparation and is not a cache for a later Value.

## RED, GREEN and refactor evidence

Every log in the attached evidence includes UTC, working directory, exact command, output and exit status. Commands below run from the repository. `P` below means the literal prefix `env PYTHONPATH=build/Release-linux/dal-python python -m pytest`; it is expanded in every log.

- Product RED: `P dal-python/tests/test_script_settings.py::test_product_settings_copy_and_diagnostic_layers -q` failed once because `ScriptProductSettings_` was absent (`red-product.log`). The same command passed after the minimal product binding (`green-product.log`).
- Valuation RED: `P dal-python/tests/test_script_settings.py::test_historical_settings_value_preserves_parameter_risk -q` failed once on the absent valuation settings (`red-valuation.log`). The then-two-test file passed after native Value/Explain projection (`green-valuation.log`).
- Conversion RED: `P dal-python/tests/test_script_settings.py -q` produced 59 failures/11 passes for invalid settings/path conversions and missing context. After checked conversion helpers, `P dal-python/tests/test_script_settings.py -q -k 'not product_event_conversion and not product_date_cell'` passed 70/70 selected cases (`red-settings-boundaries.log`, `green-settings-boundaries.log`).
- Event RED: `P dal-python/tests/test_script_settings.py -q -k 'product_event_conversion or product_date_cell'` failed all nine selected cases. Native text/Cell conversion and the high-level Cell error context fixed those failures; the growing two-file suite passed 157 cases (`red-event-boundaries.log`, `green-fix-matrix.log`).
- Policy detail RED: `P dal-python/tests/test_script_settings.py -q -k 'api_keyword_boundaries or today_policy_errors or legacy_smoothing'` gave one failure/three passes: the NUL policy error omitted its allowed values. The policy context now includes both exact names; final suites pass (`red-policy-error-details.log`).
- Huge-integer RED: `P dal-python/tests/test_script_settings.py -q -k huge_path_integer` failed twice on Python's integer repr limit. Context construction now handles that limit; the complete suite at that point passed 194 cases (`red-huge-integer.log`, `green-final-boundaries.log`).
- Legacy smooth RED: `P dal-python/tests/test_script_settings.py -q -k legacy_smooth_overflow` failed once because automatic double conversion raised a generic TypeError for `10**400`. The same command passed after explicit float-protocol conversion with field context; its valid `__float__` and bool cases remain passing (`red-legacy-smoothing-overflow.log`, `green-legacy-smoothing-overflow.log`).
- Refactor while green: share positive-float conversion, use visible pointer mutation in property setters and conform local naming/formatting. `P dal-python/tests/test_script_settings.py dal-python/tests/test_fix_valuation.py -q` passed **195/195** (`green-refactor.log`), followed by final backend/full/sanitizer verification below.

Two intermediate attempted GREEN runs were still failures and are retained: `green-events-and-pricing.log` (1 failed/156 passed) used an already-historical mixed SPOT fixture, which correctly encountered `UnboundHistoricalSpot` first. The MissingDefaultIndex fixture was moved to a future event and a separate historical assertion retained. `green-complete-python-fix.log` (1 failed/191 passed) used a single-line regex for a multiline native error; DOTALL was added without removing the error ID or field assertions. Neither correction changed production semantics or relaxed a numeric oracle.

## Acceptance matrix: actual evidence

The new Python files run against the built extension, with native tests used for seams deliberately absent from the public Python API. All named existing native cases below passed in this run's final CTest log, rather than being inherited F6 results.

- **T07 / today:** `test_today_policy_independent_oracles` covers BS/Dupire, three RNGs, bridge on/off, tree/compiled and double/AAD (48 configurations). Each independently expects model 100 and history 80, verifies source/value diagnostics, rejects missing required today history and proves explicit D does not rewrite global D. `ScriptApiTest.TestTodayPolicy` and `ScriptFixingPreparationTest.TestTodayPolicyAndFutureOnly` supply native history-count evidence.
- **T15 / repricing and snapshots:** Python historical tests price explicit 80/90/80 snapshots and check risks; source-map mutation does not alter the snapshot. None/empty/noon-only inputs preserve GlobalSnapshot/ExplicitSnapshot, exact midnight, index and source-row context. Fresh native `ScriptApiTest.TestRepricingAndExplainNeverCache`, `TestExplainDoesNotFreezeGlobalMarket`, `ScriptFixingPreparationTest.TestRepricing` and `TestExplicitSnapshotNeverFallsBackToPresentGlobalHistory` cover actual global 80→90, old plan/snapshot 80, Explain-before-Value refresh and authoritative explicit-empty behavior. `ScriptObservationSimulationTest.TestAadRepricingRefreshesGlobalHistoryAndModelInputs` covers the parameter-risk variant. No public global fixing writer was added.
- **T18 / historical AAD:** `test_historical_aad_batches_and_repricing` independently asserts `PV=2*H*exp(-.05*10/365)`, `d_SCALE=PV/2`, `d_rate=-(10/365)*PV`, exactly zero spot/vol and only expected risk keys, with tree/compiled and paths 1/257/8193. Native `ScriptPastReplayTest.TestParameterRisk`, `TestCompiledParameterRisk` and the public historical entry test pass as well. PV tolerance is `1e-12*max(1,abs(expected))`, analytic AAD tolerance `1e-10`.
- **T09/T10/T12/T21 / future EQ:** `test_future_observation_and_payment_have_distinct_dates` uses both adapters, tree/compiled and double/AAD. F has no event and P is later; the independent zero-vol expectation is `100*exp(.03*3/365-.05*10/365)`. A stored future 999 is ignored, request/sample IDs are asserted and repeated same-identity requests subtract to exactly zero. Binding negatives assert MissingModelBinding, ConflictingModelBinding, UnknownModelAsset, InvalidIndex, DuplicateModelBinding and UnsupportedModelObservation with native fields. Fresh native `TestRetainedFutureFixing`, `TestPaymentNumeraireIsIndependentOfRetainedObservation`, `TestModelBindingsBeforeHistory` and `TestBothAdaptersRejectUnsupportedBeforeHistory` supply controlled-scenario and pre-history rejection evidence, including unsupported/multiple requests.
- **T20/T22 / exact and fuzzy:** `test_exact_fuzzy_and_hard_history_have_separate_oracles` asserts independent exact PV160 and fuzzy PV120, d_SCALE60, d_K−800, both engines, with same-input finite differences rebuilt at each K. Historical placement remains hard PV160, d_SCALE80 and d_K0. The finite-difference tolerance is `1e-5` at step `1e-5`; analytic risk tolerance remains `1e-10`. Native known-fixing fuzzy/analytic-path cases also passed.
- **T27 / legacy:** unchanged existing script/value tests pass. New tests exercise all 3–8 positional calls, old keywords and valid integer flags, native/high-level names, default new settings, legacy JSON schema/1, bound SPOT/FIX deduplication and separate historical/unbound errors. Native `TestDefaultSpotDeduplicatesAndLegacyGuards`, original parser tests and fixed-path compatibility tests cover the retained core boundaries; no SPOT(index) or empty FIX alias was introduced.
- **T28 / settings:** the 117-case settings file covers constructors/setters, copy/deepcopy, detached dicts, exact policy/unnamed enums, DAL/String/UTF-8 values, NUL, all strict booleans, invalid dates and smoothing, whole-settings type checks, unknown/duplicate keywords, keyword-only placement, Cell passthrough and field/row constraints. Valid integer protocol and INT_MAX conversion are checked using a null-product next-precondition seam, avoiding an INT_MAX simulation allocation. Bool/enums/float/string, zero/negative/overflow path errors are covered in both Value entries. Installed legacy/typed consumer passes.
- **T29 / diagnostics and archive:** Python asserts raw/high-level JSON parity and explicit schema fields, interleaved model/history IDs, inverse FX1.25, canonical duplicates/source uses, event-to-sample mapping and model slots. Describe is invariant to market date and lacks runtime phase/date fields. Legacy DebugJson rejects new identities/default settings. Fresh `ScriptApiTest.TestDescribeContractOnly`/`TestExplainPreparation` supply positive counter seams, zero Describe I/O/model/workers, unique history/sequence reads and zero Explain workers. `ScriptArchiveTest.TestIndexVersioning` and `ScriptContractTest.TestArchiveStaysContractOnlyAfterExplainAndAadRepricing` exercise actual v1/v2 registry persistence; no Python serializer was invented.
- **T25/T31 / lifecycle:** Python checks expired zero PV/risk, structure errors for empty/no-PAYS scripts, path exception recovery and actual Python thread progress during released-GIL pricing. The mutation test changes caller date, snapshot and flags after entry and still obtains original PV160/d_SCALE80. Native final-history/model-failure submission counters, expired invalid-settings checks and worker-drain tests passed in final CTest. No callback was exposed for testing.
- **Full example:** `009.fix_settings.py` runs import/global initialization, legal BS model, explicit midnight H and unquoted FIX indices. Native, CoDiPack and installed-module smoke all return PV `260.00000000000006`, d_SCALE `80.0`; its assertions pass.

## Build and verification

Fresh environment: Linux x86_64, WSL2 kernel 5.15.167.4, glibc2.43; GCC15.2.0, CMake4.2.3, Unix Makefiles; Anaconda Python3.13.9, pytest9.0.3 and installed pybind11 **3.0.4**. Native backend is AADET. Alternate CoDiPack is submodule `86b94d3f3c3b6659a36f8e640945a7ebe1884a4a`. Native/CoDi use Release `-O3 -DNDEBUG`; sanitizer uses `-O1 -g0 -DNDEBUG`, address+undefined and no interprocedural optimization.

Configuration/build logs retain exact commands and cache flags. Native configuration used the Release-linux preset with `DAL_BUILD_PYTHON=ON`, Python executable `/home/wegamekinglc/anaconda3/bin/python`, tests/portable Excel enabled and examples subsequently enabled. Full build was `cmake --build build/Release-linux -j 6`. Final binding rebuilds were `cmake --build build/<directory> --target _dal -j3` for Release-linux, codi-python and asan-python; the existing private quote-risk test module was built for full Python testing. All succeeded.

Final checks:

- `ctest --test-dir build/Release-linux --output-on-failure -j4`: **1,778/1,778**, 11.54s (`ctest-delivery.log`). `ctest-delivery-details.log` includes its Python entry: **597 passed**, 8.42s, and all 29 portable Excel contracts.
- `env PYTHONPATH=build/Release-linux/dal-python python -m pytest dal-python/tests -q`: **597 passed**, 8.12s before the final refactor (`python-native-delivery.log`); final CTest above re-executed the same entire suite after refactoring. The original F6 baseline was 402; F7 adds 195 collected cases.
- `env PYTHONPATH=build/codi-python/dal-python python -m pytest dal-python/tests -q`: **597 passed**, 8.62s after refactoring (`python-codi-refactor.log`).
- `env PYTHONPATH=build/asan-python/dal-python LD_PRELOAD=/usr/lib/gcc/x86_64-linux-gnu/15/libasan.so:/usr/lib/gcc/x86_64-linux-gnu/15/libstdc++.so ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 python -m pytest dal-python/tests/test_script_settings.py dal-python/tests/test_fix_valuation.py dal-python/tests/test_script.py dal-python/tests/test_value.py dal-python/tests/test_api.py dal-python/tests/test_xccy_resettable.py dal-python/tests/test_curve_pricing.py -q`: **255 passed**, 4.28s after refactoring; no ASAN/UBSAN diagnostics (`python-sanitizer-refactor.log`).
- `build/Release-linux/dal-public/dal_public_tests --gtest_filter=ScriptApiTest.*:ScriptContractTest.*:ScriptArchiveTest.*`: **23/23** (`native-focused-contracts.log`), also covered by final CTest.
- `cmake --install build/Release-linux --prefix build/f7-install`; a separate consumer CMake project uses `find_package(dal-public CONFIG REQUIRED)` and links only installed `DAL::public`. Building the existing `dal-public/test-consumer/script.cpp` and `dal-public/examples/script_settings.cpp`, then `ctest --test-dir build/f7-consumer --output-on-failure`, passes **2/2** (`configure-installed-consumer.log`, `build-installed-consumer.log`, `installed-consumer.log`). The evidence contains that small consumer CMake file. Final reinstallation updated the Python module; C++ source/libraries were unchanged.
- `env PYTHONPATH=build/Release-linux/dal-python python dal-python/examples/009.fix_settings.py`, the same with `build/codi-python/dal-python`, and with `build/f7-install`: all exit0 (`example-native-final.log`, `example-codi-final.log`, `example-installed-final.log`).
- `clang-format --dry-run --Werror dal-python/src/bindings/script.cpp dal-python/src/bindings/value.cpp dal-python/src/bindings/scriptsettings.hpp`; Black `--check --target-version py39` on both new tests and the example; staged `git diff --check`: all pass.

`identity-native.log`, `identity-codi.log`, `identity-sanitizer.log` and `identity-installed.log` capture actual absolute `dal.__file__`, `dal._dal.__file__`, extension SHA256, `ldd`, flags and clean source identity. The repository-relative native module is `build/Release-linux/dal-python/dal/_dal.cpython-313-x86_64-linux-gnu.so`; the other build modules have the same suffix under their respective build directories. Extension SHA256 values:

- Native and installed: `37d08004c8699811763f8842bcb0ae939a9d3638e6985e8514b684d3cabcc3c4`.
- CoDiPack: `594615d32b8bd5ed9c0e65ba6926d812d89e18602f29b8265078af35c446dfd6`.
- Sanitizer: `fb7dfcfb559428ae63b70f339a669698f6e1eef633124ca09a77fac1fa8213e3`.

## Limits and handoff

There is no substantive deviation from the accepted API. Invalid-input conversion now happens explicitly where pybind's automatic narrowing lost required context; valid legacy float/flag protocols remain covered. No core/public defect required expanded scope.

This environment did not test Python3.9–3.12, wheel construction with pinned/vendored pybind11 **2.11.1**, standalone Python CMake configuration, Windows/MSVC/XLL, macOS, Adept or XAD. The installed pybind11 3.0.4 result is not evidence for the wheel pin. Sanitizer checks exclude host-Python leak detection and are not a leak audit. Portable Excel tests do not establish XLL support. No new performance benchmark or performance gate was introduced; F8 was not started. Independent testing/review and documentation decisions remain outstanding.

Setup failures are preserved in logs: Ninja was unavailable, initially missing submodules prevented configuration, and formatting briefly moved the public value header before the platform header. Unix Makefiles, initializing the pinned submodules and preserving the platform include group resolved these failures. A missing repository Git identity was supplied locally as dal-implementer before commit. These failures are not reported as passing checks.

The evidence attachment contains fresh command logs, complete final CTest details, source/module identities and capture/consumer scripts. The final delivery comment records the report-only commit, PR head/tree and the single nonblocking post-push CI snapshot. Parent acceptance should retain this draft PR and implementation commit when handing off to DAL-240.
