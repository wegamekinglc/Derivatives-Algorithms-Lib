import { describe, expect, it } from "vitest";
import {
  alignFitSeries,
  heatmapModel,
  locateCalibrationField,
} from "../../src/curves/visualization";

describe("curve calibration visualization contracts", () => {
  it("aligns fit and residual values by the response axis", () => {
    const rows = alignFitSeries(
      [
        {
          instrument_id: "a".repeat(32),
          group: "single:usd",
          calibration_index: 1,
          market_rate: 0.03,
          model_rate: 0.031,
          residual: 0.001,
        },
        {
          instrument_id: "b".repeat(32),
          group: "single:usd",
          calibration_index: 0,
          market_rate: 0.02,
          model_rate: 0.019,
          residual: -0.001,
        },
      ],
      [`residual:${"b".repeat(32)}`, `residual:${"a".repeat(32)}`],
    );

    expect(rows.map((row) => row.market)).toEqual([0.02, 0.03]);
    expect(rows.map((row) => row.residualLabel)).toEqual(["-10.000 bp", "10.000 bp"]);
  });

  it("retains matrix metadata when values are unavailable", () => {
    const model = heatmapModel({
      availability: "not_available_for_mode",
      shape: [2, 3],
      row_axis: ["r0", "r1"],
      column_axis: ["c0", "c1", "c2"],
      scaling: "unscaled",
      residual_tolerance: null,
      values: null,
    });

    expect(model.available).toBe(false);
    expect(model.shapeLabel).toBe("2 × 3");
    expect(model.rows).toEqual(["r0", "r1"]);
    expect(model.columns).toEqual(["c0", "c1", "c2"]);
  });

  it("maps Pydantic locations to declaration and instrument cells", () => {
    expect(locateCalibrationField(["body", "declaration", "knot_dates", 3])).toEqual({
      section: "declaration",
      row: 3,
      field: "knot_dates",
    });
    expect(locateCalibrationField(["body", "instruments", 2, "market_rate"])).toEqual({
      section: "instrument",
      row: 2,
      field: "market_rate",
    });
  });
});
