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
          <div
            {...css("heatmap")}
            style={{ gridTemplateColumns: `minmax(150px, 1fr) repeat(${model.columns.length}, 42px)` }}
            role="grid"
            aria-label={`${title} ${model.shapeLabel}`}
          >
            <span />
            {model.columns.map((column) => <code key={column} title={column}>{column}</code>)}
            {model.rows.map((row, rowIndex) => (
              <div className="heatmap-row" key={row}>
                <code title={row}>{row}</code>
                {model.values?.[rowIndex].map((value, columnIndex) => (
                  <span
                    key={`${row}-${columnIndex}`}
                    role="gridcell"
                    title={`${row} × ${model.columns[columnIndex]} = ${value}`}
                    style={{ backgroundColor: cellColor(value, max) }}
                  >
                    {value.toExponential(1)}
                  </span>
                ))}
              </div>
            ))}
          </div>
        </div>
      )}
    </section>
  );
}
