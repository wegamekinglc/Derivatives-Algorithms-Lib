import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import QuoteBumpPanel from "../../src/components/QuoteBumpPanel";

describe("QuoteBumpPanel", () => {
  it("renders the backend preview returned for the selected quote", async () => {
    const preview = vi.fn().mockResolvedValue({
      residual_index: 0,
      instrument_id: "a".repeat(32),
      quote_bump: 0.0001,
      residual_tolerance: 1e-8,
      delta_parameters: [
        { parameter_axis: "parameter:usd:0", delta: 0.0025 },
      ],
      formula: "delta_x = effective_inverse * delta_quote / residual_tolerance",
    });
    render(<QuoteBumpPanel runId={"b".repeat(32)} preview={preview} />);

    fireEvent.change(screen.getByLabelText("Quote index"), {
      target: { value: "0" },
    });
    fireEvent.change(screen.getByLabelText("Bump size"), {
      target: { value: "0.0001" },
    });
    fireEvent.click(screen.getByRole("button", { name: "Preview bump" }));

    await waitFor(() => expect(preview).toHaveBeenCalledWith("b".repeat(32), 0, 0.0001));
    expect(await screen.findByText("0.00250000")).not.toBeNull();
  });
});
