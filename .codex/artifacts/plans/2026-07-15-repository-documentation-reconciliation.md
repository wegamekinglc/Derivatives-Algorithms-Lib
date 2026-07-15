# Repository Documentation Reconciliation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reconcile every published DAL Markdown document with merged `master`, preserve historical planning artifacts outside `docs/`, and publish a source-backed documentation-only pull request.

**Architecture:** Treat current headers, bindings, generated registrations, examples, tests, scripts, and workflows as ground truth. Keep root and component pages concise, place mathematical and ownership contracts in methodology/architecture pages, and record one disposition for each of the 33 baseline documents in a durable audit artifact.

**Tech Stack:** GitHub-flavored Markdown, DAL C++17/public headers, pybind11 bindings, Excel/Machinist registrations, CMake/build scripts, Python documentation checker.

## Global Constraints

- Baseline is merged `master` at `1589089bdf10df352ce5cf9cde963fd6b51a4f95`.
- The controlling specification is `.codex/artifacts/specs/repository-documentation-reconciliation.md`.
- No C++, Python, Excel, web, CMake, workflow, or generated-file behavior changes.
- Do not edit `CLAUDE.md`, `.claude/`, `AGENTS.md`, third-party documentation, build output, generated files, or Codex skill instructions.
- Current-state docs contain no delivery chronology or superseded implementation narrative; history belongs in `CHANGELOG.md` or `.codex/artifacts/`.
- Root/component overviews summarize and link; methodology owns formulas, invariants, limitations, matrix units, and numerical contracts.
- Every changed technical claim must be traceable to current source, tests, examples, generated registration, or build configuration.
- Preserve GitHub-renderable math, valid relative links/anchors, aligned tables, final newlines, and no trailing whitespace.
- The final tracked diff may contain published Markdown and `.codex/artifacts/{plans,reviews,specs}/` only.

---

### Task 1: Move historical implementation artifacts out of published documentation

**Files:**
- Move: `docs/superpowers/plans/2026-07-12-unified-yield-curve-interpolation-aad.md` -> `.codex/artifacts/plans/2026-07-12-unified-yield-curve-interpolation-aad.md`
- Move: `docs/superpowers/plans/2026-07-12-ycinstrument-pricing-performance.md` -> `.codex/artifacts/plans/2026-07-12-ycinstrument-pricing-performance.md`
- Move: `docs/superpowers/plans/2026-07-12-zero-rate-parameterization.md` -> `.codex/artifacts/plans/2026-07-12-zero-rate-parameterization.md`
- Move: `docs/superpowers/specs/2026-07-12-ycinstrument-pricing-performance-design.md` -> `.codex/artifacts/specs/2026-07-12-ycinstrument-pricing-performance-design.md`
- Move: `docs/superpowers/specs/2026-07-12-zero-rate-parameterization-design.md` -> `.codex/artifacts/specs/2026-07-12-zero-rate-parameterization-design.md`

**Interfaces:**
- Consumes: the current-state documentation rule and the baseline 33-file set selected by `.github/scripts/check_docs.py`.
- Produces: preserved planning history under the canonical Codex artifact directories and a 28-file current-state published-doc set.

- [ ] **Step 1: Run the structural red check**

```bash
python3 - <<'PY'
from pathlib import Path
historical = sorted(Path("docs/superpowers").rglob("*.md"))
assert not historical, "historical implementation artifacts remain in docs: " + ", ".join(map(str, historical))
PY
```

Expected: FAIL listing exactly the five assigned plan/spec files.

- [ ] **Step 2: Confirm that no destination collision or inbound published link exists**

```bash
for source in docs/superpowers/plans/*.md docs/superpowers/specs/*.md; do
  destination=".codex/artifacts/${source#docs/superpowers/}"
  test ! -e "$destination"
  rg -n --fixed-strings "$source" README.md CONTRIBUTING.md CHANGELOG.md dal-*/README.md docs --glob '*.md' || true
done
```

Expected: every destination is absent; no current-state document depends on the historical path.

- [ ] **Step 3: Move each artifact without rewriting its historical content**

