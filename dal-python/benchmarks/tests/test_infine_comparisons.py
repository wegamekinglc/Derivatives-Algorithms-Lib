"""Native InFine execution, capability and immutable-evidence contracts."""

import json
import os
from pathlib import Path
import subprocess
from types import SimpleNamespace

import pytest

from dal_comparisons.evidence import SCHEMA, check_row, source_hashes
from dal_comparisons.scenarios import (
    cases,
    expected,
    method,
    tolerance,
    unsupported_reason,
)
from dal_comparisons.suite import BACKENDS


def test_infine_covers_the_full_inventory_with_explicit_capabilities():
    assert BACKENDS == ("dal", "quantlib", "rateslib")
    supported = [c for c in cases() if unsupported_reason("infine", c) is None]
    assert len(supported) == 29
    unsupported = [c for c in cases() if unsupported_reason("infine", c)]
    assert len(unsupported) == 14
    assert {c["kind"] for c in unsupported} == {
        "bermudan",
        "american",
        "multi_joint",
        "xccy_staged",
        "xccy_joint",
    }


def test_infine_fixtures_preserve_smoke_full_sizes_and_oracle_bounds():
    from dal_comparisons.infine import fixtures

    for smoke in (True, False):
        payload = fixtures(smoke)
        inventory = cases(smoke)
        assert [row["workload"] for row in payload["cases"]] == inventory
        for row, case in zip(payload["cases"], inventory):
            reason = unsupported_reason("infine", case)
            if reason:
                assert row == dict(workload=case, unsupported=reason)
            else:
                assert row["reference"] == expected(case)
                bounds = tolerance(case, "infine")
                bounds = (
                    bounds
                    if isinstance(bounds, (list, tuple))
                    else [bounds] * len(row["reference"])
                )
                assert row["tolerance"] == list(bounds)
                assert row["method"] == method("infine", case)
        assert payload["equity"]["bumps"] == [1.0, 0.01, 0.01]
        assert payload["gsr"]["mean_reversion"] == 0


def test_infine_methods_identify_algorithm_and_runtime_differences():
    inventory = {c["name"]: c for c in cases()}
    assert "finite difference" in method("infine", inventory["irs_node_dv01_256"])
    assert "adjoint" in method("infine", inventory["mc_vanilla_greeks_65536"])
    assert "sequential" in method("infine", inventory["mc_barrier_price_65536"])
    assert "Jamshidian" in method("infine", inventory["gsr_swaption_gaussian1d_128"])
    assert tolerance(inventory["gsr_swaption_gaussian1d_128"], "infine") == 5e-6


def test_shared_conventions_include_infine_estimators_and_stream():
    from dal_comparisons.scenarios import CONVENTIONS

    assert CONVENTIONS["node_risk_methods"]["infine"] == "central finite difference"
    assert (
        "InFine vanilla: hand-written reverse adjoint"
        in CONVENTIONS["monte_carlo"]["methods"]
    )
    assert "InFine point 1" in CONVENTIONS["monte_carlo"]["sampling"]


def test_native_adapter_sources_are_hashed():
    hashes = source_hashes()
    for name in (
        "Program.cs",
        "Rates.cs",
        "MonteCarlo.cs",
        "Gsr.cs",
        "Dal.Infine.Benchmarks.csproj",
        "packages.lock.json",
    ):
        assert f"infine/{name}" in hashes
    assert "prepare_infine.py" in hashes


def test_infine_supported_failure_cannot_be_relabelled_unsupported():
    case = cases(smoke=True)[0]
    row = dict(
        name=case["name"], workload=case, status="unsupported", reason="dotnet missing"
    )
    with pytest.raises(ValueError, match="worker case failed"):
        check_row(row, case, "infine")


@pytest.fixture
def native_build(tmp_path):
    from dal_comparisons import infine

    root = tmp_path / "native"
    (root / "bin").mkdir(parents=True)
    (root / "bin" / infine.ASSEMBLY).write_bytes(b"native worker")
    runtime = root / "dotnet"
    runtime.write_bytes(b"runtime")
    manifest = dict(
        schema=infine.BUILD_SCHEMA,
        revision=infine.REVISION,
        configuration="Release",
        adapter_hashes=infine.adapter_hashes(),
        dotnet=str(runtime),
        sdk=infine.SDK_VERSION,
        runtime="8.0.32",
        runtime_files={str(runtime): infine.sha256(runtime)},
        files={infine.ASSEMBLY: infine.sha256(root / "bin" / infine.ASSEMBLY)},
    )
    (root / "manifest.json").write_text(json.dumps(manifest))
    return root, manifest


@pytest.mark.parametrize(
    "field,value,error",
    [
        ("revision", "wrong", "revision"),
        ("configuration", "Debug", "Release"),
        ("adapter_hashes", {}, "adapter changed"),
        ("files", {}, "binary identity"),
    ],
)
def test_native_build_rejects_stale_or_wrong_provenance(
    native_build, field, value, error
):
    from dal_comparisons.infine import load_build

    root, manifest = native_build
    load_build(root)
    manifest[field] = value
    (root / "manifest.json").write_text(json.dumps(manifest))
    with pytest.raises(ValueError, match=error):
        load_build(root)


