import { useState } from "react";
import type { QuoteSeriesPoint } from "../curves/curveBuilderUtils";
import { css } from "../format";

const CHART = { width: 460, height: 210, padLeft: 40, padRight: 10, padTop: 16, padBottom: 26 };

interface CurvePreviewProps {
  points: QuoteSeriesPoint[];
  buildState: string | null;
  fitState: string | null;
  includedCount: number;
  totalCount: number;
}

function yScale(value: number, low: number, high: number): number {
  const span = high - low || 1;
  const inner = CHART.height - CHART.padTop - CHART.padBottom;
  return CHART.padTop + (1 - (value - low) / span) * inner;
}

function xScale(index: number, count: number): number {
  const inner = CHART.width - CHART.padLeft - CHART.padRight;
  return count <= 1
    ? CHART.padLeft + inner / 2
    : CHART.padLeft + (index / (count - 1)) * inner;
}

export default function CurvePreview({
  points,
  buildState,
  fitState,
  includedCount,
  totalCount,
}: CurvePreviewProps) {
  const [view, setView] = useState<"raw" | "normalized">("raw");
  const hasNormalized = points.some((point) => point.normalizedPercent !== null);
  const values = points.flatMap((point) => [
    point.percent,
    ...(view === "normalized" && point.normalizedPercent !== null
      ? [point.normalizedPercent]
      : []),
  ]);
  const low = values.length > 0 ? Math.min(...values) - 0.25 : 0;
  const high = values.length > 0 ? Math.max(...values) + 0.25 : 1;
  const stateLabel = buildState ?? "DRAFT";
  const stateClass = buildState === "SUCCEEDED" ? "pos" : "warn";
  return (
    <section {...css("panel", "curve-builder-preview")} aria-label="Curve preview">
      <h3 {...css("panel-title")}>Curve preview</h3>
      <div {...css("curve-builder-preview-tabs")} role="tablist" aria-label="Preview view">
        <button
          type="button"
          role="tab"
          aria-selected={view === "raw"}
          {...css(view === "raw" && "active")}
          onClick={() => setView("raw")}
        >
          Raw quotes
        </button>
        <button
          type="button"
          role="tab"
          aria-selected={view === "normalized"}
          disabled={!hasNormalized}
          title={hasNormalized ? "Normalized quotes from the succeeded build" : "Available after a succeeded build"}
          {...css(view === "normalized" && "active")}
          onClick={() => setView("normalized")}
        >
          Normalized
        </button>
      </div>
      {points.length === 0 ? (
        <p {...css("muted")}>No plottable rate quotes yet — add included deposit, futures or swap instruments.</p>
      ) : (
        <svg
          viewBox={`0 0 ${CHART.width} ${CHART.height}`}
          role="img"
          aria-label="Quotes by tenor"
          {...css("curve-builder-chart")}
        >
          {[0, 0.5, 1].map((fraction) => {
            const value = low + fraction * (high - low);
            const y = yScale(value, low, high);
            return (
              <g key={fraction}>
                <line
                  x1={CHART.padLeft}
                  x2={CHART.width - CHART.padRight}
                  y1={y}
                  y2={y}
                  {...css("curve-builder-chart-grid")}
                />
                <text x={CHART.padLeft - 6} y={y + 3} textAnchor="end" {...css("curve-builder-chart-tick")}>
                  {value.toFixed(2)}
                </text>
              </g>
            );
          })}
          {points.map((point, index) => (
            <text
              key={`label-${point.key}`}
              x={xScale(index, points.length)}
              y={CHART.height - 8}
              textAnchor="middle"
              {...css("curve-builder-chart-tick")}
            >
              {point.tenor}
            </text>
          ))}
          <polyline
            points={points.map((point, index) => (
              `${xScale(index, points.length)},${yScale(point.percent, low, high)}`
            )).join(" ")}
            {...css("curve-builder-chart-line")}
          />
          {points.map((point, index) => (
            <circle
              key={`raw-${point.key}`}
              cx={xScale(index, points.length)}
              cy={yScale(point.percent, low, high)}
              r={3.5}
              {...css("curve-builder-chart-dot")}
            >
              <title>{`${point.tenor} · ${point.percent.toFixed(4)}%`}</title>
            </circle>
          ))}
          {view === "normalized" && points.map((point, index) => (
            point.normalizedPercent === null ? null : (
              <circle
                key={`normalized-${point.key}`}
                cx={xScale(index, points.length)}
                cy={yScale(point.normalizedPercent, low, high)}
                r={2.5}
                {...css("curve-builder-chart-dot-normalized")}
              >
                <title>{`${point.tenor} · normalized ${point.normalizedPercent.toFixed(4)}%`}</title>
              </circle>
            )
          ))}
        </svg>
      )}
      <div {...css("curve-builder-legend")}>
        <span {...css("curve-builder-legend-raw")}>Quotes (raw)</span>
        {view === "normalized" && hasNormalized && (
          <span {...css("curve-builder-legend-normalized")}>Normalized (post-build)</span>
        )}
      </div>
      <div {...css("curve-builder-stats")}>
        <div {...css("curve-builder-stat")}>
          <h4>State</h4>
          <div {...css("metric", stateClass)}>{stateLabel}</div>
        </div>
        <div {...css("curve-builder-stat")}>
          <h4>Fit</h4>
          <div {...css("metric", fitState === "NATIVE_ARCHIVE_VALIDATED" && "pos")}>{fitState ?? "—"}</div>
        </div>
        <div {...css("curve-builder-stat")}>
          <h4>Quotes</h4>
          <div {...css("metric")}>{includedCount} / {totalCount}</div>
        </div>
      </div>
    </section>
  );
}
