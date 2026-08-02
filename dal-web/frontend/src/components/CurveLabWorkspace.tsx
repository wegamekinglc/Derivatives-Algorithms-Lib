import {
  Fragment,
  forwardRef,
  useCallback,
  useEffect,
  useImperativeHandle,
  useMemo,
  useRef,
  useState,
} from "react";
import {
  api,
  ApiClientError,
  type CurveLabBuildRun,
  type CurveLabCanonicalQuote,
  type CurveLabCurveViewPoint,
  type CurveLabDraft,
  type CurveLabImportJob,
  type CurveLabMatrix,
  type CurveLabRiskRun,
  type CurveLabSuccessFamily,
  type CurveLabVersion,
} from "../api/client";
import CurvePreview from "./CurvePreview";
import { CURVE_LAB_SUCCESS_FAMILIES } from "../curves/curveLabRegistry";
import {
  declarationLabel,
  formatTenor,
  instrumentDayCount,
  quoteSeries,
  quotesAsOf,
  stepperStates,
  type CurveBuilderStepId,
  type CurveBuilderStepState,
} from "../curves/curveBuilderUtils";
import {
  curveLabErrorMessage,
  downloadCurveLabArtifacts,
  omitCurveLabInstrumentId,
} from "../curves/curveLabUtils";
import { css } from "../format";

type WorkspaceTab = "build" | "runs" | "risk" | "versions";
type CurveLabBuildMode = "SINGLE" | "MULTI_CURVE" | "STAGED_XCCY" | "JOINT_XCCY";
type CurveView = "discount" | "zero" | "forward";

export interface CurveLabWorkspaceHandle {
  // eslint-disable-next-line no-unused-vars -- core ESLint sees type-only parameter names.
  applyCanonicalQuote: (...args: [CurveLabCanonicalQuote, number]) => boolean;
}

export interface CurveLabCanonicalTarget {
  family: CurveLabSuccessFamily;
  token: number;
}

interface CurveLabWorkspaceProps {
  // eslint-disable-next-line no-unused-vars -- core ESLint sees type-only parameter names.
  onCanonicalTargetChange?: (...args: [CurveLabCanonicalTarget | null]) => void;
}

function curveViewLabel(view: CurveView): string {
  switch (view) {
    case "discount":
      return "Discount factors";
    case "zero":
      return "Zero rates";
    case "forward":
      return "Forwards";
  }
}

function curveViewValue(point: CurveLabCurveViewPoint, view: CurveView): string {
  if (view === "discount") return point.discount_factor.toFixed(10);
  const value = view === "zero" ? point.zero_rate : point.one_day_forward_rate;
  return value === null
    ? "—"
    : `${value.toFixed(10)} (${(value * 100).toFixed(6)}%)`;
}

const TABS: { id: WorkspaceTab; label: string; note: string }[] = [
  { id: "build", label: "Build", note: "Draft → solve → publish" },
  { id: "runs", label: "Runs", note: "Axes and lifecycle evidence" },
  { id: "risk", label: "Pricing & Risk", note: "PV, DV01, KRD and matrices" },
  { id: "versions", label: "Versions", note: "Clone, archive, import and export" },
];

const BUILD_MODES: { value: CurveLabBuildMode; label: string }[] = [
  { value: "SINGLE", label: "Single" },
  { value: "MULTI_CURVE", label: "Multi-Curve" },
  { value: "STAGED_XCCY", label: "Staged XCCY" },
  { value: "JOINT_XCCY", label: "Joint XCCY" },
];

const BUILDER_STEPS: { id: CurveBuilderStepId; label: string }[] = [
  { id: "declaration", label: "Declaration" },
  { id: "dependencies", label: "Dependencies" },
  { id: "instruments", label: "Instruments" },
  { id: "solve", label: "Solve" },
  { id: "validate", label: "Validate" },
];

const COMPONENT_KEY = "clab/v1/local/discount/USD/OIS";

const DEFAULT_DRAFT = {
  schema_version: 2,
  mode: "SINGLE",
  as_of_date: "2026-01-15",
  market_snapshot_id: "curve-lab-demo-2026-01-15",
  declarations: [{
    component_key: COMPONENT_KEY,
    role: "DISCOUNT",
    currency: "USD",
    parameterization: "PIECEWISE_CONSTANT_FWD",
  }],
  instruments: [{
    instrument_type: "DEPOSIT",
    trade_date: "2026-01-15",
    start_date: "2026-01-15",
    maturity_date: "2027-01-15",
    currency_or_pair: "USD",
    raw_quote: "0.04",
    source: "CURVE_LAB_UI",
    observed_at: "2026-01-15T00:00:00Z",
    included: true,
    terms: {
      component_key: COMPONENT_KEY,
      forecast_tenor: "3M",
      day_basis: "ACT_365F",
      collateral: "OIS",
      index_name: "USD-SOFR",
    },
  }],
  dependency_version_ids: [],
  solver: {
    solve_mode: "EXACT",
    parameterization: "PIECEWISE_CONSTANT_FWD",
    tolerance: 1e-8,
    fit_tolerance: 1e-6,
  },
};

const DEFAULT_TRADES = [{
  trade_id: "00000000000000000000000000000001",
  instrument_type: "DEPOSIT",
  trade_date: "2026-01-15",
  start_date: "2026-01-15",
  maturity_date: "2027-01-15",
  currency_or_pair: "USD",
  terms: {
    notional: "1000000",
    contract_rate: "0.05",
    side: "LEND",
    forecast_tenor: "3M",
    day_basis: "ACT_365F",
    collateral: "OIS",
    discount_component_key: COMPONENT_KEY,
  },
}];

function calibrationTerms(
  componentKey: string,
  currencyOrPair: string,
  instrumentType: CurveLabSuccessFamily,
): Record<string, unknown> {
  const domesticCurrency = currencyOrPair.replace("/", "-").split("-")[0] || "USD";
  const indexTerms = {
    component_key: componentKey,
    index_name: `${domesticCurrency}-SOFR`,
    forecast_tenor: "3M",
    day_basis: "ACT_365F",
    collateral: "OIS",
    use_projection_curve: false,
  };
  if (instrumentType === "DEPOSIT" || instrumentType === "FRA") return indexTerms;
  if (instrumentType === "FUTURE") {
    return { ...indexTerms, convexity_adjustment: "0" };
  }
  if (instrumentType === "OIS" || instrumentType === "IRS") {
    return {
      component_key: componentKey,
      fixed_payment_frequency: "12M",
      fixed_day_basis: "ACT_365F",
      float_payment_frequency: instrumentType === "OIS" ? "12M" : "3M",
      float_day_basis: "ACT_365F",
      float_forecast_tenor: "3M",
      float_collateral: "OIS",
      float_use_projection_curve: false,
      index_name: instrumentType === "OIS"
        ? `${domesticCurrency}-SOFR`
        : `${domesticCurrency}-IBOR-3M`,
    };
  }
  if (instrumentType === "BASIS_SWAP") {
    return {
      component_key: componentKey,
      spread_payment_frequency: "3M",
      spread_day_basis: "ACT_365F",
      spread_forecast_tenor: "3M",
      spread_collateral: "OIS",
      spread_use_projection_curve: false,
      reference_payment_frequency: "6M",
      reference_day_basis: "ACT_365F",
      reference_forecast_tenor: "6M",
      reference_collateral: "OIS",
      reference_use_projection_curve: false,
    };
  }
  return {
    component_key: componentKey,
    domestic_notional: "1000000",
    foreign_notional: "900000",
    domestic_payment_frequency: "3M",
    domestic_day_basis: "ACT_365F",
    domestic_forecast_tenor: "3M",
    domestic_collateral: "OIS",
    domestic_use_projection_curve: false,
    foreign_payment_frequency: "3M",
    foreign_day_basis: "ACT_365F",
    foreign_forecast_tenor: "3M",
    foreign_collateral: "OIS",
    foreign_use_projection_curve: false,
    fx_spot: 1.1,
    fx_forward_collateral: "OIS",
  };
}

function familyCurrencyOrPair(
  current: string,
  instrumentType: CurveLabSuccessFamily,
): string {
  const [domestic = "USD", currentForeign] = current.replace("/", "-").split("-");
  if (instrumentType !== "XCCY") return domestic;
  const foreign = currentForeign || (domestic === "EUR" ? "USD" : "EUR");
  return `${domestic}-${foreign}`;
}

