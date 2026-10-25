"""Native C++/.NET discount-query comparison tests."""

import json
import os
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

import pytest

from dal_comparisons import calibration_scenarios, infine, native_queries, scenarios


def test_native_fixture_exactly_matches_infine_inputs():
    shared = infine.fixtures(False, ["discount_queries"])
    native = native_queries.fixture(shared)
    case = shared["cases"][0]
    assert native["schema"] == "dal.native-query-fixture/1"
    assert native["dates"] == [shared["rates"]["today"], *shared["rates"]["dates"]]
    assert native["dfs"] == [1.0, *shared["rates"]["dfs"]]
    assert native["queries"] == case["queries"]
    assert native["reference"] == case["reference"]
    assert native["absolute_tolerance"] == 2e-12
    assert native["relative_tolerance"] == 1e-10
    assert native["workload"]["size"] == 4096


def test_native_arguments_validate_build_identity(tmp_path):
    root = tmp_path / "build"
    root.mkdir()
    (root / "CMakeCache.txt").write_text("CMAKE_BUILD_TYPE:STRING=Release\n")
    worker = root / "native_query_comparison"
    worker.touch()
    outside = tmp_path / "outside"
    outside.touch()
    infine_build = tmp_path / "infine"
    infine_build.mkdir()
    args = native_queries.arguments([
        "--native-worker", str(worker),
        "--native-build-root", str(root),
        "--infine-build", str(infine_build),
        "--output-dir", str(tmp_path / "out"),
    ])
    assert args.samples == 10 and args.rounds == 2
    assert args.native_worker == worker.resolve()
    with pytest.raises(ValueError, match="worker escaped native build root"):
        native_queries.arguments([
            "--native-worker", str(outside),
            "--native-build-root", str(root),
            "--infine-build", str(infine_build),
            "--output-dir", str(tmp_path / "other"),
        ])


@pytest.mark.parametrize("backend", ["dal", "quantlib"])
def test_native_cpp_worker_matches_shared_oracle(tmp_path, backend):
    worker = os.environ.get("DAL_NATIVE_QUERY_WORKER")
    if not worker:
        pytest.skip("set DAL_NATIVE_QUERY_WORKER to run C++ native integration tests")
    payload = native_queries.fixture()
    source = tmp_path / "fixture.json"
    output = tmp_path / "worker.json"
    source.write_text(json.dumps(payload))
    subprocess.run([
        worker, "--backend", backend, "--input", str(source), "--output", str(output)
    ], check=True, timeout=30, env=dict(os.environ, DAL_NUM_THREADS="1"))
    report = json.loads(output.read_text())
    assert report["schema"] == native_queries.WORKER_SCHEMA
    assert report["backend"] == backend
    assert len(report["samples_ns"]) == 1 and report["samples_ns"][0] > 0
    scenarios.validate(report["values"], payload["reference"], abs_tol=2e-12)


@pytest.mark.parametrize("backend", ["dal", "quantlib"])
def test_native_cpp_worker_enforces_shared_tolerance(tmp_path, backend):
    worker = os.environ.get("DAL_NATIVE_QUERY_WORKER")
    if not worker:
        pytest.skip("set DAL_NATIVE_QUERY_WORKER to run C++ native integration tests")
    payload = native_queries.fixture()
    source = tmp_path / "fixture.json"
    output = tmp_path / "worker.json"
    command = [worker, "--backend", backend, "--input", str(source), "--output", str(output)]
    environment = dict(os.environ, DAL_NUM_THREADS="1")
    source.write_text(json.dumps(payload))
    subprocess.run(command, check=True, timeout=30, env=environment)
    actual = json.loads(output.read_text())["values"]
    payload["reference"][0] = actual[0] + 1.01e-10
    source.write_text(json.dumps(payload))
    output.unlink()
    result = subprocess.run(command, capture_output=True, text=True, timeout=30, check=False, env=environment)
    assert result.returncode != 0
    assert "correctness failed" in result.stderr
    assert not output.exists()


def native_row(backend, payload):
    return {
        "schema": native_queries.WORKER_SCHEMAS[payload["workload"]["name"]],
        "backend": backend,
        "workload": payload["workload"],
        "method": native_queries.METHODS[payload["workload"]["name"]][backend],
        "samples_ns": [100],
        "values": list(payload["reference"]),
    }


