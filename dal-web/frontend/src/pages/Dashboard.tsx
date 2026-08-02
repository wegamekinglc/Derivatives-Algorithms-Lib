import { useEffect, useState } from "react";
import { api, type Portfolio, type Trade, type ValuationResult } from "../api/client";
import PageHeader from "../components/PageHeader";
import { css, fmtMoney, inlineStyle } from "../format";

export default function Dashboard() {
  const [portfolios, setPortfolios] = useState<Portfolio[]>([]);
  const [trades, setTrades] = useState<Trade[]>([]);
  const [valuations, setValuations] = useState<ValuationResult[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    void Promise.allSettled([
      api.listPortfolios().then((p) => { setPortfolios(p); }),
      api.listTrades().then((t) => { setTrades(t); }),
      api.listValuations().then((v) => { setValuations(v); }),
    ]).then((results) => {
      const rejected = results.find((r): r is PromiseRejectedResult => r.status === 'rejected');
      if (rejected) {
        setError(String(rejected.reason));
      }
      setLoading(false);
    });
  }, []);

  const lastByTarget = new Map<string, ValuationResult>();
  for (const v of valuations) {
    if (!lastByTarget.has(v.target_id)) {
      lastByTarget.set(v.target_id, v);
    }
  }
  const totalPv = portfolios.reduce(
    (acc, pf) => acc + (lastByTarget.get(pf.id)?.total_pv ?? 0),
    0
  );

  return (
    <div>
      <PageHeader
        eyebrow="DAL WORKBENCH / OVERVIEW"
        title="Dashboard"
        subtitle="Portfolio overview and recent valuation activity."
      />

      {error && <div {...css("error")}>{error}</div>}

      <div {...css("cards")} {...inlineStyle({ marginBottom: 24 })}>
        <div {...css("card")}>
          <h3>Portfolios</h3>
          <div {...css("metric")}>{portfolios.length}</div>
        </div>
        <div {...css("card")}>
          <h3>Trades</h3>
          <div {...css("metric")}>{trades.length}</div>
        </div>
        <div {...css("card")}>
          <h3>Latest Portfolio PV</h3>
          <div {...css(`metric ${totalPv >= 0 ? "pos" : "neg"}`)}>
            {fmtMoney(totalPv)}
          </div>
        </div>
        <div {...css("card")}>
          <h3>Valuation Runs</h3>
          <div {...css("metric")}>{valuations.length}</div>
        </div>
      </div>

      <div {...css("panel")}>
        <h3 {...css("panel-title")}>Recent valuation runs</h3>
        {loading ? (
          <p {...css("muted")}>Loading…</p>
        ) : valuations.length === 0 ? (
          <p {...css("muted")}>No valuations yet. Price a portfolio or trade to begin.</p>
        ) : (
          <div {...css("table-container")}>
            <table>
            <thead>
              <tr>
                <th>When</th>
                <th>Target</th>
                <th>Backend</th>
                <th {...css("num")}>PV</th>
                <th {...css("num")}># trades</th>
              </tr>
            </thead>
            <tbody>
              {valuations.slice(0, 8).map((v) => (
                <tr key={v.id}>
                  <td {...css("mono")}>{new Date(v.created_at).toLocaleString()}</td>
                  <td>
                    {v.target_kind}
                    {v.status === "running" && <span {...css("muted")}> (running)</span>}
                    {v.status === "failed" && <span {...css("error-inline")}> (failed)</span>}
                  </td>
                  <td>
                    {v.backend}
                    {v.is_native ? "" : " (stub)"}
                  </td>
                  <td {...css("num")}>{fmtMoney(v.total_pv)}</td>
                  <td {...css("num")}>{v.trades.length}</td>
                </tr>
              ))}
            </tbody>
          </table>
          </div>
        )}
      </div>
    </div>
  );
}
