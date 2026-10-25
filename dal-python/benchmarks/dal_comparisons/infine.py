"""Pinned native .NET worker; process startup and transport stay outside timing."""

import hashlib
import json
import os
import subprocess  # nosec B404
from pathlib import Path

from dal_benchmarks.harness import require

REVISION = "514ffe83b9ef8be0a29578c6f190b390267facf7"
SDK_VERSION = "8.0.426"
BUILD_SCHEMA = "dal.infine-build/1"
NATIVE_ROOT = Path(__file__).resolve().parents[1] / "infine"
ASSEMBLY = "Dal.Infine.Benchmarks.dll"
ENVIRONMENT = {
    "DOTNET_PROCESSOR_COUNT": "4",
    "DOTNET_TieredCompilation": "0",
    "DOTNET_CLI_TELEMETRY_OPTOUT": "1",
    "DOTNET_NOLOGO": "1",
}
CONVENTIONS = {
    "source_revision": REVISION,
    "execution": "direct C# CPU APIs, sequential Monte Carlo; optimized JIT, tiering disabled; no GPU",
    "boundary": "native timer includes output arrays; process startup, JSON and Python transport excluded; DAL Python conversion remains timed",
    "node_risk": "central finite differences on 42 prebuilt curves, 1e-6 decimal zero-rate bump",
    "sampling": "native Sobol, starts at point 1; no bridge or antithetics",
    "vanilla_greeks": "hand-written native reverse adjoint",
    "barrier_greeks": "seven fresh valuations, common-random-number central differences",
    "calibration": "native Brent bootstrap; staged forecast instrument delegates to native two-curve ParRate",
    "gsr_mc": "exact a=0 joint Gaussian state/integral kernels, stochastic discounting, two-dimensional Sobol",
    "gsr_analytic": "native a=0 Jamshidian; original gaussian1d_128 row identifies the payoff, not InFine quadrature or path counts",
}


def unsupported_reason(case):
    if case.get("kind") in ("bermudan", "american"):
        return "InFine LSM uses one path population and quadratic regression; no matching separate training/pricing cubic-policy API"
    if case["operation"] == "calibration" and case["kind"] not in (
        "single",
        "multi_staged",
    ):
        return "InFine has no matching simultaneous joint solver or native XCCY calibration instruments"
    return None


def method(case):
    op = case["operation"]
    if op == "mc_greeks":
        return (
            "Sobol + native hand-written reverse adjoint (sequential C#)"
            if case["kind"] == "vanilla"
            else "Sobol + central differences, common random numbers (sequential C#)"
        )
    return {
        "node_dv01": "central finite difference (42 prepared curves, 0.01bp zero-rate step)",
        "dv01": "central finite difference",
        "mc_price": "Sobol Monte Carlo (sequential C#)",
        "gsr_swaption_mc": "a=0 native HullWhiteStep state/integral and bond kernels, Sobol MC (sequential C#)",
        "gsr_swaption_gaussian1d": "a=0 native Jamshidian analytic swaption; no 128-point integration or Monte Carlo",
        "calibration": "native Brent log-linear bootstrap, tolerance 1e-12; staged two-curve ParRate wrapper",
        "prepared_pv": "passive, native prepared swaps (direct C#)",
        "gsr_swap_reused": "passive, native prepared swaps (direct C#)",
    }.get(op, "passive (direct C#)")


def fixture_case(case):
    from . import calibration_scenarios, scenarios

    reason = unsupported_reason(case)
    row = {"workload": case}
    if reason:
        row["unsupported"] = reason
        return row
    reference = scenarios.expected(case)
    bounds = scenarios.tolerance(case, "infine")
    row.update(
        reference=reference,
        method=method(case),
        tolerance=(
            list(bounds)
            if isinstance(bounds, (list, tuple))
            else [bounds] * len(reference)
        ),
    )
    if case["operation"] == "discount":
        row["queries"] = scenarios.queries(case["size"])
    elif case["operation"] == "calibration":
        row["queries"] = calibration_scenarios.queries(case["size"])
        row["quotes"] = calibration_scenarios.quotes(case["size"])
    elif case["name"].startswith("irs_"):
        row["trades"] = scenarios.trades(case["size"])
    return row


