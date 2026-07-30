import { fireEvent, render, screen, waitFor } from "@testing-library/react";
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
    const onCanonicalQuote = vi.fn();
    render(
      <CurveLabQuoteAuthoring
        canonicalize={normalize}
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
    render(<CurveLabQuoteAuthoring canonicalize={normalize} />);

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
    expect(screen.getByText("95.8225")).not.toBeNull();
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
    const onCanonicalQuote = vi.fn();
    render(
      <CurveLabQuoteAuthoring
        canonicalize={normalize}
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
    expect(screen.getByText("0.04")).not.toBeNull();
  });
});
