from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import time
from collections import Counter
from concurrent.futures import ProcessPoolExecutor, as_completed
from datetime import date

from generate import FAMILIES, ROOT, digest, parse_answer
from reference import SCENARIOS, reference_pv


def native_date(value):
    import dal

    parsed = date.fromisoformat(value)
    return dal.Date_(parsed.year, parsed.month, parsed.day)


def native_product(record):
    import dal

    rows = parse_answer(record["answer"])
    if rows != record["events"]:
        raise ValueError("The answer table disagrees with the stored event rows")
    dates = [
        native_date(r["date_or_definition"])
        if re.fullmatch(r"\d{4}-\d{2}-\d{2}", r["date_or_definition"])
        else r["date_or_definition"]
        for r in rows
    ]
    return dal.Product_New(
        dates,
        [r["script"] for r in rows],
        settings=dal.ScriptProductSettings_(**record["product_settings"]),
    )


def model_inputs(data, scenario):
    multiplier = scenario["spot_multiplier"] if scenario else 1.0
    rate = scenario["rate"] if scenario else data.get("rate", data.get("curve_rate"))
    return multiplier, rate


def black_scholes_model(record, scenario):
    import dal

    data = record["pricing_context"]["model"]
    multiplier, rate = model_inputs(data, scenario)
    return dal.BSModelData_New(
        data["spot"] * multiplier, 0.0 if scenario else data["vol"], rate, data["div"]
    )


def correlated_bs_model(record, scenario):
    import dal

    data = record["pricing_context"]["model"]
    multiplier, rate = model_inputs(data, scenario)
    correlation = data["correlation"]
    return dal.CorrelatedBSModelData_New(
        data["indices"],
        [s * multiplier for s in data["spots"]],
        [0.0] * len(data["vols"]) if scenario else data["vols"],
        data["divs"],
        rate,
        dal.DoubleMatrix_([[1.0, correlation], [correlation, 1.0]]),
    )


def rate_curve_and_vol(record, scenario):
    import dal

    data = record["pricing_context"]["model"]
    _, rate = model_inputs(data, scenario)
    today = native_date(record["pricing_context"]["evaluation_date"])
    horizon = native_date(data["curve_horizon"])
    curve = dal.GSRCurveData_New(
        "qa_curve",
        today,
        "USD",
        [today, horizon],
        [0.0, -rate * (horizon - today) / 365.0],
        [],
        dal.DoubleMatrix_(0, 0),
    )
    vol = dal.GSRVolData_New(
        "qa_rate_vol", [today], [0.0 if scenario else data["g"]], [today], [data["h"]]
    )
    return curve, vol


def gsr_model(record, scenario):
    import dal

    return dal.GSRModelData_New("qa_gsr", *rate_curve_and_vol(record, scenario))


def hybrid_model(record, scenario):
    import dal

    data = record["pricing_context"]["model"]
    multiplier, _ = model_inputs(data, scenario)
    curve, vol = rate_curve_and_vol(record, scenario)
    components = [
        dal.HybridBSEquityData_New(
            "ABC",
            "EQ[ABC]",
            "USD",
            "W_EQ",
            data["spot"] * multiplier,
            0.0 if scenario else data["vol"],
            data["div"],
        ),
        dal.HybridGSRRateData_New("USD_RATE", "W_RATE", curve, vol),
    ]
    correlation = data["correlation"]
    correlations = dal.HybridConstantCorrelationData_New(
        "qa_corr",
        ["W_EQ", "W_RATE"],
        dal.DoubleMatrix_([[1.0, correlation], [correlation, 1.0]]),
    )
    return dal.HybridModelData_New("qa_hybrid", "USD", components, correlations)


MODEL_BUILDERS = {
    "black_scholes": black_scholes_model,
    "correlated_bs": correlated_bs_model,
    "gsr": gsr_model,
    "hybrid_equity_gsr": hybrid_model,
}


def native_model(record, scenario=None):
    kind = record["pricing_context"]["model"]["type"]
    if kind not in MODEL_BUILDERS:
        raise ValueError(f"Unsupported model {kind}")
    return MODEL_BUILDERS[kind](record, scenario)


def native_valuation(record):
    import dal

    kwargs = {
        "evaluation_date": native_date(record["pricing_context"]["evaluation_date"])
    }
    fixings = record["pricing_context"]["model"].get("fixings")
    if fixings:
        kwargs["fixings"] = dal.MarketFixingSnapshot_New(
            {
                index: {
                    dal.DateTime_(native_date(d), 0): value
                    for d, value in values.items()
                }
                for index, values in fixings.items()
            }
        )
    return dal.ScriptValuationSettings_(**kwargs)


