import { useEffect, useState } from "react";
import { api, type EventRow, type ProductDefinition, type ProductTemplate } from "../api/client";
import { css, inlineStyle } from "../format";

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
    void api.listProducts().then(setProducts);
  }

  useEffect(() => {
    void api.listTemplates().then(setTemplates);
    refresh();
  }, []);

  function loadTemplate(key: string) {
    const tpl = templates.find((t) => t.key === key);
    if (!tpl) {
      return;
    }
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
    } catch (e: unknown) {
      setError(String(e));
    }
  }

  async function save() {
    setError(null);
    try {
      await api.createProduct({ name, description, template: null, rows });
      refresh();
    } catch (e: unknown) {
      setError(String(e));
    }
  }

  async function removeProduct(id: string) {
    await api.deleteProduct(id);
    refresh();
  }

  return (
    <div>
      <div {...css("page-header")}>
        <div>
          <h1>Product Builder</h1>
          <p>Compose DAL scripted products as a schedule of (date, event) rows.</p>
        </div>
      </div>

      {error && <div {...css("error")}>{error}</div>}

      <div {...css("toolbar")}>
        <span {...css("muted")}>Start from template:</span>
        {templates.map((t) => (
          <button
            type="button"
            key={t.key}
            {...css("ghost")}
            onClick={() => {
              loadTemplate(t.key);
            }}
          >
            {t.name}
          </button>
        ))}
      </div>

      <div {...css("grid-2")}>
        <div {...css("panel")}>
          <h2>Definition</h2>
          <div {...css("field")}>
            <label htmlFor="product-name">Name</label>
            <input
              id="product-name"
              value={name}
              onChange={(e) => {
                setName(e.target.value);
              }}
            />
          </div>
          <div {...css("field")}>
            <label htmlFor="product-description">Description</label>
            <input
              id="product-description"
              value={description}
              onChange={(e) => {
                setDescription(e.target.value);
              }}
            />
          </div>

          <label>Event schedule</label>
          {rows.map((r, i) => (
            <div {...css("event-builder-row")} key={i}>
              <select
                value={r.date_kind}
                onChange={(e) => {
                  updateRow(i, { date_kind: e.target.value as "date" | "label" });
                }}
              >
                <option value="date">date</option>
                <option value="label">label</option>
              </select>
              {r.date_kind === "date" ? (
                <input
                  type="date"
                  value={r.date ?? ""}
                  onChange={(e) => {
                    updateRow(i, { date: e.target.value });
                  }}
                />
              ) : (
                <input
                  placeholder="STRIKE or START:… END:… FREQ:1W"
                  value={r.label ?? ""}
                  onChange={(e) => {
                    updateRow(i, { label: e.target.value });
                  }}
                />
              )}
              <textarea
                placeholder="event script, e.g. call pays MAX(spot() - STRIKE, 0.0)"
                value={r.event}
                onChange={(e) => {
                  updateRow(i, { event: e.target.value });
                }}
              />
              <button
                type="button"
                {...css("danger")}
                onClick={() => {
                  removeRow(i);
                }}
              >
                ×
              </button>
            </div>
          ))}
          <div {...css("toolbar")} {...inlineStyle({ marginTop: 12 })}>
            <button type="button" {...css("ghost")} onClick={addRow}>
              + Add row
            </button>
            <button
              type="button"
              {...css("ghost")}
              onClick={() => {
                void runDebug();
              }}
            >
              Debug (DAL)
            </button>
            <button
              type="button"
              onClick={() => {
                void save();
              }}
            >
              Save product
            </button>
          </div>
        </div>

        <div {...css("panel")}>
          <h2>DAL product debug</h2>
          {debug ? (
            <pre {...css("debug")}>{debug}</pre>
          ) : (
            <p {...css("muted")}>
              Click <b>Debug (DAL)</b> to render the product through Product_New /
              Product_Debug.
            </p>
          )}
        </div>
      </div>

      <div {...css("panel")}>
        <h2>Saved products</h2>
        <table>
          <thead>
            <tr>
              <th>Name</th>
              <th>Description</th>
              <th {...css("num")}># rows</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            {products.map((p) => (
              <tr key={p.id}>
                <td>{p.name}</td>
                <td {...css("muted")}>{p.description}</td>
                <td {...css("num")}>{p.rows.length}</td>
                <td>
                  <button
                    type="button"
                    {...css("danger")}
                    onClick={() => {
                      void removeProduct(p.id);
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
