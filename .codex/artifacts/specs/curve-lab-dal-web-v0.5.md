# Curve Lab in DAL-WEB — Product and Interaction Specification v0.5

Status: active design specification; supersedes v0.4 under owner decisions P-02 and P-03
Initiator: Cheng Li
Scope-correction role: `dal-api-designer`
Next required role: independent DAL-17 re-review

## Sources

- User request and decisions in the current Multica chat.
- `dal-web/frontend/src/App.tsx`
- `dal-web/frontend/src/styles.css`
- `dal-web/frontend/src/pages/Curves.tsx`
- `dal-web/frontend/src/pages/CurveRun.tsx`
- `dal-web/frontend/src/components/MatrixHeatmap.tsx`
- `.codex/skills/dal-web/references/design-system.md`
- `dal-cpp/dal/storage/storable.hpp`
- `dal-cpp/dal/storage/json.hpp`
- `dal-cpp/dal/storage/json.cpp`
- `dal-cpp/dal/storage/bag.hpp`
- `dal-cpp/dal/curve/yc.hpp`
- `dal-cpp/dal/curve/curveblock.hpp`

## Confirmed Product Decisions

1. Curve Lab is integrated into DAL-WEB, not delivered as a separate application.
2. Single-curve and multi-curve dependencies are both in scope.
3. Calibration Jacobian and trade-to-curve sensitivity are both in scope.
4. Risk scope is PV, DV01/PV01, and Key Rate DV01 only. Gamma, Vega, and CS01 are out of scope.
5. Curve persistence uses JSON through DAL's current `Storable_` archive capability.
6. This requirement has no approval or four-eyes workflow.
7. New UI must match the current DAL-WEB industrial terminal design system.
8. Durable RATE and SPREAD quotes use one canonical decimal unit. A one-basis-point
   raw and normalized move is always `+0.0001`; percent is presentation-only.

## Problem Statement

The current DAL-WEB Curve Workbench can submit calibration JSON and inspect persisted
calibration runs, fit diagnostics, forward Jacobians, effective inverses, and quote bumps.
It does not yet provide one coherent user flow for:

- visually constructing complete curve sets from instruments;
- binding immutable curve-set versions to trade or portfolio valuation;
- tracing sensitivity from market quotes to curve nodes and from curve nodes to trade PV;
- saving and restoring the resulting native DAL curve objects through `Storable_` JSON.

## Goals

1. Let a user construct and validate single-curve, multi-curve, staged XCCY, and joint XCCY configurations.
2. Make dependencies, market snapshot, conventions, solver settings, and validation state explicit.
3. Price one or more trades or a portfolio against an exact immutable curve-set version.
4. Present both sensitivity layers with unambiguous axes, units, bump methods, and provenance.
5. Persist and restore native curve objects without introducing a second curve serialization format.
6. Preserve the current advanced JSON calibration path for expert and troubleshooting use.

## Non-goals

- Gamma, Vega, CS01, VaR, historical simulation, or a general scenario engine.
- Workflow approval, approvers, four-eyes validation, or electronic sign-off.
- Real-time streaming quotes and automatic recalculation.
- Concurrent collaborative editing.
- A separate Curve Lab application or a new visual design language.
- Binary serialization.
- Replacing the existing DAL calibration engine or performing financial mathematics in React.

## Information Architecture

Curve Lab remains the existing top-level `Curve Lab` item in DAL-WEB's horizontal navigation.
Within Curve Lab, the user can access:

1. **Build** — visual curve-set construction and advanced JSON contract editing.
2. **Runs** — immutable calibration run lifecycle and diagnostics.
3. **Pricing & Risk** — target selection, valuation, sensitivities, and Key Rate DV01.
4. **Versions** — saved native JSON snapshots, diff, import, export, and clone.

No left application sidebar is introduced.

## Functional Requirements

### FR-1 — DAL-WEB Shell and Visual Consistency

