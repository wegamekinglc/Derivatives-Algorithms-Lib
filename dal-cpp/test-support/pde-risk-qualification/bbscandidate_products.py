import argparse
import gzip
import json
import math
import pathlib

import audit
from bbscandidate import native_request, price_id
from campaign import controls, engine
from fixtures import fixtures
from qualify import differences, digest, limits, normalized, within, write_json
from referenceplan import binding, scales

PREVIOUS = pathlib.Path(".codex/artifacts/validation/dal502-american-directions")
ORDER = ["Delta", "Gamma", "Vega", "Rate[0]", "DividendYield[0]"]


def finite_vector(vector):
    return len(vector) == 6 and all(v is not None and math.isfinite(v) for v in vector)


def candidate_error(vector, reference, smooth):
    if not smooth or not finite_vector(vector):
        return dict(
            status="FAIL", admitted=False, reason="mandatory smooth vector unavailable"
        )
    if reference.get("status") != "PASS":
        return dict(
            status="INCOMPLETE",
            admitted=False,
            reason="actual binding not self-qualified",
        )
    expected = reference["normalized"]
    if not finite_vector(expected):
        return dict(
            status="INCOMPLETE",
            admitted=False,
            reason="reference vector truncated/nonfinite",
        )
    diff = differences(vector, expected)
    return dict(
        status="PASS" if within(diff) else "FAIL",
        admitted=True,
        difference=diff,
        hard_limits=limits(6),
        reference=expected,
        maximum_hard_limit_ratio=max(d / t for d, t in zip(diff, limits(6))),
    )


def read_capture(name, request, source):
    if source["origin"] != "current":
        raise RuntimeError("candidate cannot relabel historical product capture")
    for record in source["files"].values():
        check_file(record)
    input_path = pathlib.Path(source["files"][".input.json"]["path"])
    if json.loads(input_path.read_text()) != request:
        raise RuntimeError(
            "reviewed product input differs from frozen fixture: " + name
        )
    path = pathlib.Path(source["files"][".json.gz"]["path"])
    with gzip.open(path, "rt") as stream:
        output = json.load(stream)
    for record in source["files"].values():
        check_file(record)
    return output


def check_file(record):
    path = pathlib.Path(record["path"])
    if (
        str(path.resolve(strict=True)) != record["path"]
        or digest(path) != record["sha256"]
    ):
        raise RuntimeError("actual product source/payload changed: " + str(path))


def smooth_conditions(output, request):
    risks = output["risks"]
    expected = scales(request)
    sizes = [expected[0][0], expected[0][0]] + expected[0][1:] + [expected[0][2]]
    checks = [
        output["status"] == "COMPLETE",
        finite_vector(normalized(output)),
        [r["riskId_"] for r in risks] == ORDER,
        all(r["status_"] == "AVAILABLE" for r in risks),
    ]
    checks += [risk_scales(r, h) for r, h in zip(risks, sizes)]
    checks += [
        s["status_"] == "SUCCESS" and s["solverStatus_"] == "SUCCESS"
        for s in output["scenarios"]
    ]
    return checks


def risk_scales(risk, coarse):
    return risk["coarseBump_"] == coarse and risk["fineBump_"] == coarse / 2


def product_case(name, request, source, proof):
    output = read_capture(name, request, source)
    checks = {}

    def check(key, conditions, evidence):
        checks[key] = dict(
            status="PASS" if all(conditions) else "FAIL", evidence=evidence
        )

    audit.general(check, {name: output}, {name: request})
    smooth = all(smooth_conditions(output, request))
    hard = all(row["status"] == "PASS" for row in checks.values())
    comparison = candidate_error(normalized(output), proof, smooth and hard)
    return dict(
        binding=binding(request),
        actual_scales=scales(request),
        normalized=normalized(output),
        candidate_error=comparison,
        mandatory_smooth="PASS" if smooth else "FAIL",
        hard_checks=checks,
        sources=source,
        price=output["price"],
        membership=output["scenarios"][0]["evaluationMembership_"],
    )


def premium_receipts():
    path = pathlib.Path("build/dal502-followup/products/audits-admitted.json")
    manifest = pathlib.Path(
        ".codex/artifacts/validation/dal502-followup/raw-archives.json"
    )
    expected = json.loads(manifest.read_text())["dal502-followup-controls.tar.gz"][
        "members"
    ][str(path)]
    if digest(path) != expected["sha256"]:
        raise RuntimeError("accepted scalar premium receipt changed")
    return json.loads(path.read_text()), dict(
        path=str(path), sha256=digest(path), archive_manifest_sha256=digest(manifest)
    )


