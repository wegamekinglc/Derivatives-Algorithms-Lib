import { expect, test, type Download } from "@playwright/test";

test("persists stale rebuild and version file actions through the real API", async ({
  page,
}) => {
  test.skip(
    process.env.DAL_PLAYWRIGHT_TEST_BACKEND !== "1",
    "requires the guarded canned DAL FastAPI backend",
  );

  await page.goto("/curves");
  const publish = page.getByRole("button", { name: "Publish version" });

  await page.getByRole("button", { name: "Create draft" }).click();
  await expect(page.getByText(/revision 1 is ready/)).toBeVisible();
  await page.getByRole("button", { name: "Build curve" }).click();
  await expect(page.getByText(/finished SUCCEEDED/)).toBeVisible();
  await page.getByRole("tab", { name: "Build" }).click();
  await publish.click();
  await expect(page.getByText("Published USD OIS")).toBeVisible();

  await page.getByLabel("Quote 1").fill("0.041");
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

  const downloads: Download[] = [];
  page.on("download", (download) => downloads.push(download));
  await rows.first().getByRole("button", { name: "Export" }).click();
  await expect.poll(() => downloads.length).toBe(2);
  const nativeDownload = downloads.find(
    (download) => !download.suggestedFilename().endsWith(".manifest.json"),
  );
  const manifestDownload = downloads.find(
    (download) => download.suggestedFilename().endsWith(".manifest.json"),
  );
  const exportedPath = await nativeDownload?.path() ?? null;
  const manifestPath = await manifestDownload?.path() ?? null;
  expect(exportedPath).not.toBeNull();
  expect(manifestPath).not.toBeNull();
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
  if (manifestPath === null) throw new Error("Playwright did not retain the manifest");
  await page.getByLabel("Select runtime manifest").setInputFiles(manifestPath);
  const importInput = page.getByLabel("Import native JSON");
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