```bash
mkdir -p .codex/artifacts/plans .codex/artifacts/specs
git mv docs/superpowers/plans/2026-07-12-unified-yield-curve-interpolation-aad.md .codex/artifacts/plans/
git mv docs/superpowers/plans/2026-07-12-ycinstrument-pricing-performance.md .codex/artifacts/plans/
git mv docs/superpowers/plans/2026-07-12-zero-rate-parameterization.md .codex/artifacts/plans/
git mv docs/superpowers/specs/2026-07-12-ycinstrument-pricing-performance-design.md .codex/artifacts/specs/
git mv docs/superpowers/specs/2026-07-12-zero-rate-parameterization-design.md .codex/artifacts/specs/
```

- [ ] **Step 4: Run the structural green check and documentation checker**

```bash
python3 - <<'PY'
import importlib.util
from pathlib import Path
assert not list(Path("docs/superpowers").rglob("*.md"))
spec = importlib.util.spec_from_file_location("check_docs", ".github/scripts/check_docs.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
assert len(module.DOCS) == 28, len(module.DOCS)
PY
python3 .github/scripts/check_docs.py
git diff --check
```

Expected: all commands exit 0; documentation integrity reports 28 Markdown files.

- [ ] **Step 5: Commit**

```bash
git add .codex/artifacts/plans .codex/artifacts/specs docs/superpowers
git commit -m "docs: move historical plans out of published docs"
```

### Task 2: Refresh root navigation, architecture, and current capability baseline

**Files:**
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `docs/README.md`
- Modify: `docs/architecture.md`

**Interfaces:**
- Consumes: current XCCY source/examples, fixing-snapshot ownership, current analytic-Jacobian representations, and the resolved runtime/build findings from the prior audit.
- Produces: concise root discoverability plus authoritative runtime ownership and calibration-flow navigation.

- [ ] **Step 1: Run content red checks**

```bash
rg -n "XCCY|Cross-Currency" README.md
rg -n 'Why `LOG_DISCOUNT` is the parameterization that supports the analytic Jacobian' docs/README.md
rg -n "MarketFixingSnapshot_|CalibrateJointXccyMarket|CalibrateCrossCurrencyMarket" docs/architecture.md
```

Expected: the root search returns no match; the obsolete `LOG_DISCOUNT` sentence matches; architecture lacks the snapshot and XCCY entry points.

- [ ] **Step 2: Update the root README without duplicating methodology**

Add cross-currency pricing/calibration to the opening capability sentence. Under **Examples**, add a short cross-currency paragraph with direct links to:

```markdown
- [reset-aware pricing](dal-cpp/examples/xccy_reset_pricing/)
- [staged basis calibration](dal-cpp/examples/xccy_curve_calibration/)
- [joint domestic/foreign/basis calibration](dal-cpp/examples/xccy_mtm_calibration/)
- [Python joint calibration](dal-python/examples/007.xccy_joint_calibration.py)
```

Add this concise methodology entry to the abbreviated list:

```markdown
- [Cross-Currency Pricing and Calibration](docs/methodology/xccy_calibration.md) — fixed, resettable, and MTM swaps; immutable fixing snapshots; staged basis and simultaneous domestic/foreign/basis calibration
```

- [ ] **Step 3: Correct the documentation index and heading hierarchy**

Promote `### Methodology (`methodology/`)` and `### Experimental (`experimental/`)` to level-two headings. Replace the log-discount exclusivity bullet with:

```markdown
  - Persistent log-discount coordinates, interpolation/extrapolation, basis weights, and participation in the shared analytic-Jacobian curve factory
```

Extend the XCCY and yield-Jacobian index summaries to mention immutable operation snapshots, joint matrix range layout, and effective-inverse scaling without copying formulas.

- [ ] **Step 4: Add snapshot ownership and branch the architecture calibration flow**

Add this runtime-state row, padded to the existing table widths:

```markdown
| Market-fixing snapshot | Immutable operation-level value retained by reset-aware pricing/calibration results. An explicit snapshot is authoritative; when omitted, staged/joint XCCY calibration gathers required historical requests and copies the process-wide store once before solving. |
```

Replace the single calibration diagram with one common validation/residual prefix and three named branches:

