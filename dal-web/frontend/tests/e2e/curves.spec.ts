import { expect, test } from "@playwright/test";

test("creates, polls, visualizes and reloads a persisted calibration", async ({ page }) => {
  test.skip(
    process.env.DAL_PLAYWRIGHT_TEST_BACKEND !== "1",
    "Requires the deliberate canned DAL backend",
  );

  await page.goto("/curves");
  await expect(page.getByRole("heading", { name: "Curve Workbench" })).toBeVisible();
  await page.getByRole("button", { name: "Run calibration" }).click();

  await expect(page).toHaveURL(/\/curves\/runs\/[0-9a-f]{32}$/);
  await expect(page.getByText("completed", { exact: true })).toBeVisible({ timeout: 15_000 });
  await expect(page.getByRole("heading", { name: "Market fit & residuals" })).toBeVisible();
  await expect(page.getByRole("heading", { name: "Forward Jacobian" })).toBeVisible();
  await expect(page.getByRole("heading", { name: "Effective inverse" })).toBeVisible();

  await page.reload();
  await expect(page.getByText("completed", { exact: true })).toBeVisible();
  await expect(page.getByRole("heading", { name: "Persisted curves" })).toBeVisible();
});

test("shows failed lifecycle evidence and unavailable matrix metadata", async ({ page }) => {
  const runId = "f".repeat(32);
  const base = {
    id: runId,
    kind: "single",
    name: "failed/restarted evidence",
    created_at: "2026-01-02T00:00:00Z",
    started_at: "2026-01-02T00:00:01Z",
    finished_at: "2026-01-02T00:00:02Z",
    requested_jacobian_mode: "BUMPED",
    actual_jacobian_mode: null,
    curves: [],
    instrument_diagnostics: [],
    solver_diagnostics: null,
    fx_forwards: null,
    named_ranges: null,
    jacobian: null,
    effective_inverse: null,
    quote_bump_preview: null,
  };
  await page.route(`**/api/calibrations/${runId}`, async (route) => {
    await route.fulfill({
      status: 200,
      contentType: "application/json",
      body: JSON.stringify({
        ...base,
        status: "failed",
        phase: "finished",
        error: {
          code: "INTERRUPTED_BY_RESTART",
          message: "Calibration interrupted by backend restart",
          location: null,
          context: {},
        },
      }),
    });
  });
  await page.goto(`/curves/runs/${runId}`);
  await expect(page.getByText("INTERRUPTED_BY_RESTART")).toBeVisible();
  await expect(page.getByText("Calibration interrupted by backend restart")).toBeVisible();

  const completedId = "e".repeat(32);
  await page.route(`**/api/calibrations/${completedId}`, async (route) => {
    await route.fulfill({
      status: 200,
      contentType: "application/json",
      body: JSON.stringify({
        ...base,
        id: completedId,
        name: "matrix unavailable",
        status: "completed",
        phase: "finished",
        actual_jacobian_mode: "BUMPED",
        curves: [{
          id: "d".repeat(32),
          name: "usd_ois",
          currency: "USD",
          role: "discount",
          parameterization: "PIECEWISE_CONSTANT_FWD",
          node_dates: ["2027-01-02"],
          parameters: { right_forwards: [0.04] },
        }],
        instrument_diagnostics: [{
          instrument_id: "a".repeat(32),
          group: "single:usd",
          calibration_index: 0,
          market_rate: 0.04,
          model_rate: 0.04,
          residual: 0,
        }],
        solver_diagnostics: {
          status: "converged",
          max_abs_residual: 0,
          rms_residual: 0,
          evaluations: 1,
        },
        named_ranges: { parameters: [], residuals: [] },
        jacobian: {
          availability: "not_available_for_mode",
          shape: [1, 1],
          row_axis: [`residual:${"a".repeat(32)}`],
          column_axis: ["parameter:usd:0"],
          scaling: "unscaled",
          residual_tolerance: null,
          values: null,
        },
        effective_inverse: {
          availability: "not_requested",
          shape: [1, 1],
          row_axis: ["parameter:usd:0"],
          column_axis: [`residual:${"a".repeat(32)}`],
          scaling: "solver_scaled",
          residual_tolerance: 1e-8,
          values: null,
        },
        error: null,
      }),
    });
  });
  await page.goto(`/curves/runs/${completedId}`);
  await expect(page.getByText("not available for mode")).toBeVisible();
  await expect(page.getByText("not requested")).toBeVisible();
});
