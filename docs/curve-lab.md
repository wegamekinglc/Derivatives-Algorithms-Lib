# Curve Lab

Curve Lab is DAL-WEB's visual workflow for constructing, publishing, importing,
pricing against, and measuring first-order risk on versioned rate-curve
artifacts. The browser drives the same typed C++ and Python surfaces used by the
backend; Advanced JSON is an escape hatch, not a separate execution path.

This guide describes the supported current state. For local service setup, see
the [web application guide](../dal-web/README.md). For installed native entry
points, see the [public API guide](public-api.md). Curve construction and
Jacobian methodology remain in
[yield_curve.md](methodology/yield_curve.md) and
[yield_curve_jacobian.md](methodology/yield_curve_jacobian.md).

## Supported surface

Curve Lab accepts exactly seven rate-instrument families:

| Family       | Quote coordinate | Canonical raw unit | Exact raw risk bump | Normalized bump |
|--------------|------------------|--------------------|---------------------|-----------------|
| `DEPOSIT`    | `RATE`           | `DECIMAL`          | `0.0001`            | `0.0001`        |
| `FRA`        | `RATE`           | `DECIMAL`          | `0.0001`            | `0.0001`        |
| `FUTURE`     | `PRICE`          | `PRICE_POINTS`     | `-0.01`             | `0.0001`        |
| `OIS`        | `RATE`           | `DECIMAL`          | `0.0001`            | `0.0001`        |
| `IRS`        | `RATE`           | `DECIMAL`          | `0.0001`            | `0.0001`        |
| `BASIS_SWAP` | `SPREAD`         | `DECIMAL`          | `0.0001`            | `0.0001`        |
| `XCCY`       | `SPREAD`         | `DECIMAL`          | `0.0001`            | `0.0001`        |

Rate and spread input may use decimal or percent conventions. Futures use
price points. The backend canonicalizes every accepted quote to a durable
decimal string before draft persistence; clients should not reproduce that
logic. For example:

```http
POST /api/curve-lab/quote-canonicalizations
Content-Type: application/json

{
  "instrument_type": "FUTURE",
  "input_lexeme": "95.25",
  "input_convention": "PRICE_POINTS"
}
```

The four supported build topologies are closed contracts:

| Mode           | Declaration topology                                                        |
|----------------|-----------------------------------------------------------------------------|
| `SINGLE`       | Exactly one discount component                                               |
| `MULTI_CURVE`  | One-currency discount and projection components; no basis component          |
| `STAGED_XCCY`  | Domestic and foreign components plus exactly one basis component, staged     |
| `JOINT_XCCY`   | Domestic and foreign components plus exactly one basis component, solved jointly |

Component roles are `DISCOUNT`, `PROJECTION`, and `BASIS`. Supported native
parameter coordinates are `PIECEWISE_CONSTANT_FWD`,
`PIECEWISE_LINEAR_FWD`, `ZERO_RATE`, and `LOG_DISCOUNT`.

Curve Lab V2 does not add gamma, vega, CS01, binary trade ingestion, approval
workflow, implicit "latest" version resolution, or Excel projection of its
private persistence bridge. Risk requests contain an explicit typed trade list;
they do not reference DAL-WEB portfolio IDs.

## Browser workflow

The **Curve Lab** screen has four tabs:

1. **Build** — choose the topology, declare components, add typed instruments
   and version dependencies, then create or save a draft.
2. **Runs** — start an immutable build snapshot and poll it to `SUCCEEDED`,
   `FAILED`, or `TIMED_OUT`.
3. **Pricing & Risk** — price typed trades against a published version and
   request PV, DV01, key-rate DV01, and optional diagnostic matrices.
4. **Versions** — publish a successful, non-stale run; clone a version to a new
   draft; import or export native JSON and runtime metadata; or archive a
   version.

A draft starts at revision 1. Updating it requires the current revision as a
quoted integer in the `If-Match` header, for example `If-Match: "1"`. A
successful update increments the revision, changes the
fingerprint when financial content changes, marks any earlier run stale, and
requires a rebuild before publishing. Concurrent or stale writes return
`409 Conflict`; the UI never silently merges financial documents.

The visual controls and Advanced JSON edit the same version-2 draft document.
Validation rejects extra fields, invalid topology, missing component ownership,
duplicate identities, and unsupported instrument terms before native work is
admitted.

## REST and OpenAPI

All Curve Lab endpoints are under `/api/curve-lab`:

