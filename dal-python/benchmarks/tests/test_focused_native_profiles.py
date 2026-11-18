"""Opt-in preflights for build-tree benchmark drivers, without timing thresholds."""

import json
import os
from pathlib import Path
import subprocess

import pytest


def driver(variable):
    value = os.environ.get(variable)
    if not value:
        pytest.skip(f"set {variable} to the build-tree executable")
    path = Path(value).resolve()
    assert path.is_file()
    return path


def run(path, *args):
    return subprocess.run(
        [str(path), *args],
        capture_output=True,
        text=True,
        env={**os.environ, "DAL_NUM_THREADS": "1"},
        timeout=90,
        check=False,
    )


@pytest.mark.parametrize("case,dimension", [("small", 64), ("monthly-asian", 69)])
def test_market_stage_preflight_and_complete_inner_samples(case, dimension):
    result = run(
        driver("DAL_MARKET_IV_PROFILE_BINARY"),
        "--market-iv-local-vol",
        case,
        "stages",
        "64",
    )
    assert result.returncode == 0, result.stdout + result.stderr
    rows = [
        json.loads(line.removeprefix("PROFILE_SAMPLES "))
        for line in result.stdout.splitlines()
        if line.startswith("PROFILE_SAMPLES ")
    ]
    assert {row["case"] for row in rows} == {"reallocate_only", "init_frozen_values"}
    assert len(rows) == 2
    for row in rows:
        assert row["inner_loops"] in {2**i for i in range(15)}
        assert len(row["samples_ns"]) == len(row["per_iteration_ns"]) == 10
        assert all(value > 0 for value in row["samples_ns"])
        assert row["dimension"] == dimension
    assert "after_cleanup_bytes=0" in result.stdout
    assert "fixed_normals=verified" in result.stdout


def test_native_identity_fixture_and_inspection_samples():
    path = driver("DAL_MARKET_IV_PROFILE_BINARY")
    identities = []
    for count in (0, 1, 365):
        result = run(
            path, "--market-iv-local-vol", "identity", "inspection", "64", str(count)
        )
        assert result.returncode == 0, result.stdout + result.stderr
        row = next(
            json.loads(line.removeprefix("PROFILE_SAMPLES "))
            for line in result.stdout.splitlines()
            if line.startswith("PROFILE_SAMPLES ")
        )
        assert row["case"] == f"inspection_fixings_{count}"
        assert len(row["samples_ns"]) == 10
        identity = next(
            line.partition("=")[2]
            for line in result.stdout.splitlines()
            if line.startswith("request=")
        )
        assert identity.startswith("sha256:") and len(identity) == 71
        identities.append(identity)
        assert "unit_payment=verified" in result.stdout
    assert len(set(identities)) == 3


@pytest.mark.parametrize(
    "args",
    [
        ["identity", "inspection", "64", "2"],
        ["identity", "inspection", "64", "-1"],
        ["identity", "prepare", "64"],
        ["large", "stages", "64"],
        ["small", "steady", "64", "365"],
    ],
)
def test_market_new_selectors_reject_invalid_combinations(args):
    result = run(driver("DAL_MARKET_IV_PROFILE_BINARY"), "--market-iv-local-vol", *args)
    assert result.returncode == 2


@pytest.mark.parametrize("mode,count", [("None", 30), ("Lockout", 1825)])
def test_ois_selector_validates_projected_shapes(mode, count):
    result = run(
        driver("DAL_OIS_AUDIT_PROFILE_BINARY"), "--mode", mode, "--days", str(count)
    )
    assert result.returncode == 0, result.stdout + result.stderr
    for columns in (22, 26):
        assert f"rows={count + 1} columns={columns}" in result.stdout
    assert f"{mode}_D{count} owning_evaluation_end_to_end" in result.stdout


def test_ois_selector_rejects_unsupported_boundary():
    result = run(
        driver("DAL_OIS_AUDIT_PROFILE_BINARY"), "--mode", "None", "--days", "365"
    )
    assert result.returncode == 1
