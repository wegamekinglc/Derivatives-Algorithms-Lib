import { useEffect, useState } from "react";
import { api, Portfolio, Trade } from "../api/client";
import { fmtMoney } from "../format";
import ValuationPanel from "../components/ValuationPanel";

export default function Portfolios() {
  const [portfolios, setPortfolios] = useState<Portfolio[]>([]);
  const [allTrades, setAllTrades] = useState<Trade[]>([]);
  const [selected, setSelected] = useState<Portfolio | null>(null);
  const [members, setMembers] = useState<Trade[]>([]);
  const [name, setName] = useState("New Portfolio");
  const [error, setError] = useState<string | null>(null);
  const [addTradeId, setAddTradeId] = useState("");

  function refresh() {
    api.listPortfolios().then(setPortfolios);
    api.listTrades().then(setAllTrades);
  }
  useEffect(refresh, []);

  async function selectPortfolio(pf: Portfolio) {
    setSelected(pf);
    setMembers(await api.portfolioTrades(pf.id));
  }

  async function create() {
    setError(null);
    try {
      await api.createPortfolio({ name });
      refresh();
    } catch (e) {
      setError(String(e));
    }
  }

  async function addTrade() {
    if (!selected || !addTradeId) return;
    const pf = await api.addTradeToPortfolio(selected.id, addTradeId);
    setSelected(pf);
    setMembers(await api.portfolioTrades(pf.id));
    refresh();
  }

  async function removeTrade(tid: string) {
    if (!selected) return;
    const pf = await api.removeTradeFromPortfolio(selected.id, tid);
    setSelected(pf);
    setMembers(await api.portfolioTrades(pf.id));
    refresh();
  }

  return (
    <div>
      <div className="page-header">
        <div>
          <h1>Portfolios</h1>
          <p>Group trades into books and price the whole book at once.</p>
        </div>
      </div>

      {error && <div className="error">{error}</div>}

      <div className="grid-2">
        <div className="panel">
          <h2>Books</h2>
          <div className="row" style={{ marginBottom: 12 }}>
            <input value={name} onChange={(e) => setName(e.target.value)} />
            <button onClick={create} style={{ flex: "0 0 auto" }}>
              Create
            </button>
          </div>
          <table>
            <thead>
              <tr>
                <th>Name</th>
                <th className="num"># trades</th>
                <th></th>
              </tr>
            </thead>
            <tbody>
              {portfolios.map((pf) => (
                <tr key={pf.id}>
                  <td>{pf.name}</td>
                  <td className="num">{pf.trade_ids.length}</td>
                  <td>
                    <button className="ghost" onClick={() => selectPortfolio(pf)}>
                      Open
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        <div className="panel">
          <h2>{selected ? selected.name : "Select a portfolio"}</h2>
          {selected && (
            <>
              <div className="row" style={{ marginBottom: 12 }}>
                <select value={addTradeId} onChange={(e) => setAddTradeId(e.target.value)}>
                  <option value="">— pick a trade —</option>
                  {allTrades
                    .filter((t) => !selected.trade_ids.includes(t.id))
                    .map((t) => (
                      <option key={t.id} value={t.id}>
                        {t.name}
                      </option>
                    ))}
                </select>
                <button onClick={addTrade} style={{ flex: "0 0 auto" }} disabled={!addTradeId}>
                  Add trade
                </button>
              </div>
              <table>
                <thead>
                  <tr>
                    <th>Trade</th>
                    <th>Book</th>
                    <th className="num">Notional</th>
                    <th></th>
                  </tr>
                </thead>
                <tbody>
                  {members.map((t) => (
                    <tr key={t.id}>
                      <td>{t.name}</td>
                      <td>{t.book}</td>
                      <td className="num">{fmtMoney(t.notional)}</td>
                      <td>
                        <button className="danger" onClick={() => removeTrade(t.id)}>
                          Remove
                        </button>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </>
          )}
        </div>
      </div>

      {selected && (
        <ValuationPanel
          title={`Price portfolio: ${selected.name}`}
          onRun={(config) => api.valuePortfolio(selected.id, config)}
        />
      )}
    </div>
  );
}
