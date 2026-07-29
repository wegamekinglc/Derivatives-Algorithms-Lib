import { useCallback, useEffect, useMemo, useState } from "react";
import {
  api,
  ApiClientError,
  type CurveLabBuildRun,
  type CurveLabDraft,
  type CurveLabImportJob,
  type CurveLabMatrix,
  type CurveLabRiskRun,
  type CurveLabSuccessFamily,
  type CurveLabVersion,
} from "../api/client";
import { css } from "../format";

type WorkspaceTab = "build" | "runs" | "risk" | "versions";
type CurveLabBuildMode = "SINGLE" | "MULTI_CURVE" | "STAGED_XCCY" | "JOINT_XCCY";

const TABS: { id: WorkspaceTab; label: string; note: string }[] = [
  { id: "build", label: "Build", note: "Draft → solve → publish" },
  { id: "runs", label: "Runs", note: "Axes and lifecycle evidence" },
  { id: "risk", label: "Pricing & Risk", note: "PV, DV01, KRD and matrices" },
  { id: "versions", label: "Versions", note: "Clone, archive, import and export" },
];

const COMPONENT_KEY = "clab/v1/local/discount/USD/OIS";
const CURVE_LAB_FAMILIES: readonly CurveLabSuccessFamily[] = [
  "DEPOSIT",
  "FRA",
  "FUTURE",
  "OIS",
  "IRS",
  "BASIS_SWAP",
  "XCCY",
];

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