def price(
    record, product, model, compiled=True, paths=1024, aad=False, deterministic=False
):
    import dal

    options = {
        "method": "sobol",
        "compiled": compiled,
        "enable_aad": aad,
        "use_bb": False,
        "lsmc_basis_degree": 2,
        "lsmc_training_paths": 32 if deterministic else max(paths, 256),
    }
    result = dal.MonteCarlo_ValueWithSettings(
        product,
        model,
        paths,
        valuation=native_valuation(record),
        simulation=dal.MonteCarloSettings_(**options),
    )
    if "PV" not in result or any(not math.isfinite(value) for value in result.values()):
        raise ValueError(f"Non-finite Monte Carlo result: {result}")
    return result


def clean_dictionary(node, event_date, record):
    kind = node.get("kind")
    if kind == "const_var":
        return {"kind": "const", "value": node["value"]}
    if kind in ("spot", "fix"):
        index = node.get("index_canonical") or record["product_settings"].get(
            "default_index"
        )
        return {
            "kind": "observation",
            "index": index,
            "fixing_date": node.get("fixing_date") or event_date,
        }
    discarded = {
        "id",
        "name",
        "source",
        "origins",
        "event_id",
        "index_original",
        "mode",
        "eps",
    }
    return {
        key: clean_ast(value, event_date, record)
        for key, value in node.items()
        if key not in discarded
    }


def clean_ast(node, event_date, record):
    if isinstance(node, list):
        return [clean_ast(value, event_date, record) for value in node]
    if isinstance(node, dict):
        return clean_dictionary(node, event_date, record)
    return node


def canonical_contract(description, record):
    events = [
        {
            "date": event["date"],
            "statements": clean_ast(event["statements"], event["date"], record),
        }
        for event in description["events"]
    ]
    return digest(
        {
            "events": events,
            "history": record["pricing_context"]["model"].get("fixings", {}),
        }
    )


def validate_record(record, paths, aad_sample):
    import dal

    start = time.monotonic()
    product = native_product(record)
    description = dal.Product_Describe(product)
    fingerprint = canonical_contract(description, record)
    model = native_model(record)
    tree = price(record, product, model, compiled=False, paths=paths)["PV"]
    compiled = price(record, product, model, compiled=True, paths=paths)["PV"]
    difference = abs(tree - compiled)
    if difference > 1e-9 * max(1, abs(tree), abs(compiled)):
        raise ValueError(f"Tree/compiled mismatch: {tree} vs {compiled}")
    references = []
    for scenario in SCENARIOS:
        expected = reference_pv(record, scenario)
        actual = price(
            record,
            product,
            native_model(record, scenario),
            compiled=True,
            paths=32,
            deterministic=True,
        )["PV"]
        error = abs(expected - actual)
        if error > 2e-9 * max(1, abs(expected)):
            raise ValueError(
                f"Independent reference mismatch for {scenario['name']}: expected {expected}, actual {actual}, error {error}"
            )
        references.append(
            {
                "scenario": scenario["name"],
                "expected_pv": expected,
                "mc_pv": actual,
                "absolute_error": error,
            }
        )
    aad_checks = []
    if aad_sample:
        for mode in (False, True):
            aad = price(record, product, model, compiled=mode, paths=256, aad=True)
            aad_checks.append(
                {
                    "compiled": mode,
                    "pv": aad["PV"],
                    "finite_derivative_count": len(aad) - 1,
                }
            )
        if abs(aad_checks[0]["pv"] - aad_checks[1]["pv"]) > 1e-8 * max(
            1, abs(aad_checks[0]["pv"])
        ):
            raise ValueError("AAD tree/compiled mismatch")
    return {
        "id": record["id"],
        "status": "passed",
        "family": record["family"],
        "canonical_contract_fingerprint": fingerprint,
        "event_count_after_expansion": len(description["events"]),
        "tree_pv": tree,
        "compiled_pv": compiled,
        "tree_compiled_absolute_difference": difference,
        "zero_volatility_references": references,
        "aad_checks": aad_checks,
        "elapsed_seconds": time.monotonic() - start,
    }


def validate_batch(records, paths):
    result = []
    for record in records:
        try:
            variant = (int(record["id"].split("-")[-1]) - 1) % 100
            result.append(validate_record(record, paths, variant in (0, 49, 99)))
        except (
            ValueError,
            RuntimeError,
            KeyError,
            TypeError,
            IndexError,
            ArithmeticError,
        ) as error:
            result.append(
                {
                    "id": record["id"],
                    "family": record["family"],
                    "status": "failed",
                    "error": str(error),
                }
            )
    return result


def unique_checks(records):
    for field in ("id", "question", "answer", "contract_fingerprint"):
        if len({r[field] for r in records}) != len(records):
            raise ValueError(f"Duplicate {field}")
    for record in records:
        if parse_answer(record["answer"]) != record["events"]:
            raise ValueError(f"Markdown/rows mismatch in {record['id']}")