def premium_case(name, case, receipt, plan, raw):
    saved = receipt[name + "-exercise-premium"]
    evidence = saved["evidence"]
    if saved["status"] != "PASS" or evidence["price"] != case["price"]:
        raise RuntimeError("premium receipt belongs to a different owning capture")
    request = fixtures()[name]
    members = [
        raw[price_id(native_request(request, n), plan["execution"])]["result"]
        for n in [16384, 16385]
    ]
    checks = [
        case["price"] > evidence["european"],
        case["membership"] == "CONTINUATION",
    ]
    checks += [bbs_continuation(r, evidence["european"]) for r in members]
    return dict(
        status="PASS" if all(checks) else "FAIL",
        scalar_reevaluated=False,
        product_price=case["price"],
        european=evidence["european"],
        premium=case["price"] - evidence["european"],
        membership=case["membership"],
        final_bbs_members=members,
    )


def bbs_continuation(row, european):
    return (
        row["price"] > european
        and not row["root_exercised"]
        and row["root_continuation"] > row["root_intrinsic"]
        and row["obstacle_hits"] > 0
    )


def direction_case(old, cases):
    names = old["evidence"]["fixtures"]
    direction_sources(old, cases)
    values = [cases[n]["normalized"] for n in names]
    if values != old["evidence"]["values"]:
        raise RuntimeError("direction raw values differ from reviewed evidence")
    diff = differences(values[-1], values[-2])
    if diff != old["evidence"]["last_difference"]:
        raise RuntimeError("direction arithmetic differs")
    passed = all(finite_vector(v) for v in values) and within(diff, 0.5)
    finest = cases[old["finest_fixture"]]
    statuses = [
        old["isolation_status"],
        finest["candidate_error"]["status"],
        "PASS" if passed else "FAIL",
    ]
    return dict(
        direction_status="PASS" if passed else "FAIL",
        candidate_status=combined_status(statuses),
        finest_fixture=old["finest_fixture"],
        finest_binding=finest["binding"],
        finest_comparison=finest["candidate_error"],
        values=values,
        last_difference=diff,
        required_fixtures=names,
        previous_direction_status=old["direction_status"],
        previous_qualification_status=old["qualification_status"],
    )


def direction_sources(old, cases):
    for name, source in old["required_sources"].items():
        if cases[name]["sources"] != source:
            raise RuntimeError("direction isolation source identity differs")


def combined_status(statuses):
    if "FAIL" in statuses:
        return "FAIL"
    return "PASS" if statuses and all(s == "PASS" for s in statuses) else "INCOMPLETE"


def analyze(directory, plan, result):
    sources = json.loads((PREVIOUS / "view-sources.json").read_text())
    selected = {
        name: r
        for name, r in controls().items()
        if engine(name) == "tree" and name.startswith(("N3-", "N6-", "N7-"))
    }
    cases = {
        name: product_case(name, r, sources[name], result["proofs"][binding(r)])
        for name, r in selected.items()
    }
    receipt, origin = premium_receipts()
    raw = json.loads((directory / "all-raw.json").read_text())
    premiums = premium_cases(cases, receipt, plan, raw)
    previous = json.loads((PREVIOUS / "directions.json").read_text())
    directions = {name: direction_case(row, cases) for name, row in previous.items()}
    groups = group_status(cases, premiums, directions)
    return dict(
        cases=cases,
        premiums=premiums,
        directions=directions,
        candidate_groups=groups,
        scalar_premium_origin=origin,
        new_scalar_calls=0,
        new_product_calls=0,
        mandatory_tree_replaced=False,
        qualification_promoted=False,
        current_qualification="9 PASS / 3 INCOMPLETE; N3/N6/N7 unchanged",
        source_records={
            str(p): digest(p)
            for p in [PREVIOUS / "view-sources.json", PREVIOUS / "directions.json"]
        },
    )


def premium_cases(cases, receipt, plan, raw):
    return {
        n: premium_case(n, c, receipt, plan, raw)
        for n, c in cases.items()
        if n.startswith("N3-")
    }


def group_status(cases, premiums, directions):
    n3 = [
        c["candidate_error"]["status"] for n, c in cases.items() if n.startswith("N3-")
    ]
    n3 += [p["status"] for p in premiums.values()]
    return dict(
        N3=combined_status(n3),
        **{
            group: combined_status(
                [
                    row["candidate_status"]
                    for name, row in directions.items()
                    if name.startswith(group + "-")
                ]
            )
            for group in ["N6", "N7"]
        }
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--plan", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    target = args.output / "products-candidate.json"
    if target.exists():
        raise RuntimeError("preserve existing candidate product audit")
    plan = json.loads(args.plan.read_text())
    result = json.loads((args.output / "result-qualified.json").read_text())
    write_json(target, analyze(args.output, plan, result))
