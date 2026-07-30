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
  error_message?: string | null;
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

export type CalibrationKind = "single" | "xccy_staged" | "xccy_joint";
export type CalibrationStatus = "running" | "completed" | "failed";
export type CurveLabSuccessFamily =
  | "DEPOSIT"
  | "FRA"
  | "FUTURE"
  | "OIS"
  | "IRS"
  | "BASIS_SWAP"
  | "XCCY";
export type QuoteCoordinateKind = "RATE" | "PRICE" | "SPREAD";
export type QuoteInputConvention = "DECIMAL" | "PERCENT" | "PRICE_POINTS";
export type QuoteDisplayConvention = "DECIMAL" | "PERCENT" | "PRICE_POINTS";

export interface CurveLabQuoteAuthoringRequest {
  instrument_type: CurveLabSuccessFamily;
  input_lexeme: string;
  input_convention: QuoteInputConvention;
}

export interface CurveLabCanonicalQuote {
  instrument_type: CurveLabSuccessFamily;
  quote_coordinate_kind: QuoteCoordinateKind;
  canonical_raw_unit: "DECIMAL" | "PRICE_POINTS";
  raw_quote: string;
  normalized_quote: string;
  normalized_unit: "DECIMAL_RATE";
  exact_risk_raw_bump: string;
  normalized_risk_bump: string;
}

export interface CurveLabQuoteRenderingRequest {
  instrument_type: CurveLabSuccessFamily;
  canonical_raw_quote: string;
  display_convention: QuoteDisplayConvention;
  display_scale: number;
}

export interface CurveLabRenderedQuote {
  rendered_quote: string;
}

export interface CurveLabDraft {
  id: string;
  schema_version: 2;
  revision: number;
  fingerprint: string;
  state: "READY_TO_BUILD" | "MODIFIED";
  document: Record<string, unknown>;
  created_at: string;
  updated_at: string;
}

export interface CurveLabAxisEntry {
  global_quote_index?: number;
  global_parameter_index?: number;
  quote_id?: string;
  parameter_id?: string;
  instrument_id?: string;
  component_key: string;
  display_label: string;
  raw_quote?: string;
  normalized_quote?: string;
  coordinate_kind?: string;
  node_date?: string;
}

export interface CurveLabBuildRun {
  id: string;
  draft_id: string;
  draft_revision: number;
  draft_fingerprint: string;
  state: string;
  stale: boolean;
  request: Record<string, unknown>;
  resolved_plan: Record<string, unknown>;
  quote_axis: CurveLabAxisEntry[];
  parameter_axis: CurveLabAxisEntry[];
  dependency_manifest: {
    version_id: string;
    content_hash: string;
    root_kind: "DISCOUNT_CURVE" | "CURVE_SET";
  }[];
  diagnostics: Record<string, unknown> | null;
  native_payload_hash: string | null;
  error: { code: string; message: string; field: string } | null;
  created_at: string;
  finished_at: string | null;
}

export interface CurveLabVersion {
  id: string;
  source_kind: "BUILD" | "IMPORT";
  build_run_id: string | null;
  import_job_id: string | null;
  name: string;
  version_note: string | null;
  tags: string[];
  native_payload_length: number;
  native_payload_hash: string;
  root_kind: "DISCOUNT_CURVE" | "CURVE_SET";
  build_validation_state: "VERIFIED" | "IMPORT_RECONSTRUCTED";
  visibility_state: "VISIBLE" | "ARCHIVED";
  created_at: string;
}

export interface CurveLabRiskRun {
  id: string;
  curve_version_id: string;
  source_kind: "BUILD_VERSION" | "IMPORT_VERSION";
  state: string;
  quote_axis: CurveLabAxisEntry[] | null;
  parameter_axis: CurveLabAxisEntry[];
  estimated_work: Record<string, number | boolean>;
  result: {
    pricing?: Record<string, unknown>[];
    dv01?: Record<string, unknown>[];
    key_rate_sum?: Record<string, unknown>[];
    nonlinear_reconciliation?: Record<string, unknown>[];
    sensitivity_matrices?: {
      matrix_id: string;
      availability: string;
      method: string;
    }[];
  } | null;
  error: { code: string; message: string; field: string } | null;
  created_at: string;
  finished_at: string | null;
}

export interface CurveLabImportJob {
  id: string;
  state: string;
  resulting_version_id: string | null;
  error: { code: string; message: string } | null;
}

export interface CurveLabMatrix {
  matrix_id: string;
  mathematical_name: string;
  orientation: string;
  row_axis_ref: string;
  column_axis_ref: string;
  rows: number;
  columns: number;
  availability: string;
  availability_reason_code: string | null;
  availability_reason: string | null;
  method: string;
  input_unit: string;
  output_unit: string;
  values: string[][] | null;
}

