import { useMemo, useState } from "react";
import { useNavigate } from "react-router-dom";
import {
  api,
  ApiClientError,
  type CalibrationKind,
} from "../api/client";
import { calibrationExamples } from "../curves/examples";
import { locateCalibrationField, type LocatedField } from "../curves/visualization";
import { css } from "../format";

const MODES: { value: CalibrationKind; label: string; note: string }[] = [
  { value: "single", label: "Single", note: "One discount or projection curve" },
  { value: "xccy_staged", label: "Staged XCCY", note: "Basis over persisted curve blocks" },
  { value: "xccy_joint", label: "Joint XCCY", note: "Domestic, foreign and basis in one solve" },
];

function formattedExample(kind: CalibrationKind): string {
  switch (kind) {
  case "single":
    return JSON.stringify(calibrationExamples.single, null, 2);
  case "xccy_staged":
    return JSON.stringify(calibrationExamples.xccy_staged, null, 2);
  case "xccy_joint":
    return JSON.stringify(calibrationExamples.xccy_joint, null, 2);
  }
}

interface CalibrationErrorDetail {
  location?: (string | number)[] | null;
  message?: string;
  msg?: string;
}

interface PresentedCalibrationError {
  message: string;
  located: LocatedField | null;
}

function calibrationErrorDetail(value: unknown): CalibrationErrorDetail {
  return typeof value === "object" && value !== null
    ? value as CalibrationErrorDetail
    : {};
}

function presentCalibrationError(reason: unknown): PresentedCalibrationError {
  if (!(reason instanceof ApiClientError)) {
    return { message: String(reason), located: null };
  }
  const detail = calibrationErrorDetail(reason.detail);
  return {
    message: detail.message ?? detail.msg ?? reason.message,
    located: detail.location ? locateCalibrationField(detail.location) : null,
  };
}

function submitCalibrationRequest(
  mode: CalibrationKind,
  source: string,
  onInvalidJson: (message: string) => void,
  onStarted: () => void,
  onCompleted: (runId: string) => void,
  onFailed: (error: PresentedCalibrationError) => void,
  onFinished: () => void,
): void {
  let body: unknown;
  try {
    body = JSON.parse(source);
  } catch (reason) {
    onInvalidJson(`Invalid JSON: ${String(reason)}`);
    return;
  }
  onStarted();
  void api.submitCalibration(mode, body)
    .then((run) => {
      onCompleted(run.id);
    })
    .catch((reason: unknown) => {
      onFailed(presentCalibrationError(reason));
    })
    .finally(onFinished);
}

export default function Curves() {
  const navigate = useNavigate();
  const [mode, setMode] = useState<CalibrationKind>("single");
  const [source, setSource] = useState(() => formattedExample("single"));
  const [error, setError] = useState<string | null>(null);
  const [located, setLocated] = useState<LocatedField | null>(null);
  const [submitting, setSubmitting] = useState(false);
  const lines = useMemo(() => source.split("\n").length, [source]);

  return (
    <div {...css("curve-workbench")}>
      <div {...css("page-header")}>
        <div>
          <span {...css("eyebrow")}>CALIBRATION / RISK</span>
          <h1>Curve Workbench</h1>
          <p>Author a deterministic solve, then inspect fit, matrices and persisted reconstruction state.</p>
        </div>
        <div {...css("workbench-stat")}>
          <strong>{lines}</strong>
          <span>request lines</span>
        </div>
      </div>

      <div {...css("mode-tabs")} role="tablist" aria-label="Calibration mode">
        {MODES.map((item, index) => (
          <button
            type="button"
            role="tab"
            aria-selected={mode === item.value}
            {...css("mode-tab", mode === item.value && "active")}
            key={item.value}
            onClick={() => {
              setMode(item.value);
              setSource(formattedExample(item.value));
              setError(null);
              setLocated(null);
            }}
          >
            <span>0{index + 1}</span>
            <strong>{item.label}</strong>
            <small>{item.note}</small>
          </button>
        ))}
      </div>

      <div {...css("workbench-layout")}>
        <aside {...css("workbench-outline")}>
          <h2>Request anatomy</h2>
          {["Declaration & representation", "Knots & initial seeds", "Instruments & conventions", "Solver & matrix settings"].map((item, index) => (
            <div key={item} {...css("outline-row", located?.row === index && "has-error")}>
              <span>{index + 1}</span>
              <p>{item}</p>
            </div>
          ))}
          <div {...css("contract-note")}>
            <strong>Native boundary</strong>
            <p>Validation, planning and calibration are executed by the compiled DAL gateway.</p>
          </div>
        </aside>

        <section {...css("request-editor")}>
          <div {...css("editor-heading")}>
            <div>
              <span {...css("tag")}>{mode.replace("_", " ")}</span>
              <strong>JSON contract editor</strong>
            </div>
            <button
              type="button"
              {...css("ghost")}
              onClick={() => {
                setSource(formattedExample(mode));
              }}
            >
              Reset example
            </button>
          </div>
          <label>
            <span>Calibration request JSON</span>
            <textarea
              value={source}
              spellCheck={false}
              rows={Math.max(24, Math.min(44, lines))}
              onChange={(event) => {
                setSource(event.target.value);
                setError(null);
                setLocated(null);
              }}
            />
          </label>
          {error && (
            <div {...css("error", "editor-error")}>
              <strong>{located ? `${located.section} · ${located.field}` : "Request error"}</strong>
              <span>{error}</span>
            </div>
          )}
          <div {...css("submit-row")}>
            <p>202 creates an immutable run; this page never performs calibration math.</p>
            <button
              type="button"
              disabled={submitting}
              onClick={() => {
                submitCalibrationRequest(
                  mode,
                  source,
                  setError,
                  () => {
                    setSubmitting(true);
                    setError(null);
                  },
                  (runId) => {
                    navigate(`/curves/runs/${runId}`);
                  },
                  (presented) => {
                    setLocated(presented.located);
                    setError(presented.message);
                  },
                  () => {
                    setSubmitting(false);
                  },
                );
              }}
            >
              {submitting ? "Submitting…" : "Run calibration"}
            </button>
          </div>
        </section>
      </div>
    </div>
  );
}
