import type {
  CurveLabSuccessFamily,
  QuoteCoordinateKind,
  QuoteInputConvention,
} from "../api/client";

interface CurveLabFamilyProjection {
  instrumentType: CurveLabSuccessFamily;
  coordinate: QuoteCoordinateKind;
  canonicalRawUnit: "DECIMAL" | "PRICE_POINTS";
  inputConventions: readonly QuoteInputConvention[];
}

// Generated projection of CurveLabV1SuccessFamily in the committed OpenAPI
// contract. The backend registry test pins this tuple byte-for-byte.
export const CURVE_LAB_FAMILY_REGISTRY = [
  { instrumentType: "DEPOSIT", coordinate: "RATE", canonicalRawUnit: "DECIMAL", inputConventions: ["DECIMAL", "PERCENT"] },
  { instrumentType: "FRA", coordinate: "RATE", canonicalRawUnit: "DECIMAL", inputConventions: ["DECIMAL", "PERCENT"] },
  { instrumentType: "FUTURE", coordinate: "PRICE", canonicalRawUnit: "PRICE_POINTS", inputConventions: ["PRICE_POINTS"] },
  { instrumentType: "OIS", coordinate: "RATE", canonicalRawUnit: "DECIMAL", inputConventions: ["DECIMAL", "PERCENT"] },
  { instrumentType: "IRS", coordinate: "RATE", canonicalRawUnit: "DECIMAL", inputConventions: ["DECIMAL", "PERCENT"] },
  { instrumentType: "BASIS_SWAP", coordinate: "SPREAD", canonicalRawUnit: "DECIMAL", inputConventions: ["DECIMAL", "PERCENT"] },
  { instrumentType: "XCCY", coordinate: "SPREAD", canonicalRawUnit: "DECIMAL", inputConventions: ["DECIMAL", "PERCENT"] },
] as const satisfies readonly CurveLabFamilyProjection[];

export const CURVE_LAB_SUCCESS_FAMILIES = CURVE_LAB_FAMILY_REGISTRY.map(
  (row) => row.instrumentType,
);

export function curveLabFamily(
  instrumentType: CurveLabSuccessFamily,
): (typeof CURVE_LAB_FAMILY_REGISTRY)[number] {
  const result = CURVE_LAB_FAMILY_REGISTRY.find(
    (row) => row.instrumentType === instrumentType,
  );
  if (!result) {
    throw new Error(`Unsupported Curve Lab family: ${instrumentType}`);
  }
  return result;
}
