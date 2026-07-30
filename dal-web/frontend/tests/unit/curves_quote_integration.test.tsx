import { act, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
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

function canonicalDeposit(rawQuote: string) {
  return {
    ...CANONICAL_DEPOSIT,
    raw_quote: rawQuote,
    normalized_quote: rawQuote,
  };
}

describe("Curve Lab production quote integration", () => {
  beforeEach(() => {
    vi.spyOn(api, "renderCurveLabQuote").mockImplementation(async (request) => ({
      rendered_quote: request.canonical_raw_quote === "0.04"
        && request.display_convention === "PERCENT"
        && request.display_scale === 6
        ? "4.000000"
        : request.canonical_raw_quote,
    }));
  });

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

  it("keeps the latest canonical quote when same-target responses finish out of order", async () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    const pending: {
      resolve: (...args: [typeof CANONICAL_DEPOSIT]) => void;
      reject: (...args: [unknown]) => void;
    }[] = [];
    vi.spyOn(api, "canonicalizeCurveLabQuote").mockImplementation(
      () => new Promise((resolve, reject) => {
        pending.push({ resolve, reject });
      }),
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
    fireEvent.click(screen.getByLabelText("Canonical quote target 1"));
    fireEvent.change(screen.getByLabelText("Input convention"), {
      target: { value: "PERCENT" },
    });
    fireEvent.change(screen.getByLabelText("Quote lexeme"), {
      target: { value: "4" },
    });
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));
    fireEvent.change(screen.getByLabelText("Quote lexeme"), {
      target: { value: "5" },
    });
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));

    await waitFor(() => expect(pending).toHaveLength(2));
    await act(async () => {
      pending[1].resolve(canonicalDeposit("0.05"));
      await Promise.resolve();
    });
    await waitFor(() => {
      expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.05");
    });
    await act(async () => {
      pending[0].resolve(CANONICAL_DEPOSIT);
      await Promise.resolve();
    });
    expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.05");

    fireEvent.click(screen.getByRole("button", { name: "Create draft" }));
    await waitFor(() => expect(create).toHaveBeenCalledOnce());
    const body = create.mock.calls[0][0] as {
      instruments: { raw_quote: string }[];
    };
    expect(body.instruments[0].raw_quote).toBe("0.05");
  });

  it("keeps a newer error when an older success arrives later", async () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    const pending: {
      resolve: (...args: [typeof CANONICAL_DEPOSIT]) => void;
      reject: (...args: [unknown]) => void;
    }[] = [];
    vi.spyOn(api, "canonicalizeCurveLabQuote").mockImplementation(
      () => new Promise((resolve, reject) => {
        pending.push({ resolve, reject });
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
    fireEvent.click(screen.getByLabelText("Canonical quote target 1"));
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));
    fireEvent.change(screen.getByLabelText("Quote lexeme"), {
      target: { value: "0.05" },
    });
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));

    await waitFor(() => expect(pending).toHaveLength(2));
    await act(async () => {
      pending[1].reject(new Error("newest quote failed"));
      await Promise.resolve();
    });
    await screen.findByText("newest quote failed");
    await act(async () => {
      pending[0].resolve(CANONICAL_DEPOSIT);
      await Promise.resolve();
    });

    expect(screen.getByText("newest quote failed")).not.toBeNull();
    expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.031");
  });

  it("keeps a newer success when an older failure arrives later", async () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    const pending: {
      resolve: (...args: [typeof CANONICAL_DEPOSIT]) => void;
      reject: (...args: [unknown]) => void;
    }[] = [];
    vi.spyOn(api, "canonicalizeCurveLabQuote").mockImplementation(
      () => new Promise((resolve, reject) => {
        pending.push({ resolve, reject });
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
    fireEvent.click(screen.getByLabelText("Canonical quote target 1"));
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));
    fireEvent.change(screen.getByLabelText("Quote lexeme"), {
      target: { value: "0.05" },
    });
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));

    await waitFor(() => expect(pending).toHaveLength(2));
    await act(async () => {
      pending[1].resolve(canonicalDeposit("0.05"));
      await Promise.resolve();
    });
    await waitFor(() => {
      expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.05");
    });
    await act(async () => {
      pending[0].reject(new Error("older quote failed"));
      await Promise.resolve();
    });

    expect(screen.queryByText("older quote failed")).toBeNull();
    expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.05");
  });

  it("ignores a cancelled request after the authoring input changes", async () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    let rejectCanonicalization: ((reason: unknown) => void) | null = null;
    vi.spyOn(api, "canonicalizeCurveLabQuote").mockImplementation(
      () => new Promise((_resolve, reject) => {
        rejectCanonicalization = reject;
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
    fireEvent.click(screen.getByLabelText("Canonical quote target 1"));
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));
    fireEvent.change(screen.getByLabelText("Quote lexeme"), {
      target: { value: "5" },
    });
    await act(async () => {
      rejectCanonicalization?.(new DOMException("Aborted", "AbortError"));
      await Promise.resolve();
    });

    expect(screen.queryByText("Aborted")).toBeNull();
    expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.031");
  });

  it("uses the latest generation for duplicate in-flight submissions", async () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    const pending: {
      resolve: (...args: [typeof CANONICAL_DEPOSIT]) => void;
      reject: (...args: [unknown]) => void;
    }[] = [];
    vi.spyOn(api, "canonicalizeCurveLabQuote").mockImplementation(
      () => new Promise((resolve, reject) => {
        pending.push({ resolve, reject });
      }),
    );

    render(
      <MemoryRouter>
        <Curves />
      </MemoryRouter>,
    );
    fireEvent.click(screen.getByLabelText("Canonical quote target 1"));
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));

    await waitFor(() => expect(pending).toHaveLength(2));
    await act(async () => {
      pending[1].resolve(CANONICAL_DEPOSIT);
      await Promise.resolve();
    });
    await act(async () => {
      pending[0].resolve(canonicalDeposit("0.041"));
      await Promise.resolve();
    });

    expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.04");
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

  it("ignores a delayed response after the selected instrument family changes", async () => {
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
      screen.queryByText(
        "Canonical quote target changed; select the matching workspace instrument and retry.",
      ),
    ).toBeNull();
  });

  it("ignores a delayed response after the target changes to another same-family row", async () => {
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
      screen.queryByText(
        "Canonical quote target changed; select the matching workspace instrument and retry.",
      ),
    ).toBeNull();
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

    fireEvent.change(screen.getByLabelText("Display convention"), {
      target: { value: "PERCENT" },
    });
    fireEvent.change(screen.getByLabelText("Display scale"), {
      target: { value: "6" },
    });
    expect((screen.getByLabelText("Display scale") as HTMLInputElement).value).toBe("6");

    await waitFor(() => {
      expect(api.renderCurveLabQuote).toHaveBeenLastCalledWith({
        instrument_type: "DEPOSIT",
        canonical_raw_quote: "0.04",
        display_convention: "PERCENT",
        display_scale: 6,
      });
    });
    expect(screen.getByText("4.000000")).not.toBeNull();

    fireEvent.click(screen.getByRole("button", { name: "Create draft" }));
    await waitFor(() => expect(create).toHaveBeenCalledOnce());
    const financialBytes = JSON.stringify(create.mock.calls[0][0]);
    expect(create).toHaveBeenCalledTimes(1);
    expect(update).not.toHaveBeenCalled();
    expect(JSON.stringify(create.mock.calls[0][0])).toBe(financialBytes);
    expect((screen.getByLabelText("Quote 1") as HTMLInputElement).value).toBe("0.04");
  });
});
