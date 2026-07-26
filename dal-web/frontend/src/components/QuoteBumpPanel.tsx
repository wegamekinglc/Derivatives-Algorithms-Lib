import { useState } from "react";
import {
  api,
  type QuoteBumpPreview,
} from "../api/client";
import { css } from "../format";

interface Props {
  runId: string;
  preview?: (
    runId: string,
    quoteIndex: number,
    bumpSize: number,
  ) => Promise<QuoteBumpPreview>;
}

export default function QuoteBumpPanel({
  runId,
  preview = async (id, index, size) => {
    const run = await api.getCalibration(id, index, size);
    if (!run.quote_bump_preview) {
      throw new Error("The backend did not return a quote-bump preview.");
    }
    return run.quote_bump_preview;
  },
}: Props) {
  const [index, setIndex] = useState(0);
  const [size, setSize] = useState(0.0001);
  const [result, setResult] = useState<QuoteBumpPreview | null>(null);
  const [error, setError] = useState<string | null>(null);

  return (
    <section {...css("panel")}>
      <h2>Quote-bump risk</h2>
      <p {...css("muted")}>
        Previewed by the backend from the persisted effective inverse.
      </p>
      <div {...css("row", "compact-row")}>
        <div>
          <label htmlFor="quote-index">Quote index</label>
          <input
            id="quote-index"
            type="number"
            min={0}
            value={index}
            onChange={(event) => setIndex(Number(event.target.value))}
          />
        </div>
        <div>
          <label htmlFor="quote-size">Bump size</label>
          <input
            id="quote-size"
            type="number"
            step="0.0001"
            value={size}
            onChange={(event) => setSize(Number(event.target.value))}
          />
        </div>
        <button
          type="button"
          onClick={() => {
            setError(null);
            void preview(runId, index, size)
              .then(setResult)
              .catch((reason: unknown) => setError(String(reason)));
          }}
        >
          Preview bump
        </button>
      </div>
      {error && <div {...css("error")}>{error}</div>}
      {result && (
        <div {...css("table-container")}>
          <table>
            <thead>
              <tr><th>Parameter axis</th><th {...css("num")}>Δ parameter</th></tr>
            </thead>
            <tbody>
              {result.delta_parameters.map((item) => (
                <tr key={item.parameter_axis}>
                  <td {...css("mono")}>{item.parameter_axis}</td>
                  <td {...css("num")}>{item.delta.toFixed(8)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </section>
  );
}
