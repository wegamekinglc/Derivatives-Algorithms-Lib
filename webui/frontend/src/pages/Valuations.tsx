import { Fragment, useEffect, useState } from "react";
import { api, type ValuationResult } from "../api/client";
import { css, fmtMoney, fmtNum, inlineStyle } from "../format";

export default function Valuations() {
  const [runs, setRuns] = useState<ValuationResult[]>([]);
  const [open, setOpen] = useState<string | null>(null);

  useEffect(() => {
    void api.listValuations().then(setRuns);
  }, []);

  return (
    <div>
      <div {...css("page-header")}>
        <div>
          <h1>Valuation Runs</h1>
          <p>Reproducible history of every Monte Carlo valuation.</p>
        </div>
      </div>

      {runs.length === 0 ? (
        <p {...css("muted")}>No valuation runs recorded yet.</p>
      ) : (
        <table>
          <thead>
            <tr>
              <th>When</th>
              <th>Target</th>
              <th>Backend</th>
              <th {...css("num")}># paths</th>
              <th>AAD</th>
              <th {...css("num")}>PV</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            {runs.map((r) => (
              <Fragment key={r.id}>
                <tr>
                  <td {...css("mono")}>{new Date(r.created_at).toLocaleString()}</td>
                  <td>{r.target_kind}</td>
                  <td>
                    {r.backend}
                    {r.is_native ? "" : " (stub)"}
                  </td>
                  <td {...css("num")}>{r.config.num_paths.toLocaleString()}</td>
                  <td>{r.config.enable_aad ? "yes" : "no"}</td>
                  <td {...css("num")}>{fmtMoney(r.total_pv)}</td>
                  <td>
                    <button
                      type="button"
                      {...css("ghost")}
                      onClick={() => {
                        setOpen(open === r.id ? null : r.id);
                      }}
                    >
                      {open === r.id ? "Hide" : "Details"}
                    </button>
                  </td>
                </tr>
                {open === r.id && (
                  <tr key={r.id + "-detail"}>
                    <td colSpan={7}>
                      <div {...css("panel")} {...inlineStyle({ margin: 0 })}>
                        <h2>Greeks</h2>
                        {Object.keys(r.total_greeks).length === 0 ? (
                          <p {...css("muted")}>No Greeks (AAD disabled).</p>
                        ) : (
                          <div>
                            {Object.entries(r.total_greeks).map(([k, v]) => (
                              <span {...css("pill greek")} key={k} {...inlineStyle({ marginRight: 6 })}>
                                {k}: {fmtNum(v, 2)}
                              </span>
                            ))}
                          </div>
                        )}
                        <h2 {...inlineStyle({ marginTop: 16 })}>Trades</h2>
                        <table>
                          <thead>
                            <tr>
                              <th>Trade</th>
                              <th {...css("num")}>Unit PV</th>
                              <th {...css("num")}>Scaled PV</th>
                            </tr>
                          </thead>
                          <tbody>
                            {r.trades.map((t) => (
                              <tr key={t.trade_id}>
                                <td>{t.trade_name}</td>
                                <td {...css("num")}>{fmtNum(t.pv)}</td>
                                <td {...css("num")}>{fmtMoney(t.scaled_pv)}</td>
                              </tr>
                            ))}
                          </tbody>
                        </table>
                      </div>
                    </td>
                  </tr>
                )}
              </Fragment>
            ))}
          </tbody>
        </table>
      )}
    </div>
  );
}
