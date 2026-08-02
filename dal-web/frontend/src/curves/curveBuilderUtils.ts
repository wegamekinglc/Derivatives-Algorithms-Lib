// Pure helpers backing the Curve Builder screen. No React, no API access:
// every function maps existing draft/build-run data into display form.

export type CurveBuilderStepId =
  | "declaration"
  | "dependencies"
  | "instruments"
  | "solve"
  | "validate";

export type CurveBuilderStepState = "done" | "active" | "todo" | "running" | "failed";

const DAY_MS = 86_400_000;

// "clab/v1/local/discount/USD/OIS" → "USD OIS · Discount".
export function declarationLabel(componentKey: string): string {
  const parts = componentKey.split("/").filter(Boolean);
  const tail = parts.slice(3);
  if (tail.length < 2) return componentKey;
  const [role, ...rest] = tail;
  const roleLabel = role.charAt(0).toUpperCase() + role.slice(1).toLowerCase();
  return `${rest.join(" ")} · ${roleLabel}`;
}

// Tenor label from the as-of date to maturity: 1D / 3M / 2Y / 5.5Y.
export function formatTenor(asOf: string, maturity: string): string {
  const start = Date.parse(asOf);
  const end = Date.parse(maturity);
  if (!Number.isFinite(start) || !Number.isFinite(end) || end <= start) return "—";
  const days = Math.round((end - start) / DAY_MS);
  if (days < 31) return `${days}D`;
  if (days < 365) return `${Math.round(days / 30.4375)}M`;
  const years = Math.round((days / 365.25) * 2) / 2;
  return `${years}Y`;
}

// First available day-basis term, display-formatted: "ACT_365F" → "ACT/365F".
export function instrumentDayCount(terms: Record<string, unknown> | undefined): string {
  const basis = ["day_basis", "fixed_day_basis", "spread_day_basis", "domestic_day_basis"]
    .map((field) => terms?.[field])
    .find((value) => typeof value === "string" && value.length > 0);
  return typeof basis === "string" ? basis.replace(/_/g, "/") : "—";
}

// Families whose raw quote is (or converts to) a percentage rate.
const RATE_FAMILIES = new Set(["DEPOSIT", "FRA", "OIS", "IRS", "FUTURE"]);

// Raw quote → percent. Futures prices convert to the implied rate (100 - price);
// decimal quotes scale to percent; percent lexemes pass through.
export function quoteToPercent(family: string, rawQuote: unknown): number | null {
  const value = Number(rawQuote);
  if (!Number.isFinite(value)) return null;
  if (family === "FUTURE") return 100 - value;
  return Math.abs(value) < 1 ? value * 100 : value;
}

export interface QuoteSeriesPoint {
  key: string;
  instrumentIndex: number;
  tenor: string;
  days: number;
  percent: number;
  normalizedPercent: number | null;
}

export interface QuoteAxisLike {
  instrument_id?: string;
  normalized_quote?: string;
}

// Plottable points for the curve preview: included, rate-family instruments with a
// parseable quote and a maturity after the as-of date, sorted by tenor. When a
// succeeded build supplies its quote axis, normalized quotes join by instrument_id.
export function quoteSeries(
  instruments: Record<string, unknown>[],
  asOf: string,
  quoteAxis?: QuoteAxisLike[] | null,
): QuoteSeriesPoint[] {
  const normalizedByInstrumentId = new Map<string, string>();
  (quoteAxis ?? []).forEach((entry) => {
    if (entry.instrument_id && typeof entry.normalized_quote === "string") {
      normalizedByInstrumentId.set(entry.instrument_id, entry.normalized_quote);
    }
  });
  const start = Date.parse(asOf);
  if (!Number.isFinite(start)) return [];
  return instruments.flatMap((instrument, instrumentIndex) => {
    if (instrument.included === false) return [];
    const family = String(instrument.instrument_type ?? "");
    if (!RATE_FAMILIES.has(family)) return [];
    const percent = quoteToPercent(family, instrument.raw_quote);
    if (percent === null) return [];
    const maturity = Date.parse(String(instrument.maturity_date ?? ""));
    if (!Number.isFinite(maturity) || maturity <= start) return [];
    const normalizedRaw = typeof instrument.instrument_id === "string"
      ? normalizedByInstrumentId.get(instrument.instrument_id)
      : undefined;
    const normalizedValue = Number(normalizedRaw);
    const days = Math.round((maturity - start) / DAY_MS);
    return [{
      key: String(instrument.instrument_id ?? instrumentIndex),
      instrumentIndex,
      tenor: formatTenor(asOf, String(instrument.maturity_date)),
      days,
      percent,
      normalizedPercent: Number.isFinite(normalizedValue)
        ? Math.abs(normalizedValue) < 1 ? normalizedValue * 100 : normalizedValue
        : null,
    }];
  }).sort((left, right) => left.days - right.days);
}

export interface CurveBuilderStepperInput {
  declarationCount: number;
  dependencyCount: number;
  dependencyAvailable: number;
  includedInstrumentCount: number;
  buildState: string | null;
  fitState: string | null;
  mode: string;
}

const NON_TERMINAL_BUILD_STATES = new Set(["ADMITTED", "QUEUED", "RUNNING"]);

export function stepperStates(
  input: CurveBuilderStepperInput,
): Record<CurveBuilderStepId, CurveBuilderStepState> {
  const declaration: CurveBuilderStepState = input.declarationCount > 0 ? "done" : "active";
  const dependenciesDone = input.dependencyCount > 0
    || input.dependencyAvailable === 0
    || input.mode === "SINGLE";
  const dependencies: CurveBuilderStepState = dependenciesDone
    ? "done"
    : declaration === "done" ? "active" : "todo";
  const instrumentsDone = input.includedInstrumentCount > 0;
  const instruments: CurveBuilderStepState = instrumentsDone
    ? "done"
    : dependencies === "done" ? "active" : "todo";
  let solve: CurveBuilderStepState = "todo";
  if (input.buildState !== null) {
    if (NON_TERMINAL_BUILD_STATES.has(input.buildState)) solve = "running";
    else if (input.buildState === "SUCCEEDED") solve = "done";
    else solve = "failed";
  }
  const validate: CurveBuilderStepState = input.fitState === "NATIVE_ARCHIVE_VALIDATED"
    ? "done"
    : "todo";
  return { declaration, dependencies, instruments, solve, validate };
}

// Latest observed_at across instruments, falling back to the as-of date.
export function quotesAsOf(instruments: Record<string, unknown>[], asOf: string): string {
  const observed = instruments
    .map((instrument) => String(instrument.observed_at ?? ""))
    .filter((value) => value.length > 0)
    .sort();
  return observed.length > 0 ? observed[observed.length - 1] : asOf;
}
