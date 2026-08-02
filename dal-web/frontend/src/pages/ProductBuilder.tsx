import { useCallback, useEffect, useRef, useState } from "react";
import { api, type EventRow, type ProductDefinition, type ProductTemplate } from "../api/client";
import PageHeader from "../components/PageHeader";
import { css, inlineStyle } from "../format";

const EMPTY_ROW: EventRow = { date_kind: "label", label: "", event: "" };

type EditorRow = EventRow & { row_id: number };

export default function ProductBuilder() {
  const rowSeq = useRef(0);
  const makeRow = (row: EventRow = EMPTY_ROW): EditorRow => {
    const id = rowSeq.current++;
    return { ...row, row_id: id };
  };

  const [templates, setTemplates] = useState<ProductTemplate[]>([]);
  const [products, setProducts] = useState<ProductDefinition[]>([]);
  const [name, setName] = useState("My Product");
  const [description, setDescription] = useState("");
  const [rows, setRows] = useState<EditorRow[]>([makeRow()]);
  const [debug, setDebug] = useState<string>("");
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  function apiRows(): EventRow[] {
    return rows.map((r) => ({
      date_kind: r.date_kind,
      date: r.date,
      label: r.label,
      event: r.event,
    }));
  }

  const refresh = useCallback(() => {
    return api.listProducts().then((p) => { setProducts(p); });
  }, []);

  useEffect(() => {
    void Promise.allSettled([
      api.listTemplates().then((t) => { setTemplates(t); }),
      refresh(),
    ]).then((results) => {
      const rejected = results.find((r): r is PromiseRejectedResult => r.status === 'rejected');
      if (rejected) {
        setError(String(rejected.reason));
      }
      setLoading(false);
    });
  }, [refresh]);

  function loadSavedProduct(product: ProductDefinition) {
    setName(product.name);
    setDescription(product.description);
    setRows(product.rows.map((r) => makeRow(r)));
    setDebug("");
    setError(null);
  }

  function loadTemplate(key: string) {
    const tpl = templates.find((t) => t.key === key);
    if (!tpl) {
      return;
    }
    setName(tpl.name);
    setDescription(tpl.description);
    setRows(tpl.rows.map((r) => makeRow(r)));
    setDebug("");
  }

  function updateRow(rowId: number, patch: Partial<EventRow>) {
    setRows((rs) => rs.map((r) => (r.row_id === rowId ? { ...r, ...patch } : r)));
  }

  function addRow() {
    setRows((rs) => [...rs, makeRow()]);
  }

  function removeRow(rowId: number) {
    setRows((rs) => rs.filter((r) => r.row_id !== rowId));
  }

  async function runDebug() {
    setError(null);
    try {
      const res = await api.debugProduct(apiRows());
      setDebug(res.debug);
    } catch (e: unknown) {
      setError(String(e));
    }
  }

  async function save() {
    setError(null);
    try {
      await api.createProduct({ name, description, template: null, rows: apiRows() });
      await refresh();
    } catch (e: unknown) {
      setError(String(e));
    }
  }

  async function removeProduct(id: string) {
    const product = products.find((p) => p.id === id);
    if (!window.confirm(`Delete product "${product?.name ?? id}"? This cannot be undone.`)) {
      return;
    }
    try {
      await api.deleteProduct(id);
      await refresh();
    } catch (e: unknown) {
      setError(String(e));
    }
  }

  return (
    <div>
      <PageHeader
        eyebrow="BOOKS / SCRIPTED PRODUCTS"
        title="Product Builder"
        subtitle="Compose DAL scripted products as a schedule of (date, event) rows."
      />

      {error && <div {...css("error")}>{error}</div>}

      {loading ? (
        <div {...css("panel")}>
          <p {...css("muted")}>Loading products…</p>
        </div>
      ) : (
      <>
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
          <h3 {...css("panel-title")}>Definition</h3>
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

          <h3 {...css("panel-title")}>Event schedule</h3>
          {rows.map((r) => (
            <div {...css("event-builder-row")} key={r.row_id}>
              <select
                value={r.date_kind}
                onChange={(e) => {
                  updateRow(r.row_id, { date_kind: e.target.value as "date" | "label" });
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
                    updateRow(r.row_id, { date: e.target.value });
                  }}
                />
              ) : (
                <input
                  placeholder="STRIKE or START:… END:… FREQ:1W"
                  value={r.label ?? ""}
                  onChange={(e) => {
                    updateRow(r.row_id, { label: e.target.value });
                  }}
                />
              )}
              <textarea
                placeholder="event script, e.g. call pays MAX(spot() - STRIKE, 0.0)"
                value={r.event}
                onChange={(e) => {
                  updateRow(r.row_id, { event: e.target.value });
                }}
              />
              <button
                type="button"
                {...css("danger")}
                onClick={() => {
                  removeRow(r.row_id);
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
          <h3 {...css("panel-title")}>DAL product debug</h3>
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
        <h3 {...css("panel-title")}>Saved products</h3>
        {products.length === 0 ? (
          <p {...css("muted")}>No saved products yet. Build one above and click Save.</p>
        ) : (
          <div {...css("table-container")}>
            <table>
              <thead>
                <tr>
                  <th>Name</th>
                  <th>Description</th>
                  <th {...css("num")}># rows</th>
                  <th></th>
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
                        {...css("ghost")}
                        onClick={() => {
                          loadSavedProduct(p);
                        }}
                      >
                        Load
                      </button>
                    </td>
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
        )}
      </div>
      </>
      )}
    </div>
  );
}