1. Curve Lab shall render inside the existing `DAL Workbench` shell and top navigation.
2. It shall use the tokens and component behavior in `dal-web/frontend/src/styles.css`:
   - `#0f1419`, `#161b22`, `#21262d`, and `#2d333b` background layers;
   - gold active/primary state, green success, red error/negative, amber warning/running;
   - DM Sans UI text and JetBrains Mono financial data;
   - 4–6 px radii, compact full-width layout, uppercase labels, right-aligned tabular numbers.
3. It shall not introduce a sidebar, light theme, glow effects, large radii, decorative gradients,
   background textures, or a content max-width.
4. Existing global health, backend, and evaluation-date indicators shall remain visible.

### FR-2 — Curve Construction Modes

The Build screen shall support:

1. Single discount or projection curve.
2. Multi-curve sets containing at least one discount curve and one or more projection curves.
3. Staged XCCY calibration over persisted curve blocks.
4. Joint XCCY calibration.

Each curve or curve set shall expose:

- name, currency, role, as-of date, and market snapshot;
- parameterization, interpolation, extrapolation, and day-count conventions;
- exact dependent curve versions;
- solver tolerance, maximum evaluations/restarts, and Jacobian mode;
- current state: Draft, Building, Failed, Validated, Stale, or Archived.

### FR-3 — Instrument Authoring

1. Users shall add, edit, include/exclude, and remove instruments.
2. The canonical ordered V1 success-family allowlist is exactly `DEPOSIT`, `FRA`,
   `FUTURE`, `OIS`, `IRS`, `BASIS_SWAP`, and `XCCY`.
3. The grid shall show type, tenor/maturity, quote, quote convention, day count, source,
   timestamp, inclusion state, and validation status.
4. Bulk input shall support canonical-decimal DAL request JSON and optionally
   canonical-decimal CSV quote import.
5. The current JSON contract editor shall remain available as an Advanced view.
6. Visual-builder changes and Advanced JSON shall round-trip to the same canonical request model;
   the UI must not maintain two divergent request definitions.
7. API schemas, backend DTOs, OpenAPI, persistence validation, native-gateway
   dispatch, frontend authoring choices, result rendering, examples, and tests
   shall project that exact allowlist. Package generation fails if any
   projection is missing an entry or contains an additional entry.
8. The distinct closed input- and display-convention enums are each exactly
   `DECIMAL`, `PERCENT`, and `PRICE_POINTS`. RATE/SPREAD permit `DECIMAL` or
   `PERCENT`; PRICE permits only `PRICE_POINTS`.
9. Percent UI input shall be divided by 100 using exact base-10 arithmetic
   before construction of the durable instrument DTO. Its stored `raw_quote`,
   normalized quote, fingerprint, risk axis, and replay result shall be
   identical to the equivalent decimal input.
10. Advanced JSON, DTO, OpenAPI, persistence, and risk evidence shall expose
    RATE/SPREAD `raw_quote` only in canonical decimal units. Presentation
    preference is metadata and cannot override coordinate kind, canonical
    unit, normalized quote, or risk bump.
11. RATE/SPREAD `exact_risk_raw_bump` and normalized solver bump are fixed at
    `+0.0001`. FUTURE keeps the `PRICE_POINTS` raw coordinate, `-0.01` raw
    bump, and `+0.0001` normalized solver bump.

### FR-4 — Modified Draft and Stale State

Any change to instruments, quotes, market snapshot, conventions, curve dependencies,
parameterization, interpolation/extrapolation, or solver configuration shall:

1. mark the draft modified;
2. mark existing build and risk results stale;
3. display `Rebuild required`;
4. prevent saving the stale result as a new Validated version;
5. preserve old results as read-only evidence until a new run succeeds.

Changing only the presentation convention or its display precision does not
change the canonical quote, financial fingerprint, draft revision, stale
state, persisted run evidence, or replay result.

### FR-5 — Build and Validation

1. Build shall use the existing asynchronous immutable calibration-run pattern.
2. The lifecycle shall expose declaration, dependency resolution, instrument normalization,
   solve, diagnostics, serialization, and terminal status.
3. Successful validation shall display convergence, iterations/evaluations, maximum absolute
   residual, RMS residual, quote residuals, node count, and curve representation.
4. Failures shall identify the request field or instrument where possible and retain the draft.
5. `Validated` in this feature means numerical/build validation only; it is not an approval.

