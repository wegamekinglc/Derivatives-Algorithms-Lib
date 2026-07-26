import { useEffect, useMemo, useState } from "react";
import { Link, useParams } from "react-router-dom";
import { api, type CalibrationRun } from "../api/client";
import CalibrationLifecycle from "../components/CalibrationLifecycle";
import FitPlot from "../components/FitPlot";
import MatrixHeatmap from "../components/MatrixHeatmap";
import QuoteBumpPanel from "../components/QuoteBumpPanel";
import { alignFitSeries } from "../curves/visualization";
import { css } from "../format";

export default function CurveRun() {
  const { runId = "" } = useParams();
  const [run, setRun] = useState<CalibrationRun | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    let timer: ReturnType<typeof setTimeout> | null = null;
    const load = async () => {
      try {
        const next = await api.getCalibration(runId);
        if (cancelled) return;
        setRun(next);
        setError(null);
        if (next.status === "running") timer = setTimeout(load, 300);
      } catch (reason) {
        if (!cancelled) setError(String(reason));
      }
    };
    void load();
    return () => {
      cancelled = true;
      if (timer) clearTimeout(timer);
    };
  }, [runId]);

  const fit = useMemo(() => {
    if (!run?.instrument_diagnostics.length) return [];
    const axis = run.jacobian?.row_axis ??
      run.instrument_diagnostics.map((item) => `residual:${item.instrument_id}`);
    return alignFitSeries(run.instrument_diagnostics, axis);
  }, [run]);

  if (error) return <div {...css("error")}>{error}</div>;
  if (!run) return <p {...css("muted")}>Loading calibration run…</p>;

  return (
    <div>
      <div {...css("run-header")}>
        <div>
          <Link to="/curves" {...css("back-link")}>← Curve workbench</Link>
          <span {...css("eyebrow")}>{run.kind.split("_").join(" ")} / {run.id.slice(0, 8)}</span>
          <h1>{run.name}</h1>
        </div>
        <span {...css("run-status", run.status)}>{run.status}</span>
      </div>
      <CalibrationLifecycle phase={run.phase} failed={run.status === "failed"} />

      {run.status === "running" && (
        <section {...css("panel", "running-panel")}>
          <span {...css("spinner")} />
          <div>
            <h2>Native solve in progress</h2>
            <p {...css("muted")}>Polling persisted phase: <code>{run.phase}</code></p>
          </div>
        </section>
      )}

      {run.status === "failed" && (
        <section {...css("panel", "failed-panel")}>
          <span {...css("eyebrow")}>{run.error?.code ?? "CALIBRATION_FAILED"}</span>
          <h2>{run.error?.message ?? "Calibration failed"}</h2>
          {run.error?.location && <code>{run.error.location.join(" › ")}</code>}
        </section>
      )}

      {run.status === "completed" && (
        <>
          <div {...css("cards", "result-cards")}>
            <div {...css("card")}>
              <h3>Curves</h3><div {...css("metric")}>{run.curves.length}</div>
            </div>
            <div {...css("card")}>
              <h3>Max |residual|</h3><div {...css("metric", "mono")}>{run.solver_diagnostics?.max_abs_residual.toExponential(3)}</div>
            </div>
            <div {...css("card")}>
              <h3>RMS residual</h3><div {...css("metric", "mono")}>{run.solver_diagnostics?.rms_residual.toExponential(3)}</div>
            </div>
            <div {...css("card")}>
              <h3>Jacobian mode</h3><div {...css("metric", "mode-metric")}>{run.actual_jacobian_mode}</div>
            </div>
          </div>

          <section {...css("panel")}>
            <h2>Persisted curves</h2>
            <div {...css("table-container")}>
              <table>
                <thead><tr><th>Curve</th><th>Role</th><th>Representation</th><th>Nodes</th><th>State</th></tr></thead>
                <tbody>
                  {run.curves.map((curve) => (
                    <tr key={curve.id}>
                      <td><strong>{curve.name}</strong><br /><small>{curve.currency}</small></td>
                      <td><span {...css("tag")}>{curve.role}</span></td>
                      <td {...css("mono")}>{curve.parameterization}</td>
                      <td {...css("num")}>{curve.node_dates.length}</td>
                      <td {...css("mono", "curve-state")}>
                        {Object.entries(curve.parameters).map(([key, values]) => `${key}: [${values.map((value) => value.toPrecision(6)).join(", ")}]`).join(" · ")}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </section>

          {fit.length > 0 && <FitPlot rows={fit} />}

          <div {...css("matrix-grid")}>
            {run.jacobian && <MatrixHeatmap title="Forward Jacobian" matrix={run.jacobian} />}
            {run.effective_inverse && <MatrixHeatmap title="Effective inverse" matrix={run.effective_inverse} />}
          </div>

          {run.fx_forwards && (
            <section {...css("panel")}>
              <h2>FX forward curve</h2>
              <div {...css("fx-strip")}>
                {run.fx_forwards.dates.map((item, index) => (
                  <div key={item}><span>{item}</span><strong>{run.fx_forwards?.forwards[index].toFixed(8)}</strong></div>
                ))}
              </div>
            </section>
          )}

          {run.effective_inverse?.availability === "available" && (
            <QuoteBumpPanel runId={run.id} />
          )}
        </>
      )}
    </div>
  );
}
