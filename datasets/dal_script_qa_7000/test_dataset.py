import copy
import json
import math
import shutil
from datetime import date
from unittest import TestCase

import dal
import pytest
from generate import ROOT, build_record, markdown_table, parse_answer
from reference import libor_zero_vol, reference_pv, swap_zero_vol
from validate import (
    canonical_contract,
    native_model,
    native_product,
    price,
    unique_checks,
)

check = TestCase()


def with_rows(record, rows):
    record = copy.deepcopy(record)
    record["events"] = rows
    record["answer"] = markdown_table(
        ["日期 / 定义", "DAL script / 数值"],
        [[f"`{r['date_or_definition']}`", f"`{r['script']}`"] for r in rows],
    )
    return record


def test_jsonl_has_exact_count_unique_records_and_table_roundtrips():
    records = [
        json.loads(line)
        for line in (ROOT / "qa.jsonl").read_text(encoding="utf-8").splitlines()
    ]
    check.assertEqual(len(records), 7000)
    unique_checks(records)
    check.assertEqual(len({r["family"] for r in records}), 70)
    check.assertTrue(all(parse_answer(r["answer"]) == r["events"] for r in records))


def test_every_contract_can_be_regenerated_without_changing_the_dataset():
    with (ROOT / "qa.jsonl").open(encoding="utf-8") as stream:
        for index, line in enumerate(stream):
            check.assertEqual(build_record(index // 100, index % 100), json.loads(line))


@pytest.mark.parametrize("family_index", [50, 51])
def test_named_observation_and_fixed_coupon_contracts_do_not_advertise_a_spot_binding(
    family_index,
):
    for variant in range(4, 100, 5):
        record = build_record(family_index, variant)
        check.assertNotIn("default_index", record["product_settings"])
        check.assertNotIn("default_index_binding", record["features"])


@pytest.mark.parametrize("corruption", ["skewed", "unknown_family"])
def test_validator_requires_100_contracts_for_each_declared_family(
    tmp_path, monkeypatch, corruption
):
    import validate

    records = [
        json.loads(line) for line in (ROOT / "qa.jsonl").read_text().splitlines()
    ]
    if corruption == "skewed":
        records[0]["family"] = records[100]["family"]
    else:
        for record in records[:100]:
            record["family"] = "unknown_family"
    (tmp_path / "qa.jsonl").write_text(
        "\n".join(json.dumps(record) for record in records) + "\n"
    )
    monkeypatch.setattr(validate, "ROOT", tmp_path)
    with pytest.raises(ValueError, match="record/family count"):
        validate.load_records()


@pytest.mark.parametrize(
    "summary",
    [
        "1 passed in 0.1s\n",
        "1 failed, 101 passed in 0.1s\n",
        "101 passed in 0.1s\nInterrupted\n",
    ],
)
def test_manifest_requires_a_clean_complete_final_pytest_summary(
    tmp_path, monkeypatch, summary
):
    import verify_artifacts

    root = tmp_path / "datasets" / "qa"
    root.mkdir(parents=True)
    logs = tmp_path / "build"
    logs.mkdir()
    (logs / "qa_existing_tests.log").write_text(summary)
    monkeypatch.setattr(verify_artifacts, "ROOT", root)
    with pytest.raises(ValueError, match="pytest"):
        verify_artifacts.pytest_result("qa_existing_tests.log")


@pytest.mark.parametrize("separator", ["invalid", "| --- |", "| --- | --- | --- |"])
def test_malformed_markdown_separator_is_rejected(separator):
    record = build_record(0, 0)
    lines = record["answer"].splitlines()
    lines[1] = separator
    with pytest.raises(ValueError, match="separator"):
        parse_answer("\n".join(lines))


@pytest.mark.parametrize("revision", ["HEAD", "a" * 39, "g" * 40])
def test_engine_provenance_requires_a_full_commit_id(revision):
    from validate import validated_revision

    with pytest.raises(ValueError, match="commit"):
        validated_revision(revision)


def test_artifact_verifier_rejects_a_falsified_reference_result(tmp_path, monkeypatch):
    import verify_artifacts

    destination = tmp_path / "repo" / "datasets" / "qa"
    destination.mkdir(parents=True)
    for path in ROOT.iterdir():
        if path.is_file():
            shutil.copyfile(path, destination / path.name)
    logs = destination.parents[1] / "build"
    logs.mkdir()
    for name in ("qa_dataset_tests.log", "qa_existing_tests.log"):
        shutil.copyfile(ROOT.parents[1] / "build" / name, logs / name)
    result_path = destination / "validation.jsonl"
    lines = result_path.read_text().splitlines()
    first = json.loads(lines[0])
    first["zero_volatility_references"][0]["mc_pv"] += 10000
    lines[0] = json.dumps(first)
    result_path.write_text("\n".join(lines) + "\n")
    monkeypatch.setattr(verify_artifacts, "ROOT", destination)
    with pytest.raises(ValueError, match="reference"):
        verify_artifacts.verify()


def test_duplicate_question_is_rejected_even_with_different_ids():
    first, second = build_record(0, 0), build_record(0, 1)
    second["question"] = first["question"]
    with pytest.raises(ValueError, match="Duplicate question"):
        unique_checks([first, second])


def test_answer_corruption_is_rejected_before_pricing():
    record = build_record(0, 0)
    record["answer"] = record["answer"].replace("210", "211")
    with pytest.raises(ValueError, match="disagrees"):
        native_product(record)


def test_canonical_contract_ignores_macros_and_receiver_names():
    record = build_record(0, 0)
    replacement = with_rows(
        record,
        [
            {
                "date_or_definition": record["contract_terms"]["maturity"],
                "script": "another_name PAYS 1 * MAX(FIX(EQ[ABC]) - 210, 0)",
            }
        ],
    )
    original_hash = canonical_contract(
        dal.Product_Describe(native_product(record)), record
    )
    replacement_hash = canonical_contract(
        dal.Product_Describe(native_product(replacement)), replacement
    )
    check.assertEqual(original_hash, replacement_hash)


def test_spot_binding_and_named_fix_have_same_contract_identity():
    record = build_record(0, 4)
    replacement = copy.deepcopy(record)
    replacement["product_settings"].pop("default_index")
    rows = copy.deepcopy(replacement["events"])
    for row in rows:
        row["script"] = row["script"].replace("SPOT()", "FIX(EQ[ABC])")
    replacement = with_rows(replacement, rows)
    check.assertEqual(
        canonical_contract(dal.Product_Describe(native_product(record)), record),
        canonical_contract(
            dal.Product_Describe(native_product(replacement)), replacement
        ),
    )


def test_terminal_script_change_is_caught_by_independent_reference():
    record = build_record(0, 0)
    scenario = {"spot_multiplier": 1.45, "rate": 0.04}
    replacement = with_rows(
        record,
        [
            {
                "date_or_definition": record["contract_terms"]["maturity"],
                "script": "payoff PAYS MAX(210 - FIX(EQ[ABC]), 0)",
            }
        ],
    )
    wrong = price(
        replacement,
        native_product(replacement),
        native_model(replacement, scenario),
        deterministic=True,
        paths=32,
    )["PV"]
    correct = reference_pv(record, scenario)
    check.assertGreater(correct, 50)
    check.assertEqual(wrong, 0)
    check.assertGreater(abs(correct - wrong), 50)


def test_memory_coupon_does_not_pay_forfeited_final_memory():
    record = build_record(40, 0)
    terms = record["contract_terms"]
    scenario = {"spot_multiplier": 0.5, "rate": 0.01}
    maturity = date.fromisoformat(terms["maturity"])
    expected = terms["principal"] * math.exp(
        -0.01 * (maturity - date(2026, 10, 4)).days / 365
    )
    check.assertEqual(reference_pv(record, scenario), pytest.approx(expected, abs=1e-9))


def test_libor_uses_three_calendar_months_after_two_business_days():
    fixing = date(2026, 10, 9)
    # Friday fixing -> Tuesday start, then three calendar months, 92 ACT/360 days.
    expected = math.expm1(0.025 * 92 / 365) / (92 / 360)
    check.assertEqual(libor_zero_vol(fixing, 0.025), pytest.approx(expected, abs=1e-12))


def test_swap_forward_schedule_retains_month_end_drift_and_final_stub():
    rate = 0.015
    # Sequential six-month increments reach March 30, then a final one-day stub.
    annuity = 0.5 * math.exp(-rate * 183 / 365) + 0.5 * math.exp(-rate * 365 / 365)
    expected = (1 - math.exp(-rate * 366 / 365)) / annuity
    check.assertEqual(
        swap_zero_vol(date(2027, 3, 31), rate), pytest.approx(expected, abs=1e-12)
    )