```text
instrument/convention builders
  -> validated spec, curve layout, and model-rate residuals
  -> single/staged curves: CalibrateYieldCurve / CalibrateMultiCurve
  -> staged XCCY basis: CalibrateCrossCurrencyMarket
       -> supplied domestic/foreign blocks + basis parameters + one fixing snapshot
  -> joint XCCY: CalibrateJointXccyMarket
       -> domestic declarations + foreign declarations + basis declaration
       -> one fixing snapshot + named parameter/residual ranges
  -> exact or approximate underdetermined solve
       -> eligible AAD analytic Jacobian or explicit bumped mode
  -> solved curves, diagnostics, and optional matrices
```

Link the paragraph to `methodology/xccy_calibration.md` as well as the existing yield-curve pages.

- [ ] **Step 5: Reconcile the changelog baseline and qualifying XCCY entry**

Replace the baseline cross-currency bullet with current-state wording covering fixed/resettable/MTM pricing, immutable timestamped rate/FX snapshots, staged basis fitting, simultaneous domestic/foreign/basis calibration, and named Jacobian ranges.

Remove the obsolete baseline bullet that presents analytic calibration as a `CurveJacobianMode`-only capability; analytic Jacobians now cover every implemented curve representation subject to normal eligibility gates. In the dated XCCY entry, avoid enum shorthand that looks like a callable C++ identifier and qualify the cross-surface statement: public C++ and joint Python expose both joint matrices, while Excel exposes the joint forward Jacobian/ranges but no effective-inverse worksheet getter.

- [ ] **Step 6: Verify and commit**

```bash
python3 .github/scripts/check_docs.py
git diff --check
rg -n "Cross-Currency Pricing and Calibration" README.md docs/README.md CHANGELOG.md
rg -n "Market-fixing snapshot|CalibrateCrossCurrencyMarket|CalibrateJointXccyMarket" docs/architecture.md
git add README.md CHANGELOG.md docs/README.md docs/architecture.md
git commit -m "docs: refresh architecture and navigation"
```

### Task 3: Correct quantitative methodology contracts

**Files:**
- Modify: `docs/experimental/replicate-ptirds-single-currency-curve.md`
- Modify: `docs/methodology/aad.md`
- Modify: `docs/methodology/interpolation.md`
- Modify: `docs/methodology/matrix.md`
- Modify: `docs/methodology/random.md`

**Interfaces:**
- Consumes: `/tmp/dal-doc-audit-quant.md` and the cited current source/tests.
- Produces: exact current-state numerical and runtime semantics without changing implementation.

- [ ] **Step 1: Run terminology/formula red checks**

```bash
rg -n "all-days|matches calendar = all|does better" docs/experimental/replicate-ptirds-single-currency-curve.md
rg -n "first-order accurate|not-a-knot|O\\(n m\\)" docs/methodology/interpolation.md docs/methodology/matrix.md
rg -n 'full `SkipTo` support|re-seed via `Branch`/`Clone`|repositions the generator without replay' docs/methodology/random.md
```

Expected: each stale claim is present before editing.

- [ ] **Step 2: Correct PTIRDS calendar reproduction and neutralize comparison language**

State that the external target uses an every-day calendar, while DAL reproduces the supplied explicit dates with `Holidays::None()` plus `Unadjusted` accrual/payment/index conventions because `Holidays::None()` removes named holidays but still treats weekends as non-business days. Replace “does better” with the factual statement that eligible `LOG_DISCOUNT` calibration uses an AAD-derived analytic Jacobian.

- [ ] **Step 3: Distinguish full-tape rewind from checkpoint rewind**

Rename the AAD memory heading to `Mark / RewindToMark`. State exactly:

```markdown
`RewindToMark(tape)` discards nodes recorded after the current mark and is used after each Monte Carlo path. `Rewind(tape)` resets the tape to the beginning of its recording and is used before a fresh simulation or calibration recording.
```

- [ ] **Step 4: Correct interpolation accuracy, formula, and boundary semantics**

Describe piecewise-linear interpolation as degree-one/piecewise affine with `O(h^2)` interpolation error for a sufficiently smooth scalar function. Replace the cubic evaluation formula with:

```math
f(x)=a f_i+b f_{i+1}-\frac{a b h^2}{6}\left[(1+a)f_i''+(1+b)f_{i+1}''\right].
```

