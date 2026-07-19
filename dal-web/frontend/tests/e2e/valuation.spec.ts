import { expect, test } from "@playwright/test";

// The app formats numbers via toLocaleString(undefined, ...); derive expected
// text instead of hard-coding en-US separators.
const TOTAL_PV_TEXT = new Intl.NumberFormat(undefined, { maximumFractionDigits: 2 }).format(8_000_000);
const PATH_COUNT_TEXT = (1024).toLocaleString();

test("runs a trade valuation end to end", async ({ page }) => {
  test.skip(
    process.env.DAL_PLAYWRIGHT_TEST_BACKEND !== "1",
    "Only applies to the explicit Playwright test backend"
  );

  await page.goto("/products");
  await page.locator("#product-name").fill("E2E call");
  await page.getByPlaceholder(/STRIKE or START/).fill("STRIKE");
  await page.getByPlaceholder(/event script/).fill("call pays MAX(spot() - STRIKE, 0.0)");
  await page.getByRole("button", { name: "Save product" }).click();
  await expect(page.getByRole("row").filter({ hasText: "E2E call" })).toBeVisible();

  await page.goto("/models");
  await page.locator("#model-name").fill("E2E BS");
  await page.getByRole("button", { name: "Create model" }).click();
  await expect(page.getByRole("row").filter({ hasText: "E2E BS" })).toBeVisible();

  await page.goto("/trades");
  await page.locator("#trade-name").fill("E2E trade");
  await page.locator("#trade-product").selectOption({ label: "E2E call" });
  await page.locator("#trade-model").selectOption({ label: "E2E BS" });
  await page.getByRole("button", { name: "Create" }).click();

  const tradeRow = page.getByRole("row").filter({ hasText: "E2E trade" });
  await expect(tradeRow).toBeVisible();
  await tradeRow.getByRole("button", { name: "Price" }).click();

  await expect(page.getByRole("heading", { name: "Price trade: E2E trade" })).toBeVisible();
  await page.getByLabel("Number of paths").fill("1024");
  await page.getByRole("button", { name: "Run valuation" }).click();

  // The backend accepts the run as pending; the panel polls until it settles.
  await expect(page.getByRole("button", { name: /submitting…|pricing…/ })).toBeDisabled();

  // Canned backend prices every trade at unit PV 8.0; notional 1,000,000 x 1.
  await expect(page.getByText("Total PV")).toBeVisible({ timeout: 15_000 });
  await expect(page.getByText(TOTAL_PV_TEXT)).toBeVisible();
  await expect(page.getByRole("heading", { name: "d_spot" })).toBeVisible();
  await expect(page.getByRole("button", { name: "Run valuation" })).toBeEnabled();

  await page.goto("/valuations");
  const runRow = page.getByRole("row").filter({ hasText: "canned-dal" }).first();
  await expect(runRow).toContainText("trade");
  await expect(runRow).toContainText("completed");
  await expect(runRow).toContainText(PATH_COUNT_TEXT);

  await runRow.getByRole("button", { name: "Details" }).click();
  await expect(page.getByRole("heading", { name: "Greeks" })).toBeVisible();
  await expect(page.getByText(/d_spot:/)).toBeVisible();
  await expect(page.getByRole("cell", { name: "E2E trade", exact: true })).toBeVisible();
});
