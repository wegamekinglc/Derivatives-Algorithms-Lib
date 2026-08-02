import { expect, test, type Download, type Route } from "@playwright/test";

test("canonical authoring drives identical persisted and replayed financial identity", async ({
  page,
}) => {
  test.setTimeout(120_000);
  test.skip(
    process.env.DAL_PLAYWRIGHT_TEST_BACKEND !== "1",
    "requires the guarded canned DAL FastAPI backend",
  );

  await page.goto("/curves");
  await page.getByText("Quote authoring tools").click();
  await page.getByLabel("Version name").fill("Canonical equivalence");
  await page.getByLabel("Canonical quote target 1").check();

  const canonicalize = async (convention: "PERCENT" | "DECIMAL", lexeme: string) => {
    await page.getByLabel("Input convention").selectOption(convention);
    await page.getByLabel("Quote lexeme").fill(lexeme);
    const responsePromise = page.waitForResponse(
      (response) => response.url().endsWith("/api/curve-lab/quote-canonicalizations")
        && response.request().method() === "POST",
    );
    await page.getByRole("button", { name: "Canonicalize quote" }).click();
    const response = await responsePromise;
    expect(response.status()).toBe(200);
    const canonical = await response.json() as {
      instrument_type: string;
      raw_quote: string;
      normalized_quote: string;
    };
    expect(canonical).toMatchObject({
      instrument_type: "DEPOSIT",
      raw_quote: "0.04",
      normalized_quote: "0.04",
    });
    await expect(page.getByLabel("Quote 1")).toHaveValue("0.04");
  };

  const createDraft = async () => {
    const responsePromise = page.waitForResponse(
      (response) => response.url().endsWith("/api/curve-lab/drafts")
        && response.request().method() === "POST",
    );
    await page.getByRole("button", { name: "Create draft" }).click();
    const response = await responsePromise;
    expect(response.status()).toBe(201);
    return await response.json() as {
      id: string;
      revision: number;
      fingerprint: string;
      document: Record<string, unknown> & {
        instruments: Record<string, unknown>[];
      };
    };
  };

  const buildPublishAndRisk = async () => {
    const buildResponsePromise = page.waitForResponse(
      (response) => response.url().includes("/api/curve-lab/drafts/")
        && response.url().endsWith("/build-runs")
        && response.request().method() === "POST",
    );
    await page.getByRole("button", { name: "Build curve" }).click();
    const admittedBuild = await (await buildResponsePromise).json() as { id: string };
    await expect(
      page.getByText(`Build ${admittedBuild.id.slice(0, 8)} finished SUCCEEDED.`),
    ).toBeVisible({ timeout: 30_000 });
    const buildResponse = await page.request.get(
      `/api/curve-lab/build-runs/${admittedBuild.id}`,
    );
    expect(buildResponse.status()).toBe(200);
    const build = await buildResponse.json() as {
      id: string;
      state: string;
      stale: boolean;
      quote_axis: Record<string, unknown>[];
    };

    await page.getByRole("tab", { name: "Build" }).click();
    const versionResponsePromise = page.waitForResponse(
      (response) => response.url().endsWith("/api/curve-lab/versions")
        && response.request().method() === "POST",
    );
    await page.getByRole("button", { name: "Publish version" }).click();
    const versionResponse = await versionResponsePromise;
    expect(versionResponse.status()).toBe(201);
    const version = await versionResponse.json() as {
      id: string;
      native_payload_hash: string;
    };

    await page.getByRole("tab", { name: "Pricing & Risk" }).click();
    const riskResponsePromise = page.waitForResponse(
      (response) => response.url().endsWith("/api/curve-lab/risk-runs")
        && response.request().method() === "POST",
    );
    await page.getByRole("button", { name: "Run pricing & risk" }).click();
    const admittedRisk = await (await riskResponsePromise).json() as { id: string };
    await expect(
      page.getByText(new RegExp(`Risk run ${admittedRisk.id.slice(0, 8)} finished`)),
    ).toBeVisible({ timeout: 30_000 });
    const riskResponse = await page.request.get(
      `/api/curve-lab/risk-runs/${admittedRisk.id}`,
    );
    expect(riskResponse.status()).toBe(200);
    const risk = await riskResponse.json() as {
      id: string;
      state: string;
      quote_axis: Record<string, unknown>[];
      result: Record<string, unknown>;
      error: Record<string, unknown> | null;
    };
    expect(risk.state, JSON.stringify(risk.error)).toBe("SUCCEEDED");
    const replayResponse = await page.request.get(
      `/api/curve-lab/risk-runs/${admittedRisk.id}`,
    );
    expect(await replayResponse.json()).toEqual(risk);
    const matrixResponse = await page.request.get(
      `/api/curve-lab/risk-runs/${admittedRisk.id}/matrices/key-rate-dv01`,
    );
    expect(matrixResponse.status()).toBe(200);

    return {
      build,
      version,
      risk,
      keyRateMatrix: await matrixResponse.json() as Record<string, unknown>,
    };
  };

  await canonicalize("PERCENT", "4");
  const decimalRender = page.waitForResponse(
    (response) => response.url().endsWith("/api/curve-lab/quote-renderings")
      && response.request().method() === "POST",
  );
  await page.getByLabel("Display scale").fill("6");
  expect((await decimalRender).status()).toBe(200);
  const percentRender = page.waitForResponse(
    (response) => response.url().endsWith("/api/curve-lab/quote-renderings")
      && response.request().method() === "POST",
  );
  await page.getByLabel("Display convention").selectOption("PERCENT");
  const rendered = await percentRender;
  expect(rendered.status()).toBe(200);
  expect(await rendered.json()).toEqual({ rendered_quote: "4.000000" });
  await expect(page.getByText("4.000000", { exact: true })).toBeVisible();
  const percentDraft = await createDraft();
  const percentEvidence = await buildPublishAndRisk();

  await page.getByRole("tab", { name: "Build" }).click();
  await page.getByLabel("Display scale").fill("5");
  const unchangedBuildResponse = await page.request.get(
    `/api/curve-lab/build-runs/${percentEvidence.build.id}`,
  );
  const unchangedBuild = await unchangedBuildResponse.json() as {
    stale: boolean;
    draft_fingerprint: string;
  };
  expect(unchangedBuild.stale).toBe(false);
  expect(unchangedBuild.draft_fingerprint).toBe(percentDraft.fingerprint);

  await canonicalize("DECIMAL", "0.04");
  const decimalDraft = await createDraft();
  const decimalEvidence = await buildPublishAndRisk();

  expect(decimalDraft.document).toEqual(percentDraft.document);
  expect(decimalDraft.fingerprint).toBe(percentDraft.fingerprint);
  expect(decimalEvidence.build.quote_axis).toEqual(percentEvidence.build.quote_axis);
  expect(decimalEvidence.version.native_payload_hash).toBe(
    percentEvidence.version.native_payload_hash,
  );
  expect(decimalEvidence.risk.quote_axis).toEqual(percentEvidence.risk.quote_axis);
  expect(decimalEvidence.risk.result).toEqual(percentEvidence.risk.result);
  expect(decimalEvidence.keyRateMatrix).toMatchObject({
    availability: percentEvidence.keyRateMatrix.availability,
    orientation: percentEvidence.keyRateMatrix.orientation,
    values: percentEvidence.keyRateMatrix.values,
  });
});