Describe order-3 endpoint boundaries as pinning the endpoint segment's third derivative to the supplied value; remove “not-a-knot family.”

- [ ] **Step 5: Correct matrix notation and complexity**

After the document's existing definitions (`a` super-diagonal, `c` sub-diagonal), use:

```math
x_i=\frac{b_i-c_{i-1}x_{i-1}}{\beta_i},
\qquad
x_{i-1}\leftarrow x_{i-1}-\frac{a_{i-1}}{\beta_{i-1}}x_i.
```

State that band Cholesky factorization is `O(n m^2)` for lower bandwidth `m`; band triangular solve/multiply is `O(n m)`. Explain that dense `CholeskyImpl` clips negative pivot residuals to zero and regularizes the reciprocal diagonal; it does not factor an explicitly shifted `A + lambda I` matrix.

- [ ] **Step 6: Document implementation-specific random seeking**

State that `SkipTo` is implementation-specific: Sobol reconstructs state directly; `ShuffledIRN_::SkipTo` is a no-op; its `Clone` restarts from the original seed; `Branch` creates another seeded generator. For MRG32, state that the current skip accounting matches antithetic `FillUniform`, while `FillNormal` consumes a fresh uniform per component and bypasses that cache, so MRG32 seeking must not be promised as replay-equivalent normal-path substreams. Keep Sobol as the verified normal-path seeking surface.

- [ ] **Step 7: Verify and commit**

```bash
python3 .github/scripts/check_docs.py
git diff --check
rg -n "RewindToMark|O\\(h\\^2\\)|O\\(n m\\^2\\)|implementation-specific" docs/methodology
git add docs/experimental/replicate-ptirds-single-currency-curve.md docs/methodology/aad.md docs/methodology/interpolation.md docs/methodology/matrix.md docs/methodology/random.md
git commit -m "docs: correct numerical methodology contracts"
```

### Task 4: Complete XCCY fixing, Jacobian, example, and performance methodology

**Files:**
- Modify: `docs/methodology/xccy_calibration.md`
- Modify: `docs/methodology/yield_curve_jacobian.md`

**Interfaces:**
- Consumes: core XCCY/fixing implementations, `/tmp/dal-doc-audit-curves.md`, and existing generic inverse-risk convention.
- Produces: authoritative core contracts and one concise cross-surface availability summary for Task 5 to mirror.

- [ ] **Step 1: Run XCCY methodology red checks**

```bash
rg -n "FX\\[foreign/domestic\\]|reciprocal|1e-10|totalParameters x totalResiduals|xccy_perf|007.xccy_joint_calibration" docs/methodology/xccy_calibration.md docs/methodology/yield_curve_jacobian.md
```

Expected: the required terms are absent or incomplete.

- [ ] **Step 2: Complete reset and immutable-snapshot rules**

Document all of these current rules together:

- `FxIndexName(domestic, foreign)` is `FX[foreign/domestic]`.
- Direct lookup wins; if absent, the reciprocal of the reverse canonical observation is used.
- If both directions exist at one timestamp, `abs(direct * reverse - 1) <= 1e-10` is required.
- Index names are non-empty, timestamps valid, and observations positive and finite.
- Core/Python map inputs provide at most one value per pair; Excel parallel arrays reject duplicates explicitly.
- Historical requirements are deduplicated and include only observations feeding unsettled coupons, notionals, or MTM reset dependencies.
- Explicit snapshots, including explicit empty snapshots, are authoritative; omitted snapshots are copied once from global fixings.

- [ ] **Step 3: Add matrix dimensions, solved-state construction, scaling, and population rules**

State staged shapes as `nInstruments x nBasisParameters` and `nBasisParameters x nInstruments`. State joint shapes as `totalResiduals x totalParameters` and `totalParameters x totalResiduals`. Explain that the exposed forward Jacobian is unscaled, while the effective inverse is the weighted pseudoinverse of the solver's tolerance-scaled Jacobian captured at the accepted exact solution. Include:

```math
\Delta x = \mathrm{effJacobianInverse}\,\Delta q / \mathrm{tolerance}.
```

State: exact analytic may produce both matrices; exact bumped produces only the effective inverse; approximate produces neither; options suppress each independently. Both matrices live on the top-level joint result, not per-group diagnostics.

