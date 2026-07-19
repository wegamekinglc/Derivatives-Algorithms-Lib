import { beforeEach, describe, expect, it, vi } from "vitest";
import { fireEvent, render, screen } from "@testing-library/react";
import ValuationPanel from "../../src/components/ValuationPanel";
import { api, type ValuationConfig, type ValuationResult } from "../../src/api/client";

function makeResult(overrides: Partial<ValuationResult> = {}): ValuationResult {
  return {
    id: "v1",
    target_kind: "trade",
    target_id: "t1",
    backend: "canned-dal",
    is_native: false,
    config: {
      num_paths: 1024,
      method: "sobol",
      use_brownian_bridge: false,
      enable_aad: true,
      smooth: 0.01,
      evaluation_date: "2022-09-15",
    },
    total_pv: 8_000_000,
    total_greeks: { d_spot: 500_000, d_vol: 200_000 },
    trades: [
      { trade_id: "t1", trade_name: "Trade One", pv: 8, scaled_pv: 8_000_000, greeks: { d_spot: 500_000 } },
    ],
    created_at: "2026-07-19T00:00:00Z",
    status: "completed",
    ...overrides,
  };
}

function runButton(): HTMLButtonElement {
  return screen.getByRole("button", { name: /run valuation|pricing|submitting/i });
}

// fmtMoney renders via toLocaleString(undefined, ...); derive the expected
// text instead of hard-coding en-US separators.
const TOTAL_PV_TEXT = new Intl.NumberFormat(undefined, { maximumFractionDigits: 2 }).format(8_000_000);

