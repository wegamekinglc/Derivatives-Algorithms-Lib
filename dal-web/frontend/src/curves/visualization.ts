import type {
  CalibrationMatrix,
  InstrumentDiagnostic,
} from "../api/client";

export interface FitSeriesRow {
  id: string;
  label: string;
  market: number;
  model: number;
  residual: number;
  marketLabel: string;
  modelLabel: string;
  residualLabel: string;
}

function rate(value: number): string {
  return value.toFixed(8);
}

function basisPoints(value: number): string {
  return `${(value * 10_000).toFixed(3)} bp`;
}

export function alignFitSeries(
  diagnostics: InstrumentDiagnostic[],
  residualAxis: string[],
): FitSeriesRow[] {
  const byId = new Map(diagnostics.map((item) => [item.instrument_id, item]));
  return residualAxis.map((axis, index) => {
    const id = axis.startsWith("residual:") ? axis.slice(9) : "";
    const item =
      byId.get(id) ??
      diagnostics.find((candidate) => candidate.calibration_index === index);
    if (!item) {
      throw new Error(`Missing instrument diagnostic for ${axis}`);
    }
    return {
      id: item.instrument_id,
      label: `${item.group} · ${item.calibration_index}`,
      market: item.market_rate,
      model: item.model_rate,
      residual: item.residual,
      marketLabel: rate(item.market_rate),
      modelLabel: rate(item.model_rate),
      residualLabel: basisPoints(item.residual),
    };
  });
}

export function heatmapModel(matrix: CalibrationMatrix) {
  return {
    available: matrix.availability === "available",
    reason: matrix.availability,
    shapeLabel: `${matrix.shape[0]} × ${matrix.shape[1]}`,
    rows: [...matrix.row_axis],
    columns: [...matrix.column_axis],
    values: matrix.values?.map((row) => [...row]) ?? null,
  };
}

export interface LocatedField {
  section: "declaration" | "instrument" | "request";
  row: number | null;
  field: string;
}

export function locateCalibrationField(
  location: Array<string | number>,
): LocatedField {
  const path = location[0] === "body" ? location.slice(1) : location;
  const last = path[path.length - 1];
  const previous = path[path.length - 2];
  const instrument = path.findIndex((item) => item === "instruments");
  if (instrument >= 0) {
    return {
      section: "instrument",
      row: typeof path[instrument + 1] === "number" ? path[instrument + 1] as number : null,
      field: String(last ?? "instruments"),
    };
  }
  const declaration = path.findIndex((item) =>
    ["declaration", "declarations", "basis"].includes(String(item)),
  );
  if (declaration >= 0) {
    return {
      section: "declaration",
      row: typeof last === "number" ? last : null,
      field: String(previous ?? path[declaration]),
    };
  }
  return {
    section: "request",
    row: null,
    field: String(last ?? "request"),
  };
}
