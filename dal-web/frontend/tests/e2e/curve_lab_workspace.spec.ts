import { expect, test } from "@playwright/test";

test("persists stale rebuild and version file actions through the real API", async ({
  page,
}) => {
  test.skip(
    process.env.DAL_PLAYWRIGHT_TEST_BACKEND !== "1",
    "requires the guarded canned DAL FastAPI backend",
  );

  await page.goto("/curves");
  const source = page.getByLabel("Build document JSON");
  const publish = page.getByRole("button", { name: "Publish version" });

  await page.getByRole("button", { name: "Create draft" }).click();
  await expect(page.getByText(/revision 1 is ready/)).toBeVisible();
  await page.getByRole("button", { name: "Build curve" }).click();
  await expect(page.getByText(/finished SUCCEEDED/)).toBeVisible();
  await page.getByRole("tab", { name: "Build" }).click();
  await publish.click();
  await expect(page.getByText("Published USD OIS")).toBeVisible();

  await source.fill((await source.inputValue()).replace('"raw_quote": "0.04"', '"raw_quote": "0.041"'));
  await page.getByRole("button", { name: "Save draft changes" }).click();
  await expect(page.getByText(/revision 2; rebuild required/)).toBeVisible();
  await expect(publish).toBeDisabled();

  await page.getByRole("button", { name: "Build curve" }).click();
  await expect(page.getByText(/finished SUCCEEDED/)).toBeVisible();
  await page.getByRole("tab", { name: "Build" }).click();
  await publish.click();
  await expect(page.getByText("Published USD OIS")).toBeVisible();

  await page.getByRole("tab", { name: "Versions" }).click();
  let rows = page.getByRole("row").filter({ hasText: "USD OIS" });
  await expect(rows).toHaveCount(2);

  const downloadPromise = page.waitForEvent("download");
  await rows.first().getByRole("button", { name: "Export" }).click();
  const download = await downloadPromise;
  const exportedPath = await download.path();
  expect(exportedPath).not.toBeNull();
  await expect(page.getByText("Exported native JSON for USD OIS.")).toBeVisible();

  await rows.first().getByRole("button", { name: "Clone" }).click();
  await expect(page.getByText(/Cloned USD OIS into draft/)).toBeVisible();
  await expect(page.getByLabel("Build document JSON")).toHaveValue(
    /"source_instrument_id": "[0-9a-f]{32}"/,
  );

  await page.getByRole("tab", { name: "Versions" }).click();
  rows = page.getByRole("row").filter({ hasText: "USD OIS" });
  await rows.first().getByRole("button", { name: "Archive" }).click();
  await expect(page.getByText("Archived USD OIS.")).toBeVisible();
  await expect(page.getByRole("row").filter({ hasText: "USD OIS" })).toHaveCount(1);

  if (exportedPath === null) throw new Error("Playwright did not retain the export");
  const importInput = page.locator('input[type="file"][accept*="application/json"]');
  await importInput.setInputFiles(exportedPath);
  await expect(page.getByText(/Import [0-9a-f]{8} finished SUCCEEDED/)).toBeVisible();
  await expect(page.getByRole("row").filter({ hasText: "Imported DiscountPWC" })).toHaveCount(1);

  await importInput.setInputFiles({
    name: "malformed.json",
    mimeType: "application/json",
    buffer: Buffer.from('{"~type":"Bag"} trailing'),
  });
  await expect(page.getByText(/ARCHIVE_JSON_TRAILING_BYTES/)).toBeVisible();
});
