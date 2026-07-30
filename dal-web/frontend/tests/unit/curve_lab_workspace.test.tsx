import { act, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";
import { api } from "../../src/api/client";
import CurveLabWorkspace from "../../src/components/CurveLabWorkspace";

describe("Curve Lab V2 workspace", () => {
  afterEach(() => {
    vi.useRealTimers();
    vi.restoreAllMocks();
  });

  it("uses visual curve and instrument controls as the primary authoring surface", () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);

    render(<CurveLabWorkspace />);

    expect(screen.getByRole("heading", { name: "Curve topology" })).not.toBeNull();
    expect(screen.getByRole("heading", { name: "Calibration instruments" })).not.toBeNull();
    expect(screen.getByLabelText("As-of date")).not.toBeNull();
    expect(screen.getByLabelText("Declaration role 1")).not.toBeNull();
    expect(screen.getByLabelText("Declaration component key 1")).not.toBeNull();
    expect(screen.getByLabelText("Quote 1")).not.toBeNull();
    expect(screen.getByRole("button", { name: "Add declaration" })).not.toBeNull();
    expect(screen.getByRole("button", { name: "Add instrument" })).not.toBeNull();
    const advanced = screen.getByText("Advanced JSON").closest("details");
    expect(advanced?.open).toBe(false);
  });

  it("materializes a legal visual topology when the build mode changes", () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);

    render(<CurveLabWorkspace />);
    fireEvent.change(screen.getByLabelText("Build mode"), {
      target: { value: "STAGED_XCCY" },
    });

    expect(screen.getAllByLabelText(/Declaration role/)).toHaveLength(3);
    expect((screen.getByLabelText("Declaration role 3") as HTMLSelectElement).value).toBe("BASIS");
    expect((screen.getByLabelText("Declaration currency 1") as HTMLInputElement).value).toBe("USD");
    expect((screen.getByLabelText("Declaration currency 2") as HTMLInputElement).value).toBe("EUR");
    expect((screen.getByLabelText("Declaration currency 3") as HTMLInputElement).value).toBe("USD");
    expect(screen.getAllByLabelText(/Family/)).toHaveLength(3);
  });

  it.each([
    ["DEPOSIT", "USD", "0.04", {
      component_key: "clab/v1/local/discount/USD/OIS",
      index_name: "USD-SOFR",
      forecast_tenor: "3M",
      day_basis: "ACT_365F",
      collateral: "OIS",
      use_projection_curve: false,
    }],
    ["FRA", "USD", "0.04", {
      component_key: "clab/v1/local/discount/USD/OIS",
      index_name: "USD-SOFR",
      forecast_tenor: "3M",
      day_basis: "ACT_365F",
      collateral: "OIS",
      use_projection_curve: false,
    }],
    ["FUTURE", "USD", "95.8225", {
      component_key: "clab/v1/local/discount/USD/OIS",
      index_name: "USD-SOFR",
      forecast_tenor: "3M",
      day_basis: "ACT_365F",
      collateral: "OIS",
      use_projection_curve: false,
      convexity_adjustment: "0",
    }],
    ["OIS", "USD", "0.04", {
      component_key: "clab/v1/local/discount/USD/OIS",
      fixed_payment_frequency: "12M",
      fixed_day_basis: "ACT_365F",
      float_payment_frequency: "12M",
      float_day_basis: "ACT_365F",
      float_forecast_tenor: "3M",
      float_collateral: "OIS",
      float_use_projection_curve: false,
      index_name: "USD-SOFR",
    }],
    ["IRS", "USD", "0.04", {
      component_key: "clab/v1/local/discount/USD/OIS",
      fixed_payment_frequency: "12M",
      fixed_day_basis: "ACT_365F",
      float_payment_frequency: "3M",
      float_day_basis: "ACT_365F",
      float_forecast_tenor: "3M",
      float_collateral: "OIS",
      float_use_projection_curve: false,
      index_name: "USD-IBOR-3M",
    }],
    ["BASIS_SWAP", "USD", "0.001", {
      component_key: "clab/v1/local/discount/USD/OIS",
      spread_payment_frequency: "3M",
      spread_day_basis: "ACT_365F",
      spread_forecast_tenor: "3M",
      spread_collateral: "OIS",
      spread_use_projection_curve: false,
      reference_payment_frequency: "6M",
      reference_day_basis: "ACT_365F",
      reference_forecast_tenor: "6M",
      reference_collateral: "OIS",
      reference_use_projection_curve: false,
    }],
    ["XCCY", "USD-EUR", "0.001", {
      component_key: "clab/v1/local/discount/USD/OIS",
      domestic_notional: "1000000",
      foreign_notional: "900000",
      domestic_payment_frequency: "3M",
      domestic_day_basis: "ACT_365F",
      domestic_forecast_tenor: "3M",
      domestic_collateral: "OIS",
      domestic_use_projection_curve: false,
      foreign_payment_frequency: "3M",
      foreign_day_basis: "ACT_365F",
      foreign_forecast_tenor: "3M",
      foreign_collateral: "OIS",
      foreign_use_projection_curve: false,
      fx_spot: 1.1,
      fx_forward_collateral: "OIS",
    }],
  ])(
    "creates a legal %s draft from the primary visual family control",
    async (family, currencyOrPair, rawQuote, terms) => {
      vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
      const create = vi.spyOn(api, "createCurveLabDraft").mockImplementation(
        async (body) => ({
          id: "a".repeat(32),
          schema_version: 2,
          revision: 1,
          fingerprint: "b".repeat(64),
          state: "READY_TO_BUILD",
          document: body as Record<string, unknown>,
          created_at: "2026-01-15T00:00:00Z",
          updated_at: "2026-01-15T00:00:00Z",
        }),
      );

      render(<CurveLabWorkspace />);
      fireEvent.change(screen.getByLabelText("Family 1"), {
        target: { value: family },
      });
      fireEvent.click(screen.getByRole("button", { name: "Create draft" }));

      await waitFor(() => expect(create).toHaveBeenCalledOnce());
      const body = create.mock.calls[0][0] as {
        instruments: Record<string, unknown>[];
      };
      expect(body.instruments[0]).toMatchObject({
        instrument_type: family,
        currency_or_pair: currencyOrPair,
        raw_quote: rawQuote,
      });
      expect(body.instruments[0].terms).toEqual(terms);
    },
  );

  it("authors dependency version ids with visible controls", async () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([
      {
        id: "e".repeat(32),
        source_kind: "BUILD",
        build_run_id: "c".repeat(32),
        import_job_id: null,
        name: "EUR OIS",
        version_note: null,
        tags: [],
        native_payload_length: 10,
        native_payload_hash: "f".repeat(64),
        root_kind: "DISCOUNT_CURVE",
        build_validation_state: "VERIFIED",
        visibility_state: "VISIBLE",
        created_at: "2026-01-15T00:00:02Z",
      },
    ]);

    render(<CurveLabWorkspace />);
    const dependency = await screen.findByLabelText("Use EUR OIS as dependency");
    fireEvent.click(dependency);

    expect((dependency as HTMLInputElement).checked).toBe(true);
  });

  it("builds, publishes, and exposes all four durable workflow tabs", async () => {
    const draft = {
      id: "a".repeat(32),
      schema_version: 2 as const,
      revision: 1,
      fingerprint: "b".repeat(64),
      state: "READY_TO_BUILD" as const,
      document: {},
      created_at: "2026-01-15T00:00:00Z",
      updated_at: "2026-01-15T00:00:00Z",
    };
    const build = {
      id: "c".repeat(32),
      draft_id: draft.id,
      draft_revision: 1,
      draft_fingerprint: draft.fingerprint,
      state: "SUCCEEDED",
      stale: false,
      request: {},
      resolved_plan: { mode: "SINGLE" },
      quote_axis: [],
      parameter_axis: [],
      dependency_manifest: [],
      diagnostics: { fit_state: "NATIVE_ARCHIVE_VALIDATED" },
      native_payload_hash: "d".repeat(64),
      error: null,
      created_at: "2026-01-15T00:00:00Z",
      finished_at: "2026-01-15T00:00:01Z",
    };
    const version = {
      id: "e".repeat(32),
      source_kind: "BUILD" as const,
      build_run_id: build.id,
      import_job_id: null,
      name: "USD OIS",
      version_note: null,
      tags: [],
      native_payload_length: 10,
      native_payload_hash: "f".repeat(64),
      root_kind: "DISCOUNT_CURVE" as const,
      build_validation_state: "VERIFIED" as const,
      visibility_state: "VISIBLE" as const,
      created_at: "2026-01-15T00:00:02Z",
    };
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    vi.spyOn(api, "createCurveLabDraft").mockResolvedValue(draft);
    vi.spyOn(api, "createCurveLabBuildRun").mockResolvedValue(build);
    vi.spyOn(api, "createCurveLabVersion").mockResolvedValue(version);

    render(<CurveLabWorkspace />);

    for (const name of ["Build", "Runs", "Pricing & Risk", "Versions"]) {
      expect(screen.getByRole("tab", { name })).not.toBeNull();
    }
    fireEvent.click(screen.getByRole("button", { name: "Create draft" }));
    await waitFor(() => expect(api.createCurveLabDraft).toHaveBeenCalledOnce());
    fireEvent.click(screen.getByRole("button", { name: "Build curve" }));
    await waitFor(() => expect(api.createCurveLabBuildRun).toHaveBeenCalledWith(draft.id));
    fireEvent.click(screen.getByRole("tab", { name: "Build" }));
    fireEvent.change(screen.getByLabelText("Version name"), {
      target: { value: "USD OIS" },
    });
    fireEvent.click(screen.getByRole("button", { name: "Publish version" }));
    await waitFor(() => expect(api.createCurveLabVersion).toHaveBeenCalledWith(
      expect.objectContaining({
        draft_id: draft.id,
        build_run_id: build.id,
        name: "USD OIS",
      }),
    ));
    expect(screen.getByText("Published USD OIS")).not.toBeNull();
  });

  it("keeps an admitted build and reaches terminal after more than ten seconds", async () => {
    vi.useFakeTimers();
    const draft = {
      id: "a".repeat(32),
      schema_version: 2 as const,
      revision: 1,
      fingerprint: "b".repeat(64),
      state: "READY_TO_BUILD" as const,
      document: {},
      created_at: "2026-01-15T00:00:00Z",
      updated_at: "2026-01-15T00:00:00Z",
    };
    const admitted = {
      id: "c".repeat(32),
      draft_id: draft.id,
      draft_revision: 1,
      draft_fingerprint: draft.fingerprint,
      state: "QUEUED",
      stale: false,
      request: {},
      resolved_plan: { mode: "SINGLE" },
      quote_axis: [],
      parameter_axis: [],
      dependency_manifest: [],
      diagnostics: null,
      native_payload_hash: null,
      error: null,
      created_at: "2026-01-15T00:00:00Z",
      finished_at: null,
    };
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    vi.spyOn(api, "createCurveLabDraft").mockResolvedValue(draft);
    vi.spyOn(api, "createCurveLabBuildRun").mockResolvedValue(admitted);
    const load = vi.spyOn(api, "getCurveLabBuildRun").mockImplementation(
      async () => load.mock.calls.length > 205
        ? { ...admitted, state: "SUCCEEDED", finished_at: "2026-01-15T00:00:11Z" }
        : admitted,
    );

    render(<CurveLabWorkspace />);
    await act(async () => {
      fireEvent.click(screen.getByRole("button", { name: "Create draft" }));
      await Promise.resolve();
    });
    await act(async () => {
      fireEvent.click(screen.getByRole("button", { name: "Build curve" }));
      await Promise.resolve();
    });

    expect(screen.getByText(/Build cccccccc admitted QUEUED/)).not.toBeNull();
    await act(async () => {
      await vi.advanceTimersByTimeAsync(10_500);
    });

    expect(load.mock.calls.length).toBeGreaterThan(200);
    expect(screen.getByText("Build cccccccc finished SUCCEEDED.")).not.toBeNull();
    expect(screen.queryByText(/UI deadline/)).toBeNull();
  });

  it("retains an admitted build id and resumes polling after a transport error", async () => {
    vi.useFakeTimers();
    const draft = {
      id: "a".repeat(32),
      schema_version: 2 as const,
      revision: 1,
      fingerprint: "b".repeat(64),
      state: "READY_TO_BUILD" as const,
      document: {},
      created_at: "2026-01-15T00:00:00Z",
      updated_at: "2026-01-15T00:00:00Z",
    };
    const admitted = {
      id: "c".repeat(32),
      draft_id: draft.id,
      draft_revision: 1,
      draft_fingerprint: draft.fingerprint,
      state: "RUNNING",
      stale: false,
      request: {},
      resolved_plan: { mode: "SINGLE" },
      quote_axis: [],
      parameter_axis: [],
      dependency_manifest: [],
      diagnostics: null,
      native_payload_hash: null,
      error: null,
      created_at: "2026-01-15T00:00:00Z",
      finished_at: null,
    };
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    vi.spyOn(api, "createCurveLabDraft").mockResolvedValue(draft);
    vi.spyOn(api, "createCurveLabBuildRun").mockResolvedValue(admitted);
    const load = vi.spyOn(api, "getCurveLabBuildRun")
      .mockRejectedValueOnce(new Error("network unavailable"))
      .mockResolvedValueOnce({
        ...admitted,
        state: "SUCCEEDED",
        finished_at: "2026-01-15T00:00:11Z",
      });

    render(<CurveLabWorkspace />);
    await act(async () => {
      fireEvent.click(screen.getByRole("button", { name: "Create draft" }));
      await Promise.resolve();
    });
    await act(async () => {
      fireEvent.click(screen.getByRole("button", { name: "Build curve" }));
      await Promise.resolve();
      await vi.advanceTimersByTimeAsync(50);
    });

    expect(screen.getByText("network unavailable")).not.toBeNull();
    const resume = screen.getByRole("button", {
      name: "Resume build polling cccccccc",
    });
    await act(async () => {
      fireEvent.click(resume);
      await vi.advanceTimersByTimeAsync(50);
    });

    expect(load).toHaveBeenCalledTimes(2);
    expect(screen.getByText("Build cccccccc finished SUCCEEDED.")).not.toBeNull();
  });
});
