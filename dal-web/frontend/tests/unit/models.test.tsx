import { beforeEach, describe, expect, it, vi } from "vitest";
import { fireEvent, render, screen } from "@testing-library/react";
import Models from "../../src/pages/Models";
import { api, type ModelDefinition } from "../../src/api/client";

function makeModel(overrides: Partial<ModelDefinition> = {}): ModelDefinition {
  return { id: "m1", name: "m", kind: "BSModelData_", ...overrides };
}

describe("Models", () => {
  let createModelSpy: ReturnType<typeof vi.spyOn>;

  beforeEach(() => {
    vi.spyOn(api, "listModels").mockResolvedValue([]);
    createModelSpy = vi.spyOn(api, "createModel").mockResolvedValue(makeModel());
  });

  it("creates a Black-Scholes model from the scalar form fields", async () => {
    render(<Models />);
    await screen.findByText("New model");

    fireEvent.change(screen.getByLabelText("Name"), { target: { value: "BS test" } });
    fireEvent.change(screen.getByLabelText("Spot"), { target: { value: "105" } });
    fireEvent.change(screen.getByLabelText("Vol"), { target: { value: "0.25" } });
    fireEvent.change(screen.getByLabelText("Rate"), { target: { value: "0.01" } });
    fireEvent.change(screen.getByLabelText("Dividend"), { target: { value: "0.02" } });
    fireEvent.click(screen.getByRole("button", { name: "Create model" }));

    await vi.waitFor(() => {
      expect(createModelSpy).toHaveBeenCalledTimes(1);
    });
    expect(createModelSpy.mock.calls[0][0]).toEqual({
      name: "BS test",
      kind: "BSModelData_",
      bs: { spot: 105, vol: 0.25, rate: 0.01, div: 0.02 },
    });
  });

  it("parses Dupire strikes, times, and the vols matrix from text", async () => {
    render(<Models />);
    await screen.findByText("New model");

    fireEvent.change(screen.getByLabelText("Model kind"), { target: { value: "DupireModelData_" } });
    fireEvent.change(screen.getByLabelText("Spot strikes (comma-separated)"), {
      target: { value: "90, 100" },
    });
    fireEvent.change(screen.getByLabelText("Times in years (comma-separated)"), {
      target: { value: "0.25, 0.5" },
    });
    fireEvent.change(screen.getByLabelText("Vols matrix (one row per strike, whitespace-separated)"), {
      target: { value: "0.24, 0.23\n0.21, 0.20" },
    });
    fireEvent.click(screen.getByRole("button", { name: "Create model" }));

    await vi.waitFor(() => {
      expect(createModelSpy).toHaveBeenCalledTimes(1);
    });
    expect(createModelSpy.mock.calls[0][0]).toEqual({
      name: "BS spot=100 vol=20%",
      kind: "DupireModelData_",
      dupire: {
        spot: 100,
        rate: 0,
        repo: 0,
        spots: [90, 100],
        times: [0.25, 0.5],
        vols: [
          [0.24, 0.23],
          [0.21, 0.2],
        ],
      },
    });
  });

  it("tolerates mixed separators and blank lines in the numeric text fields", async () => {
    render(<Models />);
    await screen.findByText("New model");

    fireEvent.change(screen.getByLabelText("Model kind"), { target: { value: "DupireModelData_" } });
    fireEvent.change(screen.getByLabelText("Spot strikes (comma-separated)"), {
      target: { value: "90  100,\t110" },
    });
    fireEvent.change(screen.getByLabelText("Vols matrix (one row per strike, whitespace-separated)"), {
      target: { value: "0.24\n\n0.21   0.20\n" },
    });
    fireEvent.click(screen.getByRole("button", { name: "Create model" }));

    await vi.waitFor(() => {
      expect(createModelSpy).toHaveBeenCalledTimes(1);
    });
    const dupire = createModelSpy.mock.calls[0][0].dupire;
    expect(dupire?.spots).toEqual([90, 100, 110]);
    expect(dupire?.vols).toEqual([[0.24], [0.21, 0.2]]);
  });
});
