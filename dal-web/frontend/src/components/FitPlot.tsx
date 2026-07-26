import type { FitSeriesRow } from "../curves/visualization";
import { css } from "../format";

function scale(value: number, low: number, high: number): number {
  return high === low ? 50 : 8 + ((value - low) / (high - low)) * 84;
}

export default function FitPlot({ rows }: { rows: FitSeriesRow[] }) {
  const rates = rows.flatMap((row) => [row.market, row.model]);
  const low = Math.min(...rates);
  const high = Math.max(...rates);
  const point = (value: number, index: number) =>
    `${scale(value, low, high)},${10 + index * (80 / Math.max(1, rows.length - 1))}`;
  return (
    <section {...css("panel")}>
      <h2>Market fit & residuals</h2>
      <div {...css("plot-grid")}>
        <svg viewBox="0 0 100 100" role="img" aria-labelledby="fit-title fit-desc">
          <title id="fit-title">Market and model calibration rates</title>
          <desc id="fit-desc">Gold marks market rates; green marks model rates, aligned by residual axis.</desc>
          <polyline points={rows.map((row, index) => point(row.market, index)).join(" ")} className="market-line" />
          <polyline points={rows.map((row, index) => point(row.model, index)).join(" ")} className="model-line" />
          {rows.map((row, index) => (
            <g key={row.id}>
              <circle cx={scale(row.market, low, high)} cy={10 + index * (80 / Math.max(1, rows.length - 1))} r="1.8" className="market-dot" />
              <circle cx={scale(row.model, low, high)} cy={10 + index * (80 / Math.max(1, rows.length - 1))} r="1.8" className="model-dot" />
            </g>
          ))}
        </svg>
        <div {...css("fit-legend")}>
          {rows.map((row) => (
            <div key={row.id} {...css("fit-row")}>
              <span>{row.label}</span>
              <code>{row.marketLabel}</code>
              <code>{row.modelLabel}</code>
              <strong className={row.residual < 0 ? "negative" : ""}>{row.residualLabel}</strong>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
