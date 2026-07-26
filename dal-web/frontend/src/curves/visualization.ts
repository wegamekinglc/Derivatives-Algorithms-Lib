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

function hasRenderableMatrixValues(
  matrix: CalibrationMatrix,
): matrix is CalibrationMatrix & { values: number[][] } {
  const [rows, columns] = matrix.shape;
  return (
    matrix.availability === "available" &&
    matrix.values !== null &&
    matrix.values.length === rows &&
    matrix.values.every((row) => row.length === columns) &&
    matrix.row_axis.length === rows &&
    matrix.column_axis.length === columns
  );
}

export function heatmapModel(matrix: CalibrationMatrix) {
  const metadata = {
    shapeLabel: `${matrix.shape[0]} × ${matrix.shape[1]}`,
    rows: [...matrix.row_axis],
    columns: [...matrix.column_axis],
  };
  if (!hasRenderableMatrixValues(matrix)) {
    return {
      ...metadata,
      available: false,
      reason:
        matrix.availability === "available"
          ? "invalid_matrix_values"
          : matrix.availability,
      values: null,
      gridRows: [],
    };
  }
  const rowValues = matrix.values.values();
  return {
    ...metadata,
    available: true,
    reason: matrix.availability,
    values: matrix.values.map((row) => [...row]),
    gridRows: matrix.row_axis.map((row) => {
      const nextRow = rowValues.next();
      if (nextRow.done) {
        throw new Error("matrix rows changed after validation");
      }
      const columnValues = nextRow.value.values();
      return {
        row,
        cells: matrix.column_axis.map((column) => {
          const nextValue = columnValues.next();
          if (nextValue.done) {
            throw new Error("matrix columns changed after validation");
          }
          return { column, value: nextValue.value };
        }),
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
