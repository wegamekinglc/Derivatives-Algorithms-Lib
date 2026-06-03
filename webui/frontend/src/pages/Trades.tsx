import { useCallback, useEffect, useState } from "react";
import {
  api,
  type ModelDefinition,
  type ProductDefinition,
  type Trade,
} from "../api/client";
import { css, fmtMoney, inlineStyle, labelFor } from "../format";
import ValuationPanel from "../components/ValuationPanel";

export default function Trades() {
  const [trades, setTrades] = useState<Trade[]>([]);
  const [products, setProducts] = useState<ProductDefinition[]>([]);
  const [models, setModels] = useState<ModelDefinition[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [selected, setSelected] = useState<string | null>(null);

  const [name, setName] = useState("New Trade");
  const [book, setBook] = useState("EQ-EXOTICS");
  const [counterparty, setCounterparty] = useState("");
  const [notional, setNotional] = useState(1_000_000);
  const [quantity, setQuantity] = useState(1);
  const [productId, setProductId] = useState("");
  const [modelId, setModelId] = useState("");

  const refresh = useCallback(() => {
    void api.listTrades().then(setTrades);
  }, []);

  useEffect(() => {
    refresh();
    void api.listProducts().then((p) => {
      setProducts(p);
      if (p[0]) {
        setProductId(p[0].id);
      }
    });
    void api.listModels().then((m) => {
      setModels(m);
      if (m[0]) {
        setModelId(m[0].id);
      }
    });
  }, [refresh]);

  async function create() {
    setError(null);
    try {
      await api.createTrade({
        name,
        book,
        counterparty,
        notional,
        quantity,
        product_id: productId,
        model_id: modelId,
      });
      refresh();
    } catch (e: unknown) {
      setError(String(e));
    }
  }

  async function remove(id: string) {
    await api.deleteTrade(id);
    refresh();
  }

  const nameById = (list: { id: string; name: string }[], id: string) =>
    list.find((x) => x.id === id)?.name ?? id;

  return (
    <div>
      <div {...css("page-header")}>
        <div>
          <h1>Trades</h1>
          <p>Each trade links a scripted product to a model and a notional.</p>
        </div>
      </div>

      {error && <div {...css("error")}>{error}</div>}

      <div {...css("panel")}>
        <h2>New trade</h2>
        <div {...css("row")} {...inlineStyle({ marginBottom: 12 })}>
          <div>
            <label {...labelFor("trade-name")}>Name</label>
            <input
              id="trade-name"
              value={name}
              onChange={(e) => {
                setName(e.target.value);
              }}
            />
          </div>
          <div>
            <label {...labelFor("trade-book")}>Book</label>
            <input
              id="trade-book"
              value={book}
              onChange={(e) => {
                setBook(e.target.value);
              }}
            />
          </div>
          <div>
            <label {...labelFor("trade-counterparty")}>Counterparty</label>
            <input
              id="trade-counterparty"
              value={counterparty}
              onChange={(e) => {
                setCounterparty(e.target.value);
              }}
            />
          </div>
        </div>
        <div {...css("row")}>
          <div>
            <label {...labelFor("trade-product")}>Product</label>
            <select
              id="trade-product"
              value={productId}
              onChange={(e) => {
                setProductId(e.target.value);
              }}
            >
              {products.map((p) => (
                <option key={p.id} value={p.id}>
                  {p.name}
                </option>
              ))}
            </select>
          </div>
          <div>
            <label {...labelFor("trade-model")}>Model</label>
            <select
              id="trade-model"
              value={modelId}
              onChange={(e) => {
                setModelId(e.target.value);
              }}
            >
              {models.map((m) => (
                <option key={m.id} value={m.id}>
                  {m.name}
                </option>
              ))}
            </select>
          </div>
          <div>
            <label {...labelFor("trade-notional")}>Notional</label>
            <input
              id="trade-notional"
              type="number"
              value={notional}
              onChange={(e) => {
                setNotional(Number(e.target.value));
              }}
            />
          </div>
          <div>
            <label {...labelFor("trade-quantity")}>Quantity</label>
            <input
              id="trade-quantity"
              type="number"
              value={quantity}
              onChange={(e) => {
                setQuantity(Number(e.target.value));
              }}
            />
          </div>
          <button
            type="button"
            onClick={() => {
              void create();
            }}
            disabled={!productId || !modelId}
          >
            Create
          </button>
        </div>
      </div>

      <table>
        <thead>
          <tr>
            <th>Name</th>
            <th>Book</th>
            <th>Product</th>
            <th>Model</th>
            <th {...css("num")}>Notional</th>
            <th></th>
          </tr>
        </thead>
        <tbody>
          {trades.map((t) => (
            <tr key={t.id}>
              <td>{t.name}</td>
              <td>{t.book}</td>
              <td>{nameById(products, t.product_id)}</td>
              <td>{nameById(models, t.model_id)}</td>
              <td {...css("num")}>{fmtMoney(t.notional)}</td>
              <td>
                <button
                  type="button"
                  {...css("ghost")}
                  onClick={() => {
                    setSelected(t.id);
                  }}
                >
                  Price
                </button>{" "}
                <button
                  type="button"
                  {...css("danger")}
                  onClick={() => {
                    void remove(t.id);
                  }}
                >
                  Delete
                </button>
              </td>
            </tr>
          ))}
        </tbody>
      </table>

      {selected && (
        <div {...inlineStyle({ marginTop: 18 })}>
          <ValuationPanel
            title={`Price trade: ${nameById(trades, selected)}`}
            onRun={(config) => api.valueTrade(selected, config)}
          />
        </div>
      )}
    </div>
  );
}
