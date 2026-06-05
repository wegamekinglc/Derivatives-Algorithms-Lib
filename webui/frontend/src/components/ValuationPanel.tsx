import { useState } from "react";
import { api, type ValuationConfig, type ValuationResult } from "../api/client";
import { css, fmtMoney, fmtNum, inlineStyle } from "../format";

interface Props {
  onRun: (config: ValuationConfig) => Promise<ValuationResult>;
  title?: string;
}

const PATH_CHOICES = [10, 12, 14, 16, 18, 20];
const POLL_INTERVAL_MS = 300;
const MAX_POLL_ATTEMPTS = 200;

export default function ValuationPanel({ onRun, title = "Run valuation" }: Props) {
  const [pathsPow, setPathsPow] = useState(16);
  const [method, setMethod] = useState<"sobol" | "pseudo">("sobol");
  const [aad, setAad] = useState(true);
  const [bb, setBb] = useState(false);
  const [evalDate, setEvalDate] = useState("2022-09-15");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<ValuationResult | null>(null);
  const [statusLabel, setStatusLabel] = useState<string | null>(null);

  async function run() {
    setBusy(true);
    setError(null);
    setStatusLabel("submitting…");
    try {
      const request: ValuationConfig = {
        num_paths: 2 ** pathsPow,
        method,
        use_brownian_bridge: bb,
        enable_aad: aad,
        smooth: 0.01,
        evaluation_date: evalDate || null,
      };
      // Backend now returns a pending result with status="running".
      const pending = await onRun(request);
      setResult(pending);
      setStatusLabel("pricing…");

      // Poll the backend until the background pricing task completes.
      let current = pending;
      for (let i = 0; i < MAX_POLL_ATTEMPTS && current.status === "running"; i++) {
        await new Promise((r) => setTimeout(r, POLL_INTERVAL_MS));
        current = await api.getValuation(pending.id);
        setResult(current);
      }
      if (current.status === "running") {
        setError("Valuation timed out — check the Valuations page for progress.");
      } else if (current.status === "failed") {
        setError("Valuation failed on the server. Check the backend logs.");
      }
      setStatusLabel(null);
    } catch (e: unknown) {
      setError(String(e));
      setStatusLabel(null);
    } finally {
      setBusy(false);
    }
  }

  return (
    <div {...css("panel")}>
      <h2>{title}</h2>
      {error && <div {...css("error")}>{error}</div>}
      <div {...css("row")} {...inlineStyle({ marginBottom: 12 })}>
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
      <div {...css("row")} {...inlineStyle({ marginBottom: 12 })}>
        <label {...inlineStyle({ display: "flex", gap: 8, alignItems: "center" })}>
          <input
            type="checkbox"
            {...inlineStyle({ width: "auto" })}
            checked={aad}
            onChange={(e) => {
              setAad(e.target.checked);
            }}
          />
          Enable AAD (Greeks)
        </label>
        <label {...inlineStyle({ display: "flex", gap: 8, alignItems: "center" })}>
          <input
            type="checkbox"
            {...inlineStyle({ width: "auto" })}
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
          {busy ? (statusLabel ?? "Pricing…") : "Run valuation"}
        </button>
      </div>

      {result && result.status !== "running" && (
        <div>
          <div {...css("cards")} {...inlineStyle({ marginBottom: 12 })}>
            <div {...css("card")}>
              <h3>Total PV</h3>
              <div {...css(`metric ${result.total_pv >= 0 ? "pos" : "neg"}`)}>
                {fmtMoney(result.total_pv)}
              </div>
            </div>
            {Object.entries(result.total_greeks).map(([k, v]) => (
              <div {...css("card")} key={k}>
                <h3>{k}</h3>
                <div {...css("metric")}>{fmtNum(v, 2)}</div>
              </div>
            ))}
          </div>
          {result.trades.length > 1 && (
            <table>
              <thead>
                <tr>
                  <th>Trade</th>
                  <th {...css("num")}>Unit PV</th>
                  <th {...css("num")}>Scaled PV</th>
                  <th>Greeks</th>
                </tr>
              </thead>
              <tbody>
                {result.trades.map((t) => (
                  <tr key={t.trade_id}>
                    <td>{t.trade_name}</td>
                    <td {...css("num")}>{fmtNum(t.pv)}</td>
                    <td {...css("num")}>{fmtMoney(t.scaled_pv)}</td>
                    <td>
                      {t.error ? (
                        <span {...css("muted")}>err: {t.error}</span>
                      ) : (
                        Object.entries(t.greeks).map(([k, v]) => (
                          <span {...css("pill greek")} key={k} {...inlineStyle({ marginRight: 4 })}>
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
