import { expect, test } from "@playwright/test";

test("runs a trade valuation end to end", async ({ page }) => {
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
  await expect(page.getByText("8,000,000")).toBeVisible();
  await expect(page.getByRole("heading", { name: "d_spot" })).toBeVisible();
  await expect(page.getByRole("button", { name: "Run valuation" })).toBeEnabled();

  await page.goto("/valuations");
  const runRow = page.getByRole("row").filter({ hasText: "canned-dal" }).first();
  await expect(runRow).toContainText("trade");
  await expect(runRow).toContainText("completed");
  await expect(runRow).toContainText("1,024");

  await runRow.getByRole("button", { name: "Details" }).click();
  await expect(page.getByRole("heading", { name: "Greeks" })).toBeVisible();
  await expect(page.getByText(/d_spot:/)).toBeVisible();
  await expect(page.getByRole("cell", { name: "E2E trade", exact: true })).toBeVisible();
});