| Resource                       | Endpoints                                                                                  |
|--------------------------------|--------------------------------------------------------------------------------------------|
| Capabilities and quotes        | `GET /capabilities`, `POST /quote-canonicalizations`                                       |
| Drafts                         | `POST /drafts`, `GET /drafts/{id}`, `PUT /drafts/{id}`                                     |
| Build runs                     | `POST /drafts/{id}/build-runs`, `GET /build-runs/{id}`                                     |
| Versions                       | `POST /versions`, `GET /versions`, `GET /versions/{id}`, `POST /versions/{id}/archive`     |
| Version portability            | `POST /versions/{id}/clone`, `GET /versions/{id}/native-json`, `GET /versions/{id}/runtime-manifest` |
| Imports                        | `POST /import-jobs`, `GET /import-jobs`, `GET /import-jobs/{id}`                          |
| Fixing snapshots               | `POST /fixing-snapshots`, `GET /fixing-snapshots`, `GET /fixing-snapshots/{id}`            |
| Pricing and risk               | `POST /risk-runs`, `GET /risk-runs`, `GET /risk-runs/{id}`                                |
| Materialized matrices          | `GET /risk-runs/{id}/matrices/{matrix_id}`                                                 |

The live Swagger UI is served at `/docs`. The committed contract is
`dal-web/backend/openapi/dal-web.openapi.json`; regenerate it from
`dal-web/backend` with:

```bash
uv run --no-sync python scripts/generate_openapi.py
```

Build, import, and risk submission returns `202 Accepted` and a persisted
`QUEUED` record. Construct the corresponding build-run, import-job, or risk-run
polling endpoint from the returned ID and poll until its state is terminal. A
new publication returns `201 Created` only after comparing the requested draft
revision, draft fingerprint, build run, dependency hashes, and idempotency key
in one transaction. An identical idempotent replay returns the existing
version with `200 OK`.

Validation failures use structured `422` details. Resource conflicts use
`409`; shared queue exhaustion uses `429` with `Retry-After` and does not
persist a job. A job that exceeds its deadline ends as `TIMED_OUT`. Errors
retain a stable code, message, field, optional value and resource ID, and
structured details; clients should branch on the code rather than parsing the
message.

### Minimal single-curve draft

```json
{
  "schema_version": 2,
  "mode": "SINGLE",
  "as_of_date": "2026-01-15",
  "market_snapshot_id": "market-2026-01-15",
  "declarations": [
    {
      "component_key": "clab/v1/local/discount/USD/OIS",
      "role": "DISCOUNT",
      "currency": "USD",
      "parameterization": "PIECEWISE_CONSTANT_FWD"
    }
  ],
  "instruments": [
    {
      "instrument_type": "DEPOSIT",
      "trade_date": "2026-01-15",
      "start_date": "2026-01-16",
      "maturity_date": "2026-04-16",
      "currency_or_pair": "USD",
      "raw_quote": "0.04",
      "source": "MARKET",
      "observed_at": "2026-01-15T00:00:00Z",
      "included": true,
      "terms": {
        "index": "USD-SOFR"
      }
    }
  ],
  "dependency_version_ids": [],
  "solver": {
    "solve_mode": "EXACT",
    "parameterization": "PIECEWISE_CONSTANT_FWD"
  }
}
```

The response adds the draft ID, revision, fingerprint, state, stable instrument
and quote identities, canonical quote values, and exact risk bumps.

## Versions, archives, and portability

A successful build serializes the native result through `Storable_` JSON:

| Build shape      | REST `root_kind`  | Native root                                                     |
|------------------|-------------------|-----------------------------------------------------------------|
| Single component | `DISCOUNT_CURVE`  | `DiscountCurve_`                                                |
| Multiple roots   | `CURVE_SET`       | `Bag_` containing named `DiscountCurve_` / `CurveBlock_` values |

`CURVE_SET` is the REST vocabulary; `Bag` is the native archive tag. Published
versions are immutable records containing the exact canonical native payload,
SHA-256 content hash, `JSON_MAX_DIGITS10_V1` numeric-format marker, provenance,
and `VERIFIED` validation state. Archiving changes visibility to `ARCHIVED`; it
does not delete bytes or rewrite lineage. Cloning creates a new draft and new
instrument identities.

Export returns the exact stored canonical JSON bytes. Export the runtime
manifest alongside them when the version will be imported for pricing or risk:
the native archive reconstructs curves, while the manifest restores Curve Lab
component keys, roles, parameter axes, mode, date, and market-snapshot context.