@pytest.mark.parametrize(
    "filename,error",
    [("bin/Dal.Infine.Benchmarks.dll", "binary"), ("dotnet", "runtime")],
)
def test_native_build_rejects_binary_or_runtime_mutation(native_build, filename, error):
    from dal_comparisons.infine import load_build

    root, _ = native_build
    (root / filename).write_bytes(b"changed")
    with pytest.raises(ValueError, match=error):
        load_build(root)


def test_native_build_requires_explicit_available_dependency(tmp_path):
    from dal_comparisons.infine import load_build

    for build in (None, tmp_path):
        with pytest.raises(ValueError, match="InFine build"):
            load_build(build)


def test_runtime_identity_handles_spaces_and_ignores_preview_runtimes(
    tmp_path, monkeypatch
):
    import prepare_infine

    root = tmp_path / "dot net with spaces"
    runtime = root / "shared/Microsoft.NETCore.App/8.0.32"
    runtime.mkdir(parents=True)
    executable = root / "dotnet"
    executable.write_bytes(b"host")
    library = runtime / "libclrjit.dylib"
    library.write_bytes(b"JIT")
    output = f"Microsoft.NETCore.App 8.0.32 [{runtime.parent}]\nMicrosoft.NETCore.App 8.0.99-preview.1 [{runtime.parent}]\n"
    monkeypatch.setattr(
        prepare_infine.subprocess, "check_output", lambda *_args, **_kwargs: output
    )
    version, files = prepare_infine.runtime_files(executable)
    assert version == "8.0.32"
    assert str(executable) in files
    assert str(library) in files


def test_native_process_timing_is_retained_without_python_transport(
    native_build, tmp_path, monkeypatch
):
    from dal_comparisons import infine

    root, _ = native_build
    output = tmp_path / "worker"
    output.mkdir()
    args = SimpleNamespace(infine_build=root, output_dir=output, smoke=True, timeout=30)
    samples = [{"samples_ns": [123]}]

    def execute(command, **kwargs):
        assert command[:7] == [
            str(root / "dotnet"),
            "exec",
            "--fx-version",
            "8.0.32",
            "--roll-forward",
            "Disable",
            str(root / "bin" / infine.ASSEMBLY),
        ]
        assert kwargs["shell"] is False
        assert kwargs["env"]["DOTNET_TieredCompilation"] == "0"
        Path(command[-1]).write_text(
            json.dumps(dict(schema=SCHEMA, backend="infine", results=samples))
        )

    monkeypatch.setattr(infine.subprocess, "run", execute)
    rows, _, metadata = infine.measure_cases(args)
    assert rows == samples
    assert metadata["revision"] == infine.REVISION


@pytest.mark.parametrize("smoke", [True, False])
def test_native_worker_matches_all_supported_oracles(tmp_path, smoke):
    from dal_comparisons import infine
    from dal_comparisons.evidence import check_report
    from dal_comparisons.scenarios import CONVENTIONS

    build = os.environ.get("DAL_INFINE_BUILD")
    if not build:
        pytest.skip("set DAL_INFINE_BUILD to run native .NET integration tests")
    args = SimpleNamespace(
        infine_build=build, output_dir=tmp_path, smoke=smoke, case=[], timeout=60
    )
    rows, _, metadata = infine.measure_cases(args)
    report = {
        "schema": SCHEMA,
        "status": "passed",
        "backend": "infine",
        "smoke": smoke,
        "conventions": CONVENTIONS,
        "source_hashes": source_hashes(),
        "results": rows,
    }
    check_report(report, "infine", smoke, source_hashes())
    assert metadata["revision"] == infine.REVISION
    assert len([r for r in report["results"] if r["status"] == "passed"]) == 29
    assert len([r for r in report["results"] if r["status"] == "unsupported"]) == 14


def test_native_oracle_failure_exits_nonzero_without_success_report(tmp_path):
    from dal_comparisons import infine

    build = os.environ.get("DAL_INFINE_BUILD")
    if not build:
        pytest.skip("set DAL_INFINE_BUILD to run native .NET integration tests")
    manifest, _ = infine.load_build(build)
    payload = infine.fixtures(True)
    payload["cases"][0]["reference"][0] += 1
    fixture = tmp_path / "input.json"
    fixture.write_text(json.dumps(payload))
    output = tmp_path / "native.json"
    result = subprocess.run(
        [
            manifest["dotnet"],
            "exec",
            "--fx-version",
            manifest["runtime"],
            "--roll-forward",
            "Disable",
            str(Path(build) / "bin" / infine.ASSEMBLY),
            str(fixture),
            str(output),
        ],
        capture_output=True,
        text=True,
        check=False,
        timeout=30,
        env=dict(os.environ, **infine.ENVIRONMENT),
    )
    assert result.returncode == 1
    assert "discount_queries" in result.stderr
    assert not output.exists()


def test_infine_fixtures_respect_case_filter():
    from dal_comparisons.infine import fixtures

    payload = fixtures(False, ["discount_queries"])
    assert [row["workload"]["name"] for row in payload["cases"]] == ["discount_queries"]
    assert payload["rates"]["dates"]
    assert payload["schema"] == "dal.python-comparisons/8"


def test_infine_empty_case_selection_preserves_default_fixtures():
    from dal_comparisons import scenarios
    from dal_comparisons.infine import fixtures

    payload = fixtures(True, [])
    assert [row["workload"]["name"] for row in payload["cases"]] == [
        case["name"] for case in scenarios.cases(True)
    ]