function message(reason: unknown): string {
  return reason instanceof Error ? reason.message : String(reason);
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
              <tr><th>#</th><th>Label</th><th>Component</th><th>Coordinate</th></tr>
            </thead>
            <tbody>
              {rows.map((row, index) => (
                <tr key={row.quote_id ?? row.parameter_id ?? index}>
                  <td {...css("mono")}>{row.global_quote_index ?? row.global_parameter_index}</td>
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

export default function CurveLabWorkspace() {
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
  const [curveView, setCurveView] = useState<"discount" | "zero" | "forward">("discount");
  const [evaluationTime, setEvaluationTime] = useState("2026-01-15T10:30:00Z");
  const [fixingSnapshotId, setFixingSnapshotId] = useState("curve-lab-ui-fixings");
  const [baseCurrency, setBaseCurrency] = useState("USD");
  const [compareVersionId, setCompareVersionId] = useState("");
  const [busy, setBusy] = useState(false);
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

  const updateVisualDraft = (
    // eslint-disable-next-line no-unused-vars -- core ESLint sees type-only parameter names.
    transform: (...args: [Record<string, unknown>]) => Record<string, unknown>,
  ) => {
    setDraftSource(JSON.stringify(transform(visualDraft), null, 2));
  };
  const setDraftField = (field: string, value: unknown) => {
    updateVisualDraft((current) => ({ ...current, [field]: value }));
  };
  const setBuildMode = (mode: CurveLabBuildMode) => {
    const topology = topologyForMode(mode);
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
      ...template,
      instrument_id: undefined,
      raw_quote: "0.04",
    };
    delete next.instrument_id;
    updateVisualDraft((current) => ({
      ...current,
      instruments: [...visualInstruments, next],
    }));
  };
  const removeInstrument = (index: number) => {
    updateVisualDraft((current) => ({
      ...current,
      instruments: visualInstruments.filter((_, position) => position !== index),
    }));
  };
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
      setError(message(reason));
    });
  }, [refreshVersions]);

  const execute = async (action: () => Promise<void>) => {
    setBusy(true);
    setError(null);
    try {
      await action();
    } catch (reason) {
      setError(message(reason));
    } finally {
      setBusy(false);
    }
  };

  const createDraft = () => execute(async () => {
    const created = await api.createCurveLabDraft(parseJson(draftSource));
    setDraft(created);
    setBuild(null);
    setStatus(`Draft ${created.id.slice(0, 8)} revision ${created.revision} is ready.`);
  });

  const pollBuild = async (initial: CurveLabBuildRun) => {
    const terminal = await waitForTerminal(
      initial,
      api.getCurveLabBuildRun,
      setBuild,
    );
    setStatus(`Build ${terminal.id.slice(0, 8)} finished ${terminal.state}.`);
    setTab("runs");
  };

  const buildCurve = () => execute(async () => {
    if (!draft) throw new Error("Create a draft before building.");
    const created = await api.createCurveLabBuildRun(draft.id);
    setBuild(created);
    setStatus(`Build ${created.id.slice(0, 8)} admitted ${created.state}.`);
    await pollBuild(created);
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
    setBuild(null);
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
    setDraftSource(JSON.stringify(editableDocument(cloned.document), null, 2));
    setBuild(null);
    setTab("build");
    setStatus(`Cloned ${version.name} into draft ${cloned.id.slice(0, 8)}.`);
  });

  const download = (version: CurveLabVersion) => execute(async () => {
    const [payload, manifest] = await Promise.all([
      api.downloadCurveLabVersion(version.id),
      api.getCurveLabRuntimeManifest(version.id),
    ]);
    const manifestUrl = URL.createObjectURL(new Blob(
      [JSON.stringify(manifest, null, 2)],
      { type: "application/json" },
    ));
    const manifestAnchor = document.createElement("a");
    manifestAnchor.href = manifestUrl;
    manifestAnchor.download = `${version.name.replace(/ /g, "-")}-${version.id.slice(0, 8)}.manifest.json`;
    manifestAnchor.click();
    URL.revokeObjectURL(manifestUrl);
    const url = URL.createObjectURL(payload);
    const anchor = document.createElement("a");
    anchor.href = url;
    anchor.download = `${version.name.replace(/ /g, "-")}-${version.id.slice(0, 8)}.json`;
    anchor.click();
    URL.revokeObjectURL(url);
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

  return (
    <section {...css("curve-lab-v2")} aria-labelledby="curve-lab-v2-title">
      <div {...css("curve-lab-v2-heading")}>
        <div>
          <span {...css("eyebrow")}>CURVE LAB / REVISION 8</span>
          <h2 id="curve-lab-v2-title">Durable curve workflow</h2>
        </div>
        <span {...css("tag")}>native JSON · exact axes · replayable risk</span>
      </div>

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
        <div {...css("curve-lab-flow-layout")}>
          <section {...css("request-editor", "curve-lab-flow-editor")}>
            <div {...css("editor-heading")}>
              <strong>Visual curve builder</strong>
              <span {...css("tag")}>closed V2 controls</span>
            </div>
            <section {...css("panel")}>
              <h3>Curve topology</h3>
              <div {...css("form-grid")}>
                <label>
                  <span>Build mode</span>
                  <select
                    value={String(visualDraft.mode ?? "SINGLE")}
                    onChange={(event) => {
                      setBuildMode(event.target.value as CurveLabBuildMode);
                    }}
                  >
                    <option value="SINGLE">Single curve</option>
                    <option value="MULTI_CURVE">Joint multi-curve</option>
                    <option value="STAGED_XCCY">XCCY staged</option>
                    <option value="JOINT_XCCY">XCCY joint</option>
                  </select>
                </label>
                <label>
                  <span>As-of date</span>
                  <input
                    type="date"
                    value={String(visualDraft.as_of_date ?? "")}
                    onChange={(event) => {
                      setDraftField("as_of_date", event.target.value);
                    }}
                  />
                </label>
                <label>
                  <span>Market snapshot</span>
                  <input
                    value={String(visualDraft.market_snapshot_id ?? "")}
                    onChange={(event) => {
                      setDraftField("market_snapshot_id", event.target.value);
                    }}
                  />
                </label>
              </div>
              <div {...css("curve-lab-section-heading")}>
                <div>
                  <h3>Curve declarations</h3>
                  <p {...css("muted")}>
                    Each component owns an included calibration instrument.
                  </p>
                </div>
                <button
                  type="button"
                  disabled={visualDraft.mode === "SINGLE"}
                  onClick={addDeclaration}
                >
                  Add declaration
                </button>
              </div>
              <div {...css("table-container")}>
                <table aria-label="Curve declarations">
                  <thead>
                    <tr>
                      <th>Role</th>
                      <th>Currency</th>
                      <th>Component key</th>
                      <th>Parameterization</th>
                      <th>Actions</th>
                    </tr>
                  </thead>
                  <tbody>
                    {visualDeclarations.map((declaration, index) => (
                      <tr key={String(declaration.component_key ?? index)}>
                        <td>
                          <select
                            aria-label={`Declaration role ${index + 1}`}
                            value={String(declaration.role ?? "DISCOUNT")}
                            onChange={(event) => {
                              setDeclarationField(index, "role", event.target.value);
                            }}
                          >
                            <option value="DISCOUNT">Discount</option>
                            <option value="PROJECTION">Projection</option>
                            <option value="BASIS">Basis</option>
                          </select>
                        </td>
                        <td>
                          <input
                            aria-label={`Declaration currency ${index + 1}`}
                            value={String(declaration.currency ?? "")}
                            maxLength={3}
                            onChange={(event) => {
                              setDeclarationField(
                                index,
                                "currency",
                                event.target.value.toUpperCase(),
                              );
                            }}
                          />
                        </td>
                        <td>
                          <input
                            aria-label={`Declaration component key ${index + 1}`}
                            {...css("mono")}
                            value={String(declaration.component_key ?? "")}
                            onChange={(event) => {
                              setDeclarationField(
                                index,
                                "component_key",
                                event.target.value,
                              );
                            }}
                          />
                        </td>
                        <td>
                          <select
                            aria-label={`Declaration parameterization ${index + 1}`}
                            value={String(
                              declaration.parameterization ?? "PIECEWISE_CONSTANT_FWD"
                            )}
                            onChange={(event) => {
                              setDeclarationField(
                                index,
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
                        </td>
                        <td>
                          <button
                            type="button"
                            {...css("danger")}
                            aria-label={`Remove declaration ${index + 1}`}
                            disabled={visualDeclarations.length === 1}
                            onClick={() => {
                              removeDeclaration(index);
                            }}
                          >
                            Remove
                          </button>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </section>
            <section {...css("panel")}>
              <div {...css("curve-lab-section-heading")}>
                <div>
                  <h3>Curve dependencies</h3>
                  <p {...css("muted")}>
                    Bind visible immutable versions needed by this build.
                  </p>
                </div>
                <span {...css("tag")}>{dependencyVersionIds.length} selected</span>
              </div>
              {versions.length === 0 ? (
                <p {...css("muted")}>No visible curve versions are available.</p>
              ) : (
                <div {...css("curve-lab-dependency-list")}>
                  {versions.map((version) => (
                    <label key={version.id}>
                      <input
                        type="checkbox"
                        aria-label={`Use ${version.name} as dependency`}
                        checked={dependencyVersionIds.includes(version.id)}
                        onChange={(event) => {
                          toggleDependency(
                            version.id,
                            event.target.checked,
                          );
                        }}
                      />
                      <span>
                        <strong>{version.name}</strong>
                        <small {...css("mono")}>
                          {version.id.slice(0, 8)} · {version.root_kind}
                        </small>
                      </span>
                    </label>
                  ))}
                </div>
              )}
            </section>
            <section {...css("panel")}>
              <div {...css("curve-lab-section-heading")}>
                <div>
                  <h3>Calibration instruments</h3>
                  <p {...css("muted")}>Edit registry family, dates, inclusion and canonical quote.</p>
                </div>
                <button type="button" onClick={addInstrument}>Add instrument</button>
              </div>
              <div {...css("table-container")}>
                <table>
                  <thead><tr><th>Family</th><th>Maturity</th><th>Quote</th><th>Included</th><th>Actions</th></tr></thead>
                  <tbody>
                    {visualInstruments.map((instrument, index) => (
                      <tr key={String(instrument.instrument_id ?? index)}>
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
                            {CURVE_LAB_FAMILIES.map(
                              (family) => <option key={family} value={family}>{family}</option>,
                            )}
                          </select>
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
                        <td>
                          <input
                            aria-label={`Quote ${index + 1}`}
                            inputMode="decimal"
                            value={String(instrument.raw_quote ?? "")}
                            onChange={(event) => {
                              setInstrumentField(index, "raw_quote", event.target.value);
                            }}
                          />
                        </td>
                        <td>
                          <input
                            aria-label={`Included ${index + 1}`}
                            type="checkbox"
                            checked={Boolean(instrument.included)}
                            onChange={(event) => {
                              setInstrumentField(index, "included", event.target.checked);
                            }}
                          />
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
                    ))}
                  </tbody>
                </table>
              </div>
            </section>
            <section {...css("panel")}>
              <h3>Solver controls</h3>
              <p {...css("muted")}>
                {String((visualDraft.solver as Record<string, unknown> | undefined)?.solve_mode ?? "EXACT")}
                {" · "}
                {String((visualDraft.solver as Record<string, unknown> | undefined)?.parameterization ?? "PIECEWISE_CONSTANT_FWD")}
              </p>
            </section>
            <details>
              <summary>Advanced JSON</summary>
              <label>
                <span>Build document JSON</span>
                <textarea
                  value={draftSource}
                  rows={24}
                  spellCheck={false}
                  onChange={(event) => {
                    setDraftSource(event.target.value);
                  }}
                />
              </label>
            </details>
            <div {...css("submit-row")}>
              <p>Server-derived quote and parameter axes are frozen at build time.</p>
              <button type="button" disabled={busy} onClick={() => void createDraft()}>
                Create draft
              </button>
            </div>
          </section>
          <aside {...css("panel", "curve-lab-actions")}>
            <h3>Build controls</h3>
            <dl>
              <dt>Draft</dt><dd>{draft ? `${draft.id.slice(0, 8)} · r${draft.revision}` : "not created"}</dd>
              <dt>Build</dt><dd>{build ? `${build.id.slice(0, 8)} · ${build.state}` : "not run"}</dd>
            </dl>
            <button type="button" disabled={busy || !draft} onClick={() => void buildCurve()}>
              Build curve
            </button>
            {build && !isTerminal(build) && (
              <button
                type="button"
                disabled={busy}
                aria-label={`Resume build polling ${build.id.slice(0, 8)}`}
                onClick={() => void resumeBuild()}
              >
                Resume build polling
              </button>
            )}
            <button type="button" disabled={busy || !draft} onClick={() => void saveDraft()}>
              Save draft changes
            </button>
            <label>
              <span>Version name</span>
              <input value={versionName} onChange={(event) => {
                setVersionName(event.target.value);
              }} />
            </label>
            <button type="button" disabled={busy || build?.state !== "SUCCEEDED"} onClick={() => void publishVersion()}>
              Publish version
            </button>
          </aside>
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
                  {curveView === "discount" && "Native discount-factor view on the persisted node axis."}
                  {curveView === "zero" && "Continuously compounded zero-rate view on the persisted node axis."}
                  {curveView === "forward" && "Instantaneous forward coordinates in native parameter order."}
                </p>
                <div {...css("table-container")}>
                  <table>
                    <thead><tr><th>Node</th><th>Component</th><th>Native coordinate</th></tr></thead>
                    <tbody>
                      {build.parameter_axis.map((axis) => (
                        <tr key={String(axis.parameter_id)}>
                          <td>{String(axis.node_date)}</td>
                          <td {...css("mono")}>{String(axis.component_key)}</td>
                          <td>{String(axis.coordinate_kind)}</td>
                        </tr>
                      ))}
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
                            {CURVE_LAB_FAMILIES.map(
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
                      setError(message(reason));
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
}
