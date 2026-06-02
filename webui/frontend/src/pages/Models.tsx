import { useEffect, useState } from "react";
import { api, ModelDefinition } from "../api/client";
import { fmtNum } from "../format";

export default function Models() {
  const [models, setModels] = useState<ModelDefinition[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [name, setName] = useState("BS spot=100 vol=20%");
  const [spot, setSpot] = useState(100);
  const [vol, setVol] = useState(0.2);
  const [rate, setRate] = useState(0.0);
  const [div, setDiv] = useState(0.0);

  function refresh() {
    api.listModels().then(setModels).catch((e) => setError(String(e)));
  }
  useEffect(refresh, []);

  async function create() {
    setError(null);
    try {
      await api.createModel({
        name,
        kind: "BSModelData_",
        bs: { spot, vol, rate, div },
      });
      refresh();
    } catch (e) {
      setError(String(e));
    }
  }

  async function remove(id: string) {
    await api.deleteModel(id);
    refresh();
  }

  return (
    <div>
      <div className="page-header">
        <div>
          <h1>Models</h1>
          <p>Black-Scholes model data passed to DAL via BSModelData_New.</p>
        </div>
      </div>

      {error && <div className="error">{error}</div>}

      <div className="panel">
        <h2>New Black-Scholes model</h2>
        <div className="field">
          <label>Name</label>
          <input value={name} onChange={(e) => setName(e.target.value)} />
        </div>
        <div className="row">
          <div>
            <label>Spot</label>
            <input type="number" value={spot} onChange={(e) => setSpot(Number(e.target.value))} />
          </div>
          <div>
            <label>Vol</label>
            <input type="number" step="0.01" value={vol} onChange={(e) => setVol(Number(e.target.value))} />
          </div>
          <div>
            <label>Rate</label>
            <input type="number" step="0.01" value={rate} onChange={(e) => setRate(Number(e.target.value))} />
          </div>
          <div>
            <label>Dividend</label>
            <input type="number" step="0.01" value={div} onChange={(e) => setDiv(Number(e.target.value))} />
          </div>
          <button onClick={create}>Create model</button>
        </div>
      </div>

      <table>
        <thead>
          <tr>
            <th>Name</th>
            <th>Kind</th>
            <th className="num">Spot</th>
            <th className="num">Vol</th>
            <th className="num">Rate</th>
            <th className="num">Div</th>
            <th></th>
          </tr>
        </thead>
        <tbody>
          {models.map((m) => (
            <tr key={m.id}>
              <td>{m.name}</td>
              <td className="mono">{m.kind}</td>
              <td className="num">{m.bs ? fmtNum(m.bs.spot, 2) : "-"}</td>
              <td className="num">{m.bs ? fmtNum(m.bs.vol, 4) : "-"}</td>
              <td className="num">{m.bs ? fmtNum(m.bs.rate, 4) : "-"}</td>
              <td className="num">{m.bs ? fmtNum(m.bs.div, 4) : "-"}</td>
              <td>
                <button className="danger" onClick={() => remove(m.id)}>
                  Delete
                </button>
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
