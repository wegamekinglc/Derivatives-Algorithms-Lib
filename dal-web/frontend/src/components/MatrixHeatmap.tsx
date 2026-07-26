import type { CalibrationMatrix } from "../api/client";
import { heatmapModel } from "../curves/visualization";
import { css } from "../format";

function cellColor(value: number, max: number): string {
  const intensity = max === 0 ? 0 : Math.min(1, Math.abs(value) / max);
  const color = value < 0 ? "218,54,51" : "46,160,67";
  return `rgba(${color},${0.12 + intensity * 0.78})`;
}

export default function MatrixHeatmap({
  title,
  matrix,
}: {
  title: string;
  matrix: CalibrationMatrix;
}) {
  const model = heatmapModel(matrix);
  const values = model.values?.flat() ?? [];
  const max = Math.max(0, ...values.map(Math.abs));
  return (
    <section {...css("panel", "matrix-panel")}>
      <div {...css("matrix-heading")}>
        <h2>{title}</h2>
        <span {...css("tag")}>{model.shapeLabel} · {matrix.scaling}</span>
      </div>
      {!model.available ? (
        <div {...css("matrix-unavailable")}>
          <strong>{model.reason.split("_").join(" ")}</strong>
          <span>Axes remain available for audit.</span>
          <code>{model.rows.join(" · ") || "No rows"}</code>
          <code>{model.columns.join(" · ") || "No columns"}</code>
        </div>
      ) : (
        <div {...css("heatmap-scroll")}>
          <table
            {...css("heatmap")}
            aria-label={`${title} ${model.shapeLabel}`}
          >
            <thead>
              <tr>
                <th />
                {model.columns.map((column) => <th key={column} title={column}><code>{column}</code></th>)}
              </tr>
            </thead>
            <tbody>
              {model.gridRows.map(({ row, cells }) => (
                <tr key={row}>
                  <th title={row}><code>{row}</code></th>
                  {cells.map(({ column, value }) => (
                    <td
                      key={`${row}-${column}`}
                      title={`${row} × ${column} = ${value}`}
                      style={{ backgroundColor: cellColor(value, max) }}
                    >
                      {value.toExponential(1)}
                    </td>
                  ))}
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </section>
  );
}