def test_native_rotation_aggregation_and_boundary_summary():
    assert native_queries.backend_order(0, 0) == ["dal", "quantlib", "infine"]
    assert native_queries.backend_order(1, 0) == ["quantlib", "infine", "dal"]
    assert native_queries.backend_order(2, 0) == ["infine", "dal", "quantlib"]
    assert native_queries.backend_order(0, 1) == ["infine", "quantlib", "dal"]
    payload = native_queries.fixture()
    reports = {backend: [native_row(backend, payload)] for backend in native_queries.BACKENDS}
    row = native_queries.aggregate(reports)
    assert row["workload"]["name"] == "discount_queries"
    assert row["third_party_over_dal"] == {"quantlib": 1.0, "infine": 1.0}
    text = native_queries.summary({"status": "passed", "rounds": []})
    assert "DAL C++" in text and "QuantLib C++" in text and "InFine C#" in text
    assert "Python/JSON/IPC/startup are outside timing" in text


def test_native_worker_rejects_wrong_backend_values_and_timings():
    payload = native_queries.fixture()
    row = native_row("dal", payload)
    native_queries.check_worker(row, "dal", payload)
    for mutate in (
        lambda value: value.update(backend="quantlib"),
        lambda value: value.update(samples_ns=[]),
        lambda value: value.update(samples_ns=[0]),
        lambda value: value["values"].__setitem__(0, value["values"][0] + 1.0),
    ):
        changed = native_row("dal", payload)
        mutate(changed)
        with pytest.raises(ValueError):
            native_queries.check_worker(changed, "dal", payload)


def test_native_worker_rejects_binary_drift(tmp_path, monkeypatch):
    payload = native_queries.fixture()
    args = SimpleNamespace(native_worker=tmp_path / "worker", timeout=30)
    args.native_worker.touch()
    directory = tmp_path / "sample"
    directory.mkdir()
    hashes = iter(("before", "after"))
    monkeypatch.setattr(native_queries, "sha256", lambda path: next(hashes))

    def execute(command, **kwargs):
        output = Path(command[command.index("--output") + 1])
        assert output == directory / "worker.json"
        output.write_text(json.dumps(native_row("dal", payload)))

    monkeypatch.setattr(native_queries.subprocess, "run", execute)
    with pytest.raises(ValueError, match="native worker changed during execution"):
        native_queries.invoke_cpp(args, "dal", directory, payload)


def test_native_cli_help_does_not_import_python_backends():
    benchmarks = Path(__file__).resolve().parents[1]
    launcher = '''import importlib.abc
import runpy
import sys
from pathlib import Path
class Reject(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path=None, target=None):
        if fullname.split(".")[0] in ("dal", "QuantLib", "rateslib"):
            raise RuntimeError("Python backend imported")
sys.meta_path.insert(0, Reject())
root = Path(sys.argv.pop(1))
sys.path.insert(0, str(root))
sys.argv = [str(root / "run_native_queries.py"), "--help"]
runpy.run_path(sys.argv[0], run_name="__main__")
'''
    result = subprocess.run(
        [sys.executable, "-s", "-c", launcher, str(benchmarks)],
        capture_output=True, text=True, timeout=30, check=False,
    )
    assert result.returncode == 0, result.stderr
    assert "--native-worker" in result.stdout


def test_native_run_retains_workers_and_rechecks_cmake(tmp_path, monkeypatch):
    payload = native_queries.fixture()
    worker = tmp_path / "worker"
    worker.touch()
    args = SimpleNamespace(
        native_worker=worker,
        native_build_root=tmp_path,
        infine_build=tmp_path,
        output_dir=tmp_path / "out",
        samples=1,
        rounds=1,
        timeout=30,
    )
    monkeypatch.setattr(native_queries, "native_source_hashes", lambda: {"source": "hash"})
    monkeypatch.setattr(native_queries, "sha256", lambda path: "binary")
    monkeypatch.setattr(native_queries, "git_identity", lambda: {"revision": "test", "dirty": False})
    cmake_calls = []

    def cmake(build_root):
        cmake_calls.append(build_root)
        return {"CMAKE_BUILD_TYPE": "Release"}

    monkeypatch.setattr(native_queries, "cmake_identity", cmake)

    def cpp(_args, backend, _directory, _payload):
        row = native_row(backend, payload)
        row["identity"] = {"worker": backend}
        return row

    def managed(_args, _directory, _shared):
        row = native_row("infine", payload)
        row["identity"] = {"infine": "identity"}
        return row

    monkeypatch.setattr(native_queries, "invoke_cpp", cpp)
    monkeypatch.setattr(native_queries, "invoke_infine", managed)
    assert native_queries.run(args) == 0
    assert len(cmake_calls) == 2
    report = json.loads((args.output_dir / "results.json").read_text())
    workers = report["rounds"][0]["workers"]
    assert set(workers) == set(native_queries.BACKENDS)
    for backend in native_queries.BACKENDS:
        assert workers[backend][0]["values"] == payload["reference"]
        assert workers[backend][0]["samples_ns"] == [100]


