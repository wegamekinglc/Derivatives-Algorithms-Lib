import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { api } from "../../src/api/client";
import CurveLabWorkspace from "../../src/components/CurveLabWorkspace";

describe("Curve Lab V2 workspace", () => {
  it("uses visual curve and instrument controls as the primary authoring surface", () => {
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);

    render(<CurveLabWorkspace />);

    expect(screen.getByRole("heading", { name: "Curve topology" })).not.toBeNull();
    expect(screen.getByRole("heading", { name: "Calibration instruments" })).not.toBeNull();
    expect(screen.getByLabelText("As-of date")).not.toBeNull();
    expect(screen.getByLabelText("Quote 1")).not.toBeNull();
    expect(screen.getByRole("button", { name: "Add instrument" })).not.toBeNull();
    const advanced = screen.getByText("Advanced JSON").closest("details");
    expect(advanced?.open).toBe(false);
  });

  it("builds, publishes, and exposes all four durable workflow tabs", async () => {
    const draft = {
      id: "a".repeat(32),
      schema_version: 2 as const,
      revision: 1,
      fingerprint: "b".repeat(64),
      state: "READY_TO_BUILD" as const,
      document: {},
      created_at: "2026-01-15T00:00:00Z",
      updated_at: "2026-01-15T00:00:00Z",
    };
    const build = {
      id: "c".repeat(32),
      draft_id: draft.id,
      draft_revision: 1,
      draft_fingerprint: draft.fingerprint,
      state: "SUCCEEDED",
      stale: false,
      request: {},
      resolved_plan: { mode: "SINGLE" },
      quote_axis: [],
      parameter_axis: [],
      dependency_manifest: [],
      diagnostics: { fit_state: "NATIVE_ARCHIVE_VALIDATED" },
      native_payload_hash: "d".repeat(64),
      error: null,
      created_at: "2026-01-15T00:00:00Z",
      finished_at: "2026-01-15T00:00:01Z",
    };
    const version = {
      id: "e".repeat(32),
      source_kind: "BUILD" as const,
      build_run_id: build.id,
      import_job_id: null,
      name: "USD OIS",
      version_note: null,
      tags: [],
      native_payload_length: 10,
      native_payload_hash: "f".repeat(64),
      root_kind: "DISCOUNT_CURVE" as const,
      build_validation_state: "VERIFIED" as const,
      visibility_state: "VISIBLE" as const,
      created_at: "2026-01-15T00:00:02Z",
    };
    vi.spyOn(api, "listCurveLabVersions").mockResolvedValue([]);
    vi.spyOn(api, "createCurveLabDraft").mockResolvedValue(draft);
    vi.spyOn(api, "createCurveLabBuildRun").mockResolvedValue(build);
    vi.spyOn(api, "createCurveLabVersion").mockResolvedValue(version);

    render(<CurveLabWorkspace />);

    for (const name of ["Build", "Runs", "Pricing & Risk", "Versions"]) {
      expect(screen.getByRole("tab", { name })).not.toBeNull();
    }
    fireEvent.click(screen.getByRole("button", { name: "Create draft" }));
    await waitFor(() => expect(api.createCurveLabDraft).toHaveBeenCalledOnce());
    fireEvent.click(screen.getByRole("button", { name: "Build curve" }));
    await waitFor(() => expect(api.createCurveLabBuildRun).toHaveBeenCalledWith(draft.id));
    fireEvent.click(screen.getByRole("tab", { name: "Build" }));
    fireEvent.change(screen.getByLabelText("Version name"), {
      target: { value: "USD OIS" },
    });
    fireEvent.click(screen.getByRole("button", { name: "Publish version" }));
    await waitFor(() => expect(api.createCurveLabVersion).toHaveBeenCalledWith(
      expect.objectContaining({
        draft_id: draft.id,
        build_run_id: build.id,
        name: "USD OIS",
      }),
    ));
    expect(screen.getByText("Published USD OIS")).not.toBeNull();
  });
});