### FR-6 — Curve Views

For each result curve, users shall inspect:

- zero rates;
- discount factors;
- forward rates;
- nodes and underlying numeric representation;
- market quote fit and residuals.

Every chart shall identify curve name, exact version/run, as-of date, units, interpolation, and
whether the display is current or stale.

### FR-7 — Pricing Targets and Curve Context

1. The Pricing & Risk screen shall accept either selected trades or a portfolio.
2. The user shall select an exact immutable curve-set version; `latest` shall not be an implicit dependency.
3. The screen shall show the complete dependency map and market snapshot before execution.
4. Missing required discount/projection/XCCY dependencies shall block execution and identify affected trades.
5. Partial trade failures shall not hide successful results; the UI shall report success and failure counts
   and expose per-trade errors.

### FR-8 — Pricing and Risk Outputs

The MVP shall output:

- PV by trade and aggregate;
- DV01/PV01 by trade and aggregate;
- Key Rate DV01 by curve, tenor bucket, trade, and portfolio aggregation.
- Per-trade pricing responses whose success and failure variants are closed
  schemas with `additionalProperties=false`; each variant contains only the
  common identity/status evidence and the fields required for that outcome.

Gamma, Vega, and CS01 shall not appear in the MVP UI or API response for this workflow.
Every result shall record target fingerprint, curve-set version, market snapshot, evaluation date,
base currency, computation timestamp, and per-trade status.
Every quote-axis row shall additionally record its registry-owned coordinate
kind, canonical raw unit, canonical `raw_quote`, normalized quote, and fixed
raw/normalized bump. None is caller-configurable.

### FR-9 — Two Sensitivity Layers

The UI shall distinguish:

1. **Calibration Jacobian** — curve node parameters with respect to calibration market quotes.
2. **Trade-to-curve sensitivity** — trade PV with respect to curve nodes.

For each matrix the UI/API shall state:

- mathematical orientation;
- row and column labels;
- units;
- bump size and bump target;
- finite-difference/AAD method;
- one-sided, central, analytical, or effective-inverse interpretation;
- curve-set/run version and market snapshot.

Key Rate DV01 may be composed from the two layers only when the applied mapping and aggregation
are explicitly identified.

### FR-10 — Native `Storable_` JSON Persistence

1. Curve serialization shall use DAL `Storable_::Write` through `Dal::JSON::WriteString`.
2. Curve deserialization shall use `Dal::JSON::ReadString`; DAL-WEB shall verify the restored root type.
3. The UI shall offer only `Storable_ JSON`; no binary format selector shall be shown.
4. A single-root curve shall serialize its actual native `Storable_` curve object.
5. A multi-root curve set shall use the existing `Bag_` storable container with stable semantic keys
   for the discount, projection, domestic, foreign, and basis components as applicable.
6. The JSON payload shall retain DAL type tags and archive references. DAL-WEB shall not flatten the
   native payload into a competing web-specific curve schema.
7. DAL-WEB metadata may wrap or accompany the native payload and shall include:
   - saved-version ID and name;
   - build run ID and status;
   - as-of and market snapshot;
   - dependency version IDs;
   - tags and version note;
   - SHA-256 content hash;
   - creator and timestamps.
8. Saved versions shall be immutable. Opening a saved version is read-only; editing clones it as a Draft.
9. Save and import shall perform a native round-trip check before committing the version.
10. Optional diagnostics, calibration inputs, and matrices shall be stored outside the native curve payload
    in versioned DAL-WEB metadata/result fields, unless they are already members of the chosen native storable.

### FR-11 — Version Library

Users shall be able to:

- search and filter by name, currency, curve type, status, tag, hash, and as-of;
- preview metadata, curve graph, native root type, type tag, hash, and dependencies;
- compare two versions' metadata, curve nodes, quotes, conventions, and dependencies;
- import Storable JSON;
- export the exact native JSON payload;
- open read-only;
- clone as a new Draft;
- archive a version without deleting its referenced data.

### FR-12 — Audit Without Approval