- [ ] **Step 4: Add a compact surface-availability summary**

Use these exact bullet entries:

- **Core/public C++:** staged XCCY has full options, a forward Jacobian, and an
  effective inverse; joint XCCY has full options and both top-level matrices.
- **Python:** staged XCCY has the default solve, market/FX-forward output, and
  fit diagnostics, but no staged matrix bindings; joint XCCY has options, named
  ranges, a forward Jacobian, and an effective inverse.
- **Excel:** staged XCCY has a basis handle and fit diagnostics, but no staged
  matrix views; joint XCCY has options plus forward-Jacobian/range views, but no
  effective-inverse worksheet getter.

- [ ] **Step 5: Complete examples and performance smoke navigation**

List `xccy_curve_calibration`, `xccy_reset_pricing`, `xccy_mtm_calibration`, and `dal-python/examples/007.xccy_joint_calibration.py`. Correct the joint example wording to “five declaration blocks across the domestic, foreign, and basis groups.” Add a **Performance Smoke Surface** subsection: `xccy_perf` emits 24 rows covering four pricing cases (future fixed/resettable/MTM and started MTM), staged calibration including the reset-aware analytic case, and joint calibration; Linux/Windows execute it to completion, but it is not in the paired base/head regression allowlist.

- [ ] **Step 6: Verify documented workflows and commit**

```bash
build/Release-linux/dal-cpp/examples/xccy_curve_calibration/xccy_curve_calibration
build/Release-linux/dal-cpp/examples/xccy_reset_pricing/xccy_reset_pricing
build/Release-linux/dal-cpp/examples/xccy_mtm_calibration/xccy_mtm_calibration
PYTHONPATH="$PWD/build/stage/Release-linux" dal-python/.venv/bin/python dal-python/examples/007.xccy_joint_calibration.py
DAL_NUM_THREADS=4 build/Release-linux/dal-cpp/benchmarks/xccy_perf/xccy_perf | python3 -c 'import re,sys; row=re.compile(r"^\S(?:.*?\S)?\s+[0-9]+(?:\.[0-9]+)?\s+(?:ns|us|ms|s)\s+[0-9]+(?:\.[0-9]+)?\s+(?:ns|us|ms|s)\s+[0-9]+(?:\.[0-9]+)?\s+(?:ns|us|ms|s)\s+[0-9]+\s*$"); names=[line.rsplit(None,7)[0] for line in sys.stdin if row.match(line)]; assert len(names)==len(set(names))==24, (len(names),len(set(names)))'
python3 .github/scripts/check_docs.py
git diff --check
git add docs/methodology/xccy_calibration.md docs/methodology/yield_curve_jacobian.md
git commit -m "docs: complete XCCY methodology coverage"
```

### Task 5: Reconcile public API and Python/Excel README availability

**Files:**
- Modify: `docs/public-api.md`
- Modify: `dal-python/README.md`
- Modify: `dal-excel/README.md`

**Interfaces:**
- Consumes: the Task 4 terminology/table, exact pybind11 fields, and exact Excel accepted selectors/settings.
- Produces: copy-safe C++ identifiers and binding-specific capability statements without promising missing accessors.

- [ ] **Step 1: Run API-surface red checks**

```bash
rg -n "XccyNotionalMode_::\\{FIXED|effJacobianInverse|staged" docs/public-api.md dal-python/README.md dal-excel/README.md
```

Expected: invalid grouped C++ spelling exists; staged/joint matrix distinctions are absent or incomplete.

- [ ] **Step 2: Correct C++ enum spelling and staged/joint API availability**

In `docs/public-api.md`, spell all constants fully:

```text
XccyNotionalMode_::Value_::FIXED
XccyNotionalMode_::Value_::RESETTABLE
XccyNotionalMode_::Value_::MARK_TO_MARKET
```

State that C++ staged diagnostics own forward/effective-inverse matrices and options through the included core types; joint results own both top-level matrices. Existing `JointXccyResult*` facade helpers expose the documented handles/vectors/ranges, while C++ reads `effJacobianInverse_` directly because no dedicated facade getter exists.

