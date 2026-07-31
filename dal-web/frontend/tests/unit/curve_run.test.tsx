import { readFileSync } from "node:fs";
import { render, screen } from "@testing-library/react";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import { describe, expect, it, vi } from "vitest";
import { api, type CalibrationRun } from "../../src/api/client";
import CurveRun from "../../src/pages/CurveRun";

const styles = readFileSync("src/styles.css", "utf8");

function runningRun(): CalibrationRun {
  return {
    id: "run-1",
    kind: "single",
    name: "USD OIS",
    status: "running",
    phase: "solving",
    created_at: "2026-01-15T10:00:00Z",
    started_at: "2026-01-15T10:00:01Z",
    finished_at: null,
    requested_jacobian_mode: "ANALYTIC",
    actual_jacobian_mode: null,
    curves: [],
    instrument_diagnostics: [],
    solver_diagnostics: null,
    fx_forwards: null,
    named_ranges: null,
    jacobian: null,
    effective_inverse: null,
    quote_bump_preview: null,
    error: null,
  };
}

describe("CurveRun running state", () => {
  it("keeps meaningful running text while hiding the animated glyph", async () => {
    vi.spyOn(api, "getCalibration").mockResolvedValue(runningRun());

    render(
      <MemoryRouter initialEntries={["/curves/runs/run-1"]}>
        <Routes>
          <Route path="/curves/runs/:runId" element={<CurveRun />} />
        </Routes>
      </MemoryRouter>,
    );

    expect(
      await screen.findByRole("heading", { name: "Native solve in progress" }),
    ).not.toBeNull();
    expect(screen.getByText(/Polling persisted phase:/)).not.toBeNull();
    expect(document.querySelector(".spinner")?.getAttribute("aria-hidden")).toBe("true");
  });

  it("uses an opacity pulse and disables every repeating page animation for reduced motion", () => {
    expect(styles).not.toMatch(/@keyframes\s+spin|rotate\s*\(/);
    expect(styles).toMatch(
      /\.spinner\s*\{[^}]*animation:\s*pulse\s+1\.5s\s+ease-in-out\s+infinite;/s,
    );

    const repeatingSelectors = Array.from(
      styles.matchAll(/([^{}]+)\{[^{}]*animation:[^;]*infinite;[^{}]*\}/g),
      (match) => match[1].trim(),
    );
    expect(repeatingSelectors).toEqual([
      ".brand::before",
      ".status-dot",
      ".spinner",
      ".status-running::before",
    ]);
    const reducedMotionStart = styles.indexOf("@media (prefers-reduced-motion: reduce)");
    expect(reducedMotionStart).toBeGreaterThanOrEqual(0);
    const reducedMotion = styles.slice(reducedMotionStart);
    for (const selector of repeatingSelectors) {
      expect(reducedMotion).toContain(selector);
    }
    expect(reducedMotion).toMatch(/animation:\s*none;/);
  });
});
