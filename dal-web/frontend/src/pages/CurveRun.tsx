import {
  type Dispatch,
  type SetStateAction,
  useEffect,
  useMemo,
  useState,
} from "react";
import { Link, useParams } from "react-router-dom";
import { api, type CalibrationRun } from "../api/client";
import CalibrationLifecycle from "../components/CalibrationLifecycle";
import FitPlot from "../components/FitPlot";
import MatrixHeatmap from "../components/MatrixHeatmap";
import QuoteBumpPanel from "../components/QuoteBumpPanel";
import { alignFitSeries } from "../curves/visualization";
import { css } from "../format";

interface PollControl {
  cancelled: boolean;
  timer: ReturnType<typeof setTimeout> | null;
  setRun: Dispatch<SetStateAction<CalibrationRun | null>>;
  setError: Dispatch<SetStateAction<string | null>>;
}

function pollCalibrationRun(
  runId: string,
  control: PollControl,
): void {
  void api.getCalibration(runId)
    .then((next) => {
      if (control.cancelled) return;
      control.setRun(next);
      control.setError(null);
      if (next.status === "running") {
        control.timer = setTimeout(() => {
          pollCalibrationRun(runId, control);
        }, 300);
      }
    })
    .catch((reason: unknown) => {
      if (!control.cancelled) control.setError(String(reason));
    });
}

function useCalibrationRun(runId: string) {
  const [run, setRun] = useState<CalibrationRun | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const control: PollControl = {
      cancelled: false,
      timer: null,
      setRun,
      setError,
    };
    pollCalibrationRun(runId, control);
    return () => {
      control.cancelled = true;
      if (control.timer) clearTimeout(control.timer);
    };
  }, [runId]);
  return { run, error };
}

function CurveRunHeader({ run }: { run: CalibrationRun }) {
  return (
    <div {...css("run-header")}>
      <div>
        <Link to="/curves" {...css("back-link")}>← Curve workbench</Link>
        <span {...css("eyebrow")}>{run.kind.split("_").join(" ")} / {run.id.slice(0, 8)}</span>
        <h1>{run.name}</h1>
      </div>
      <span {...css("run-status", run.status)}>{run.status}</span>
    </div>
  );
}

function RunningPanel({ run }: { run: CalibrationRun }) {
  return (
    <section {...css("panel", "running-panel")}>
      <span {...css("spinner")} aria-hidden="true" />
      <div>
        <h2>Native solve in progress</h2>
        <p {...css("muted")}>Polling persisted phase: <code>{run.phase}</code></p>
      </div>
    </section>
  );
}

function failedRunDetails(error: CalibrationRun["error"]) {
  if (!error) {
    return {
      code: "CALIBRATION_FAILED",
      message: "Calibration failed",
      location: null,
    };
  }
  return error;
}

function FailedPanel({ run }: { run: CalibrationRun }) {
  const error = failedRunDetails(run.error);
  return (
    <section {...css("panel", "failed-panel")}>
      <span {...css("eyebrow")}>{error.code}</span>
      <h2>{error.message}</h2>
      {error.location && <code>{error.location.join(" › ")}</code>}
    </section>
  );
}

function RunMetrics({ run }: { run: CalibrationRun }) {
  return (
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
  );
}

function PersistedCurves({ run }: { run: CalibrationRun }) {
  return (
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
  );
}

function FxForwardCurve({ run }: { run: CalibrationRun }) {
  const forwards = run.fx_forwards;
  if (!forwards) return null;
  const forwardValues = forwards.forwards[Symbol.iterator]();
  return (
    <section {...css("panel")}>
      <h2>FX forward curve</h2>
      <div {...css("fx-strip")}>
        {forwards.dates.map((item) => (
          <div key={item}><span>{item}</span><strong>{forwardValues.next().value?.toFixed(8)}</strong></div>
        ))}
      </div>
    </section>
  );
}

function CompletedRun({ run, fit }: { run: CalibrationRun; fit: ReturnType<typeof alignFitSeries> }) {
  return (
    <>
      <RunMetrics run={run} />
      <PersistedCurves run={run} />
      {fit.length > 0 && <FitPlot rows={fit} />}
      <div {...css("matrix-grid")}>
        {run.jacobian && <MatrixHeatmap title="Forward Jacobian" matrix={run.jacobian} />}
        {run.effective_inverse && <MatrixHeatmap title="Effective inverse" matrix={run.effective_inverse} />}
      </div>
      <FxForwardCurve run={run} />
      {run.effective_inverse?.availability === "available" && <QuoteBumpPanel runId={run.id} />}
    </>
  );
}

export default function CurveRun() {
  const { runId = "" } = useParams();
  const { run, error } = useCalibrationRun(runId);

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
      <CurveRunHeader run={run} />
      <CalibrationLifecycle phase={run.phase} failed={run.status === "failed"} />

      {run.status === "running" && <RunningPanel run={run} />}
      {run.status === "failed" && <FailedPanel run={run} />}
      {run.status === "completed" && <CompletedRun run={run} fit={fit} />}
    </div>
  );
}
