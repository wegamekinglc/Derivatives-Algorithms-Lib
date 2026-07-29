import { useState } from "react";
import {
  api,
  ApiClientError,
  type CurveLabCanonicalQuote,
  type CurveLabQuoteAuthoringRequest,
  type CurveLabSuccessFamily,
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
  onCanonicalQuote?: (...args: [CurveLabCanonicalQuote]) => void;
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
}: CurveLabQuoteAuthoringProps) {
  const [family, setFamily] = useState<CurveLabSuccessFamily>("DEPOSIT");
  const [convention, setConvention] = useState<QuoteInputConvention>("DECIMAL");
  const [lexeme, setLexeme] = useState("0.04");
  const [canonical, setCanonical] = useState<CurveLabCanonicalQuote | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);
  const projection = curveLabFamily(family);

  const submit = async () => {
    setSubmitting(true);
    setError(null);
    try {
      const result = await canonicalize({
        instrument_type: family,
        input_lexeme: lexeme,
        input_convention: convention,
      });
      setCanonical(result);
      onCanonicalQuote?.(result);
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
            value={family}
            onChange={(event) => {
              const next = event.target.value as CurveLabSuccessFamily;
              const nextProjection = curveLabFamily(next);
              setFamily(next);
              setConvention(nextProjection.inputConventions[0]);
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
            value={convention}
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

        <button type="button" disabled={submitting} onClick={() => void submit()}>
          {submitting ? "Canonicalizing…" : "Canonicalize quote"}
        </button>
      </div>

      <p {...css("curve-lab-boundary-note")}>
        Percent is accepted only here. Durable requests receive canonical decimal strings and fixed registry-owned risk bumps.
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
        </output>
      )}
    </section>
  );
}