describe("ValuationPanel", () => {
  let getValuationSpy: ReturnType<typeof vi.spyOn>;

  beforeEach(() => {
    getValuationSpy = vi.spyOn(api, "getValuation");
  });

  it("submits the configured valuation request and renders an immediately-completed result", async () => {
    const onRun = vi.fn<(config: ValuationConfig) => Promise<ValuationResult>>().mockResolvedValue(makeResult());
    render(<ValuationPanel onRun={onRun} />);

    fireEvent.change(screen.getByLabelText("Number of paths"), { target: { value: "2048" } });
    fireEvent.change(screen.getByLabelText("RNG method"), { target: { value: "pseudo" } });
    fireEvent.click(screen.getByLabelText("Enable AAD (Greeks)"));
    fireEvent.click(screen.getByLabelText("Brownian bridge"));
    fireEvent.change(screen.getByLabelText("Evaluation date"), { target: { value: "" } });
    fireEvent.click(runButton());

    await screen.findByText("Total PV");
    expect(onRun).toHaveBeenCalledTimes(1);
    expect(onRun.mock.calls[0][0]).toEqual({
      num_paths: 2048,
      method: "pseudo",
      use_brownian_bridge: true,
      enable_aad: false,
      smooth: 0.01,
      evaluation_date: null,
    });
    expect(getValuationSpy).not.toHaveBeenCalled();
    expect(screen.getByText(TOTAL_PV_TEXT)).toBeTruthy();
    expect(screen.getByText("d_spot")).toBeTruthy();
    expect(screen.getByText("d_vol")).toBeTruthy();
    expect(runButton().disabled).toBe(false);
  });

  it("clamps the path count to the allowed maximum", async () => {
    const onRun = vi.fn<(config: ValuationConfig) => Promise<ValuationResult>>().mockResolvedValue(makeResult());
    render(<ValuationPanel onRun={onRun} />);

    const input = screen.getByLabelText("Number of paths") as HTMLInputElement;
    fireEvent.change(input, { target: { value: "999999999" } });
    expect(input.value).toBe(String(2 ** 24));

    fireEvent.click(runButton());
    await screen.findByText("Total PV");
    expect(onRun.mock.calls[0][0].num_paths).toBe(2 ** 24);
  });

  it("resets the path count to 1 on non-numeric input", () => {
    const onRun = vi.fn<(config: ValuationConfig) => Promise<ValuationResult>>();
    render(<ValuationPanel onRun={onRun} />);

    const input = screen.getByLabelText("Number of paths") as HTMLInputElement;
    fireEvent.change(input, { target: { value: "" } });
    expect(input.value).toBe("1");
    expect(onRun).not.toHaveBeenCalled();
  });

  it("polls while running and renders cards once completed", async () => {
    const onRun = vi
      .fn<(config: ValuationConfig) => Promise<ValuationResult>>()
      .mockResolvedValue(makeResult({ status: "running" }));
    getValuationSpy
      .mockResolvedValueOnce(makeResult({ status: "running" }))
      .mockResolvedValueOnce(makeResult());
    render(<ValuationPanel onRun={onRun} />);

    fireEvent.click(runButton());

    expect(await screen.findByRole("button", { name: "pricing…" })).toHaveProperty("disabled", true);

    await screen.findByText("Total PV", undefined, { timeout: 3000 });
    expect(getValuationSpy).toHaveBeenCalledTimes(2);
    expect(getValuationSpy).toHaveBeenCalledWith("v1");
    expect(screen.getByText(TOTAL_PV_TEXT)).toBeTruthy();
    expect(screen.queryByText("Unit PV")).toBeNull();
    expect(runButton().disabled).toBe(false);
  });

  it("renders the per-trade table for multi-trade results", async () => {
    const onRun = vi.fn<(config: ValuationConfig) => Promise<ValuationResult>>().mockResolvedValue(
      makeResult({
        trades: [
          { trade_id: "t1", trade_name: "Trade One", pv: 8, scaled_pv: 8_000_000, greeks: {} },
          { trade_id: "t2", trade_name: "Trade Two", pv: 3, scaled_pv: 3_000_000, greeks: {} },
        ],
      }),
    );
    render(<ValuationPanel onRun={onRun} />);

    fireEvent.click(runButton());

    await screen.findByText("Unit PV");
    expect(screen.getByText("Trade One")).toBeTruthy();
    expect(screen.getByText("Trade Two")).toBeTruthy();
  });

  it("surfaces a server-side valuation failure without result cards", async () => {
    const onRun = vi
      .fn<(config: ValuationConfig) => Promise<ValuationResult>>()
      .mockResolvedValue(makeResult({ status: "running" }));
    getValuationSpy.mockResolvedValue(makeResult({ status: "failed" }));
    render(<ValuationPanel onRun={onRun} />);

    fireEvent.click(runButton());

    await screen.findByText("Valuation failed on the server. Check the backend logs.", undefined, {
      timeout: 3000,
    });
    expect(screen.queryByText("Total PV")).toBeNull();
    expect(runButton().disabled).toBe(false);
  });

  it("surfaces submission errors and re-enables the form", async () => {
    const onRun = vi
      .fn<(config: ValuationConfig) => Promise<ValuationResult>>()
      .mockRejectedValue(new Error("404: trade not found"));
    render(<ValuationPanel onRun={onRun} />);

    fireEvent.click(runButton());

    await screen.findByText(/404: trade not found/);
    expect(runButton().disabled).toBe(false);
  });

  it("disables the run button while the submission is in flight", async () => {
    let resolveRun: (result: ValuationResult) => void = () => undefined;
    const onRun = vi.fn<(config: ValuationConfig) => Promise<ValuationResult>>().mockImplementation(
      () =>
        new Promise<ValuationResult>((resolve) => {
          resolveRun = resolve;
        }),
    );
    render(<ValuationPanel onRun={onRun} />);

    fireEvent.click(runButton());

    const busy = (await screen.findByRole("button", { name: "submitting…" })) as HTMLButtonElement;
    expect(busy.disabled).toBe(true);

    resolveRun(makeResult());
    await screen.findByText("Total PV");
    expect(runButton().disabled).toBe(false);
  });
});