def test_native_git_identity_ignores_only_dirty_submodules(tmp_path, monkeypatch):
    monkeypatch.setattr(native_queries, "REPO", tmp_path)
    (tmp_path / ".git").mkdir()
    commands = []

    def execute(command, **kwargs):
        commands.append(command)
        return "revision\n" if "rev-parse" in command else ""

    monkeypatch.setattr(native_queries.subprocess, "check_output", execute)
    identity = native_queries.git_identity()
    assert identity["revision"] == "revision"
    assert identity["submodule_policy"] == "ignore dirty worktrees"
    assert "--ignore-submodules=dirty" in commands[1]


@pytest.mark.parametrize(
    "case_name,kind,size",
    [
        ("calibration_single_5", "single", 5),
        ("calibration_single_15", "single", 15),
        ("calibration_multi_staged_5", "multi_staged", 5),
        ("calibration_multi_staged_15", "multi_staged", 15),
    ],
)
def test_native_calibration_fixture_and_case_arguments(tmp_path, case_name, kind, size):
    shared = infine.fixtures(False, [case_name])
    payload = native_queries.fixture(shared, case_name)
    case = shared["cases"][0]
    assert payload["schema"] == "dal.native-calibration-fixture/1"
    assert payload["workload"] == case["workload"]
    assert payload["dates"] == [value.isoformat() for value in calibration_scenarios.dates(size)]
    assert payload["queries"] == case["queries"]
    assert payload["quotes"] == case["quotes"]["usd_discount"]
    assert payload["reference"] == case["reference"]
    assert len(payload["reference"]) == (2 if kind == "single" else 4) * size
    assert payload["absolute_tolerance"] == 2e-8
    if kind == "multi_staged":
        assert payload["forward_quotes"] == case["quotes"]["usd_forward"]
    root = tmp_path / "build"
    root.mkdir()
    (root / "CMakeCache.txt").write_text("CMAKE_BUILD_TYPE:STRING=Release\n")
    worker = root / "worker"
    worker.touch()
    args = native_queries.arguments([
        "--native-worker", str(worker), "--native-build-root", str(root),
        "--infine-build", str(tmp_path), "--output-dir", str(tmp_path / "out"),
        "--case", case_name,
    ])
    assert args.case == case_name
    assert native_queries.SCHEMAS[case_name] == "dal.native-calibration-comparison/1"


@pytest.mark.parametrize("backend", ["dal", "quantlib"])
@pytest.mark.parametrize(
    "case_name,expected_width",
    [
        ("calibration_single_5", 10),
        ("calibration_single_15", 30),
        ("calibration_multi_staged_5", 20),
        ("calibration_multi_staged_15", 60),
    ],
)
def test_native_cpp_calibration_matches_shared_oracle(tmp_path, backend, case_name, expected_width):
    worker = os.environ.get("DAL_NATIVE_QUERY_WORKER")
    if not worker:
        pytest.skip("set DAL_NATIVE_QUERY_WORKER to run C++ native integration tests")
    payload = native_queries.fixture(case_name=case_name)
    source, output = tmp_path / "fixture.json", tmp_path / "worker.json"
    source.write_text(json.dumps(payload))
    subprocess.run([
        worker, "--backend", backend, "--input", str(source), "--output", str(output),
        "--case", case_name,
    ], check=True, timeout=30, env=dict(os.environ, DAL_NUM_THREADS="1"))
    report = json.loads(output.read_text())
    assert report["schema"] == "dal.native-calibration-worker/1"
    assert report["workload"] == payload["workload"]
    assert len(report["samples_ns"]) == 1 and report["samples_ns"][0] > 0
    assert len(report["values"]) == expected_width
    scenarios.validate(report["values"], payload["reference"], abs_tol=2e-8)