Build, save, import, export, clone, and archive actions shall record actor, timestamp, target ID,
input/content hash, and outcome. No approver or approval-status fields shall be required.

## Inputs

- DAL calibration request and accepted instruments.
- As-of/evaluation date.
- Market snapshot and quote timestamps.
- Curve conventions, solver settings, and exact curve dependencies.
- Trade IDs or portfolio ID.
- Risk bump configuration limited to DV01/PV01 and Key Rate DV01.
- Native `Storable_` JSON on import.

## Outputs

- Immutable calibration run with curves and diagnostics.
- Native curve or `Bag_` curve-set handle.
- PV, DV01/PV01, Key Rate DV01, calibration Jacobian, and trade-to-curve sensitivity.
- Immutable saved-version record with exact native JSON, metadata, content hash, and dependency links.
- Exported native DAL JSON.

## Edge Cases

1. Duplicate instrument IDs or maturities.
2. Missing, stale, non-finite, or convention-incompatible quotes.
3. Cyclic curve dependencies.
4. Dependency version missing, archived, or no longer reconstructible.
5. Underdetermined/overdetermined solves and unavailable Jacobian modes.
6. Matrix dimensions over existing response limits.
7. Mixed-currency portfolio without a required FX/XCCY dependency.
8. Partial pricing failure.
9. Imported JSON with unknown type tag, malformed archive references, hash mismatch, or wrong root type.
10. Native JSON that parses but fails reconstruction.
11. Save interrupted after serialization but before database commit.
12. Source version archived after a dependent curve set has been saved.

## Non-functional Requirements

1. All financial calculations and native JSON serialization run outside the React client.
2. UI input objects are treated immutably across asynchronous work.
3. Existing calibration matrix and serialized-response size limits remain enforced until separately changed.
4. Numeric values use tabular monospace formatting and expose full precision on demand.
5. Keyboard navigation, focus visibility, semantic tables, and chart alternatives meet WCAG 2.1 AA.
6. No saved version becomes visible before native round-trip validation and the database transaction succeed.
7. Saved native JSON is byte-for-byte exportable after persistence.

## Acceptance Criteria

### AC-1 — Visual Integration

A Playwright screenshot of each new Curve Lab view demonstrates the existing DAL-WEB top bar,
dark palette, gold primary action, no sidebar, full-width content, compact tables, and monospace numbers.
A static review finds no new hard-coded competing palette outside approved CSS variables.

### AC-2 — Construction Modes

Frontend and backend tests submit and complete one fixture for each mode: single, multi-curve,
staged XCCY, and joint XCCY. Each completed run exposes dependencies, curves, fit diagnostics,
and terminal status.

### AC-3 — Stale Invalidation

An end-to-end test builds a valid curve, changes one quote, observes `Rebuild required`,
and verifies pricing and Validated save are disabled until a successful rebuild.

### AC-4 — Exact Curve Binding

An end-to-end test selects a portfolio and an immutable curve-set version, runs pricing,
and verifies the returned and displayed result references the same curve-set version,
market snapshot, and target fingerprint.

### AC-5 — Risk Scope

Contract and UI tests verify PV, DV01/PV01, and Key Rate DV01 are present. They also verify
Gamma, Vega, and CS01 fields/tabs are absent from this workflow.

### AC-6 — Sensitivity Orientation

Unit tests render both matrices with row/column labels, units, bump size, method, and version.
Fixture dimensions match their declared axes, and a transposed fixture fails validation.

### AC-7 — Native Single-Curve Round Trip

A C++ test serializes a representative `YieldCurve_` implementation with `JSON::WriteString`,
restores it with `JSON::ReadString`, verifies the type and name, and compares discount/forward
values at node and non-node dates within the existing numerical tolerance.

### AC-8 — Native Multi-Curve Round Trip

A C++ test places the component storables of a multi-curve or XCCY curve set in a keyed `Bag_`,
serializes/restores it, verifies keys and runtime types, and reproduces representative discount,
forward, and basis values.

### AC-9 — DAL-WEB Version Round Trip

A backend test saves native JSON only after a successful native read-back, restarts the store,
loads the version, exports the exact payload, and verifies SHA-256 equality and dependency metadata.