export interface InstrumentDiagnostic {
  instrument_id: string;
  group: string;
  calibration_index: number;
  market_rate: number;
  model_rate: number;
  residual: number;
}

export interface CalibrationMatrix {
  availability: "available" | "not_requested" | "not_available_for_mode";
  shape: [number, number];
  row_axis: string[];
  column_axis: string[];
  scaling: "unscaled" | "solver_scaled";
  residual_tolerance: number | null;
  values: number[][] | null;
}

export interface CalibrationCurve {
  id: string;
  name: string;
  currency: string;
  role: "discount" | "forward" | "basis" | "base";
  parameterization: string;
  node_dates: string[];
  parameters: Record<string, number[]>;
}

export interface QuoteBumpPreview {
  residual_index: number;
  instrument_id: string;
  quote_bump: number;
  residual_tolerance: number;
  delta_parameters: { axis: string; value: number }[];
  formula: "delta_x = effective_inverse * delta_quote / residual_tolerance";
}

export interface CalibrationRun {
  id: string;
  kind: CalibrationKind;
  name: string;
  status: CalibrationStatus;
  phase: "queued" | "solving" | "serializing" | "persisting" | "finished";
  created_at: string;
  started_at: string | null;
  finished_at: string | null;
  requested_jacobian_mode: "ANALYTIC" | "BUMPED";
  actual_jacobian_mode: "ANALYTIC" | "BUMPED" | null;
  curves: CalibrationCurve[];
  instrument_diagnostics: InstrumentDiagnostic[];
  solver_diagnostics: {
    status: string;
    max_abs_residual: number;
    rms_residual: number;
    evaluations: number | null;
  } | null;
  fx_forwards: {
    pair: { domestic: string; foreign: string };
    dates: string[];
    forwards: number[];
  } | null;
  named_ranges: {
    parameters: { name: string; offset: number; size: number }[];
    residuals: { name: string; offset: number; size: number }[];
  } | null;
  jacobian: CalibrationMatrix | null;
  effective_inverse: CalibrationMatrix | null;
  quote_bump_preview: QuoteBumpPreview | null;
  error: {
    code: string;
    message: string;
    location: (string | number)[] | null;
    context: Record<string, unknown>;
  } | null;
}

export class ApiClientError extends Error {
  readonly status: number;
  readonly detail: unknown;

  constructor(message: string, status: number, detail: unknown) {
    super(message);
    this.status = status;
    this.detail = detail;
  }
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

function calibrationPath(kind: CalibrationKind): string {
  switch (kind) {
  case "single":
    return "/calibrations/single";
  case "xccy_staged":
    return "/calibrations/xccy/staged";
  case "xccy_joint":
    return "/calibrations/xccy/joint";
  }
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
    let rawDetail: unknown = detail;
    try {
      const body = await resp.json();
      rawDetail = body.detail ?? body.error ?? body;
      detail = typeof rawDetail === "string" ? rawDetail : JSON.stringify(rawDetail);
    } catch {
      // ignore body parse errors
    }
    throw new ApiClientError(`${resp.status}: ${detail}`, resp.status, rawDetail);
  }
  if (resp.status === 204) {
    return undefined as T;
  }
  return (await resp.json()) as T;
}

async function requestBytes(path: string): Promise<Blob> {
  const url = new URL(apiPath(path), window.location.origin);
  const resp = await fetch(url, { headers: {} });
  if (!resp.ok) {
    throw new ApiClientError(
      `${resp.status}: ${resp.statusText}`,
      resp.status,
      resp.statusText,
    );
  }
  return resp.blob();
}

