import { afterEach, describe, expect, it, vi } from "vitest";
import { ApiClientError } from "../../src/api/client";
import {
  curveLabErrorMessage,
  downloadCurveLabArtifacts,
  omitCurveLabInstrumentId,
} from "../../src/curves/curveLabUtils";

describe("Curve Lab UI utilities", () => {
  afterEach(() => {
    vi.unstubAllGlobals();
    vi.restoreAllMocks();
  });

  it("projects structured API errors consistently", () => {
    expect(curveLabErrorMessage(new ApiClientError(
      "422: invalid quote",
      422,
      {
        code: "QUOTE_DECIMAL_INVALID",
        message: "Use a plain decimal.",
        field: "raw_quote",
      },
    ))).toBe("QUOTE_DECIMAL_INVALID · raw_quote: Use a plain decimal.");
    expect(curveLabErrorMessage(new ApiClientError(
      "422: invalid quote",
      422,
      {
        code: "QUOTE_DECIMAL_INVALID",
        message: "Use a plain decimal.",
      },
    ))).toBe("QUOTE_DECIMAL_INVALID · Use a plain decimal.");
    expect(curveLabErrorMessage(new ApiClientError(
      "422: invalid quote",
      422,
      { field: "raw_quote" },
    ))).toBe("422: invalid quote");
    expect(curveLabErrorMessage(new Error("network unavailable"))).toBe(
      "network unavailable",
    );
    expect(curveLabErrorMessage("unknown failure")).toBe("unknown failure");
  });

  it("omits server-owned instrument ids during construction", () => {
    const source = {
      instrument_id: "server-owned",
      instrument_type: "DEPOSIT",
      raw_quote: "0.04",
    };

    const result = omitCurveLabInstrumentId(source);

    expect(result).toEqual({
      instrument_type: "DEPOSIT",
      raw_quote: "0.04",
    });
    expect(source.instrument_id).toBe("server-owned");
  });

  it("downloads manifest and native payload with one shared browser helper", () => {
    const createObjectURL = vi.fn()
      .mockReturnValueOnce("blob:manifest")
      .mockReturnValueOnce("blob:payload");
    const revokeObjectURL = vi.fn();
    vi.stubGlobal("URL", { createObjectURL, revokeObjectURL });
    const click = vi.spyOn(HTMLAnchorElement.prototype, "click").mockImplementation(() => {});

    downloadCurveLabArtifacts({
      payload: new Blob(["payload"], { type: "application/json" }),
      manifest: { schema_version: 1 },
      versionName: "USD OIS",
      versionId: "1234567890abcdef",
    });

    expect(createObjectURL).toHaveBeenCalledTimes(2);
    expect(click).toHaveBeenCalledTimes(2);
    expect(revokeObjectURL.mock.calls).toEqual([
      ["blob:manifest"],
      ["blob:payload"],
    ]);
  });
});
