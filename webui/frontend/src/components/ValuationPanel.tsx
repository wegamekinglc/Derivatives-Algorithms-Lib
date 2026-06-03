import { useState } from "react";
import { type ValuationConfig, type ValuationResult } from "../api/client";
import { fmtMoney, fmtNum } from "../format";

interface Props {
  onRun: (config: ValuationConfig) => Promise<ValuationResult>;
  title?: string;
}

const PATH_CHOICES = [10, 12, 14, 16, 18, 20];

export default function ValuationPanel({ onRun, title = "Run valuation" }: Props) {
  const [pathsPow, setPathsPow] = useState(16);
  const [method, setMethod] = useState<"sobol" | "pseudo">("sobol");
  const [aad, setAad] = useState(true);
  const [bb, setBb] = useState(false);
  const [evalDate, setEvalDate] = useState("2022-09-15");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<ValuationResult | null>(null);

  async function run() {
    setBusy(true);
    setError(null);
    try {
      const request: ValuationConfig = {
        num_paths: 2 ** pathsPow,
        method,
        use_brownian_bridge: bb,
        enable_aad: aad,
        smooth: 0.01,
        evaluation_date: evalDate || null,
      };
      setResult(await onRun(request));
    } catch (e: unknown) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  }

  return (
    <div className="panel">
      <h2>{title}</h2>
      {error && <div className="error">{error}</div>}
      <div className="row" style={{ marginBottom: 12 }}>
        <div>
          <label htmlFor="valuation-paths"># paths (2^n)</label>
          <select
            id="valuation-paths"
            value={pathsPow}
            onChange={(e) => {
              setPathsPow(Number(e.target.value));
            }}
          >
            {PATH_CHOICES.map((p) => (
              <option key={p} value={p}>
                2^{p} = {(2 ** p).toLocaleString()}
              </option>
            ))}
          </select>
        </div>
        <div>
          <label htmlFor="valuation-method">RNG method</label>
          <select
            id="valuation-method"
            value={method}
            onChange={(e) => {
              setMethod(e.target.value as "sobol" | "pseudo");
            }}
          >
            <option value="sobol">sobol</option>
            <option value="pseudo">pseudo</option>
          </select>
        </div>
        <div>
          <label htmlFor="valuation-date">Evaluation date</label>
          <input
            id="valuation-date"
            type="date"
            value={evalDate}
            onChange={(e) => {
              setEvalDate(e.target.value);
            }}
          />
        </div>
      </div>
      <div className="row" style={{ marginBottom: 12 }}>
        <label style={{ display: "flex", gap: 8, alignItems: "center" }}>
          <input
            type="checkbox"
            style={{ width: "auto" }}
            checked={aad}
            onChange={(e) => {
              setAad(e.target.checked);
            }}
          />
          Enable AAD (Greeks)
        </label>
        <label style={{ display: "flex", gap: 8, alignItems: "center" }}>
          <input
            type="checkbox"
            style={{ width: "auto" }}
            checked={bb}
            onChange={(e) => {
              setBb(e.target.checked);
            }}
          />
          Brownian bridge
        </label>
        <button
          type="button"
          onClick={() => {
            void run();
          }}
          disabled={busy}
        >
          {busy ? "Pricing…" : "Run valuation"}
        </button>
      </div>

      {result && (
        <div>
          <div className="cards" style={{ marginBottom: 12 }}>
            <div className="card">
              <h3>Total PV</h3>
              <div className={"metric " + (result.total_pv >= 0 ? "pos" : "neg")}>
                {fmtMoney(result.total_pv)}
              </div>
            </div>
            {Object.entries(result.total_greeks).map(([k, v]) => (
              <div className="card" key={k}>
                <h3>{k}</h3>
                <div className="metric">{fmtNum(v, 2)}</div>
              </div>
            ))}
          </div>
          {result.trades.length > 1 && (
            <table>
              <thead>
                <tr>
                  <th>Trade</th>
                  <th className="num">Unit PV</th>
                  <th className="num">Scaled PV</th>
                  <th>Greeks</th>
                </tr>
              </thead>
              <tbody>
                {result.trades.map((t) => (
                  <tr key={t.trade_id}>
                    <td>{t.trade_name}</td>
                    <td className="num">{fmtNum(t.pv)}</td>
                    <td className="num">{fmtMoney(t.scaled_pv)}</td>
                    <td>
                      {t.error ? (
                        <span className="muted">err: {t.error}</span>
                      ) : (
                        Object.entries(t.greeks).map(([k, v]) => (
                          <span className="pill greek" key={k} style={{ marginRight: 4 }}>
                            {k}: {fmtNum(v, 2)}
                          </span>
                        ))
                      )}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      )}
    </div>
  );
}