### AC-10 — Import Rejection

Backend tests reject malformed JSON, hash mismatch, unknown type tags, wrong root types,
broken references, and failed native reconstruction without creating a partial version row.

### AC-11 — Immutability

API tests verify saved version payload and metadata cannot be updated; clone creates a new Draft
with a new ID and an explicit source-version link.

### AC-12 — No Approval Workflow

Schema and UI tests verify no approval, approver, reviewer, or four-eyes field/action is required
to build, save, load, import, export, or clone a curve version.

### AC-13 — Closed Success-family and Result Projections

A generated contract test compares the canonical ordered success-family
allowlist byte-for-byte with the backend DTO enum, OpenAPI enum, persistence
validator, native-gateway dispatch table, frontend authoring registry, result
renderer registry, examples, and test fixture registry. Every comparison
asserts the exact seven values and rejects missing or additional values.

OpenAPI and DTO snapshot tests also compare each discriminated pricing-result
variant against its exact required-key set with `additionalProperties=false`.
Adding any result member without first changing this normative specification
fails package assembly.

### AC-14 — Canonical Quote Units and Fixed Risk Bumps

For each of `DEPOSIT`, `FRA`, `OIS`, `IRS`, `BASIS_SWAP`, and `XCCY`,
contract tests submit equivalent `PERCENT` and `DECIMAL` presentation inputs.
They assert byte-identical canonical stored requests and quote axes, identical
normalized `+0.0001` moves, and identical DV01/KRD replay results.

Additional tests cover FUTURE price conversion and `-0.01` raw bump,
presentation round-trip with fixed half-even display rounding, negative and
zero RATE/SPREAD inputs, finite native-conversion boundaries, and rejection
of excess canonical length, overflow, underflow, non-finite values, invalid
convention/family pairs, and attempted overrides of derived unit/coordinate/
bump fields. A percent-input replay fixture fails if a 100-times-smaller raw
move is applied.

## Compatibility

- Existing `/curves` JSON contract editing and calibration-run URLs remain supported.
- Existing persisted calibration runs remain readable.
- Existing DAL archive type tags and reader registration determine native JSON compatibility.
- New DAL-WEB metadata is versioned independently from the native archive payload.
- Native type/API exposure to `dal-python` must be additive.

## AAD Behavior

- The UI does not assume all sensitivities are AAD-derived.
- The backend must report the actual method used for each matrix.
- AAD tape lifetime and mutable evaluation-date scope remain native-backend concerns.
- PV and first-order risk from an AAD path must be compared against finite-difference fixtures.
- No second-order AAD behavior is required.

## Performance Targets

Targets require validation against representative fixture sizes during implementation:

- UI interaction feedback: under 100 ms for local selection/filtering.
- Build/pricing command acknowledgement: under 300 ms before showing running state.
- Matrix rendering: 100 × 100 within 500 ms on the supported desktop browser.
- Version list first page: under 1 second for 1,000 saved-version records.
- Native JSON save/load timing and size shall be recorded; no hard latency target is set until
  representative single-, multi-, and XCCY payloads are benchmarked.

## Open Technical Questions for `dal-api-designer`

1. `dal-python` does not currently expose `Dal::JSON::WriteString`, `ReadString`, or `Bag_`.
   Define the smallest typed binding that lets DAL-WEB serialize/restore native storables
   without exposing unsafe arbitrary handle operations.
2. Confirm stable semantic keys and the root-type policy for single, multi-curve, staged XCCY,
   and joint XCCY saved sets.
3. Confirm whether saved-version metadata should embed the native JSON as text or store it in
   a separate immutable blob column.
4. Define matrix method/orientation fields shared by calibration Jacobian and trade sensitivity.

## Workflow Route

Selected route:

`dal-spec-writer → dal-critic → dal-api-designer → dal-implementer → dal-tester → dal-reviewer → dal-doc-writer`

Current gate:

- Revision 8 awaits independent DAL-17 technical-design re-review.
- Implementation, testing, later review, and documentation remain parked.

Active package asset: `curve-lab-dal-web-v0.5.md`