- [ ] **Step 3: Correct Python staged/joint wording and test inventory**

State that staged Python exposes the one-argument default solve, calibrated market, FX forwards, and scalar/vector fit diagnostics only. State that joint Python exposes `JointXccyCalibrationOptions_`, `jacobian_at_solution`, `eff_jacobian_inverse`, and named ranges. Add the joint inverse dimensions and `/ tolerance_` risk-transform warning with a methodology link. Extend **Testing** with curve construction/calibration, staged XCCY, resettable/MTM snapshots, and joint XCCY diagnostics/matrix coverage.

- [ ] **Step 4: Complete Excel snapshot and matrix-visibility wording**

Add duplicate `(index name, timestamp)` rejection, canonical/reverse FX lookup, and the `1e-10` reciprocal rule. State explicitly:

- staged `XCCYCALIBRATIONRESULT.GET` exposes fit vectors/scalars only and neither matrix;
- joint settings can request both computations;
- `JOINTXCCYCALIBRATIONRESULT.GET("jacobian")` exposes the forward matrix and ranges;
- no worksheet selector exposes the retained joint effective inverse.

- [ ] **Step 5: Verify identifiers against source and commit**

```bash
rg -n "enum class Value_|FIXED|RESETTABLE|MARK_TO_MARKET" dal-cpp/dal/auto/MG_XccyNotionalMode_enum.hpp
rg -n "jacobian_at_solution|eff_jacobian_inverse|CalibrateXccyMarket|CalibrateJointXccyMarket" dal-python/src/bindings/curve.cpp
rg -n "acceptedAttributes|effJacobianInverse|computeEffJacobianInverse|jacobian" dal-excel/src/__xccycalibration.cpp
python3 .github/scripts/check_docs.py
git diff --check
git add docs/public-api.md dal-python/README.md dal-excel/README.md
git commit -m "docs: clarify XCCY API availability"
```

### Task 6: Record the 33-document audit and run the final documentation gate

**Files:**
- Create: `.codex/artifacts/reviews/repository-documentation-reconciliation.md`

**Interfaces:**
- Consumes: baseline audit reports `/tmp/dal-doc-audit-quant.md`, `/tmp/dal-doc-audit-curves.md`, `/tmp/dal-doc-audit-platform.md` when available, all Task 1-5 commits, the controlling spec, and the prior audit.
- Produces: final evidence-backed disposition matrix and branch-level verification record.

- [ ] **Step 1: Create the audit artifact with exactly 33 baseline dispositions**

Use this structure:

```markdown
# DAL Repository Documentation Reconciliation

- Baseline: `1589089b...`
- Published baseline set: 33 files from `.github/scripts/check_docs.py`

## Executive Summary
The baseline set contains 14 changed documents, five historical artifacts moved to
`.codex/artifacts/`, and 14 documents verified current without edits.

## Document Dispositions
| Baseline document | Disposition | Source evidence | Result |
|-------------------|-------------|-----------------|--------|
Use the exact 33-row mapping below and fill each evidence/result cell from the three
source-backed audit reports and the Task 1-5 diffs.

## Prior Audit Reconciliation
| 2026-07-10 finding | Current status | Evidence |
|--------------------|----------------|----------|
Group and record the prior numerical, runtime, packaging, web, build, API, and
documentation findings. Mark fixed behavior as resolved with current source/test
evidence; retain the documented MRG32 normal-path seeking limitation as current.

## Verification
Record the documentation checker count, diff/scope checks, three C++ example runs,
Python example run, 24-row benchmark parse, and reviewer verdict.
```

The exact baseline disposition mapping is:

