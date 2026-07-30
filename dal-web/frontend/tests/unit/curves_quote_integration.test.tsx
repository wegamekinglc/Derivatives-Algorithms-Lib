import { act, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";
import { MemoryRouter } from "react-router-dom";
import { api } from "../../src/api/client";
import Curves from "../../src/pages/Curves";

const CANONICAL_DEPOSIT = {
  instrument_type: "DEPOSIT" as const,
  quote_coordinate_kind: "RATE" as const,
  canonical_raw_unit: "DECIMAL" as const,
  raw_quote: "0.04",
  normalized_quote: "0.04",
  normalized_unit: "DECIMAL_RATE" as const,
  exact_risk_raw_bump: "0.0001",
  normalized_risk_bump: "0.0001",
};

describe("Curve Lab production quote integration", () => {
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it("writes server canonical bytes into the selected workspace instrument", async () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    vi.spyOn(api, "canonicalizeCurveLabQuote").mockResolvedValue(
      CANONICAL_DEPOSIT,
    );
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

    render(
      <MemoryRouter>
        <Curves />
      </MemoryRouter>,
    );
    fireEvent.change(screen.getByLabelText("Quote 1"), {
      target: { value: "9.9" },
    });
    fireEvent.click(screen.getByLabelText("Canonical quote target 1"));
    fireEvent.change(screen.getByLabelText("Input convention"), {
      target: { value: "PERCENT" },
    });
    fireEvent.change(screen.getByLabelText("Quote lexeme"), {
      target: { value: "4" },
    });
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));

    await waitFor(() => {
      expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.04");
    });
    fireEvent.click(screen.getByRole("button", { name: "Create draft" }));

    await waitFor(() => expect(create).toHaveBeenCalledOnce());
    const body = create.mock.calls[0][0] as {
      instruments: { raw_quote: string }[];
    };
    expect(body.instruments[0].raw_quote).toBe("0.04");
  });

  it("requires an explicit selected workspace instrument", () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);

    render(
      <MemoryRouter>
        <Curves />
      </MemoryRouter>,
    );

    expect(
      (screen.getByRole("button", { name: "Canonicalize quote" }) as HTMLButtonElement)
        .disabled,
    ).toBe(true);
    expect(
      screen.getByText("Select one workspace instrument before applying a canonical quote."),
    ).not.toBeNull();
  });

  it("does not modify any workspace row when canonicalization fails", async () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    vi.spyOn(api, "canonicalizeCurveLabQuote").mockRejectedValue(
      new Error("canonicalization failed"),
    );

    render(
      <MemoryRouter>
        <Curves />
      </MemoryRouter>,
    );
    fireEvent.change(screen.getByLabelText("Quote 1"), {
      target: { value: "0.031" },
    });
    fireEvent.click(screen.getByLabelText("Canonical quote target 1"));
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));

    await screen.findByText("canonicalization failed");
    expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.031");
  });

  it("repeated application is idempotent and changes only the selected row", async () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    const canonicalize = vi.spyOn(api, "canonicalizeCurveLabQuote").mockResolvedValue(
      CANONICAL_DEPOSIT,
    );
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

    render(
      <MemoryRouter>
        <Curves />
      </MemoryRouter>,
    );
    fireEvent.change(screen.getByLabelText("Quote 1"), {
      target: { value: "0.031" },
    });
    fireEvent.click(screen.getByRole("button", { name: "Add instrument" }));
    fireEvent.change(screen.getByLabelText("Quote 2"), {
      target: { value: "0.052" },
    });
    fireEvent.click(screen.getByLabelText("Canonical quote target 2"));
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));
    await waitFor(() => {
      expect((screen.getByLabelText("Quote 2") as HTMLInputElement).value).toBe("0.04");
    });
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));
    await waitFor(() => expect(canonicalize).toHaveBeenCalledTimes(2));

    expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.031");
    fireEvent.click(screen.getByRole("button", { name: "Create draft" }));
    await waitFor(() => expect(create).toHaveBeenCalledOnce());
    const body = create.mock.calls[0][0] as {
      instruments: { raw_quote: string }[];
    };
    expect(body.instruments.map((instrument) => instrument.raw_quote)).toEqual([
      "0.031",
      "0.04",
    ]);
  });

  it("rejects a delayed response after the selected instrument family changes", async () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    let resolveCanonicalization: ((value: typeof CANONICAL_DEPOSIT) => void) | null = null;
    vi.spyOn(api, "canonicalizeCurveLabQuote").mockImplementation(
      () => new Promise((resolve) => {
        resolveCanonicalization = resolve;
      }),
    );

    render(
      <MemoryRouter>
        <Curves />
      </MemoryRouter>,
    );
    fireEvent.click(screen.getByLabelText("Canonical quote target 1"));
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));
    fireEvent.change(screen.getByLabelText("Family 1"), {
      target: { value: "FUTURE" },
    });
    await act(async () => {
      resolveCanonicalization?.(CANONICAL_DEPOSIT);
      await Promise.resolve();
    });

    expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("95.8225");
    expect((screen.getByLabelText("Instrument family") as HTMLSelectElement).value).toBe(
      "FUTURE",
    );
    expect(
      screen.getByText(
        "Canonical quote target changed; select the matching workspace instrument and retry.",
      ),
    ).not.toBeNull();
  });

  it("rejects a delayed response after the target changes to another same-family row", async () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    const delayed = {
      ...CANONICAL_DEPOSIT,
      raw_quote: "0.041",
      normalized_quote: "0.041",
    };
    let resolveCanonicalization: ((value: typeof delayed) => void) | null = null;
    vi.spyOn(api, "canonicalizeCurveLabQuote").mockImplementation(
      () => new Promise((resolve) => {
        resolveCanonicalization = resolve;
      }),
    );

    render(
      <MemoryRouter>
        <Curves />
      </MemoryRouter>,
    );
    fireEvent.click(screen.getByRole("button", { name: "Add instrument" }));
    fireEvent.change(screen.getByLabelText("Quote 1"), {
      target: { value: "0.031" },
    });
    fireEvent.change(screen.getByLabelText("Quote 2"), {
      target: { value: "0.052" },
    });
    fireEvent.click(screen.getByLabelText("Canonical quote target 1"));
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));
    fireEvent.click(screen.getByLabelText("Canonical quote target 2"));
    await act(async () => {
      resolveCanonicalization?.(delayed);
      await Promise.resolve();
    });

    expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.031");
    expect((screen.getByLabelText("Quote 2") as HTMLInputElement).value).toBe("0.052");
    expect(
      screen.getByText(
        "Canonical quote target changed; select the matching workspace instrument and retry.",
      ),
    ).not.toBeNull();
  });

  it("does not mutate workspace financial bytes when display preference changes", async () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    vi.spyOn(api, "canonicalizeCurveLabQuote").mockResolvedValue(
      CANONICAL_DEPOSIT,
    );
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
    const update = vi.spyOn(api, "updateCurveLabDraft");

    render(
      <MemoryRouter>
        <Curves />
      </MemoryRouter>,
    );
    fireEvent.click(screen.getByLabelText("Canonical quote target 1"));
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));
    await waitFor(() => {
      expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.04");
    });
    fireEvent.click(screen.getByRole("button", { name: "Create draft" }));
    await waitFor(() => expect(create).toHaveBeenCalledOnce());
    const financialBytes = JSON.stringify(create.mock.calls[0][0]);

    fireEvent.change(screen.getByLabelText("Display convention"), {
      target: { value: "PERCENT" },
    });
    fireEvent.change(screen.getByLabelText("Display scale"), {
      target: { value: "6" },
    });

    expect(create).toHaveBeenCalledTimes(1);
    expect(update).not.toHaveBeenCalled();
    expect(JSON.stringify(create.mock.calls[0][0])).toBe(financialBytes);
    expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.04");
  });
});
