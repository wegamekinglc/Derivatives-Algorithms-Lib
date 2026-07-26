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
  const rowValues = matrix.values?.[Symbol.iterator]();
  return {
    available: matrix.availability === "available",
    reason: matrix.availability,
    shapeLabel: `${matrix.shape[0]} × ${matrix.shape[1]}`,
    rows: [...matrix.row_axis],
    columns: [...matrix.column_axis],
    values: matrix.values?.map((row) => [...row]) ?? null,
    gridRows: matrix.row_axis.map((row) => {
      const values = rowValues?.next().value ?? [];
      const columnValues = values[Symbol.iterator]();
      return {
        row,
        cells: matrix.column_axis.map((column) => ({
          column,
          value: columnValues.next().value as number,
        })),
      };
    }),
  };
}

export interface LocatedField {
  section: "declaration" | "instrument" | "request";
  row: number | null;
  field: string;
}

function stringValue(
  value: string | number | undefined,
  fallback: string,
): string {
  if (value === undefined) return fallback;
  return String(value);
}

export function locateCalibrationField(
  location: (string | number)[],
): LocatedField {
  const path = location[0] === "body" ? location.slice(1) : location;
  const reversedTail = [...path];
  const last = reversedTail.pop();
  const previous = reversedTail.pop();
  const instrument = path.indexOf("instruments");
  if (instrument >= 0) {
    const afterInstrument = path.slice(instrument + 1);
    const row = afterInstrument.shift();
    return {
      section: "instrument",
      row: typeof row === "number" ? row : null,
      field: stringValue(last, "instruments"),
    };
  }
  const declaration = path.find((item) =>
    ["declaration", "declarations", "basis"].includes(String(item)),
  );
  if (declaration !== undefined) {
    return {
      section: "declaration",
      row: typeof last === "number" ? last : null,
      field: stringValue(previous, String(declaration)),
    };
  }
  return {
    section: "request",
    row: null,
    field: stringValue(last, "request"),
  };
}