| Baseline document | Disposition |
|-------------------|-------------|
| `CHANGELOG.md` | `changed` |
| `CONTRIBUTING.md` | `verified-current` |
| `README.md` | `changed` |
| `dal-cpp/README.md` | `verified-current` |
| `dal-excel/README.md` | `changed` |
| `dal-public/README.md` | `verified-current` |
| `dal-python/README.md` | `changed` |
| `dal-web/README.md` | `verified-current` |
| `docs/README.md` | `changed` |
| `docs/architecture.md` | `changed` |
| `docs/experimental/aad-analytic-jacobian-curve-calibration.md` | `verified-current` |
| `docs/experimental/replicate-ptirds-single-currency-curve.md` | `changed` |
| `docs/installation.md` | `verified-current` |
| `docs/methodology/aad.md` | `changed` |
| `docs/methodology/black_scholes.md` | `verified-current` |
| `docs/methodology/dupire.md` | `verified-current` |
| `docs/methodology/interpolation.md` | `changed` |
| `docs/methodology/log_discount_curve.md` | `verified-current` |
| `docs/methodology/matrix.md` | `changed` |
| `docs/methodology/pde.md` | `verified-current` |
| `docs/methodology/quadrature.md` | `verified-current` |
| `docs/methodology/random.md` | `changed` |
| `docs/methodology/script_engine.md` | `verified-current` |
| `docs/methodology/underdetermined_search.md` | `verified-current` |
| `docs/methodology/xccy_calibration.md` | `changed` |
| `docs/methodology/yield_curve.md` | `verified-current` |
| `docs/methodology/yield_curve_jacobian.md` | `changed` |
| `docs/public-api.md` | `changed` |
| `docs/superpowers/plans/2026-07-12-unified-yield-curve-interpolation-aad.md` | `moved-to-.codex` |
| `docs/superpowers/plans/2026-07-12-ycinstrument-pricing-performance.md` | `moved-to-.codex` |
| `docs/superpowers/plans/2026-07-12-zero-rate-parameterization.md` | `moved-to-.codex` |
| `docs/superpowers/specs/2026-07-12-ycinstrument-pricing-performance-design.md` | `moved-to-.codex` |
| `docs/superpowers/specs/2026-07-12-zero-rate-parameterization-design.md` | `moved-to-.codex` |

Use only `changed`, `moved-to-.codex`, and `verified-current` dispositions. Cite symbols/files, not source line numbers, in the durable audit.

- [ ] **Step 2: Prove matrix completeness against the baseline file list**

```bash
python3 - <<'PY'
from pathlib import Path
import importlib.util
import re
audit = Path('.codex/artifacts/reviews/repository-documentation-reconciliation.md').read_text()
rows = [line for line in audit.splitlines() if line.startswith('| `') and ('changed' in line or 'moved-to-.codex' in line or 'verified-current' in line)]
assert len(rows) == 33, len(rows)
paths = [re.match(r"\| `([^`]+)`", row).group(1) for row in rows]
assert len(paths) == len(set(paths)) == 33
spec = importlib.util.spec_from_file_location('check_docs', '.github/scripts/check_docs.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
current = {module.relative(path) for path in module.DOCS}
moved = {
    'docs/superpowers/plans/2026-07-12-unified-yield-curve-interpolation-aad.md',
    'docs/superpowers/plans/2026-07-12-ycinstrument-pricing-performance.md',
    'docs/superpowers/plans/2026-07-12-zero-rate-parameterization.md',
    'docs/superpowers/specs/2026-07-12-ycinstrument-pricing-performance-design.md',
    'docs/superpowers/specs/2026-07-12-zero-rate-parameterization-design.md',
}
assert set(paths) == current | moved, (set(paths) ^ (current | moved))
PY
```

Expected: exit 0 with 33 unique rows.

- [ ] **Step 3: Run final scope and documentation checks**

```bash
python3 .github/scripts/check_docs.py
git diff --check 1589089b..HEAD
git diff --name-only 1589089b..HEAD | while IFS= read -r doc_path; do case "$doc_path" in *.md) ;; *) printf '%s\n' "$doc_path";; esac; done
git diff --name-only 1589089b..HEAD -- CLAUDE.md .claude AGENTS.md dal-cpp/dal/auto dal-excel/auto
```

Expected: docs checker passes for 28 current-state Markdown files; all three diff/scope commands emit no unexpected paths.

- [ ] **Step 4: Commit the audit record**

```bash
git add .codex/artifacts/reviews/repository-documentation-reconciliation.md
git commit -m "docs: record repository documentation audit"
```

- [ ] **Step 5: Prepare final review evidence**

```bash
git log --oneline 1589089b..HEAD
git diff --stat 1589089b..HEAD
git status --short
```

Expected: six scoped documentation commits after the spec commit, a clean worktree, and no production/generated changes.