Import accepts raw JSON bytes, optional `Content-Encoding: gzip`, and an
optional `X-Curve-Lab-Runtime-Manifest` header. It performs bounded structural
preflight before calling the native reader, then reconstructs and reserializes
through DAL. A successful import publishes an immutable version with validation
state `IMPORT_RECONSTRUCTED`.

A malformed runtime-manifest header returns `422` before admission. A native
JSON structural-preflight rejection persists a failed import job and returns
`422` with its resource ID. Once preflight admits a payload, the endpoint
returns `202`; native reconstruction or runtime-manifest post-validation
failures then appear as a persisted `FAILED` job and must be observed by
polling.

The native allowlist accepts supported discount-curve roots and `Bag` /
`Bag_v1` collections only. Unknown tags, duplicate names, unsupported graphs,
non-finite numerics, malformed references, and trailing tokens are rejected.
The underscore-prefixed Python functions used for this bridge are integration
helpers, not a general public serialization API.

## Pricing, fixings, and risk

Create an immutable fixing snapshot before submitting a risk run. Each
observation is uniquely keyed by `(index_name, fixing_time)` and is either a
decimal rate or a positive domestic-per-foreign FX value. The response adds a
content hash used in every downstream provenance record.

Pricing planning enumerates all required historical rate and FX observations
before valuation. Each trade then returns a closed success or failure variant.
One trade's missing fixing or pricing error does not erase successful rows from
the same run. Successful rows retain PV, currency, normalized plan hash,
required fixing keys, and dependency component keys; failed rows retain their
structured error and missing-fixing set.

Risk measures have the following semantics:

- **PV** is the base valuation for each admitted trade.
- **DV01** is the PV difference after one parallel exact quote-coordinate bump
  and a full recalibration.
- **KEY_RATE_DV01** independently applies the exact bump to each quote and
  performs a full recalibration and repricing for every bucket.
- `key_rate_sum` is reported separately from
  `nonlinear_reconciliation = dv01 - key_rate_sum`; no equality is implied.
- If a required key-rate bump fails, the key-rate matrix is marked failed and
  values and aggregates are not published as if they were complete.

Optional matrix layers are self-describing:

| Layer                         | Mathematical name                      | Orientation         | Method and units                                                                 |
|-------------------------------|----------------------------------------|---------------------|----------------------------------------------------------------------------------|
| `TRADE_TO_NODE`               | `trade_to_node_pv_gradient`            | trade × parameter   | Native AAD after parity verification, otherwise central parameter bump; PV per native parameter unit |
| `CALIBRATION_JACOBIAN`        | `d_parameter_d_normalized_quote`       | parameter × quote   | Central full recalibration; native parameter unit per decimal-rate quote unit    |
| `COMPOSED_QUOTE_DIAGNOSTIC`   | `trade_to_node_times_calibration_jacobian` | trade × quote   | Matrix composition; base-currency PV per decimal-rate quote unit                 |
| Key-rate result               | independently recalibrated quote risk  | trade × quote       | Full recalibration at the exact family-specific quote bump                       |

Every matrix response carries row and column axis references, dimensions,
availability and reason, method, unit and bump metadata, version and fixing
hashes, axis hashes, evaluation time, and base currency. Trade-to-node results
also expose per-trade method and AAD parity evidence.

Native `RateTradeNodeSensitivities` currently admits deposit trades only.
Other families return `TRADE_FAMILY_NOT_AAD_ENABLED`; Curve Lab uses the
declared central-parameter fallback where requested and records that method
rather than presenting it as AAD.

## Native C++ and Python

Include `<dal-public/src/curvepricing.hpp>` for the additive typed pricing
surface. Core definitions live in
`<dal/curve/ratecashflowpricing.hpp>`. The principal C++ entry points are:

- `BuildRateCashflowPlan` (trade plus valuation time)
- `PriceRateTrade` and `PriceRateTrades`
- `RateTradeNodeSensitivities`
- `CurvePricingFamilyRegistry`

The public types include `RateTradeDefinition_`, the seven family-specific
terms structs, `RatePricingMarket_`, `RateCashflowPlan_`,
`RatePricingTradeResult_`, and `RateTradeNodeSensitivityResult_`.

Python exports the corresponding enum, typed terms, trade, market, pricing, and
sensitivity types. Pricing and sensitivity calls are keyword-only and release
the GIL while native work runs:

