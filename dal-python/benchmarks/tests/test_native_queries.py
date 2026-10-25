"""Native C++/.NET discount-query comparison tests."""

import json
import os
import subprocess

import pytest

from dal_comparisons import infine, native_queries, scenarios


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