test("latest same-target canonicalization wins when browser responses finish out of order", async ({
  page,
}) => {
  test.skip(
    process.env.DAL_PLAYWRIGHT_TEST_BACKEND !== "1",
    "requires the guarded canned DAL FastAPI backend",
  );

  const pending: Route[] = [];
  await page.route("**/api/curve-lab/quote-canonicalizations", async (route) => {
    pending.push(route);
  });
  await page.goto("/curves");
  await page.getByText("Quote authoring tools").click();
  await page.getByLabel("Canonical quote target 1").check();
  await page.getByLabel("Input convention").selectOption("PERCENT");
  await page.getByLabel("Quote lexeme").fill("4");
  await page.getByRole("button", { name: "Canonicalize quote" }).click();
  await page.getByLabel("Quote lexeme").fill("5");
  await page.getByRole("button", { name: "Canonicalize quote" }).click();
  await expect.poll(() => pending.length).toBe(2);

  const response = (rawQuote: string) => ({
    instrument_type: "DEPOSIT",
    quote_coordinate_kind: "RATE",
    canonical_raw_unit: "DECIMAL",
    raw_quote: rawQuote,
    normalized_quote: rawQuote,
    normalized_unit: "DECIMAL_RATE",
    exact_risk_raw_bump: "0.0001",
    normalized_risk_bump: "0.0001",
  });
  await pending[1]?.fulfill({
    status: 200,
    contentType: "application/json",
    body: JSON.stringify(response("0.05")),
  });
  await expect(page.getByLabel("Quote 1")).toHaveValue("0.05");
  await pending[0]?.fulfill({
    status: 200,
    contentType: "application/json",
    body: JSON.stringify(response("0.04")),
  });
  await expect(page.getByLabel("Quote 1")).toHaveValue("0.05");

  const draftResponse = page.waitForResponse(
    (candidate) => candidate.url().endsWith("/api/curve-lab/drafts")
      && candidate.request().method() === "POST",
  );
  await page.getByRole("button", { name: "Create draft" }).click();
  const created = await draftResponse;
  expect(created.status()).toBe(201);
  const draft = await created.json() as {
    document: { instruments: { raw_quote: string }[] };
  };
  expect(draft.document.instruments[0].raw_quote).toBe("0.05");
});

test("creates a legal draft for every family through the visual control", async ({
  page,
}) => {
  test.skip(
    process.env.DAL_PLAYWRIGHT_TEST_BACKEND !== "1",
    "requires the guarded canned DAL FastAPI backend",
  );

  await page.goto("/curves");
  for (const family of [
    "DEPOSIT",
    "FRA",
    "FUTURE",
    "OIS",
    "IRS",
    "BASIS_SWAP",
    "XCCY",
  ]) {
    await page.getByLabel("Family 1").selectOption(family);
    const responsePromise = page.waitForResponse(
      (response) => response.url().endsWith("/api/curve-lab/drafts")
        && response.request().method() === "POST",
    );
    await page.getByRole("button", { name: "Create draft" }).click();
    const response = await responsePromise;
    expect(response.status()).toBe(201);
    const created = await response.json() as {
      document: { instruments: { instrument_type: string }[] };
    };
    expect(created.document.instruments[0].instrument_type).toBe(family);
  }
});

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
