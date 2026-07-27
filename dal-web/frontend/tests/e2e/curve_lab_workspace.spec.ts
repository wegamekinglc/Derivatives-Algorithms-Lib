import { expect, test } from "@playwright/test";

const draftId = "a".repeat(32);
const buildId = "b".repeat(32);
const versionId = "c".repeat(32);
const riskId = "d".repeat(32);
const tradeId = "0".repeat(31) + "1";

const draft = {
  id: draftId,
  schema_version: 2,
  revision: 1,
  fingerprint: "1".repeat(64),
  state: "READY_TO_BUILD",
  document: {},
  created_at: "2026-01-15T00:00:00Z",
  updated_at: "2026-01-15T00:00:00Z",
};

const build = {
  id: buildId,
  draft_id: draftId,
  draft_revision: 1,
  draft_fingerprint: draft.fingerprint,
  state: "SUCCEEDED",
  stale: false,
  request: {},
  resolved_plan: { mode: "SINGLE" },
  quote_axis: [{
    global_quote_index: 0,
    quote_id: "2".repeat(32),
    component_key: "clab/v1/local/discount/USD/OIS",
    display_label: "DEPOSIT 2027-01-15",
    normalized_quote: "0.04",
  }],
  parameter_axis: [{
    global_parameter_index: 0,
    parameter_id: "USD:PWC:2027-01-15:RIGHT",
    component_key: "clab/v1/local/discount/USD/OIS",
    display_label: "USD OIS 2027-01-15 RIGHT",
    coordinate_kind: "PIECEWISE_CONSTANT_FWD",
    node_date: "2027-01-15",
  }],
  dependency_manifest: [],
  diagnostics: { fit_state: "NATIVE_ARCHIVE_VALIDATED" },
  native_payload_hash: "3".repeat(64),
  error: null,
  created_at: "2026-01-15T00:00:00Z",
  finished_at: "2026-01-15T00:00:01Z",
};

const version = {
  id: versionId,
  source_kind: "BUILD",
  build_run_id: buildId,
  import_job_id: null,
  name: "USD OIS",
  version_note: null,
  tags: ["curve-lab"],
  native_payload_length: 128,
  native_payload_hash: "4".repeat(64),
  root_kind: "DISCOUNT_CURVE",
  build_validation_state: "VERIFIED",
  visibility_state: "VISIBLE",
  created_at: "2026-01-15T00:00:02Z",
};

test("runs the durable build, risk, matrix, and version workflow", async ({ page }) => {
  await page.route("**/api/health", (route) => route.fulfill({
    status: 200,
    contentType: "application/json",
    body: JSON.stringify({
      status: "ok",
      backend: "dal",
      is_native: true,
      evaluation_date: "2026-01-15",
    }),
  }));
  await page.route("**/api/curve-lab/versions?include_archived=false", (route) =>
    route.fulfill({
      status: 200,
      contentType: "application/json",
      body: JSON.stringify([version]),
    }));
  await page.route("**/api/curve-lab/drafts", (route) => route.fulfill({
    status: 201,
    contentType: "application/json",
    body: JSON.stringify(draft),
  }));
  await page.route(`**/api/curve-lab/drafts/${draftId}/build-runs`, (route) =>
    route.fulfill({
      status: 202,
      contentType: "application/json",
      body: JSON.stringify(build),
    }));
  await page.route("**/api/curve-lab/versions", (route) => route.fulfill({
    status: 201,
    contentType: "application/json",
    body: JSON.stringify(version),
  }));
  await page.route("**/api/curve-lab/risk-runs", (route) => route.fulfill({
    status: 202,
    contentType: "application/json",
    body: JSON.stringify({
      id: riskId,
      curve_version_id: versionId,
      source_kind: "BUILD_VERSION",
      state: "SUCCEEDED",
      quote_axis: build.quote_axis,
      parameter_axis: build.parameter_axis,
      estimated_work: { price_evaluations: 8 },
      result: {
        pricing: [{
          trade_id: tradeId,
          instrument_type: "DEPOSIT",
          status: "SUCCEEDED",
          pv: "9615.38",
          currency: "USD",
        }],
        dv01: [{ trade_id: tradeId, value: "-0.96" }],
        key_rate_sum: [{ trade_id: tradeId, value: "-0.95" }],
      },
      error: null,
      created_at: "2026-01-15T00:00:03Z",
      finished_at: "2026-01-15T00:00:04Z",
    }),
  }));
  await page.route(`**/api/curve-lab/risk-runs/${riskId}/matrices/*`, (route) => {
    const matrixId = route.request().url().split("/").at(-1);
    return route.fulfill({
      status: 200,
      contentType: "application/json",
      body: JSON.stringify({
        matrix_id: matrixId,
        mathematical_name: matrixId,
        orientation: "TRADE_X_QUOTE",
        row_axis_ref: "request.target.trades",
        column_axis_ref: "quote_axis",
        rows: 1,
        columns: 1,
        availability: "AVAILABLE",
        availability_reason_code: null,
        availability_reason: null,
        method: "FULL_RECALIBRATION",
        input_unit: "DECIMAL_RATE",
        output_unit: "USD",
        values: [["-0.95"]],
      }),
    });
  });

  await page.goto("/curves");
  await expect(page.getByRole("heading", { name: "Durable curve workflow" })).toBeVisible();
  await page.getByRole("button", { name: "Create draft" }).click();
  await page.getByRole("button", { name: "Build curve" }).click();
  await expect(page.getByRole("heading", { name: "Quote axis" })).toBeVisible();
  await page.getByRole("tab", { name: "Build" }).click();
  await page.getByRole("button", { name: "Publish version" }).click();
  await expect(page.getByText("Published USD OIS")).toBeVisible();

  await page.getByRole("tab", { name: "Pricing & Risk" }).click();
  await page.getByRole("button", { name: "Run pricing & risk" }).click();
  await expect(page.getByRole("cell", { name: "9615.38" })).toBeVisible();
  await expect(page.getByRole("heading", { name: "trade-to-node" })).toBeVisible();

  await page.getByRole("tab", { name: "Versions" }).click();
  await expect(page.getByRole("cell", { name: /USD OIS/ })).toBeVisible();
  await expect(page.getByRole("button", { name: "Clone" })).toBeVisible();
  await expect(page.getByRole("button", { name: "Export" })).toBeVisible();
  await expect(page.getByRole("button", { name: "Archive" })).toBeVisible();
});
