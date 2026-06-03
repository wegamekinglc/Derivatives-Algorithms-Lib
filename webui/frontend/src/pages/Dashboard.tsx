import { useEffect, useState } from "react";
import { api, type Portfolio, type Trade, type ValuationResult } from "../api/client";
import { fmtMoney } from "../format";

export default function Dashboard() {
  const [portfolios, setPortfolios] = useState<Portfolio[]>([]);
  const [trades, setTrades] = useState<Trade[]>([]);
  const [valuations, setValuations] = useState<ValuationResult[]>([]);

  useEffect(() => {
    void api.listPortfolios().then(setPortfolios);
    void api.listTrades().then(setTrades);
    void api.listValuations().then(setValuations);
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
      <div className="page-header">
        <div>
          <h1>Dashboard</h1>
          <p>Portfolio overview and recent valuation activity.</p>
        </div>
      </div>

      <div className="cards" style={{ marginBottom: 24 }}>
        <div className="card">
          <h3>Portfolios</h3>
          <div className="metric">{portfolios.length}</div>
        </div>
        <div className="card">
          <h3>Trades</h3>
          <div className="metric">{trades.length}</div>
        </div>
        <div className="card">
          <h3>Latest Portfolio PV</h3>
          <div className={"metric " + (totalPv >= 0 ? "pos" : "neg")}>
            {fmtMoney(totalPv)}
          </div>
        </div>
        <div className="card">
          <h3>Valuation Runs</h3>
          <div className="metric">{valuations.length}</div>
        </div>
      </div>

      <div className="panel">
        <h2>Recent valuation runs</h2>
        {valuations.length === 0 ? (
          <p className="muted">No valuations yet. Price a portfolio or trade to begin.</p>
        ) : (
          <table>
            <thead>
              <tr>
                <th>When</th>
                <th>Target</th>
                <th>Backend</th>
                <th className="num">PV</th>
                <th className="num"># trades</th>
              </tr>
            </thead>
            <tbody>
              {valuations.slice(0, 8).map((v) => (
                <tr key={v.id}>
                  <td className="mono">{new Date(v.created_at).toLocaleString()}</td>
                  <td>{v.target_kind}</td>
                  <td>
                    {v.backend}
                    {v.is_native ? "" : " (stub)"}
                  </td>
                  <td className="num">{fmtMoney(v.total_pv)}</td>
                  <td className="num">{v.trades.length}</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}
