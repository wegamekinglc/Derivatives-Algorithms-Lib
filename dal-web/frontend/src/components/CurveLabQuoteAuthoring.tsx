import { useEffect, useState } from "react";
import {
  api,
  ApiClientError,
  type CurveLabCanonicalQuote,
  type CurveLabQuoteAuthoringRequest,
  type CurveLabSuccessFamily,
  type QuoteDisplayConvention,
  type QuoteInputConvention,
} from "../api/client";
import {
  CURVE_LAB_FAMILY_REGISTRY,
  curveLabFamily,
} from "../curves/curveLabRegistry";
import { css } from "../format";

type CanonicalizeQuote = (
  // eslint-disable-next-line no-unused-vars -- core ESLint sees type-only parameter names.
  ...args: [CurveLabQuoteAuthoringRequest]
) => Promise<CurveLabCanonicalQuote>;

interface CurveLabQuoteAuthoringProps {
  canonicalize?: CanonicalizeQuote;
  // eslint-disable-next-line no-unused-vars -- core ESLint sees type-only parameter names.
  onCanonicalQuote?: (...args: [CurveLabCanonicalQuote, number?]) => boolean | void;
  targetInstrumentFamily?: CurveLabSuccessFamily | null;
  targetToken?: number;
}

function signedBump(value: string): string {
  return value.startsWith("-") ? value : `+${value}`;
}

function errorMessage(reason: unknown): string {
  if (reason instanceof ApiClientError && typeof reason.detail === "object" && reason.detail) {
    const detail = reason.detail as { message?: string; field?: string };
    return detail.field && detail.message
      ? `${detail.field}: ${detail.message}`
      : detail.message ?? reason.message;
  }
  return reason instanceof Error ? reason.message : String(reason);
}

