import { expect, test } from "@playwright/test";

test("uses the deliberate canned DAL backend when requested", async ({ request }) => {
  test.skip(
    process.env.DAL_PLAYWRIGHT_TEST_BACKEND !== "1",
    "Only applies to the explicit Playwright test backend"
  );

  const response = await request.get("/api/health");
  expect(response.ok()).toBeTruthy();
  expect(await response.json()).toMatchObject({
    status: "ok",
    backend: "canned-dal",
    is_native: false,
  });
});

test("shows the dashboard shell", async ({ page }) => {
  await page.goto("/");

  await expect(page.getByRole("heading", { name: "Dashboard" })).toBeVisible();
  await expect(page.getByRole("link", { name: "Portfolios" })).toBeVisible();
  await expect(page.getByText("online")).toBeVisible();
});

test("navigates between primary pages", async ({ page }) => {
  await page.goto("/products");
  await expect(page.getByRole("heading", { name: "Product Builder" })).toBeVisible();

  await page.goto("/models");
  await expect(page.getByRole("heading", { name: "Models", exact: true })).toBeVisible();

  await page.goto("/valuations");
  await expect(page.getByRole("heading", { name: "Valuation Runs" })).toBeVisible();
});

test("creates a non-flat Dupire surface", async ({ page }) => {
  await page.goto("/models");
  await page.locator("#model-kind").selectOption("DupireModelData_");
  await page.locator("#model-name").fill("E2E skewed Dupire");
  await page.locator("#dupire-vols").fill(
    "0.24, 0.23, 0.22\n0.21, 0.20, 0.19\n0.19, 0.18, 0.17"
  );

  await page.getByRole("button", { name: "Create model" }).click();

  const row = page.getByRole("row").filter({ hasText: "E2E skewed Dupire" });
  await expect(row).toContainText("DupireModelData_");
  await expect(row).toContainText("(surface)");
});