export const api = {
  health: () => request<Health>("/health"),

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

  listModels: () => request<ModelDefinition[]>("/models"),
  createModel: (body: Omit<ModelDefinition, "id">) =>
    request<ModelDefinition>("/models", { method: "POST", body: JSON.stringify(body) }),
  updateModel: (id: string, patch: Partial<Omit<ModelDefinition, "id">>) =>
    request<ModelDefinition>(`/models/${id}`, {
      method: "PUT",
      body: JSON.stringify(patch),
    }),
  deleteModel: (id: string) => request<undefined>(`/models/${id}`, { method: "DELETE" }),

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

  listValuations: () => request<ValuationResult[]>("/valuations"),
  getValuation: (id: string) => request<ValuationResult>(`/valuations/${id}`),
  canonicalizeCurveLabQuote: (body: CurveLabQuoteAuthoringRequest) =>
    request<CurveLabCanonicalQuote>("/curve-lab/quote-canonicalizations", {
      method: "POST",
      body: JSON.stringify(body),
    }),
  renderCurveLabQuote: (body: CurveLabQuoteRenderingRequest) =>
    request<CurveLabRenderedQuote>("/curve-lab/quote-renderings", {
      method: "POST",
      body: JSON.stringify(body),
    }),
  createCurveLabDraft: (body: unknown) =>
    request<CurveLabDraft>("/curve-lab/drafts", {
      method: "POST",
      body: JSON.stringify(body),
    }),
  getCurveLabDraft: (id: string) =>
    request<CurveLabDraft>(`/curve-lab/drafts/${id}`),
  updateCurveLabDraft: (id: string, revision: number, body: unknown) =>
    request<CurveLabDraft>(`/curve-lab/drafts/${id}`, {
      method: "PUT",
      headers: { "If-Match": `"${revision}"` },
      body: JSON.stringify(body),
    }),
  createCurveLabBuildRun: (draftId: string) =>
    request<CurveLabBuildRun>(`/curve-lab/drafts/${draftId}/build-runs`, {
      method: "POST",
    }),
  getCurveLabBuildRun: (id: string) =>
    request<CurveLabBuildRun>(`/curve-lab/build-runs/${id}`),
  createCurveLabVersion: (body: {
    draft_id: string;
    draft_revision: number;
    draft_fingerprint: string;
    build_run_id: string;
    name: string;
    version_note?: string | null;
    tags?: string[];
    idempotency_key: string;
  }) =>
    request<CurveLabVersion>("/curve-lab/versions", {
      method: "POST",
      body: JSON.stringify(body),
    }),
  listCurveLabVersions: (includeArchived = false) =>
    request<CurveLabVersion[]>(
      `/curve-lab/versions?include_archived=${String(includeArchived)}`,
    ),
  archiveCurveLabVersion: (id: string) =>
    request<CurveLabVersion>(`/curve-lab/versions/${id}/archive`, {
      method: "POST",
    }),
  cloneCurveLabVersion: (id: string) =>
    request<CurveLabDraft>(`/curve-lab/versions/${id}/clone`, {
      method: "POST",
    }),
  downloadCurveLabVersion: (id: string) =>
    requestBytes(`/curve-lab/versions/${id}/native-json`),
  getCurveLabRuntimeManifest: (id: string) =>
    request<Record<string, unknown>>(`/curve-lab/versions/${id}/runtime-manifest`),
  importCurveLabVersion: (
    payload: Blob,
    runtimeManifest?: Record<string, unknown>,
  ) =>
    request<CurveLabImportJob>("/curve-lab/import-jobs", {
      method: "POST",
      body: payload,
      headers: {
        "Content-Type": "application/json",
        ...(runtimeManifest
          ? { "X-Curve-Lab-Runtime-Manifest": JSON.stringify(runtimeManifest) }
          : {}),
      },
    }),
  getCurveLabImportJob: (id: string) =>
    request<CurveLabImportJob>(`/curve-lab/import-jobs/${id}`),
  createCurveLabFixingSnapshot: (body: {
    id: string;
    observations: {
      index_name: string;
      fixing_time: string;
      kind: "RATE" | "FX";
      units: "DECIMAL_RATE" | "DOMESTIC_PER_FOREIGN";
      value: string;
    }[];
  }) =>
    request<{ id: string; content_hash: string }>("/curve-lab/fixing-snapshots", {
      method: "POST",
      body: JSON.stringify(body),
    }),
  createCurveLabRiskRun: (body: unknown) =>
    request<CurveLabRiskRun>("/curve-lab/risk-runs", {
      method: "POST",
      body: JSON.stringify(body),
    }),
  getCurveLabRiskRun: (id: string) =>
    request<CurveLabRiskRun>(`/curve-lab/risk-runs/${id}`),
  getCurveLabMatrix: (runId: string, matrixId: string) =>
    request<CurveLabMatrix>(
      `/curve-lab/risk-runs/${runId}/matrices/${matrixId}`,
    ),

  submitCalibration: (kind: CalibrationKind, body: unknown) => {
    return request<CalibrationRun>(calibrationPath(kind), {
      method: "POST",
      body: JSON.stringify(body),
    });
  },
  getCalibration: (
    id: string,
    quoteBumpIndex?: number,
    quoteBumpSize?: number,
  ) => {
    const query =
      quoteBumpIndex === undefined || quoteBumpSize === undefined
        ? ""
        : `?quote_bump_index=${encodeURIComponent(quoteBumpIndex)}&quote_bump_size=${encodeURIComponent(quoteBumpSize)}`;
    return request<CalibrationRun>(`/calibrations/${id}${query}`);
  },
  getCurve: (id: string) => request<CalibrationCurve>(`/curves/${id}`),
};