export default function CurveLabQuoteAuthoring({
  canonicalize = api.canonicalizeCurveLabQuote,
  onCanonicalQuote,
  targetInstrumentFamily,
  targetToken,
}: CurveLabQuoteAuthoringProps) {
  const [family, setFamily] = useState<CurveLabSuccessFamily>("DEPOSIT");
  const [convention, setConvention] = useState<QuoteInputConvention>("DECIMAL");
  const [displayConvention, setDisplayConvention] =
    useState<QuoteDisplayConvention>("DECIMAL");
  const [displayScale, setDisplayScale] = useState("4");
  const [lexeme, setLexeme] = useState("0.04");
  const [canonical, setCanonical] = useState<CurveLabCanonicalQuote | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);
  const activeFamily = targetInstrumentFamily ?? family;
  const projection = curveLabFamily(activeFamily);
  const activeInputConvention = projection.inputConventions.some(
    (item) => item === convention,
  )
    ? convention
    : projection.inputConventions[0];
  const activeDisplayConvention = projection.inputConventions.some(
    (item) => item === displayConvention,
  )
    ? displayConvention
    : projection.inputConventions[0];

  useEffect(() => {
    if (targetInstrumentFamily === undefined) return;
    setCanonical(null);
    setError(null);
    if (targetInstrumentFamily === null || targetInstrumentFamily === family) return;
    const targetProjection = curveLabFamily(targetInstrumentFamily);
    setFamily(targetInstrumentFamily);
    setConvention(targetProjection.inputConventions[0]);
    setDisplayConvention(targetProjection.inputConventions[0]);
  }, [family, targetInstrumentFamily, targetToken]);

  const submit = async () => {
    setSubmitting(true);
    setError(null);
    try {
      const result = await canonicalize({
        instrument_type: activeFamily,
        input_lexeme: lexeme,
        input_convention: activeInputConvention,
      });
      const applied = targetToken === undefined
        ? onCanonicalQuote?.(result)
        : onCanonicalQuote?.(result, targetToken);
      if (applied === false) {
        throw new Error(
          "Canonical quote target changed; select the matching workspace instrument and retry.",
        );
      }
      setCanonical(result);
    } catch (reason) {
      setCanonical(null);
      setError(errorMessage(reason));
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <section {...css("curve-lab-authoring")} aria-labelledby="quote-authoring-title">
      <div {...css("curve-lab-authoring-heading")}>
        <div>
          <span {...css("eyebrow")}>VISUAL AUTHORING / EXACT DECIMAL</span>
          <h2 id="quote-authoring-title">Canonical quote boundary</h2>
        </div>
        <span {...css("tag")}>{projection.coordinate}</span>
      </div>

      <div {...css("curve-lab-authoring-grid")}>
        <label>
          <span>Instrument family</span>
          <select
            value={activeFamily}
            disabled={targetInstrumentFamily !== undefined}
            onChange={(event) => {
              const next = event.target.value as CurveLabSuccessFamily;
              const nextProjection = curveLabFamily(next);
              setFamily(next);
              setConvention(nextProjection.inputConventions[0]);
              setDisplayConvention(nextProjection.inputConventions[0]);
              setCanonical(null);
              setError(null);
            }}
          >
            {CURVE_LAB_FAMILY_REGISTRY.map((row) => (
              <option key={row.instrumentType} value={row.instrumentType}>
                {row.instrumentType.replace("_", " ")}
              </option>
            ))}
          </select>
        </label>

        <label>
          <span>Input convention</span>
          <select
            value={activeInputConvention}
            onChange={(event) => {
              setConvention(event.target.value as QuoteInputConvention);
              setCanonical(null);
            }}
          >
            {projection.inputConventions.map((item) => (
              <option key={item} value={item}>{item.replace("_", " ")}</option>
            ))}
          </select>
        </label>

        <label>
          <span>Quote lexeme</span>
          <input
            value={lexeme}
            inputMode="decimal"
            spellCheck={false}
            onChange={(event) => {
              setLexeme(event.target.value);
              setCanonical(null);
              setError(null);
            }}
            onKeyDown={(event) => {
              if (event.key === "Enter") {
                event.preventDefault();
                void submit();
              }
            }}
          />
        </label>

        <label>
          <span>Display convention</span>
          <select
            value={activeDisplayConvention}
            onChange={(event) => {
              setDisplayConvention(event.target.value as QuoteDisplayConvention);
            }}
          >
            {projection.inputConventions.map((item) => (
              <option key={item} value={item}>{item.replace("_", " ")}</option>
            ))}
          </select>
        </label>

        <label>
          <span>Display scale</span>
          <input
            type="number"
            min={0}
            max={12}
            value={displayScale}
            onChange={(event) => {
              if (/^(?:[0-9]|1[0-2])$/.test(event.target.value)) {
                setDisplayScale(event.target.value);
              }
            }}
          />
        </label>

        <button
          type="button"
          disabled={submitting || targetInstrumentFamily === null}
          onClick={() => void submit()}
        >
          {submitting ? "Canonicalizing…" : "Canonicalize quote"}
        </button>
      </div>

      <p {...css("curve-lab-boundary-note")}>
        {targetInstrumentFamily === null
          ? "Select one workspace instrument before applying a canonical quote."
          : "Percent is accepted only here. Durable requests receive canonical decimal strings and fixed registry-owned risk bumps."}
      </p>

      {error && <div {...css("error")}>{error}</div>}
      {canonical && (
        <output {...css("curve-lab-canonical-output")} aria-live="polite">
          <div>
            <span>Durable raw quote</span>
            <strong>{canonical.raw_quote}</strong>
            <small>{canonical.canonical_raw_unit}</small>
          </div>
          <div>
            <span>Solver coordinate</span>
            <strong>{canonical.normalized_quote} normalized</strong>
            <small>{canonical.normalized_unit}</small>
          </div>
          <div>
            <span>Exact 1 bp move</span>
            <strong>
              {signedBump(canonical.exact_risk_raw_bump)} raw /{" "}
              {signedBump(canonical.normalized_risk_bump)} normalized
            </strong>
            <small>{canonical.quote_coordinate_kind}</small>
          </div>
          <div>
            <span>Presentation preference</span>
            <strong>{activeDisplayConvention}</strong>
            <small>scale {displayScale} · local only</small>
          </div>
        </output>
      )}
    </section>
  );
}
