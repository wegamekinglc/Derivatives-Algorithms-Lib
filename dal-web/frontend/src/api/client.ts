// Typed API client for the DAL portfolio-management backend.

export type DateKind = "date" | "label";

export interface EventRow {
  date_kind: DateKind;
  date?: string | null;
  label?: string | null;
  event: string;
}

export interface ProductDefinition {
  id: string;
  name: string;
  description: string;
  template?: string | null;
  rows: EventRow[];
}

export interface BSModelParams {
  spot: number;
  vol: number;
  rate: number;
  div: number;
}

export interface DupireModelParams {
  spot: number;
  rate: number;
  repo: number;
  spots: number[];
  times: number[];
  vols: number[][];
}

export type ModelKind = "BSModelData_" | "DupireModelData_";

export interface ModelDefinition {
  id: string;
  name: string;
  kind: ModelKind;
  bs?: BSModelParams | null;
  dupire?: DupireModelParams | null;
}

export interface Trade {
  id: string;
  name: string;
  book: string;
  counterparty: string;
  notional: number;
  quantity: number;
  product_id: string;
  model_id: string;
  tags: string[];
}

export interface Portfolio {
  id: string;
  name: string;
  description: string;
  trade_ids: string[];
}

export interface ValuationConfig {
  num_paths: number;
  method: "sobol" | "pseudo";
  use_brownian_bridge: boolean;
  enable_aad: boolean;
  smooth: number;
  evaluation_date?: string | null;
}

export interface TradeValuation {
  trade_id: string;
  trade_name: string;
  pv: number;
  scaled_pv: number;
  greeks: Record<string, number>;
  error?: string | null;
}

export interface ValuationResult {
  id: string;
  target_kind: "trade" | "portfolio";
  target_id: string;
  backend: string;
  is_native: boolean;
  config: ValuationConfig;
  total_pv: number;
  total_greeks: Record<string, number>;
  trades: TradeValuation[];
  created_at: string;
  status: "running" | "completed" | "failed";
}

export interface Health {
  status: string;
  backend: string;
  is_native: boolean;
  evaluation_date: string;
}

export interface ProductTemplate {
  key: string;
  name: string;
  description: string;
  rows: EventRow[];
}

const BASE = "/api";

function apiPath(path: string): string {
  // SSRF guard: reject absolute URLs, protocol-relative URLs, and path
  // traversal segments so callers cannot escape the /api prefix.
  if (!path.startsWith("/") || path.startsWith("//") || path.includes("://")) {
    throw new Error(`Invalid API path: ${path}`);
  }
  if (path.split("/").some((segment) => segment === ".." || segment === ".")) {
    throw new Error(`Invalid API path: ${path}`);
  }

  const endpoint = `${BASE}${path}`;
  if (!endpoint.startsWith(`${BASE}/`)) {
    throw new Error(`Invalid API path: ${path}`);
  }
  return endpoint;
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const headers: Record<string, string> = { ...(init?.headers as Record<string, string> | undefined) };
  // Only attach a JSON content-type when the request actually carries a
  // body — GET / DELETE are "simple" requests and do not need it.
  if (init?.body != null) {
    headers["Content-Type"] ??= "application/json";
  }
  // Build a fully-qualified URL from the validated path so the static
  // analyser can see that dynamic segments never drive the fetch origin.
  const url = new URL(apiPath(path), window.location.origin);
  const resp = await fetch(url, { ...init, headers });
  if (!resp.ok) {
    let detail = resp.statusText;
    try {
      const body = await resp.json();
      detail = typeof body.detail === "string" ? body.detail : JSON.stringify(body.detail);
    } catch {
      // ignore body parse errors
    }
    throw new Error(`${resp.status}: ${detail}`);
  }
  if (resp.status === 204) {
    return undefined as T;
  }
  return (await resp.json()) as T;
}

export const api = {
  health: () => request<Health>("/health"),

  // products
  listTemplates: () => request<ProductTemplate[]>("/products/templates"),
  listProducts: () => request<ProductDefinition[]>("/products"),
  createProduct: (body: Omit<ProductDefinition, "id">) =>
    request<ProductDefinition>("/products", { method: "POST", body: JSON.stringify(body) }),
  updateProduct: (id: string, patch: Partial<Omit<ProductDefinition, "id">>) =>
    request<ProductDefinition>(`/products/${id}`, {
      method: "PUT",
      body: JSON.stringify(patch),
    }),
  deleteProduct: (id: string) =>
    request<undefined>(`/products/${id}`, { method: "DELETE" }),
  debugProduct: (rows: EventRow[]) =>
    request<{ debug: string }>("/products/debug", {
      method: "POST",
      body: JSON.stringify({ rows }),
    }),

  // models
  listModels: () => request<ModelDefinition[]>("/models"),
  createModel: (body: Omit<ModelDefinition, "id">) =>
    request<ModelDefinition>("/models", { method: "POST", body: JSON.stringify(body) }),
  updateModel: (id: string, patch: Partial<Omit<ModelDefinition, "id">>) =>
    request<ModelDefinition>(`/models/${id}`, {
      method: "PUT",
      body: JSON.stringify(patch),
    }),
  deleteModel: (id: string) => request<undefined>(`/models/${id}`, { method: "DELETE" }),

  // trades
  listTrades: () => request<Trade[]>("/trades"),
  createTrade: (body: Omit<Trade, "id" | "tags"> & { tags?: string[] }) =>
    request<Trade>("/trades", { method: "POST", body: JSON.stringify(body) }),
  updateTrade: (id: string, patch: Partial<Omit<Trade, "id">>) =>
    request<Trade>(`/trades/${id}`, { method: "PUT", body: JSON.stringify(patch) }),
  deleteTrade: (id: string) => request<undefined>(`/trades/${id}`, { method: "DELETE" }),
  valueTrade: (id: string, config: ValuationConfig) =>
    request<ValuationResult>(`/trades/${id}/value`, {
      method: "POST",
      body: JSON.stringify(config),
    }),

  // portfolios
  listPortfolios: () => request<Portfolio[]>("/portfolios"),
  createPortfolio: (body: { name: string; description?: string }) =>
    request<Portfolio>("/portfolios", { method: "POST", body: JSON.stringify(body) }),
  deletePortfolio: (id: string) =>
    request<undefined>(`/portfolios/${id}`, { method: "DELETE" }),
  portfolioTrades: (id: string) => request<Trade[]>(`/portfolios/${id}/trades`),
  addTradeToPortfolio: (pid: string, tid: string) =>
    request<Portfolio>(`/portfolios/${pid}/trades/${tid}`, { method: "POST" }),
  removeTradeFromPortfolio: (pid: string, tid: string) =>
    request<Portfolio>(`/portfolios/${pid}/trades/${tid}`, { method: "DELETE" }),
  valuePortfolio: (id: string, config: ValuationConfig) =>
    request<ValuationResult>(`/portfolios/${id}/value`, {
      method: "POST",
      body: JSON.stringify(config),
    }),

  // valuations
  listValuations: () => request<ValuationResult[]>("/valuations"),
  getValuation: (id: string) => request<ValuationResult>(`/valuations/${id}`),
};
