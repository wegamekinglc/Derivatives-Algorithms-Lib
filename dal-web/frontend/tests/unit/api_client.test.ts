import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { api, type ProductDefinition } from "../../src/api/client";

const ORIGIN = window.location.origin;

function jsonResponse(body: unknown, init: ResponseInit = { status: 200 }): Response {
  return new Response(JSON.stringify(body), {
    ...init,
    headers: { "Content-Type": "application/json", ...(init.headers ?? {}) },
  });
}

describe("api client", () => {
  let fetchMock: ReturnType<typeof vi.fn>;

  beforeEach(() => {
    fetchMock = vi.fn();
    vi.stubGlobal("fetch", fetchMock);
  });

  afterEach(() => {
    vi.unstubAllGlobals();
  });

  it("does not expose the zero-caller Curve Lab draft getter", () => {
    expect("getCurveLabDraft" in api).toBe(false);
  });

  it("issues GET requests without a JSON content-type", async () => {
    fetchMock.mockResolvedValue(jsonResponse({ status: "ok", backend: "b", is_native: true, evaluation_date: "d" }));

    await api.health();

    expect(fetchMock).toHaveBeenCalledTimes(1);
    const [url, init] = fetchMock.mock.calls[0] as [URL, RequestInit];
    expect(String(url)).toBe(`${ORIGIN}/api/health`);
    expect(init.headers).toEqual({});
  });

  it("attaches a JSON content-type and serialized body on POST", async () => {
    const created: ProductDefinition = { id: "p1", name: "n", description: "", rows: [] };
    fetchMock.mockResolvedValue(jsonResponse(created));

    const result = await api.createProduct({ name: "n", description: "", rows: [] });

    expect(result).toEqual(created);
    const [url, init] = fetchMock.mock.calls[0] as [URL, RequestInit];
    expect(String(url)).toBe(`${ORIGIN}/api/products`);
    expect(init.method).toBe("POST");
    expect(init.headers).toEqual({ "Content-Type": "application/json" });
    expect(JSON.parse(String(init.body))).toEqual({ name: "n", description: "", rows: [] });
  });

  it("interpolates path parameters under the /api prefix", async () => {
    fetchMock.mockResolvedValue(jsonResponse({ id: "v42" }));

    await api.getValuation("v42");

    const [url] = fetchMock.mock.calls[0] as [URL, RequestInit];
    expect(String(url)).toBe(`${ORIGIN}/api/valuations/v42`);
  });

  it("requests quote-bump previews from the persisted run endpoint", async () => {
    fetchMock.mockResolvedValue(jsonResponse({ id: "c".repeat(32) }));

    await api.getCalibration("c".repeat(32), 4, 0.0001);

    const [url, init] = fetchMock.mock.calls[0] as [URL, RequestInit];
    expect(String(url)).toBe(
      `${ORIGIN}/api/calibrations/${"c".repeat(32)}?quote_bump_index=4&quote_bump_size=0.0001`,
    );
    expect(init.headers).toEqual({});
  });

  it("sends quote authoring lexemes as strings to the stateless adapter", async () => {
    fetchMock.mockResolvedValue(jsonResponse({
      instrument_type: "IRS",
      quote_coordinate_kind: "RATE",
      canonical_raw_unit: "DECIMAL",
      raw_quote: "0.04",
      normalized_quote: "0.04",
      normalized_unit: "DECIMAL_RATE",
      exact_risk_raw_bump: "0.0001",
      normalized_risk_bump: "0.0001",
    }));

    await api.canonicalizeCurveLabQuote({
      instrument_type: "IRS",
      input_lexeme: "4",
      input_convention: "PERCENT",
    });

    const [url, init] = fetchMock.mock.calls[0] as [URL, RequestInit];
    expect(String(url)).toBe(`${ORIGIN}/api/curve-lab/quote-canonicalizations`);
    expect(JSON.parse(String(init.body))).toEqual({
      instrument_type: "IRS",
      input_lexeme: "4",
      input_convention: "PERCENT",
    });
    expect(typeof JSON.parse(String(init.body)).input_lexeme).toBe("string");
  });

  it("requests exact quote rendering with presentation-only string input", async () => {
    fetchMock.mockResolvedValue(jsonResponse({ rendered_quote: "4.000000" }));

    await expect(api.renderCurveLabQuote({
      instrument_type: "IRS",
      canonical_raw_quote: "0.04",
      display_convention: "PERCENT",
      display_scale: 6,
    })).resolves.toEqual({ rendered_quote: "4.000000" });

    const [url, init] = fetchMock.mock.calls[0] as [URL, RequestInit];
    expect(String(url)).toBe(`${ORIGIN}/api/curve-lab/quote-renderings`);
    expect(JSON.parse(String(init.body))).toEqual({
      instrument_type: "IRS",
      canonical_raw_quote: "0.04",
      display_convention: "PERCENT",
      display_scale: 6,
    });
    expect(typeof JSON.parse(String(init.body)).canonical_raw_quote).toBe("string");
  });

  it("resolves 204 No Content to undefined", async () => {
    fetchMock.mockResolvedValue(new Response(null, { status: 204 }));

    await expect(api.deleteProduct("p1")).resolves.toBeUndefined();
    const [url, init] = fetchMock.mock.calls[0] as [URL, RequestInit];
    expect(String(url)).toBe(`${ORIGIN}/api/products/p1`);
    expect(init.method).toBe("DELETE");
  });

  it("throws status and string detail from error responses", async () => {
    fetchMock.mockResolvedValue(jsonResponse({ detail: "trade not found" }, { status: 404 }));

    await expect(api.getValuation("missing")).rejects.toThrow("404: trade not found");
  });

  it("serializes structured error details", async () => {
    fetchMock.mockResolvedValue(
      jsonResponse({ detail: [{ loc: ["body", "num_paths"], msg: "too big" }] }, { status: 422 }),
    );

    await expect(api.getValuation("x")).rejects.toThrow('422: [{"loc":["body","num_paths"],"msg":"too big"}]');
  });

  it("falls back to statusText when the error body is not JSON", async () => {
    fetchMock.mockResolvedValue(new Response("gateway exploded", { status: 502, statusText: "Bad Gateway" }));

    await expect(api.listModels()).rejects.toThrow("502: Bad Gateway");
  });

  it("propagates network failures", async () => {
    fetchMock.mockRejectedValue(new TypeError("fetch failed"));

    await expect(api.listTrades()).rejects.toThrow("fetch failed");
  });

  it.each(["..", ".", "a..b/../c", "x://evil"])(
    "rejects SSRF-shaped path segment %r before any fetch",
    async (badId) => {
      await expect(api.getValuation(badId)).rejects.toThrow("Invalid API path");
      expect(fetchMock).not.toHaveBeenCalled();
    },
  );
});
