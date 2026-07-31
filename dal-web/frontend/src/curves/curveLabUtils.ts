import { ApiClientError } from "../api/client";

export function curveLabErrorMessage(reason: unknown): string {
  if (reason instanceof ApiClientError && typeof reason.detail === "object" && reason.detail) {
    const detail = reason.detail as { code?: string; message?: string; field?: string };
    const prefix = detail.code ? `${detail.code} · ` : "";
    if (detail.field && detail.message) {
      return `${prefix}${detail.field}: ${detail.message}`;
    }
    if (detail.message) return `${prefix}${detail.message}`;
  }
  return reason instanceof Error ? reason.message : String(reason);
}

export function omitCurveLabInstrumentId(
  instrument: Record<string, unknown>,
): Record<string, unknown> {
  const { instrument_id: _instrumentId, ...withoutInstrumentId } = instrument;
  return withoutInstrumentId;
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
