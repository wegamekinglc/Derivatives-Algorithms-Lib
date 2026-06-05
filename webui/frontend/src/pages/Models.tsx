import { useEffect, useState } from "react";
import { api, type ModelDefinition, type ModelKind } from "../api/client";
import { css, fmtNum, inlineStyle } from "../format";

export default function Models() {
  const [models, setModels] = useState<ModelDefinition[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  // Shared fields
  const [name, setName] = useState("BS spot=100 vol=20%");
  const [kind, setKind] = useState<ModelKind>("BSModelData_");

  // Black-Scholes params
  const [spot, setSpot] = useState(100);
  const [vol, setVol] = useState(0.2);
  const [rate, setRate] = useState(0.0);
  const [div, setDiv] = useState(0.0);

  // Dupire params
  const [dupireSpot, setDupireSpot] = useState(100);
  const [dupireRate, setDupireRate] = useState(0.0);
  const [dupireRepo, setDupireRepo] = useState(0.0);
  const [dupireSpotsText, setDupireSpotsText] = useState("90, 100, 110");
  const [dupireTimesText, setDupireTimesText] = useState("0.25, 0.5, 1.0");
  const [dupireVolsText, setDupireVolsText] = useState("0.22, 0.20, 0.19\n0.21, 0.20, 0.20\n0.19, 0.20, 0.22");

  function refresh() {
    return api.listModels().then(setModels).catch((e: unknown) => {
      setError(String(e));
    });
  }

  useEffect(() => {
    void refresh().finally(() => setLoading(false));
  }, []);

  function parseNumberList(text: string): number[] {
    return text
      .split(/[,\s]+/)
      .filter((s) => s.length > 0)
      .map(Number);
  }

  function parseMatrix(text: string): number[][] {
    return text
      .split("\n")
      .filter((line) => line.trim().length > 0)
      .map((line) => parseNumberList(line));
  }

  async function create() {
    setError(null);
    try {
      if (kind === "BSModelData_") {
        await api.createModel({
          name,
          kind: "BSModelData_",
          bs: { spot, vol, rate, div },
        });
      } else {
        await api.createModel({
          name,
          kind: "DupireModelData_",
          dupire: {
            spot: dupireSpot,
            rate: dupireRate,
            repo: dupireRepo,
            spots: parseNumberList(dupireSpotsText),
            times: parseNumberList(dupireTimesText),
            vols: parseMatrix(dupireVolsText),
          },
        });
      }
      refresh();
    } catch (e: unknown) {
      setError(String(e));
    }
  }

  async function remove(id: string) {
    const model = models.find((m) => m.id === id);
    if (!window.confirm(`Delete model "${model?.name ?? id}"? This cannot be undone.`)) {
      return;
    }
    try {
      await api.deleteModel(id);
      refresh();
    } catch (e: unknown) {
      setError(String(e));
    }
  }

  return (
    <div>
      <div {...css("page-header")}>
        <div>
          <h1>Models</h1>
          <p>Black-Scholes model data passed to DAL via BSModelData_New.</p>
        </div>
      </div>

      {error && <div {...css("error")}>{error}</div>}

      {loading ? (
        <div {...css("panel")}>
          <p {...css("muted")}>Loading models…</p>
        </div>
      ) : (
      <>
      <div {...css("panel")}>
        <h2>New model</h2>
        <div {...css("field")}>
          <label htmlFor="model-name">Name</label>
          <input
            id="model-name"
            value={name}
            onChange={(e) => {
              setName(e.target.value);
            }}
          />
        </div>
        <div {...css("field")}>
          <label htmlFor="model-kind">Model kind</label>
          <select
            id="model-kind"
            value={kind}
            onChange={(e) => {
              setKind(e.target.value as ModelKind);
            }}
            {...inlineStyle({ maxWidth: 320 })}
          >
            <option value="BSModelData_">Black-Scholes</option>
            <option value="DupireModelData_">Dupire (local vol surface)</option>
          </select>
        </div>

        {kind === "BSModelData_" ? (
          <div {...css("row")}>
            <div>
              <label htmlFor="model-spot">Spot</label>
              <input
                id="model-spot"
                type="number"
                value={spot}
                onChange={(e) => {
                  setSpot(Number(e.target.value));
                }}
              />
            </div>
            <div>
              <label htmlFor="model-vol">Vol</label>
              <input
                id="model-vol"
                type="number"
                step="0.01"
                value={vol}
                onChange={(e) => {
                  setVol(Number(e.target.value));
                }}
              />
            </div>
            <div>
              <label htmlFor="model-rate">Rate</label>
              <input
                id="model-rate"
                type="number"
                step="0.01"
                value={rate}
                onChange={(e) => {
                  setRate(Number(e.target.value));
                }}
              />
            </div>
            <div>
              <label htmlFor="model-dividend">Dividend</label>
              <input
                id="model-dividend"
                type="number"
                step="0.01"
                value={div}
                onChange={(e) => {
                  setDiv(Number(e.target.value));
                }}
              />
            </div>
          </div>
        ) : (
          <div>
            <p {...css("muted")} {...inlineStyle({ marginTop: 0, marginBottom: 12 })}>
              Dupire uses a local volatility surface σ(S, t). Enter spot strikes
              (one row), times (one row), and a vols matrix (one row per strike,
              one column per time).
            </p>
            <div {...css("row")}>
              <div>
                <label htmlFor="dupire-spot">Spot</label>
                <input
                  id="dupire-spot"
                  type="number"
                  value={dupireSpot}
                  onChange={(e) => setDupireSpot(Number(e.target.value))}
                />
              </div>
              <div>
                <label htmlFor="dupire-rate">Rate</label>
                <input
                  id="dupire-rate"
                  type="number"
                  step="0.01"
                  value={dupireRate}
                  onChange={(e) => setDupireRate(Number(e.target.value))}
                />
              </div>
              <div>
                <label htmlFor="dupire-repo">Repo</label>
                <input
                  id="dupire-repo"
                  type="number"
                  step="0.01"
                  value={dupireRepo}
                  onChange={(e) => setDupireRepo(Number(e.target.value))}
                />
              </div>
            </div>
            <div {...css("field")}>
              <label htmlFor="dupire-spots">Spot strikes (comma-separated)</label>
              <input
                id="dupire-spots"
                value={dupireSpotsText}
                onChange={(e) => setDupireSpotsText(e.target.value)}
              />
            </div>
            <div {...css("field")}>
              <label htmlFor="dupire-times">Times in years (comma-separated)</label>
              <input
                id="dupire-times"
                value={dupireTimesText}
                onChange={(e) => setDupireTimesText(e.target.value)}
              />
            </div>
            <div {...css("field")}>
              <label htmlFor="dupire-vols">Vols matrix (one row per strike, whitespace-separated)</label>
              <textarea
                id="dupire-vols"
                value={dupireVolsText}
                onChange={(e) => setDupireVolsText(e.target.value)}
                rows={4}
              />
            </div>
          </div>
        )}

        <div {...inlineStyle({ marginTop: 12 })}>
          <button
            type="button"
            onClick={() => {
              void create();
            }}
          >
            Create model
          </button>
        </div>
      </div>

      <table>
        <thead>
          <tr>
            <th>Name</th>
            <th>Kind</th>
            <th {...css("num")}>Spot</th>
            <th {...css("num")}>Vol</th>
            <th {...css("num")}>Rate</th>
            <th {...css("num")}>Div/Repo</th>
            <th></th>
          </tr>
        </thead>
        <tbody>
          {models.map((m) => (
            <tr key={m.id}>
              <td>{m.name}</td>
              <td {...css("mono")}>{m.kind}</td>
              <td {...css("num")}>
                {m.bs ? fmtNum(m.bs.spot, 2) : m.dupire ? fmtNum(m.dupire.spot, 2) : "-"}
              </td>
              <td {...css("num")}>
                {m.bs
                  ? fmtNum(m.bs.vol, 4)
                  : m.dupire
                  ? "(surface)"
                  : "-"}
              </td>
              <td {...css("num")}>
                {m.bs ? fmtNum(m.bs.rate, 4) : m.dupire ? fmtNum(m.dupire.rate, 4) : "-"}
              </td>
              <td {...css("num")}>
                {m.bs ? fmtNum(m.bs.div, 4) : m.dupire ? fmtNum(m.dupire.repo, 4) : "-"}
              </td>
              <td>
                <button
                  type="button"
                  {...css("danger")}
                  onClick={() => {
                    void remove(m.id);
                  }}
                >
                  Delete
                </button>
              </td>
            </tr>
          ))}
        </tbody>
      </table>
      </>
      )}
    </div>
  );
}