function familyRawQuote(instrumentType: CurveLabSuccessFamily): string {
  if (instrumentType === "FUTURE") return "95.8225";
  if (instrumentType === "BASIS_SWAP" || instrumentType === "XCCY") return "0.001";
  return "0.04";
}

function migrateCalibrationInstrument(
  instrument: Record<string, unknown>,
  instrumentType: CurveLabSuccessFamily,
): Record<string, unknown> {
  const previousTerms = instrument.terms;
  const componentKey = previousTerms && typeof previousTerms === "object"
    ? String((previousTerms as Record<string, unknown>).component_key ?? COMPONENT_KEY)
    : COMPONENT_KEY;
  const currencyOrPair = familyCurrencyOrPair(
    String(instrument.currency_or_pair ?? "USD"),
    instrumentType,
  );
  return {
    ...instrument,
    instrument_type: instrumentType,
    currency_or_pair: currencyOrPair,
    raw_quote: familyRawQuote(instrumentType),
    terms: calibrationTerms(componentKey, currencyOrPair, instrumentType),
  };
}

function calibrationInstrument(
  componentKey: string,
  currencyOrPair: string,
  instrumentType: CurveLabSuccessFamily = "DEPOSIT",
): Record<string, unknown> {
  return migrateCalibrationInstrument({
    trade_date: "2026-01-15",
    start_date: "2026-01-15",
    maturity_date: "2027-01-15",
    currency_or_pair: currencyOrPair,
    source: "CURVE_LAB_UI",
    observed_at: "2026-01-15T00:00:00Z",
    included: true,
    terms: { component_key: componentKey },
  }, instrumentType);
}

function topologyForMode(mode: CurveLabBuildMode) {
  const usdDiscount = {
    component_key: COMPONENT_KEY,
    role: "DISCOUNT",
    currency: "USD",
    parameterization: "PIECEWISE_CONSTANT_FWD",
  };
  if (mode === "SINGLE") {
    return {
      declarations: [usdDiscount],
      instruments: [calibrationInstrument(COMPONENT_KEY, "USD")],
    };
  }
  if (mode === "MULTI_CURVE") {
    const projectionKey = "clab/v1/local/projection/USD/3M";
    return {
      declarations: [
        usdDiscount,
        {
          component_key: projectionKey,
          role: "PROJECTION",
          currency: "USD",
          parameterization: "PIECEWISE_CONSTANT_FWD",
        },
      ],
      instruments: [
        calibrationInstrument(COMPONENT_KEY, "USD"),
        calibrationInstrument(projectionKey, "USD"),
      ],
    };
  }
  const eurDiscountKey = "clab/v1/local/discount/EUR/OIS";
  const basisKey = "clab/v1/local/basis/USD-EUR";
  return {
    declarations: [
      usdDiscount,
      {
        component_key: eurDiscountKey,
        role: "DISCOUNT",
        currency: "EUR",
        parameterization: "PIECEWISE_CONSTANT_FWD",
      },
      {
        component_key: basisKey,
        role: "BASIS",
        currency: "USD",
        parameterization: "PIECEWISE_CONSTANT_FWD",
      },
    ],
    instruments: [
      calibrationInstrument(COMPONENT_KEY, "USD"),
      calibrationInstrument(eurDiscountKey, "EUR"),
      calibrationInstrument(basisKey, "USD-EUR", "XCCY"),
    ],
  };
}

function itemAt<T>(values: T[] | undefined, index: number): T | undefined {
  return values?.find((_, position) => position === index);
}

function parseJson(source: string): unknown {
  return JSON.parse(source) as unknown;
}

async function waitForTerminal<T extends { id: string; state: string }>(
  initial: T,
  // eslint-disable-next-line no-unused-vars -- core ESLint sees type-only parameter names.
  load: (...args: [string]) => Promise<T>,
  // eslint-disable-next-line no-unused-vars -- core ESLint sees type-only parameter names.
  onUpdate: (...args: [T]) => void,
): Promise<T> {
  let current = initial;
  onUpdate(current);
  while (!isTerminal(current)) {
    await new Promise((resolve) => window.setTimeout(resolve, 50));
    current = await load(current.id);
    onUpdate(current);
  }
  return current;
}

function isTerminal(run: { state: string }): boolean {
  return ["SUCCEEDED", "FAILED", "TIMED_OUT"].includes(run.state);
}

function editableDocument(document: Record<string, unknown>): Record<string, unknown> {
  const computedFields = new Set([
    "quote_coordinate_kind",
    "canonical_raw_unit",
    "normalized_quote",
    "exact_risk_raw_bump",
    "normalized_risk_bump",
  ]);
  const instruments = Array.isArray(document.instruments)
    ? document.instruments.map((value) => Object.fromEntries(
        Object.entries(value as Record<string, unknown>).filter(
          ([field]) => !computedFields.has(field),
        ),
      ))
    : document.instruments;
  return { ...document, instruments };
}