def fixtures(smoke, case_names=None):
    from . import gsr_scenarios, option_scenarios, scenarios
    from .evidence import SCHEMA

    rows = [fixture_case(case) for case in scenarios.cases(smoke)]
    if case_names:
        selected = set(case_names)
        rows = [row for row in rows if row["workload"]["name"] in selected]
    payload = {
        "schema": SCHEMA,
        "cases": rows,
        "rates": {
            "today": scenarios.TODAY,
            "dates": scenarios.NODES[1:],
            "dfs": scenarios.node_dfs()[1:],
            "bump": scenarios.BUMP,
            "node_bump": scenarios.NODE_BUMP,
        },
        "equity": {
            "spot": option_scenarios.SPOT,
            "vol": option_scenarios.VOL,
            "rate": option_scenarios.RATE,
            "dividend": option_scenarios.DIVIDEND,
            "strike": option_scenarios.STRIKE,
            "barrier": option_scenarios.BARRIER,
            "maturity": option_scenarios.YEARS,
            "bumps": list(option_scenarios.BUMPS),
        },
        "gsr": {
            "today": gsr_scenarios.TODAY,
            "expiry": gsr_scenarios.EXPIRY,
            "first_coupon": gsr_scenarios.FIRST_COUPON,
            "maturity": gsr_scenarios.MATURITY,
            "rate": gsr_scenarios.INPUT_RATE,
            "strike": gsr_scenarios.STRIKE,
            "vol": gsr_scenarios.VOLATILITY,
            "mean_reversion": 0,
        },
    }
    return json.loads(json.dumps(payload, default=lambda value: value.isoformat()))


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def adapter_hashes():
    return {
        p.name: sha256(p)
        for p in sorted(NATIVE_ROOT.iterdir())
        if p.suffix in (".cs", ".csproj") or p.name == "packages.lock.json"
    }


def binary_files(directory):
    return sorted(
        {
            *directory.glob("*.dll"),
            *directory.glob("*.deps.json"),
            *directory.glob("*.runtimeconfig.json"),
        }
    )


def load_build(build):
    require(
        build is not None,
        "InFine build missing; run prepare_infine.py and pass --infine-build",
    )
    build = Path(build).resolve()
    marker = build / "manifest.json"
    require(marker.is_file(), "InFine build manifest missing; run prepare_infine.py")
    manifest = json.loads(marker.read_text(encoding="utf-8"))
    require(
        manifest["schema"] == BUILD_SCHEMA and manifest["revision"] == REVISION,
        "InFine build revision/schema mismatch",
    )
    require(manifest["configuration"] == "Release", "InFine requires a Release build")
    require(
        manifest["adapter_hashes"] == adapter_hashes(),
        "InFine adapter changed; rebuild with prepare_infine.py",
    )
    files = {p.name: sha256(p) for p in binary_files(build / "bin")}
    require(
        ASSEMBLY in files and files == manifest["files"],
        "InFine binary identity changed",
    )
    runtime = {path: sha256(Path(path)) for path in manifest["runtime_files"]}
    require(
        runtime == manifest["runtime_files"], "InFine .NET runtime identity changed"
    )
    identities = {str(build / "bin" / name): value for name, value in files.items()}
    identities.update(runtime)
    identities[str(marker)] = sha256(marker)
    return manifest, identities


def measure_cases(args, payload=None):
    from .evidence import SCHEMA, source_hashes, write_json

    before, identities = load_build(args.infine_build)
    build = Path(args.infine_build).resolve()
    hashes = source_hashes()
    selected = payload if payload is not None else fixtures(args.smoke, getattr(args, "case", None))
    write_json(args.output_dir / "infine-input.json", selected)
    output = args.output_dir / "infine-native.json"
    command = [
        before["dotnet"],
        "exec",
        "--fx-version",
        before["runtime"],
        "--roll-forward",
        "Disable",
        str(build / "bin" / ASSEMBLY),
        str(args.output_dir / "infine-input.json"),
        str(output),
    ]
    with (args.output_dir / "infine-native.log").open("w", encoding="utf-8") as log:
        # The executable and payload come from the validated local build/fixtures.
        subprocess.run(  # nosec B603  # nosemgrep
            command,
            shell=False,
            check=True,
            stdout=log,
            stderr=subprocess.STDOUT,
            env=dict(os.environ, **ENVIRONMENT),
            timeout=args.timeout,
        )
    native = json.loads(output.read_text(encoding="utf-8"))
    require(
        native["schema"] == SCHEMA and native["backend"] == "infine",
        "invalid InFine native report",
    )
    after, after_identities = load_build(args.infine_build)
    require(
        before == after and identities == after_identities,
        "InFine provenance changed during execution",
    )
    require(
        hashes == source_hashes(), "comparison source changed during native execution"
    )
    return (
        native["results"],
        identities,
        {
            "revision": REVISION,
            "sdk": before["sdk"],
            "runtime": before["runtime"],
            "thread_environment": ENVIRONMENT,
        },
    )
