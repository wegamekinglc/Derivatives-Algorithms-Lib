import { useEffect, useState } from "react";
import { api, EventRow, ProductDefinition, ProductTemplate } from "../api/client";

const EMPTY_ROW: EventRow = { date_kind: "label", label: "", event: "" };

export default function ProductBuilder() {
  const [templates, setTemplates] = useState<ProductTemplate[]>([]);
  const [products, setProducts] = useState<ProductDefinition[]>([]);
  const [name, setName] = useState("My Product");
  const [description, setDescription] = useState("");
  const [rows, setRows] = useState<EventRow[]>([{ ...EMPTY_ROW }]);
  const [debug, setDebug] = useState<string>("");
  const [error, setError] = useState<string | null>(null);

  function refresh() {
    api.listProducts().then(setProducts);
  }
  useEffect(() => {
    api.listTemplates().then(setTemplates);
    refresh();
  }, []);

  function loadTemplate(key: string) {
    const tpl = templates.find((t) => t.key === key);
    if (!tpl) return;
    setName(tpl.name);
    setDescription(tpl.description);
    setRows(tpl.rows.map((r) => ({ ...r })));
    setDebug("");
  }

  function updateRow(i: number, patch: Partial<EventRow>) {
    setRows((rs) => rs.map((r, idx) => (idx === i ? { ...r, ...patch } : r)));
  }

  function addRow() {
    setRows((rs) => [...rs, { ...EMPTY_ROW }]);
  }

  function removeRow(i: number) {
    setRows((rs) => rs.filter((_, idx) => idx !== i));
  }

  async function runDebug() {
    setError(null);
    try {
      const res = await api.debugProduct(rows);
      setDebug(res.debug);
    } catch (e) {
      setError(String(e));
    }
  }

  async function save() {
    setError(null);
    try {
      await api.createProduct({ name, description, template: null, rows });
      refresh();
    } catch (e) {
      setError(String(e));
    }
  }

  return (
    <div>
      <div className="page-header">
        <div>
          <h1>Product Builder</h1>
          <p>Compose DAL scripted products as a schedule of (date, event) rows.</p>
        </div>
      </div>

      {error && <div className="error">{error}</div>}

      <div className="toolbar">
        <span className="muted">Start from template:</span>
        {templates.map((t) => (
          <button key={t.key} className="ghost" onClick={() => loadTemplate(t.key)}>
            {t.name}
          </button>
        ))}
      </div>

      <div className="grid-2">
        <div className="panel">
          <h2>Definition</h2>
          <div className="field">
            <label>Name</label>
            <input value={name} onChange={(e) => setName(e.target.value)} />
          </div>
          <div className="field">
            <label>Description</label>
            <input value={description} onChange={(e) => setDescription(e.target.value)} />
          </div>

          <label>Event schedule</label>
          {rows.map((r, i) => (
            <div className="event-builder-row" key={i}>
              <select
                value={r.date_kind}
                onChange={(e) =>
                  updateRow(i, { date_kind: e.target.value as "date" | "label" })
                }
              >
                <option value="date">date</option>
                <option value="label">label</option>
              </select>
              {r.date_kind === "date" ? (
                <input
                  type="date"
                  value={r.date ?? ""}
                  onChange={(e) => updateRow(i, { date: e.target.value })}
                />
              ) : (
                <input
                  placeholder="STRIKE or START:… END:… FREQ:1W"
                  value={r.label ?? ""}
                  onChange={(e) => updateRow(i, { label: e.target.value })}
                />
              )}
              <textarea
                placeholder="event script, e.g. call pays MAX(spot() - STRIKE, 0.0)"
                value={r.event}
                onChange={(e) => updateRow(i, { event: e.target.value })}
              />
              <button className="danger" onClick={() => removeRow(i)}>
                ×
              </button>
            </div>
          ))}
          <div className="toolbar" style={{ marginTop: 12 }}>
            <button className="ghost" onClick={addRow}>
              + Add row
            </button>
            <button className="ghost" onClick={runDebug}>
              Debug (DAL)
            </button>
            <button onClick={save}>Save product</button>
          </div>
        </div>

        <div className="panel">
          <h2>DAL product debug</h2>
          {debug ? (
            <pre className="debug">{debug}</pre>
          ) : (
            <p className="muted">
              Click <b>Debug (DAL)</b> to render the product through Product_New /
              Product_Debug.
            </p>
          )}
        </div>
      </div>

      <div className="panel">
        <h2>Saved products</h2>
        <table>
          <thead>
            <tr>
              <th>Name</th>
              <th>Description</th>
              <th className="num"># rows</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            {products.map((p) => (
              <tr key={p.id}>
                <td>{p.name}</td>
                <td className="muted">{p.description}</td>
                <td className="num">{p.rows.length}</td>
                <td>
                  <button
                    className="danger"
                    onClick={async () => {
                      await api.deleteProduct(p.id);
                      refresh();
                    }}
                  >
                    Delete
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}