function AxisTable({
  title,
  rows,
}: {
  title: string;
  rows: CurveLabBuildRun["quote_axis"];
}) {
  return (
    <section {...css("panel", "curve-lab-axis")}>
      <div {...css("curve-lab-section-heading")}>
        <h3>{title}</h3>
        <span {...css("tag")}>{rows.length} coordinates</span>
      </div>
      {rows.length === 0 ? (
        <p {...css("muted")}>No coordinates were persisted for this axis.</p>
      ) : (
        <div {...css("table-container")}>
          <table>
            <thead>
              <tr><th {...css("num")}>#</th><th>Label</th><th>Component</th><th>Coordinate</th></tr>
            </thead>
            <tbody>
              {rows.map((row, index) => (
                <tr key={row.quote_id ?? row.parameter_id ?? index}>
                  <td {...css("mono", "num")}>{row.global_quote_index ?? row.global_parameter_index}</td>
                  <td>{row.display_label}</td>
                  <td {...css("mono")}>{row.component_key}</td>
                  <td {...css("mono")}>
                    {row.normalized_quote ?? `${row.coordinate_kind ?? ""} ${row.node_date ?? ""}`}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </section>
  );
}

function MatrixTable({ matrix }: { matrix: CurveLabMatrix }) {
  const rows = matrix.values?.map((values, rowIndex) => ({
    id: `${matrix.matrix_id}:row:${rowIndex}`,
    cells: values.map((value, columnIndex) => ({
      id: `${matrix.matrix_id}:row:${rowIndex}:column:${columnIndex}`,
      value,
    })),
  }));
  return (
    <section {...css("panel", "curve-lab-matrix")}>
      <div {...css("curve-lab-section-heading")}>
        <div>
          <span {...css("eyebrow")}>{matrix.orientation}</span>
          <h3>{matrix.mathematical_name}</h3>
        </div>
        <span {...css("run-status", matrix.availability === "AVAILABLE" ? "completed" : "failed")}>
          {matrix.availability}
        </span>
      </div>
      <p {...css("muted", "mono")}>{matrix.method} · {matrix.rows} × {matrix.columns}</p>
      {rows && (
        <div {...css("table-container")}>
          <table>
            <tbody>
              {rows.map((row) => (
                <tr key={row.id}>
                  {row.cells.map((cell) => (
                    <td key={cell.id} {...css("mono", "num")}>{cell.value}</td>
                  ))}
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
      {matrix.availability_reason && <p {...css("error")}>{matrix.availability_reason}</p>}
    </section>
  );
}

const CurveLabWorkspace = forwardRef<
  CurveLabWorkspaceHandle,
  CurveLabWorkspaceProps
>(function CurveLabWorkspace({ onCanonicalTargetChange }, ref) {
  const [tab, setTab] = useState<WorkspaceTab>("build");
  const [draftSource, setDraftSource] = useState(
    JSON.stringify(DEFAULT_DRAFT, null, 2),
  );
  const [tradeSource, setTradeSource] = useState(
    JSON.stringify(DEFAULT_TRADES, null, 2),
  );
  const [draft, setDraft] = useState<CurveLabDraft | null>(null);
  const [build, setBuild] = useState<CurveLabBuildRun | null>(null);
  const [versions, setVersions] = useState<CurveLabVersion[]>([]);
  const [selectedVersionId, setSelectedVersionId] = useState("");
  const [versionName, setVersionName] = useState("USD OIS");
  const [risk, setRisk] = useState<CurveLabRiskRun | null>(null);
  const [importJob, setImportJob] = useState<CurveLabImportJob | null>(null);
  const [matrices, setMatrices] = useState<CurveLabMatrix[]>([]);
  const [status, setStatus] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [importManifest, setImportManifest] = useState<Record<string, unknown> | null>(null);
  const [curveView, setCurveView] = useState<CurveView>("discount");
  const [selectedInstrumentIndex, setSelectedInstrumentIndex] = useState<number | null>(null);
  const canonicalTargetTokenRef = useRef(0);
  const [canonicalTargetToken, setCanonicalTargetToken] = useState(0);
  const [evaluationTime, setEvaluationTime] = useState("2026-01-15T10:30:00Z");
  const [fixingSnapshotId, setFixingSnapshotId] = useState("curve-lab-ui-fixings");
  const [baseCurrency, setBaseCurrency] = useState("USD");
  const [compareVersionId, setCompareVersionId] = useState("");
  const [busy, setBusy] = useState(false);
  const [selectedDeclarationIndex, setSelectedDeclarationIndex] = useState(0);
  const [depListOpen, setDepListOpen] = useState(false);
  const [editedAfterBuild, setEditedAfterBuild] = useState(false);
  const [bannerDismissedKey, setBannerDismissedKey] = useState<string | null>(null);
  const [lastSuccess, setLastSuccess] = useState<string | null>(null);
  const selectedVersion = useMemo(
    () => versions.find((item) => item.id === selectedVersionId) ?? null,
    [selectedVersionId, versions],
  );
  const compareVersion = useMemo(
    () => versions.find((item) => item.id === compareVersionId) ?? null,
    [compareVersionId, versions],
  );
  const aggregatePv = useMemo(() => (
    risk?.result?.pricing?.reduce((total, row) => {
      if (row.status !== "SUCCEEDED") return total;
      return total + Number(row.pv ?? 0);
    }, 0) ?? 0
  ), [risk]);
  const visualDraft = useMemo(() => {
    try {
      return parseJson(draftSource) as Record<string, unknown>;
    } catch {
      return DEFAULT_DRAFT as Record<string, unknown>;
    }
  }, [draftSource]);
  const visualInstruments = Array.isArray(visualDraft.instruments)
    ? visualDraft.instruments as Record<string, unknown>[]
    : [];
  const visualDeclarations = Array.isArray(visualDraft.declarations)
    ? visualDraft.declarations as Record<string, unknown>[]
    : [];
  const dependencyVersionIds = Array.isArray(visualDraft.dependency_version_ids)
    ? visualDraft.dependency_version_ids as string[]
    : [];
  const visualTrades = useMemo(() => {
    try {
      const parsed = parseJson(tradeSource);
      return Array.isArray(parsed) ? parsed as Record<string, unknown>[] : [];
    } catch {
      return DEFAULT_TRADES as Record<string, unknown>[];
    }
  }, [tradeSource]);

  const invalidateCanonicalTarget = () => {
    canonicalTargetTokenRef.current += 1;
    setCanonicalTargetToken(canonicalTargetTokenRef.current);
  };
  const selectCanonicalTarget = (index: number | null) => {
    invalidateCanonicalTarget();
    setSelectedInstrumentIndex(index);
  };
  const replaceDraftSource = (next: string) => {
    invalidateCanonicalTarget();
    setDraftSource(next);
  };
  const updateVisualDraft = (
    // eslint-disable-next-line no-unused-vars -- core ESLint sees type-only parameter names.
    transform: (...args: [Record<string, unknown>]) => Record<string, unknown>,
  ) => {
    setEditedAfterBuild(true);
    replaceDraftSource(JSON.stringify(transform(visualDraft), null, 2));
  };
  const setDraftField = (field: string, value: unknown) => {
    updateVisualDraft((current) => ({ ...current, [field]: value }));
  };
  const setBuildMode = (mode: CurveLabBuildMode) => {
    const topology = topologyForMode(mode);
    setSelectedInstrumentIndex(null);
    setSelectedDeclarationIndex(0);
    updateVisualDraft((current) => ({
      ...current,
      mode,
      ...topology,
    }));
  };
  const setDeclarationField = (index: number, field: string, value: unknown) => {
    updateVisualDraft((current) => {
      const previousKey = String(itemAt(visualDeclarations, index)?.component_key ?? "");
      const declarations = visualDeclarations.map((declaration, position) => (
        position === index ? { ...declaration, [field]: value } : declaration
      ));
      if (field !== "component_key") return { ...current, declarations };
      return {
        ...current,
        declarations,
        instruments: visualInstruments.map((instrument) => {
          const terms = instrument.terms;
          if (!terms || typeof terms !== "object") return instrument;
          const termsRecord = terms as Record<string, unknown>;
          return termsRecord.component_key === previousKey
            ? { ...instrument, terms: { ...termsRecord, component_key: value } }
            : instrument;
        }),
      };
    });
  };
  const addDeclaration = () => {
    const index = visualDeclarations.length + 1;
    const currency = String(visualDeclarations[0]?.currency ?? "USD");
    const componentKey = `clab/v1/local/projection/${currency}/${index}M`;
    updateVisualDraft((current) => ({
      ...current,
      declarations: [
        ...visualDeclarations,
        {
          component_key: componentKey,
          role: "PROJECTION",
          currency,
          parameterization: "PIECEWISE_CONSTANT_FWD",
        },
      ],
      instruments: [
        ...visualInstruments,
        calibrationInstrument(componentKey, currency),
      ],
    }));
  };
  const removeDeclaration = (index: number) => {
    const componentKey = itemAt(visualDeclarations, index)?.component_key;
    setSelectedInstrumentIndex(null);
    updateVisualDraft((current) => ({
      ...current,
      declarations: visualDeclarations.filter((_, position) => position !== index),
      instruments: visualInstruments.filter(
        (instrument) => {
          const terms = instrument.terms;
          return !terms
            || typeof terms !== "object"
            || (terms as Record<string, unknown>).component_key !== componentKey;
        },
      ),
    }));
  };
  const toggleDependency = (versionId: string, enabled: boolean) => {
    setDraftField(
      "dependency_version_ids",
      enabled
        ? [...new Set([...dependencyVersionIds, versionId])]
        : dependencyVersionIds.filter((id) => id !== versionId),
    );
  };
  const setInstrumentField = (index: number, field: string, value: unknown) => {
    updateVisualDraft((current) => ({
      ...current,
      instruments: visualInstruments.map((instrument, position) => (
        position === index ? { ...instrument, [field]: value } : instrument
      )),
    }));
  };
  const setInstrumentFamily = (index: number, family: CurveLabSuccessFamily) => {
    updateVisualDraft((current) => ({
      ...current,
      instruments: visualInstruments.map((instrument, position) => (
        position === index ? migrateCalibrationInstrument(instrument, family) : instrument
      )),
    }));
  };
  const addInstrument = () => {
    const template = visualInstruments.slice(-1).pop()
      ?? DEFAULT_DRAFT.instruments[0];
    const next = {
      ...omitCurveLabInstrumentId(template),
      raw_quote: "0.04",
    };
    updateVisualDraft((current) => ({
      ...current,
      instruments: [...visualInstruments, next],
    }));
  };
  const removeInstrument = (index: number) => {
    setSelectedInstrumentIndex((current) => {
      if (current === null || current < index) return current;
      if (current === index) return null;
      return current - 1;
    });
    updateVisualDraft((current) => ({
      ...current,
      instruments: visualInstruments.filter((_, position) => position !== index),
    }));
  };
  const selectedInstrumentFamily = useMemo(() => {
    if (selectedInstrumentIndex === null) return null;
    const value = itemAt(visualInstruments, selectedInstrumentIndex)?.instrument_type;
    return CURVE_LAB_SUCCESS_FAMILIES.includes(value as CurveLabSuccessFamily)
      ? value as CurveLabSuccessFamily
      : null;
  }, [selectedInstrumentIndex, visualInstruments]);
  useEffect(() => {
    onCanonicalTargetChange?.(
      selectedInstrumentFamily === null
        ? null
        : {
            family: selectedInstrumentFamily,
            token: canonicalTargetToken,
          },
    );
  }, [
    canonicalTargetToken,
    onCanonicalTargetChange,
    selectedInstrumentFamily,
  ]);
  const applyCanonicalQuote = useCallback((
    quote: CurveLabCanonicalQuote,
    expectedTargetToken: number,
  ): boolean => {
    if (
      expectedTargetToken !== canonicalTargetTokenRef.current
      || selectedInstrumentIndex === null
      || selectedInstrumentFamily === null
      || quote.instrument_type !== selectedInstrumentFamily
    ) {
      return false;
    }
    const selected = itemAt(visualInstruments, selectedInstrumentIndex);
    if (!selected) return false;
    setDraftSource(JSON.stringify({
      ...visualDraft,
      instruments: visualInstruments.map((instrument, position) => (
        position === selectedInstrumentIndex
          ? { ...instrument, raw_quote: quote.raw_quote }
          : instrument
      )),
    }, null, 2));
    return true;
  }, [
    selectedInstrumentFamily,
    selectedInstrumentIndex,
    visualDraft,
    visualInstruments,
  ]);
  useImperativeHandle(ref, () => ({ applyCanonicalQuote }), [applyCanonicalQuote]);
  const setTradeField = (index: number, field: string, value: unknown) => {
    setTradeSource(JSON.stringify(
      visualTrades.map((trade, position) => (
        position === index ? { ...trade, [field]: value } : trade
      )),
      null,
      2,
    ));
  };
  const setTradeTerm = (index: number, field: string, value: unknown) => {
    setTradeSource(JSON.stringify(
      visualTrades.map((trade, position) => {
        if (position !== index) return trade;
        return {
          ...trade,
          terms: {
            ...(trade.terms as Record<string, unknown>),
            [field]: value,
          },
        };
      }),
      null,
      2,
    ));
  };
  const addTrade = () => {
    const index = visualTrades.length + 1;
    setTradeSource(JSON.stringify([
      ...visualTrades,
      {
        ...DEFAULT_TRADES[0],
        trade_id: index.toString(16).padStart(32, "0"),
      },
    ], null, 2));
  };

  const refreshVersions = useCallback(async () => {
    const next = await api.listCurveLabVersions();
    setVersions(next);
    setSelectedVersionId((current) => current || next[0]?.id || "");
  }, []);

  useEffect(() => {
    void refreshVersions().catch((reason: unknown) => {
      setError(curveLabErrorMessage(reason));
    });
  }, [refreshVersions]);

  const execute = async (action: () => Promise<void>) => {
    setBusy(true);
    setError(null);
    try {
      await action();
    } catch (reason) {
      setError(curveLabErrorMessage(reason));
    } finally {
      setBusy(false);
    }
  };

  const createDraft = () => execute(async () => {
    const created = await api.createCurveLabDraft(parseJson(draftSource));
    setDraft(created);
    replaceDraftSource(JSON.stringify(editableDocument(created.document), null, 2));
    setBuild(null);
    setEditedAfterBuild(false);
    setStatus(`Draft ${created.id.slice(0, 8)} revision ${created.revision} is ready.`);
  });

  const pollBuild = async (
    initial: CurveLabBuildRun,
    options?: { switchToRuns?: boolean },
  ) => {
    const terminal = await waitForTerminal(
      initial,
      api.getCurveLabBuildRun,
      setBuild,
    );
    setStatus(`Build ${terminal.id.slice(0, 8)} finished ${terminal.state}.`);
    if (terminal.state === "SUCCEEDED") {
      setLastSuccess(terminal.finished_at ?? new Date().toISOString());
    }
    if (options?.switchToRuns !== false) {
      setTab("runs");
    }
  };

  const buildCurve = () => execute(async () => {
    if (!draft) throw new Error("Create a draft before building.");
    setEditedAfterBuild(false);
    const created = await api.createCurveLabBuildRun(draft.id);
    setBuild(created);
    setStatus(`Build ${created.id.slice(0, 8)} admitted ${created.state}.`);
    await pollBuild(created);
  });

  // Header orchestration: persist the current document, then build, without
  // leaving the builder screen (preview and status strip update in place).
  const buildAndValidate = () => execute(async () => {
    const persisted = draft === null
      ? await api.createCurveLabDraft(parseJson(draftSource))
      : await api.updateCurveLabDraft(draft.id, draft.revision, parseJson(draftSource));
    setDraft(persisted);
    replaceDraftSource(JSON.stringify(editableDocument(persisted.document), null, 2));
    setEditedAfterBuild(false);
    const created = await api.createCurveLabBuildRun(persisted.id);
    setBuild(created);
    setStatus(`Build ${created.id.slice(0, 8)} admitted ${created.state}.`);
    await pollBuild(created, { switchToRuns: false });
  });

  const resumeBuild = () => execute(async () => {
    if (!build || isTerminal(build)) throw new Error("No admitted build is awaiting polling.");
    await pollBuild(build);
  });

  const saveDraft = () => execute(async () => {
    if (!draft) throw new Error("Create a draft before saving changes.");
    const updated = await api.updateCurveLabDraft(
      draft.id,
      draft.revision,
      parseJson(draftSource),
    );
    setDraft(updated);
    replaceDraftSource(JSON.stringify(editableDocument(updated.document), null, 2));
    setBuild(null);
    setEditedAfterBuild(false);
    setStatus(`Saved draft ${updated.id.slice(0, 8)} revision ${updated.revision}; rebuild required.`);
  });

  const publishVersion = () => execute(async () => {
    if (!draft || !build) throw new Error("A successful build is required.");
    const published = await api.createCurveLabVersion({
      draft_id: draft.id,
      draft_revision: draft.revision,
      draft_fingerprint: draft.fingerprint,
      build_run_id: build.id,
      name: versionName,
      tags: ["curve-lab"],
      idempotency_key: `${draft.id}-${build.id}-${versionName}`,
    });
    await refreshVersions();
    setSelectedVersionId(published.id);
    setStatus(`Published ${published.name}`);
  });

  const pollRisk = async (initial: CurveLabRiskRun) => {
    const terminal = await waitForTerminal(
      initial,
      api.getCurveLabRiskRun,
      setRisk,
    );
    const matrixIds = [
      "trade-to-node",
      "calibration-jacobian",
      "composed-quote-diagnostic",
      "key-rate-dv01",
    ];
    const fetched = await Promise.all(
      matrixIds.map(async (matrixId) => {
        try {
          return await api.getCurveLabMatrix(terminal.id, matrixId);
        } catch {
          return null;
        }
      }),
    );
    setMatrices(fetched.filter((item): item is CurveLabMatrix => item !== null));
    setStatus(`Risk run ${terminal.id.slice(0, 8)} finished ${terminal.state}.`);
  };

  const runRisk = () => execute(async () => {
    if (!selectedVersionId) throw new Error("Select a visible curve version.");
    try {
      await api.createCurveLabFixingSnapshot({
        id: fixingSnapshotId,
        observations: [],
      });
    } catch (reason) {
      if (!(reason instanceof ApiClientError && reason.status === 409)) throw reason;
    }
    const created = await api.createCurveLabRiskRun({
      curve_version_id: selectedVersionId,
      target: { trades: parseJson(tradeSource) },
      measures: ["PV", "DV01", "KEY_RATE_DV01"],
      sensitivity_layers: [
        "TRADE_TO_NODE",
        "CALIBRATION_JACOBIAN",
        "COMPOSED_QUOTE_DIAGNOSTIC",
      ],
      fixing_snapshot_id: fixingSnapshotId,
      evaluation_time: evaluationTime,
      base_currency: baseCurrency,
      options: {
        aad_fallback: "ALLOW",
        jacobian_replay_fallback: "ALLOW",
      },
    });
    setRisk(created);
    setStatus(`Risk run ${created.id.slice(0, 8)} admitted ${created.state}.`);
    await pollRisk(created);
  });

  const resumeRisk = () => execute(async () => {
    if (!risk || isTerminal(risk)) throw new Error("No admitted risk run is awaiting polling.");
    await pollRisk(risk);
  });

  const archive = (version: CurveLabVersion) => execute(async () => {
    await api.archiveCurveLabVersion(version.id);
    await refreshVersions();
    setStatus(`Archived ${version.name}.`);
  });

  const clone = (version: CurveLabVersion) => execute(async () => {
    const cloned = await api.cloneCurveLabVersion(version.id);
    setDraft(cloned);
    replaceDraftSource(JSON.stringify(editableDocument(cloned.document), null, 2));
    setSelectedInstrumentIndex(null);
    setSelectedDeclarationIndex(0);
    setEditedAfterBuild(false);
    setBuild(null);
    setTab("build");
    setStatus(`Cloned ${version.name} into draft ${cloned.id.slice(0, 8)}.`);
  });

  const download = (version: CurveLabVersion) => execute(async () => {
    const [payload, manifest] = await Promise.all([
      api.downloadCurveLabVersion(version.id),
      api.getCurveLabRuntimeManifest(version.id),
    ]);
    downloadCurveLabArtifacts({
      payload,
      manifest,
      versionName: version.name,
      versionId: version.id,
    });
    setStatus(`Exported native JSON for ${version.name}.`);
  });

  const pollImport = async (initial: CurveLabImportJob) => {
    const terminal = await waitForTerminal(
      initial,
      api.getCurveLabImportJob,
      setImportJob,
    );
    await refreshVersions();
    setStatus(`Import ${terminal.id.slice(0, 8)} finished ${terminal.state}.`);
  };

  const importFile = (file: File) => execute(async () => {
    const job = await api.importCurveLabVersion(file, importManifest ?? undefined);
    setImportJob(job);
    setStatus(`Import ${job.id.slice(0, 8)} admitted ${job.state}.`);
    await pollImport(job);
  });

  const resumeImport = () => execute(async () => {
    if (!importJob || isTerminal(importJob)) {
      throw new Error("No admitted import is awaiting polling.");
    }
    await pollImport(importJob);
  });

  const asOfDate = String(visualDraft.as_of_date ?? "");
  const includedInstruments = visualInstruments.filter(
    (instrument) => instrument.included !== false,
  );
  const activeDeclarationIndex = visualDeclarations.length === 0
    ? 0
    : Math.min(selectedDeclarationIndex, visualDeclarations.length - 1);
  const activeDeclaration = itemAt(visualDeclarations, activeDeclarationIndex);
  const fitState = build?.diagnostics && typeof build.diagnostics.fit_state === "string"
    ? build.diagnostics.fit_state
    : null;
  const steps = stepperStates({
    declarationCount: visualDeclarations.length,
    dependencyCount: dependencyVersionIds.length,
    dependencyAvailable: versions.length,
    includedInstrumentCount: includedInstruments.length,
    buildState: build?.state ?? null,
    fitState,
    mode: String(visualDraft.mode ?? "SINGLE"),
  });
  const rebuildRequired = build !== null && (
    editedAfterBuild
    || build.stale
    || (draft !== null && build.draft_revision !== draft.revision)
  );
  const bannerKey = `${build?.id ?? "none"}:${draft?.revision ?? 0}:${editedAfterBuild ? "edited" : "clean"}`;
  const showRebuildBanner = rebuildRequired && bannerDismissedKey !== bannerKey;
  const previewPoints = quoteSeries(
    visualInstruments,
    asOfDate,
    build?.state === "SUCCEEDED" ? build.quote_axis : null,
  );

  return (
    <section {...css("curve-lab-v2")} aria-label="Curve Lab workflow">
      <div {...css("curve-lab-flow-tabs")} role="tablist" aria-label="Curve Lab workflow">
        {TABS.map((item, index) => (
          <button
            key={item.id}
            type="button"
            role="tab"
            aria-label={item.label}
            aria-selected={tab === item.id}
            {...css("curve-lab-flow-tab", tab === item.id && "active")}
            onClick={() => {
              setTab(item.id);
            }}
          >
            <span>0{index + 1}</span>
            <strong>{item.label}</strong>
            <small>{item.note}</small>
          </button>
        ))}
      </div>

      {error && <div {...css("error", "curve-lab-workspace-message")}>{error}</div>}
      {status && <div {...css("curve-lab-success", "curve-lab-workspace-message")}>{status}</div>}

      {tab === "build" && (
        <div {...css("curve-builder")}>
          <div {...css("curve-builder-header")}>
            <div {...css("curve-builder-header-fields")}>
              <label>
                <span>As of</span>
                <input
                  type="date"
                  aria-label="As-of date"
                  value={asOfDate}
                  onChange={(event) => {
                    setDraftField("as_of_date", event.target.value);
                  }}
                />
              </label>
              <label>
                <span>Market</span>
                <input
                  aria-label="Market snapshot"
                  value={String(visualDraft.market_snapshot_id ?? "")}
                  onChange={(event) => {
                    setDraftField("market_snapshot_id", event.target.value);
                  }}
                />
              </label>
            </div>
            <button
              type="button"
              {...css("curve-builder-primary")}
              disabled={busy}
              onClick={() => void buildAndValidate()}
            >
              Build &amp; Validate
            </button>
          </div>

          <div {...css("curve-builder-mode-tabs")} role="tablist" aria-label="Build mode">
            {BUILD_MODES.map((item, index) => (
              <button
                key={item.value}
                type="button"
                role="tab"
                aria-selected={String(visualDraft.mode ?? "SINGLE") === item.value}
                {...css(
                  "curve-builder-mode-tab",
                  String(visualDraft.mode ?? "SINGLE") === item.value && "active",
                )}
                onClick={() => {
                  setBuildMode(item.value);
                }}
              >
                <span aria-hidden="true">0{index + 1}</span>
                {item.label}
              </button>
            ))}
          </div>

          <div {...css("curve-builder-stepper")} aria-label="Build progress">
            {BUILDER_STEPS.map((step, index) => {
              const state: CurveBuilderStepState = steps[step.id];
              const previousDone = index > 0
                && steps[BUILDER_STEPS[index - 1].id] === "done";
              return (
                <Fragment key={step.id}>
                  {index > 0 && (
                    <span {...css("curve-builder-step-link", previousDone && "done")} />
                  )}
                  <div {...css("curve-builder-step", state)}>
                    <span {...css("curve-builder-step-bubble")} aria-hidden="true">
                      {state === "done" ? "✓" : state === "failed" ? "!" : index + 1}
                    </span>
                    {step.label}
                  </div>
                </Fragment>
              );
            })}
          </div>

          {showRebuildBanner && (
            <div {...css("curve-builder-banner")} role="status">
              <span aria-hidden="true">⚠</span>
              <span>Draft changed · rebuild required</span>
              <button
                type="button"
                aria-label="Dismiss rebuild notice"
                onClick={() => {
                  setBannerDismissedKey(bannerKey);
                }}
              >
                ×
              </button>
            </div>
          )}

          <div {...css("curve-builder-grid")}>
            <aside {...css("panel", "curve-builder-set")}>
              <h3 {...css("panel-title")}>Curve set</h3>
              <div {...css("curve-builder-nodes")}>
                {visualDeclarations.map((declaration, index) => {
                  const key = String(declaration.component_key ?? index);
                  return (
                    <button
                      key={key}
                      type="button"
                      aria-pressed={activeDeclarationIndex === index}
                      {...css(
                        "curve-builder-node",
                        activeDeclarationIndex === index && "selected",
                      )}
                      onClick={() => {
                        setSelectedDeclarationIndex(index);
                      }}
                    >
                      <span {...css("curve-builder-node-text")}>
                        <strong>{declarationLabel(key)}</strong>
                        <small {...css("mono")}>{key}</small>
                      </span>
                      <span {...css("curve-builder-chip", "draft")}>
                        {draft ? draft.state.replace(/_/g, " ") : "local"}
                      </span>
                    </button>
                  );
                })}
              </div>
              <div {...css("curve-builder-set-row")}>
                <button
                  type="button"
                  {...css("ghost")}
                  disabled={visualDraft.mode === "SINGLE"}
                  onClick={addDeclaration}
                >
                  Add declaration
                </button>
                <button
                  type="button"
                  {...css("danger")}
                  aria-label={`Remove declaration ${activeDeclarationIndex + 1}`}
                  disabled={visualDeclarations.length === 1}
                  onClick={() => {
                    removeDeclaration(activeDeclarationIndex);
                  }}
                >
                  Remove
                </button>
              </div>
              <button
                type="button"
                {...css("curve-builder-add-dep")}
                aria-expanded={depListOpen}
                onClick={() => {
                  setDepListOpen(!depListOpen);
                }}
              >
                + Add dependency
              </button>
              <div {...css("curve-builder-dep-list", depListOpen && "open")}>
                {versions.length === 0 ? (
                  <p {...css("muted")}>No visible curve versions are available.</p>
                ) : (
                  versions.map((version) => (
                    <label key={version.id}>
                      <input
                        type="checkbox"
                        aria-label={`Use ${version.name} as dependency`}
                        checked={dependencyVersionIds.includes(version.id)}
                        onChange={(event) => {
                          toggleDependency(version.id, event.target.checked);
                        }}
                      />
                      <span>
                        <strong>{version.name}</strong>
                        <small {...css("mono")}>
                          {version.id.slice(0, 8)} · {version.root_kind}
                        </small>
                      </span>
                    </label>
                  ))
                )}
              </div>
              {activeDeclaration && (
                <div {...css("curve-builder-fields")}>
                  <label>
                    <span>Currency</span>
                    <input
                      aria-label={`Declaration currency ${activeDeclarationIndex + 1}`}
                      value={String(activeDeclaration.currency ?? "")}
                      maxLength={3}
                      onChange={(event) => {
                        setDeclarationField(
                          activeDeclarationIndex,
                          "currency",
                          event.target.value.toUpperCase(),
                        );
                      }}
                    />
                  </label>
                  <label>
                    <span>Role</span>
                    <select
                      aria-label={`Declaration role ${activeDeclarationIndex + 1}`}
                      value={String(activeDeclaration.role ?? "DISCOUNT")}
                      onChange={(event) => {
                        setDeclarationField(activeDeclarationIndex, "role", event.target.value);
                      }}
                    >
                      <option value="DISCOUNT">Discount</option>
                      <option value="PROJECTION">Projection</option>
                      <option value="BASIS">Basis</option>
                    </select>
                  </label>
                  <label>
                    <span>Component key</span>
                    <input
                      aria-label={`Declaration component key ${activeDeclarationIndex + 1}`}
                      {...css("mono")}
                      value={String(activeDeclaration.component_key ?? "")}
                      onChange={(event) => {
                        setDeclarationField(
                          activeDeclarationIndex,
                          "component_key",
                          event.target.value,
                        );
                      }}
                    />
                  </label>
                  <label>
                    <span>Parameterization</span>
                    <select
                      aria-label={`Declaration parameterization ${activeDeclarationIndex + 1}`}
                      value={String(
                        activeDeclaration.parameterization ?? "PIECEWISE_CONSTANT_FWD",
                      )}
                      onChange={(event) => {
                        setDeclarationField(
                          activeDeclarationIndex,
                          "parameterization",
                          event.target.value,
                        );
                      }}
                    >
                      <option value="PIECEWISE_CONSTANT_FWD">PWC forward</option>
                      <option value="PIECEWISE_LINEAR_FWD">Linear forward</option>
                      <option value="ZERO_RATE">Zero rate</option>
                      <option value="LOG_DISCOUNT">Log discount</option>
                    </select>
                  </label>
                </div>
              )}
              <div {...css("curve-builder-set-actions")}>
                <dl {...css("curve-builder-ids")}>
                  <dt>Draft</dt>
                  <dd>{draft ? `${draft.id.slice(0, 8)} · r${draft.revision}` : "not created"}</dd>
                  <dt>Build</dt>
                  <dd>{build ? `${build.id.slice(0, 8)} · ${build.state}` : "not run"}</dd>
                </dl>
                <div {...css("curve-builder-set-row")}>
                  <button
                    type="button"
                    {...css("ghost")}
                    disabled={busy}
                    onClick={() => void createDraft()}
                  >
                    Create draft
                  </button>
                  <button
                    type="button"
                    {...css("ghost")}
                    disabled={busy || !draft}
                    onClick={() => void saveDraft()}
                  >
                    Save draft changes
                  </button>
                </div>
                <div {...css("curve-builder-set-row")}>
                  <button
                    type="button"
                    {...css("ghost")}
                    disabled={busy || !draft}
                    onClick={() => void buildCurve()}
                  >
                    Build curve
                  </button>
                  {build && !isTerminal(build) && (
                    <button
                      type="button"
                      {...css("ghost")}
                      disabled={busy}
                      aria-label={`Resume build polling ${build.id.slice(0, 8)}`}
                      onClick={() => void resumeBuild()}
                    >
                      Resume build polling
                    </button>
                  )}
                </div>
                <label>
                  <span>Version name</span>
                  <input
                    value={versionName}
                    onChange={(event) => {
                      setVersionName(event.target.value);
                    }}
                  />
                </label>
                <button
                  type="button"
                  disabled={busy || build?.state !== "SUCCEEDED"}
                  onClick={() => void publishVersion()}
                >
                  Publish version
                </button>
              </div>
            </aside>

            <section {...css("curve-builder-instruments")}>
              <div {...css("panel")}>
                <div {...css("curve-lab-section-heading")}>
                  <h3>Calibration instruments</h3>
                  <div {...css("curve-lab-row-actions")}>
                    <button
                      type="button"
                      {...css("ghost")}
                      disabled={selectedInstrumentIndex === null}
                      onClick={() => {
                        selectCanonicalTarget(null);
                      }}
                    >
                      Clear quote target
                    </button>
                    <button type="button" onClick={addInstrument}>Add instrument</button>
                  </div>
                </div>
                <div {...css("table-container")}>
                  <table>
                    <thead>
                      <tr>
                        <th><span {...css("sr-only")}>Canonical target</span></th>
                        <th>Include</th>
                        <th>Type</th>
                        <th>Tenor</th>
                        <th>Maturity</th>
                        <th {...css("num")}>Quote</th>
                        <th>Day count</th>
                        <th>Source</th>
                        <th>Status</th>
                        <th><span {...css("sr-only")}>Row actions</span></th>
                      </tr>
                    </thead>
                    <tbody>
                      {visualInstruments.map((instrument, index) => {
                        const terms = instrument.terms as Record<string, unknown> | undefined;
                        const included = instrument.included !== false;
                        return (
                          <tr
                            key={String(instrument.instrument_id ?? index)}
                            {...css(selectedInstrumentIndex === index && "target")}
                          >
                            <td>
                              <input
                                type="radio"
                                name="curve-lab-canonical-target"
                                aria-label={`Canonical quote target ${index + 1}`}
                                checked={selectedInstrumentIndex === index}
                                onChange={() => {
                                  selectCanonicalTarget(index);
                                }}
                              />
                            </td>
                            <td>
                              <input
                                aria-label={`Included ${index + 1}`}
                                type="checkbox"
                                checked={instrument.included !== false}
                                onChange={(event) => {
                                  setInstrumentField(index, "included", event.target.checked);
                                }}
                              />
                            </td>
                            <td>
                              <select
                                aria-label={`Family ${index + 1}`}
                                value={String(instrument.instrument_type ?? "DEPOSIT")}
                                onChange={(event) => {
                                  setInstrumentFamily(
                                    index,
                                    event.target.value as CurveLabSuccessFamily,
                                  );
                                }}
                              >
                                {CURVE_LAB_SUCCESS_FAMILIES.map(
                                  (family) => (
                                    <option key={family} value={family}>{family}</option>
                                  ),
                                )}
                              </select>
                            </td>
                            <td {...css("mono", "muted")}>
                              {formatTenor(asOfDate, String(instrument.maturity_date ?? ""))}
                            </td>
                            <td>
                              <input
                                aria-label={`Maturity ${index + 1}`}
                                type="date"
                                value={String(instrument.maturity_date ?? "")}
                                onChange={(event) => {
                                  setInstrumentField(index, "maturity_date", event.target.value);
                                }}
                              />
                            </td>
                            <td {...css("num")}>
                              <input
                                aria-label={`Quote ${index + 1}`}
                                {...css("curve-builder-quote")}
                                inputMode="decimal"
                                value={String(instrument.raw_quote ?? "")}
                                onChange={(event) => {
                                  setInstrumentField(index, "raw_quote", event.target.value);
                                }}
                              />
                            </td>
                            <td>{instrumentDayCount(terms)}</td>
                            <td {...css("muted")}>{String(instrument.source ?? "—")}</td>
                            <td>
                              <span
                                {...css(included
                                  ? "curve-builder-tag-ready"
                                  : "curve-builder-tag-excluded")}
                              >
                                {included ? "Ready" : "Excluded"}
                              </span>
                            </td>
                            <td>
                              <button
                                type="button"
                                {...css("danger")}
                                disabled={visualInstruments.length === 1}
                                onClick={() => {
                                  removeInstrument(index);
                                }}
                              >
                                Remove
                              </button>
                            </td>
                          </tr>
                        );
                      })}
                    </tbody>
                  </table>
                  <div {...css("curve-builder-table-footer")}>
                    <span>{visualInstruments.length} instruments</span>
                    <span>Quotes as of {quotesAsOf(visualInstruments, asOfDate)}</span>
                  </div>
                </div>
              </div>
              <details {...css("curve-builder-advanced")}>
                <summary>Solver &amp; advanced JSON</summary>
                <p {...css("muted", "mono")}>
                  {String(
                    (visualDraft.solver as Record<string, unknown> | undefined)?.solve_mode
                      ?? "EXACT",
                  )}
                  {" · "}
                  {String(
                    (visualDraft.solver as Record<string, unknown> | undefined)?.parameterization
                      ?? "PIECEWISE_CONSTANT_FWD",
                  )}
                </p>
                <label>
                  <span>Build document JSON</span>
                  <textarea
                    value={draftSource}
                    rows={20}
                    spellCheck={false}
                    onChange={(event) => {
                      setEditedAfterBuild(true);
                      replaceDraftSource(event.target.value);
                    }}
                  />
                </label>
              </details>
            </section>

            <CurvePreview
              points={previewPoints}
              buildState={build?.state ?? null}
              fitState={fitState}
              includedCount={includedInstruments.length}
              totalCount={visualInstruments.length}
            />
          </div>

          <footer {...css("curve-builder-statusbar")}>
            <span>
              Curve Set:{" "}
              <strong>
                {declarationLabel(String(visualDeclarations[0]?.component_key ?? "—"))}
                {" · "}
                {draft ? draft.state.replace(/_/g, " ") : "LOCAL"}
              </strong>
            </span>
            <span>Dependencies: <strong>{dependencyVersionIds.length}</strong></span>
            <span>Instruments: <strong>{visualInstruments.length}</strong></span>
            <span>
              Status:{" "}
              <strong
                {...css(rebuildRequired
                  ? "warn"
                  : build?.state === "SUCCEEDED" ? "ok" : undefined)}
              >
                {rebuildRequired ? "Rebuild required" : build?.state ?? "Draft"}
              </strong>
            </span>
            <span {...css("curve-builder-statusbar-spacer")} />
            <span>
              Last build:{" "}
              <strong {...css("mono")}>
                {build ? `${build.id.slice(0, 8)} · ${build.state}` : "—"}
              </strong>
            </span>
            <span>
              Last success:{" "}
              <strong {...css("mono")}>{lastSuccess ?? "—"}</strong>
            </span>
          </footer>
        </div>
      )}

      {tab === "runs" && (
        <div {...css("curve-lab-run-evidence")}>
          {!build ? (
            <section {...css("panel")}>
              <h3>No build selected</h3>
              <p {...css("muted")}>Create a draft and run a build to inspect immutable axes.</p>
            </section>
          ) : (
            <>
              <div {...css("cards", "result-cards")}>
                <div {...css("card")}><h3>State</h3><div {...css("metric")}>{build.state}</div></div>
                <div {...css("card")}><h3>Draft revision</h3><div {...css("metric")}>{build.draft_revision}</div></div>
                <div {...css("card")}><h3>Quotes</h3><div {...css("metric")}>{build.quote_axis.length}</div></div>
                <div {...css("card")}><h3>Parameters</h3><div {...css("metric")}>{build.parameter_axis.length}</div></div>
              </div>
              <p {...css("muted", "mono")}>
                {String(build.diagnostics?.fit_state ?? "NO_DIAGNOSTICS")} ·{" "}
                {String(build.resolved_plan.mode ?? "UNKNOWN_MODE")} ·{" "}
                {build.dependency_manifest.length} dependencies
              </p>
              <div {...css("matrix-grid")}>
                <AxisTable title="Quote axis" rows={build.quote_axis} />
                <AxisTable title="Parameter axis" rows={build.parameter_axis} />
              </div>
              {build.stale && (
                <div {...css("error")}>
                  Stale build evidence: this run no longer matches the current draft revision.
                </div>
              )}
              <section {...css("panel")}>
                <div {...css("curve-lab-section-heading")}>
                  <h3>Curve views</h3>
                  <div {...css("curve-lab-row-actions")}>
                    {([
                      ["discount", "Discount factors"],
                      ["zero", "Zero rates"],
                      ["forward", "Forwards"],
                    ] as const).map(([id, label]) => (
                      <button
                        key={id}
                        type="button"
                        {...css(curveView === id ? "active" : "ghost")}
                        onClick={() => {
                          setCurveView(id);
                        }}
                      >
                        {label}
                      </button>
                    ))}
                  </div>
                </div>
                <p {...css("muted")}>
                  {curveView === "discount" && "Native discount factors from valuation date to each persisted node."}
                  {curveView === "zero" && "Continuously compounded ACT/365F zero rates on the persisted node axis."}
                  {curveView === "forward" && "One-day continuously compounded ACT/365F forwards; LEFT samples the preceding interval."}
                </p>
                <div {...css("table-container")}>
                  <table aria-label={`${curveViewLabel(curveView)} curve values`}>
                    <thead><tr><th>Node</th><th>Component</th><th>Side</th><th {...css("num")}>Value</th></tr></thead>
                    <tbody>
                      {(build.curve_views ?? []).map((point) => (
                        <tr key={point.parameter_id}>
                          <td {...css("mono")}>{point.node_date}</td>
                          <td {...css("mono")}>{point.component_key}</td>
                          <td>{point.side ?? "—"}</td>
                          <td {...css("mono", "num")}>{curveViewValue(point, curveView)}</td>
                        </tr>
                      ))}
                      {(build.curve_views ?? []).length === 0 && (
                        <tr>
                          <td colSpan={4} {...css("muted")}>
                            Numeric curve views are unavailable for this run. Rebuild the curve to populate them.
                          </td>
                        </tr>
                      )}
                    </tbody>
                  </table>
                </div>
              </section>
            </>
          )}
          {risk && (
            <section {...css("panel")}>
              <h3>Latest risk run</h3>
              <p {...css("mono")}>{risk.id} · {risk.state}</p>
            </section>
          )}
        </div>
      )}

      {tab === "risk" && (
        <div {...css("curve-lab-risk-layout")}>
          <section {...css("request-editor", "curve-lab-flow-editor")}>
            <div {...css("editor-heading")}>
              <strong>Target trades</strong>
              <span {...css("tag")}>PV · DV01 · KRD</span>
            </div>
            <label>
              <span>Curve version</span>
              <select value={selectedVersionId} onChange={(event) => {
                setSelectedVersionId(event.target.value);
              }}>
                <option value="">Select a version</option>
                {versions.map((version) => (
                  <option key={version.id} value={version.id}>{version.name} · {version.id.slice(0, 8)}</option>
                ))}
              </select>
            </label>
            <div {...css("curve-lab-section-heading")}>
              <h3>Trade targeting</h3>
              <button type="button" onClick={addTrade}>Add trade</button>
            </div>
            <div {...css("table-container")}>
              <table>
                <thead><tr><th>Family</th><th>Maturity</th><th>Notional</th><th>Contract rate</th></tr></thead>
                <tbody>
                  {visualTrades.map((trade, index) => {
                    const terms = trade.terms as Record<string, unknown>;
                    return (
                      <tr key={String(trade.trade_id)}>
                        <td>
                          <select
                            aria-label={`Trade family ${index + 1}`}
                            value={String(trade.instrument_type)}
                            onChange={(event) => {
                              setTradeField(index, "instrument_type", event.target.value);
                            }}
                          >
                            {CURVE_LAB_SUCCESS_FAMILIES.map(
                              (family) => <option key={family}>{family}</option>,
                            )}
                          </select>
                        </td>
                        <td>
                          <input
                            type="date"
                            value={String(trade.maturity_date)}
                            onChange={(event) => {
                              setTradeField(index, "maturity_date", event.target.value);
                            }}
                          />
                        </td>
                        <td>
                          <input
                            inputMode="decimal"
                            value={String(terms.notional ?? terms.position_count ?? "")}
                            onChange={(event) => {
                              setTradeTerm(index, "notional", event.target.value);
                            }}
                          />
                        </td>
                        <td>
                          <input
                            inputMode="decimal"
                            value={String(terms.contract_rate ?? terms.contract_spread ?? "")}
                            onChange={(event) => {
                              setTradeTerm(index, "contract_rate", event.target.value);
                            }}
                          />
                        </td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            </div>
            <details>
              <summary>Advanced trade JSON</summary>
              <label>
                <span>Trade target JSON</span>
                <textarea value={tradeSource} rows={20} spellCheck={false} onChange={(event) => {
                  setTradeSource(event.target.value);
                }} />
              </label>
            </details>
            <div {...css("submit-row")}>
              <p>Full replay computes native PV, parallel DV01, KRD, G, J and GJ.</p>
              <button type="button" disabled={busy || !selectedVersionId} onClick={() => void runRisk()}>
                Run pricing & risk
              </button>
            </div>
          </section>
          <aside {...css("panel", "curve-lab-actions")}>
            <h3>Admission preview</h3>
            <p {...css("muted")}>Selected</p>
            <strong>{selectedVersion?.name ?? "No curve version"}</strong>
            <ul>
              <li>Native AAD with per-trade eligibility</li>
              <li>Exact registry-owned quote replay</li>
              <li>Persisted matrix envelopes</li>
            </ul>
            <label>
              <span>Evaluation time</span>
              <input value={evaluationTime} onChange={(event) => {
                setEvaluationTime(event.target.value);
              }} />
            </label>
            <label>
              <span>Fixing snapshot</span>
              <input value={fixingSnapshotId} onChange={(event) => {
                setFixingSnapshotId(event.target.value);
              }} />
            </label>
            <label>
              <span>Base currency</span>
              <input value={baseCurrency} onChange={(event) => {
                setBaseCurrency(event.target.value.toUpperCase());
              }} />
            </label>
            {risk && (
              <dl>
                <dt>State</dt><dd>{risk.state}</dd>
                <dt>Prices</dt><dd>{risk.result?.pricing?.length ?? 0}</dd>
                <dt>Evaluations</dt><dd>{String(risk.estimated_work.price_evaluations)}</dd>
              </dl>
            )}
            {risk && !isTerminal(risk) && (
              <button
                type="button"
                disabled={busy}
                aria-label={`Resume risk polling ${risk.id.slice(0, 8)}`}
                onClick={() => void resumeRisk()}
              >
                Resume risk polling
              </button>
            )}
          </aside>
          {risk?.result?.pricing && (
            <section {...css("panel", "curve-lab-risk-results")}>
              <div {...css("curve-lab-section-heading")}>
                <h3>Trade results</h3>
                <span {...css("tag")}>Aggregate PV {aggregatePv.toLocaleString()}</span>
              </div>
              <div {...css("table-container")}>
                <table>
                  <thead><tr><th>Trade</th><th>Status</th><th>PV</th><th>DV01</th><th>KRD sum</th><th>Failure</th></tr></thead>
                  <tbody>
                    {risk.result.pricing.map((row, index) => (
                      <tr key={String(row.trade_id)}>
                        <td {...css("mono")}>{String(row.trade_id).slice(0, 8)}</td>
                        <td>{String(row.status)}</td>
                        <td {...css("mono", "num")}>{String(row.pv ?? "—")}</td>
                        <td {...css("mono", "num")}>{String(itemAt(risk.result?.dv01, index)?.value ?? "—")}</td>
                        <td {...css("mono", "num")}>{String(itemAt(risk.result?.key_rate_sum, index)?.value ?? "—")}</td>
                        <td {...css("error")}>
                          {String((row.error as { code?: string } | undefined)?.code ?? "—")}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </section>
          )}
          {matrices.length > 0 && (
            <div {...css("matrix-grid", "curve-lab-matrices")}>
              {matrices.map((matrix) => <MatrixTable key={matrix.matrix_id} matrix={matrix} />)}
            </div>
          )}
        </div>
      )}

      {tab === "versions" && (
        <section {...css("panel", "curve-lab-versions")}>
          <div {...css("curve-lab-section-heading")}>
            <div><h3>Visible curve versions</h3><p {...css("muted")}>Immutable native archives with explicit provenance.</p></div>
            <label {...css("file-button")}>
              Import native JSON
              <input
                type="file"
                accept="application/json,.json"
                onChange={(event) => {
                  const file = event.target.files?.[0];
                  if (file) void importFile(file);
                }}
              />
            </label>
            <label {...css("file-button")}>
              Select runtime manifest
              <input
                type="file"
                accept="application/json,.json"
                onChange={(event) => {
                  const file = event.target.files?.[0];
                  if (file) {
                    void file.text().then((source) => {
                      setImportManifest(JSON.parse(source) as Record<string, unknown>);
                    }).catch((reason: unknown) => {
                      setError(curveLabErrorMessage(reason));
                    });
                  }
                }}
              />
            </label>
          </div>
          {importJob && (
            <section {...css("panel")}>
              <h3>Latest import</h3>
              <p {...css("mono")}>{importJob.id} · {importJob.state}</p>
              {!isTerminal(importJob) && (
                <button
                  type="button"
                  disabled={busy}
                  aria-label={`Resume import polling ${importJob.id.slice(0, 8)}`}
                  onClick={() => void resumeImport()}
                >
                  Resume import polling
                </button>
              )}
            </section>
          )}
          <section {...css("panel")}>
            <div {...css("curve-lab-section-heading")}>
              <div>
                <h3>Version diff</h3>
                <p {...css("muted")}>Compare immutable archive identity and lineage.</p>
              </div>
              <select
                aria-label="Compare version"
                value={compareVersionId}
                onChange={(event) => {
                  setCompareVersionId(event.target.value);
                }}
              >
                <option value="">Choose comparison</option>
                {versions.map((version) => (
                  <option key={version.id} value={version.id}>{version.name}</option>
                ))}
              </select>
            </div>
            {selectedVersion && compareVersion ? (
              <table>
                <thead><tr><th>Field</th><th>Selected</th><th>Comparison</th></tr></thead>
                <tbody>
                  <tr><td>Payload hash</td><td {...css("mono")}>{selectedVersion.native_payload_hash}</td><td {...css("mono")}>{compareVersion.native_payload_hash}</td></tr>
                  <tr><td>Source</td><td>{selectedVersion.source_kind}</td><td>{compareVersion.source_kind}</td></tr>
                  <tr><td>Root</td><td>{selectedVersion.root_kind}</td><td>{compareVersion.root_kind}</td></tr>
                  <tr><td>Validation</td><td>{selectedVersion.build_validation_state}</td><td>{compareVersion.build_validation_state}</td></tr>
                </tbody>
              </table>
            ) : (
              <p {...css("muted")}>Select the working version and a comparison version.</p>
            )}
          </section>
          <div {...css("table-container")}>
            <table>
              <thead><tr><th>Name</th><th>Source</th><th>Root</th><th>Payload</th><th>Created</th><th>Actions</th></tr></thead>
              <tbody>
                {versions.map((version) => (
                  <tr key={version.id}>
                    <td><strong>{version.name}</strong><br /><small {...css("mono")}>{version.id}</small></td>
                    <td><span {...css("tag")}>{version.source_kind}</span></td>
                    <td {...css("mono")}>{version.root_kind}</td>
                    <td {...css("mono", "num")}>{version.native_payload_length} B</td>
                    <td>{new Date(version.created_at).toLocaleString()}</td>
                    <td>
                      <div {...css("curve-lab-row-actions")}>
                        <button type="button" {...css("ghost")} onClick={() => void clone(version)}>Clone</button>
                        <button type="button" {...css("ghost")} onClick={() => void download(version)}>Export</button>
                        <button type="button" {...css("danger")} onClick={() => void archive(version)}>Archive</button>
                      </div>
                    </td>
                  </tr>
                ))}
                {versions.length === 0 && <tr><td colSpan={6} {...css("muted")}>No visible versions.</td></tr>}
              </tbody>
            </table>
          </div>
        </section>
      )}
    </section>
  );
});

export default CurveLabWorkspace;
