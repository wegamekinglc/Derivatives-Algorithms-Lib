import { expect, test } from "@playwright/test";

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
  await expect(page.getByRole("heading", { name: "Models" })).toBeVisible();

  await page.goto("/valuations");
  await expect(page.getByRole("heading", { name: "Valuation Runs" })).toBeVisible();
});