```python
import dal

today = dal.Date_(2026, 1, 15)
maturity = dal.Date_(2027, 1, 15)
curve = dal.DiscountPWC_New("usd", "USD", [maturity], [0.04])
index = dal.RateIndexConvention_New(
    dal.PeriodLength_New("3M"),
    dal.DayBasis_New("ACT_365F"),
    dal.CollateralType_OIS(),
)
terms = dal.DepositTradeTerms_(
    notional=100.0,
    contract_rate=0.05,
    lend=True,
    index=index,
    discount_component_key="discount",
)
trade = dal.RateTradeDefinition_(
    instrument_id="deposit-1",
    instrument_type=dal.RateInstrumentType.DEPOSIT,
    trade_date=today,
    start_date=today,
    maturity_date=maturity,
    currency="USD",
    terms=terms,
)
market = dal.RatePricingMarket_(
    valuation_time=dal.DateTime_(today, 10, 30),
    result_currency="USD",
    curve_components={"discount": curve},
    fixings=dal.MarketFixingSnapshot_New({}),
)

priced = dal.PriceRateTrades(trades=[trade], market=market)
sensitivity = dal.RateTradeNodeSensitivities(
    trade=trade,
    market=market,
    component_key="discount",
)
```

`Storable_` additionally exposes read-only `name` and `type` properties.
`YieldCurve_` / `CurveBlock_` and `Bag_` bindings support the Curve Lab native
object hierarchy. `_StorableToJson`, `_StorableFromJson`, `_BagNew`, and
`_BagContents` are deliberately private web-integration helpers; application
code should use version export/import unless it owns the native compatibility
contract.

## Limits and admission

Build, import, and risk work share one process-local queue with 2 workers and
100 waiting slots. A job has a 15-minute soft deadline checked between native
calls. The risk estimator rejects work before queue admission when any bound is
exceeded:

| Dimension                   | Limit   |
|-----------------------------|---------|
| Trades                      | 1,000   |
| Native parameters           | 500     |
| Quotes                      | 500     |
| Price evaluations           | 100,000 |
| Calibration solves          | 1,002   |
| AAD recordings              | 1,000   |
| Estimated wall time         | 900,000 ms |

Archive import bounds are 10 MiB on the wire, 50 MiB expanded, depth 64,
500,000 values, 10,000 objects, 50,000 references, 1 MiB per string,
250,000 numeric-array items, and 1 KiB per numeric token.

## Persistence, restart, and rollback

The default SQL store persists drafts, build and import runs, versions, audit
events, fixing snapshots, risk runs, and matrix blobs. SQLite uses write-ahead
logging, foreign-key enforcement, and immediate transactions for publication;
PostgreSQL uses row locks. Terminal runs and published versions survive a
backend restart.

Queued or running build, import, and risk rows cannot resume after process
loss. Startup reconciles each to `FAILED` with `SERVER_RESTARTED`, or to
`TIMED_OUT` when its stored deadline has already elapsed. With
`DAL_WEB_STORE=memory`, all Curve Lab records disappear on restart.

The V2 schema is additive to the pre-existing `/api/calibrations` workflow.
Existing calibration records and public curve APIs keep their compatibility
contract. To apply migrations explicitly:

```bash
cd dal-web/backend
uv run alembic upgrade head
```

Before database rollback, stop the backend and take a database backup. The
Curve Lab migration chain starts after revision `c2d8f43a9e71`; downgrading to
that revision drops Curve Lab tables and columns, including published
versions, run history, matrices, and fixing snapshots:

```bash
cd dal-web/backend
uv run alembic downgrade c2d8f43a9e71
```

Use a restored backup to recover that data. Rolling application code back
without a schema downgrade does not itself delete Curve Lab data, but the
target application's migration compatibility must be validated against a copy
of the database before production rollback.

## Operational checks

Run the checks appropriate to the changed layer:

```bash
# Native and Python, from the repository root
env NUM_CORES=2 ADDITIONAL_CMAKE_FLAGS=-DDAL_BUILD_PYTHON=ON bash ./build_linux.sh
ctest --test-dir build/Release-linux --output-on-failure
dal-python/.venv/bin/python -m pytest -q dal-python/tests

# Backend
cd dal-web/backend
uv run --no-sync ruff check app tests scripts
uv run --no-sync pytest -q
uv run --no-sync python scripts/generate_openapi.py

# Frontend
cd ../frontend
npm test
npm run build
DAL_PLAYWRIGHT_TEST_BACKEND=1 npm run test:e2e
```

After generating the API contract, verify
`dal-web/backend/openapi/dal-web.openapi.json` is unchanged unless the REST
surface intentionally changed. The canned-DAL Playwright mode retains the real
FastAPI routers and Vite frontend but is a test fixture, not a production
fallback.
