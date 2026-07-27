import { useEffect, useMemo, useState } from "react";
import {
  api,
  type CurveLabBuildRun,
  type CurveLabDraft,
  type CurveLabMatrix,
  type CurveLabRiskRun,
  type CurveLabVersion,
} from "../api/client";
import { css } from "../format";

type WorkspaceTab = "build" | "runs" | "risk" | "versions";

const TABS: Array<{ id: WorkspaceTab; label: string; note: string }> = [
  { id: "build", label: "Build", note: "Draft → solve → publish" },
  { id: "runs", label: "Runs", note: "Axes and lifecycle evidence" },
  { id: "risk", label: "Pricing & Risk", note: "PV, DV01, KRD and matrices" },
  { id: "versions", label: "Versions", note: "Clone, archive, import and export" },
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

function message(reason: unknown): string {
  return reason instanceof Error ? reason.message : String(reason);
}

function parseJson(source: string): unknown {
  return JSON.parse(source) as unknown;
}

function editableDocument(document: Record<string, unknown>): Record<string, unknown> {
  const instruments = Array.isArray(document.instruments)
    ? document.instruments.map((value) => {
        const instrument = { ...(value as Record<string, unknown>) };
        for (const field of [
          "quote_coordinate_kind",
          "canonical_raw_unit",
          "normalized_quote",
          "exact_risk_raw_bump",
          "normalized_risk_bump",
        ]) {
          delete instrument[field];
        }
        return instrument;
      })
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
      {matrix.values && (
        <div {...css("table-container")}>
          <table>
            <tbody>
              {matrix.values.map((row, rowIndex) => (
                <tr key={rowIndex}>
                  {row.map((value, columnIndex) => (
                    <td key={columnIndex} {...css("mono", "num")}>{value}</td>
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
  const [matrices, setMatrices] = useState<CurveLabMatrix[]>([]);
  const [status, setStatus] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const selectedVersion = useMemo(
    () => versions.find((item) => item.id === selectedVersionId) ?? null,
    [selectedVersionId, versions],
  );

  const refreshVersions = async () => {
    const next = await api.listCurveLabVersions();
    setVersions(next);
    setSelectedVersionId((current) => current || next[0]?.id || "");
  };

  useEffect(() => {
    void refreshVersions().catch((reason: unknown) => {
      setError(message(reason));
    });
  }, []);

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

  const buildCurve = () => execute(async () => {
    if (!draft) throw new Error("Create a draft before building.");
    const created = await api.createCurveLabBuildRun(draft.id);
    setBuild(created);
    setStatus(`Build ${created.id.slice(0, 8)} finished ${created.state}.`);
    setTab("runs");
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

  const runRisk = () => execute(async () => {
    if (!selectedVersionId) throw new Error("Select a visible curve version.");
    const created = await api.createCurveLabRiskRun({
      curve_version_id: selectedVersionId,
      target: { trades: parseJson(tradeSource) },
      measures: ["PV", "DV01", "KEY_RATE_DV01"],
      sensitivity_layers: [
        "TRADE_TO_NODE",
        "CALIBRATION_JACOBIAN",
        "COMPOSED_QUOTE_DIAGNOSTIC",
      ],
      fixing_snapshot_id: "curve-lab-ui-fixings",
      evaluation_time: "2026-01-15T10:30:00Z",
      base_currency: "USD",
      options: {
        aad_fallback: "ALLOW",
        jacobian_replay_fallback: "ALLOW",
      },
    });
    setRisk(created);
    const matrixIds = [
      "trade-to-node",
      "calibration-jacobian",
      "composed-quote-diagnostic",
      "key-rate-dv01",
    ];
    const fetched = await Promise.all(
      matrixIds.map(async (matrixId) => {
        try {
          return await api.getCurveLabMatrix(created.id, matrixId);
        } catch {
          return null;
        }
      }),
    );
    setMatrices(fetched.filter((item): item is CurveLabMatrix => item !== null));
    setStatus(`Risk run ${created.id.slice(0, 8)} finished ${created.state}.`);
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
    const payload = await api.downloadCurveLabVersion(version.id);
    const url = URL.createObjectURL(payload);
    const anchor = document.createElement("a");
    anchor.href = url;
    anchor.download = `${version.name.replace(/ /g, "-")}-${version.id.slice(0, 8)}.json`;
    anchor.click();
    URL.revokeObjectURL(url);
    setStatus(`Exported native JSON for ${version.name}.`);
  });

  const importFile = (file: File) => execute(async () => {
    const job = await api.importCurveLabVersion(file);
    await refreshVersions();
    setStatus(`Import ${job.id.slice(0, 8)} finished ${job.state}.`);
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
            aria-selected={tab === item.id}
            {...css("curve-lab-flow-tab", tab === item.id && "active")}
            onClick={() => setTab(item.id)}
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
              <strong>CurveDraftDocumentV2</strong>
              <span {...css("tag")}>canonical decimals only</span>
            </div>
            <label>
              <span>Build document JSON</span>
              <textarea
                value={draftSource}
                rows={24}
                spellCheck={false}
                onChange={(event) => setDraftSource(event.target.value)}
              />
            </label>
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
            <button type="button" disabled={busy || !draft} onClick={() => void saveDraft()}>
              Save draft changes
            </button>
            <label>
              <span>Version name</span>
              <input value={versionName} onChange={(event) => setVersionName(event.target.value)} />
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
              <div {...css("matrix-grid")}>
                <AxisTable title="Quote axis" rows={build.quote_axis} />
                <AxisTable title="Parameter axis" rows={build.parameter_axis} />
              </div>
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
              <strong>RateTradeDefinitionV2[]</strong>
              <span {...css("tag")}>PV · DV01 · KRD</span>
            </div>
            <label>
              <span>Curve version</span>
              <select value={selectedVersionId} onChange={(event) => setSelectedVersionId(event.target.value)}>
                <option value="">Select a version</option>
                {versions.map((version) => (
                  <option key={version.id} value={version.id}>{version.name} · {version.id.slice(0, 8)}</option>
                ))}
              </select>
            </label>
            <label>
              <span>Trade target JSON</span>
              <textarea value={tradeSource} rows={20} spellCheck={false} onChange={(event) => setTradeSource(event.target.value)} />
            </label>
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
              <li>Exact registry-owned quote bumps</li>
              <li>Central native-parameter fallback</li>
              <li>Persisted matrix envelopes</li>
            </ul>
            {risk && (
              <dl>
                <dt>State</dt><dd>{risk.state}</dd>
                <dt>Prices</dt><dd>{risk.result?.pricing?.length ?? 0}</dd>
                <dt>Evaluations</dt><dd>{String(risk.estimated_work.price_evaluations ?? 0)}</dd>
              </dl>
            )}
          </aside>
          {risk?.result?.pricing && (
            <section {...css("panel", "curve-lab-risk-results")}>
              <h3>Trade results</h3>
              <div {...css("table-container")}>
                <table>
                  <thead><tr><th>Trade</th><th>Status</th><th>PV</th><th>DV01</th><th>KRD sum</th></tr></thead>
                  <tbody>
                    {risk.result.pricing.map((row, index) => (
                      <tr key={String(row.trade_id)}>
                        <td {...css("mono")}>{String(row.trade_id).slice(0, 8)}</td>
                        <td>{String(row.status)}</td>
                        <td {...css("mono", "num")}>{String(row.pv ?? "—")}</td>
                        <td {...css("mono", "num")}>{String(risk.result?.dv01?.[index]?.value ?? "—")}</td>
                        <td {...css("mono", "num")}>{String(risk.result?.key_rate_sum?.[index]?.value ?? "—")}</td>
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
          </div>
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
