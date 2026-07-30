import { act, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import CurveLabQuoteAuthoring from "../../src/components/CurveLabQuoteAuthoring";
import {
  CURVE_LAB_SUCCESS_FAMILIES,
  curveLabFamily,
} from "../../src/curves/curveLabRegistry";

describe("Curve Lab quote authoring", () => {
  it("owns the exact ordered success registry and compatible conventions", () => {
    expect(CURVE_LAB_SUCCESS_FAMILIES).toEqual([
      "DEPOSIT",
      "FRA",
      "FUTURE",
      "OIS",
      "IRS",
      "BASIS_SWAP",
      "XCCY",
    ]);
    expect(curveLabFamily("FUTURE").inputConventions).toEqual(["PRICE_POINTS"]);
    expect(curveLabFamily("IRS").inputConventions).toEqual(["DECIMAL", "PERCENT"]);
  });

  it("replaces percent authoring with returned canonical financial bytes", async () => {
    const canonical = {
      instrument_type: "IRS" as const,
      quote_coordinate_kind: "RATE" as const,
      canonical_raw_unit: "DECIMAL" as const,
      raw_quote: "0.04",
      normalized_quote: "0.04",
      normalized_unit: "DECIMAL_RATE" as const,
      exact_risk_raw_bump: "0.0001",
      normalized_risk_bump: "0.0001",
    };
    const normalize = vi.fn().mockResolvedValue(canonical);
    const renderQuote = vi.fn().mockResolvedValue({ rendered_quote: "0.0400" });
    const onCanonicalQuote = vi.fn();
    render(
      <CurveLabQuoteAuthoring
        canonicalize={normalize}
        renderQuote={renderQuote}
        onCanonicalQuote={onCanonicalQuote}
      />,
    );

    fireEvent.change(screen.getByLabelText("Instrument family"), {
      target: { value: "IRS" },
    });
    fireEvent.change(screen.getByLabelText("Input convention"), {
      target: { value: "PERCENT" },
    });
    fireEvent.change(screen.getByLabelText("Quote lexeme"), {
      target: { value: "4" },
    });
    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));

    await waitFor(() => expect(normalize).toHaveBeenCalledWith({
      instrument_type: "IRS",
      input_lexeme: "4",
      input_convention: "PERCENT",
    }));
    expect(onCanonicalQuote).toHaveBeenCalledWith(canonical);
    expect(screen.getByText("0.04")).not.toBeNull();
    expect(screen.getByText("+0.0001 raw / +0.0001 normalized")).not.toBeNull();
    expect(screen.queryByText("PERCENT", { selector: "output" })).toBeNull();
  });

  it("keeps Future price points visible while exposing normalized rate detail", async () => {
    const normalize = vi.fn().mockResolvedValue({
      instrument_type: "FUTURE",
      quote_coordinate_kind: "PRICE",
      canonical_raw_unit: "PRICE_POINTS",
      raw_quote: "95.8225",
      normalized_quote: "0.041775",
      normalized_unit: "DECIMAL_RATE",
      exact_risk_raw_bump: "-0.01",
      normalized_risk_bump: "0.0001",
    });
    const renderQuote = vi.fn().mockResolvedValue({ rendered_quote: "95.8225" });
    render(
      <CurveLabQuoteAuthoring
        canonicalize={normalize}
        renderQuote={renderQuote}
      />,
    );

    fireEvent.change(screen.getByLabelText("Instrument family"), {
      target: { value: "FUTURE" },
    });
    fireEvent.change(screen.getByLabelText("Quote lexeme"), {
      target: { value: "95.8225" },
    });
    fireEvent.keyDown(screen.getByLabelText("Quote lexeme"), {
      key: "Enter",
      code: "Enter",
    });

    await waitFor(() => expect(normalize).toHaveBeenCalledTimes(1));
    expect(
      (screen.getByLabelText("Input convention") as HTMLSelectElement).value,
    ).toBe("PRICE_POINTS");
    expect(screen.getAllByText("95.8225")).toHaveLength(2);
    expect(screen.getByText("0.041775 normalized")).not.toBeNull();
  });

  it("keeps display convention and scale outside canonical financial state", async () => {
    const canonical = {
      instrument_type: "DEPOSIT" as const,
      quote_coordinate_kind: "RATE" as const,
      canonical_raw_unit: "DECIMAL" as const,
      raw_quote: "0.04",
      normalized_quote: "0.04",
      normalized_unit: "DECIMAL_RATE" as const,
      exact_risk_raw_bump: "0.0001",
      normalized_risk_bump: "0.0001",
    };
    const normalize = vi.fn().mockResolvedValue(canonical);
    const renderQuote = vi.fn().mockImplementation(async (request) => ({
      rendered_quote: request.display_convention === "PERCENT"
        && request.display_scale === 6
        ? "4.000000"
        : "0.0400",
    }));
    const onCanonicalQuote = vi.fn();
    render(
      <CurveLabQuoteAuthoring
        canonicalize={normalize}
        renderQuote={renderQuote}
        onCanonicalQuote={onCanonicalQuote}
      />,
    );

    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));
    await waitFor(() => expect(onCanonicalQuote).toHaveBeenCalledWith(canonical));

    fireEvent.change(screen.getByLabelText("Display convention"), {
      target: { value: "PERCENT" },
    });
    fireEvent.change(screen.getByLabelText("Display scale"), {
      target: { value: "6" },
    });

    expect(normalize).toHaveBeenCalledTimes(1);
    expect(onCanonicalQuote).toHaveBeenCalledTimes(1);
    await waitFor(() => expect(renderQuote).toHaveBeenLastCalledWith({
      instrument_type: "DEPOSIT",
      canonical_raw_quote: "0.04",
      display_convention: "PERCENT",
      display_scale: 6,
    }));
    expect(screen.getByText("4.000000")).not.toBeNull();
  });

  it("keeps the latest exact display when rendering responses finish out of order", async () => {
    const canonical = {
      instrument_type: "DEPOSIT" as const,
      quote_coordinate_kind: "RATE" as const,
      canonical_raw_unit: "DECIMAL" as const,
      raw_quote: "0.04",
      normalized_quote: "0.04",
      normalized_unit: "DECIMAL_RATE" as const,
      exact_risk_raw_bump: "0.0001",
      normalized_risk_bump: "0.0001",
    };
    const pending: ((value: { rendered_quote: string }) => void)[] = [];
    const renderQuote = vi.fn().mockImplementation(
      () => new Promise<{ rendered_quote: string }>((resolve) => {
        pending.push(resolve);
      }),
    );
    render(
      <CurveLabQuoteAuthoring
        canonicalize={vi.fn().mockResolvedValue(canonical)}
        renderQuote={renderQuote}
      />,
    );

    fireEvent.click(screen.getByRole("button", { name: "Canonicalize quote" }));
    await waitFor(() => expect(pending).toHaveLength(1));
    fireEvent.change(screen.getByLabelText("Display convention"), {
      target: { value: "PERCENT" },
    });
    fireEvent.change(screen.getByLabelText("Display scale"), {
      target: { value: "6" },
    });
    await waitFor(() => expect(pending).toHaveLength(3));

    await act(async () => {
      pending[2]?.({ rendered_quote: "4.000000" });
      await Promise.resolve();
    });
    await screen.findByText("4.000000");
    await act(async () => {
      pending[0]?.({ rendered_quote: "0.0400" });
      pending[1]?.({ rendered_quote: "4.0000" });
      await Promise.resolve();
    });

    expect(screen.getByText("4.000000")).not.toBeNull();
    expect(screen.queryByText("4.0000")).toBeNull();
  });
});