def validated_revision(value):
    if not re.fullmatch(r"[0-9a-fA-F]{40}", value):
        raise ValueError("Engine source revision must be a full Git commit ID")
    return value.lower()


def load_records():
    records = [
        json.loads(line)
        for line in (ROOT / "qa.jsonl").read_text(encoding="utf-8").splitlines()
    ]
    expected = {family: 100 for family, _, _ in FAMILIES}
    if Counter(r["family"] for r in records) != expected:
        raise ValueError("Wrong record/family count")
    unique_checks(records)
    return records


def validation_records(smoke):
    records = load_records()
    if smoke:
        records = [r for i, r in enumerate(records) if i % 100 in (0, 49, 99)]
    return records


def validate_parallel(records, paths, workers, batch_size):
    batches = [records[i : i + batch_size] for i in range(0, len(records), batch_size)]
    results = []
    with ProcessPoolExecutor(max_workers=workers) as executor:
        futures = [executor.submit(validate_batch, batch, paths) for batch in batches]
        for future in as_completed(futures):
            batch_results = future.result()
            results.extend(batch_results)
            failures = [
                result for result in batch_results if result["status"] != "passed"
            ]
            print(
                json.dumps(
                    {
                        "completed": len(results),
                        "total": len(records),
                        "family": batch_results[0]["family"],
                        "failures": failures,
                    },
                    ensure_ascii=False,
                ),
                flush=True,
            )
    results.sort(key=lambda result: result["id"])
    return results


def partition_results(results):
    failures = [result for result in results if result["status"] != "passed"]
    successful = [result for result in results if result["status"] == "passed"]
    return successful, failures


def check_canonical_uniqueness(successful):
    fingerprints = [result["canonical_contract_fingerprint"] for result in successful]
    if len(set(fingerprints)) != len(fingerprints):
        duplicates = [
            fingerprint
            for fingerprint, count in Counter(fingerprints).items()
            if count > 1
        ]
        raise ValueError(f"Duplicate canonical contracts: {duplicates}")


def write_results(results, prefix):
    output_path = ROOT / f"{prefix}validation.jsonl"
    with output_path.open("w", encoding="utf-8") as stream:
        for result in results:
            stream.write(
                json.dumps(
                    result, ensure_ascii=False, separators=(",", ":"), allow_nan=False
                )
                + "\n"
            )


def validation_summary(records, results, paths, revision, start):
    successful, failures = partition_results(results)
    summary = {
        "status": "failed" if failures else "passed",
        "record_count": len(records),
        "passed": len(successful),
        "failed": len(failures),
        "family_count": len(Counter(r["family"] for r in records)),
        "n_paths_per_stochastic_mode": paths,
        "stochastic_tree_and_compiled_runs": 2 * len(successful),
        "zero_volatility_oracle_runs": 3 * len(successful),
        "aad_tree_and_compiled_runs": sum(len(r["aad_checks"]) for r in successful),
        "uniqueness_checks": {
            "id": "passed",
            "question": "passed",
            "answer": "passed",
            "contract_terms": "passed",
            "normalized_AST": "incomplete" if failures else "passed",
        },
        "sha256_qa_jsonl": hashlib.sha256((ROOT / "qa.jsonl").read_bytes()).hexdigest(),
        "source_revision": revision,
        "elapsed_seconds": time.monotonic() - start,
        "failures": failures,
        "scope": "Executable product/schema correctness and deterministic payoff checks; stochastic PVs are smoke estimates, not accuracy-certified market prices. American products use a finite daily exercise grid.",
    }
    return summary


def run(paths, workers, source_revision, smoke=False):
    os.environ.setdefault("DAL_NUM_THREADS", "2")
    start = time.monotonic()
    revision = validated_revision(source_revision)
    records = validation_records(smoke)
    results = validate_parallel(records, paths, workers, 3 if smoke else 100)
    successful, failures = partition_results(results)
    check_canonical_uniqueness(successful)
    prefix = "smoke_" if smoke else ""
    write_results(results, prefix)
    summary = validation_summary(records, results, paths, revision, start)
    (ROOT / f"{prefix}validation_summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2, allow_nan=False) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(summary, ensure_ascii=False), flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--paths", type=int, default=1024)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument(
        "--source-revision",
        type=validated_revision,
        required=True,
        help="Git commit of the engine source used to build the loaded native extension",
    )
    parser.add_argument("--smoke", action="store_true")
    arguments = parser.parse_args()
    raise SystemExit(
        run(
            arguments.paths,
            arguments.workers,
            arguments.source_revision,
            arguments.smoke,
        )
    )
