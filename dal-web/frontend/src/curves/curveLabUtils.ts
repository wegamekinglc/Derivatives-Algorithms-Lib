import { ApiClientError } from "../api/client";

type CurveLabErrorDetail = { code?: string; message?: string; field?: string };

function curveLabErrorDetailMessage(detail: unknown): string | undefined {
  if (typeof detail !== "object" || !detail) return undefined;
  const { code, message, field } = detail as CurveLabErrorDetail;
  if (!message) return undefined;
  const prefix = code ? `${code} · ` : "";
  return field ? `${prefix}${field}: ${message}` : `${prefix}${message}`;
}

export function curveLabErrorMessage(reason: unknown): string {
  const detailMessage = reason instanceof ApiClientError
    ? curveLabErrorDetailMessage(reason.detail)
    : undefined;
  if (detailMessage) return detailMessage;
  return reason instanceof Error ? reason.message : String(reason);
}

export function omitCurveLabInstrumentId(
  instrument: Record<string, unknown>,
): Record<string, unknown> {
  return Object.fromEntries(
    Object.entries(instrument).filter(([key]) => key !== "instrument_id"),
  );
}

function downloadBlob(payload: Blob, fileName: string): void {
  const url = URL.createObjectURL(payload);
  try {
    const anchor = document.createElement("a");
    anchor.href = url;
    anchor.download = fileName;
    anchor.click();
  } finally {
    URL.revokeObjectURL(url);
  }
}

export function downloadCurveLabArtifacts({
  payload,
  manifest,
  versionName,
  versionId,
}: {
  payload: Blob;
  manifest: Record<string, unknown>;
  versionName: string;
  versionId: string;
}): void {
  const prefix = `${versionName.replace(/ /g, "-")}-${versionId.slice(0, 8)}`;
  downloadBlob(
    new Blob(
      [JSON.stringify(manifest, null, 2)],
      { type: "application/json" },
    ),
    `${prefix}.manifest.json`,
  );
  downloadBlob(payload, `${prefix}.json`);
}
