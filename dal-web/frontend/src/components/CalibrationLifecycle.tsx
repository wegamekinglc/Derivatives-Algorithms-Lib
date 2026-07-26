import { css } from "../format";

const PHASES = ["queued", "solving", "serializing", "persisting", "finished"] as const;

export default function CalibrationLifecycle({
  phase,
  failed,
}: {
  phase: string;
  failed: boolean;
}) {
  const current = Math.max(0, PHASES.indexOf(phase as typeof PHASES[number]));
  return (
    <ol {...css("lifecycle")} aria-label="Calibration lifecycle">
      {PHASES.map((item, index) => (
        <li
          key={item}
          {...css(
            index < current && "complete",
            index === current && (failed ? "failed" : "current"),
          )}
        >
          <span>{index + 1}</span>
          {item}
        </li>
      ))}
    </ol>
  );
}
