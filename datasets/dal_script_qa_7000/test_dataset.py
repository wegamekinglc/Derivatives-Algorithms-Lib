import copy
import json
import math
from datetime import date

import dal
import pytest

from generate import ROOT, build_record, markdown_table, parse_answer
from reference import libor_zero_vol, reference_pv, swap_zero_vol
from validate import canonical_contract, native_model, native_product, price, unique_checks


def with_rows(record, rows):
    record = copy.deepcopy(record)
    record["events"] = rows
    record["answer"] = markdown_table(["日期 / 定义", "DAL script / 数值"], [[f"`{r['date_or_definition']}`", f"`{r['script']}`"] for r in rows])
    return record


def test_jsonl_has_exact_count_unique_records_and_table_roundtrips():
    records = [json.loads(line) for line in (ROOT / "qa.jsonl").read_text(encoding="utf-8").splitlines()]
    assert len(records) == 7000
    unique_checks(records)
    assert len({r["family"] for r in records}) == 70
    assert all(parse_answer(r["answer"]) == r["events"] for r in records)


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
    replacement = with_rows(record, [{"date_or_definition": record["contract_terms"]["maturity"], "script": "another_name PAYS 1 * MAX(FIX(EQ[ABC]) - 210, 0)"}])
    original_hash = canonical_contract(dal.Product_Describe(native_product(record)), record)
    replacement_hash = canonical_contract(dal.Product_Describe(native_product(replacement)), replacement)
    assert original_hash == replacement_hash


def test_spot_binding_and_named_fix_have_same_contract_identity():
    record = build_record(0, 4)
    replacement = copy.deepcopy(record)
    replacement["product_settings"].pop("default_index")
    rows = copy.deepcopy(replacement["events"])
    for row in rows:
        row["script"] = row["script"].replace("SPOT()", "FIX(EQ[ABC])")
    replacement = with_rows(replacement, rows)
    assert canonical_contract(dal.Product_Describe(native_product(record)), record) == canonical_contract(dal.Product_Describe(native_product(replacement)), replacement)


def test_terminal_script_change_is_caught_by_independent_reference():
    record = build_record(0, 0)
    scenario = {"spot_multiplier": 1.45, "rate": 0.04}
    replacement = with_rows(record, [{"date_or_definition": record["contract_terms"]["maturity"], "script": "payoff PAYS MAX(210 - FIX(EQ[ABC]), 0)"}])
    wrong = price(replacement, native_product(replacement), native_model(replacement, scenario), deterministic=True, paths=32)["PV"]
    correct = reference_pv(record, scenario)
    assert correct > 50
    assert wrong == 0
    assert abs(correct - wrong) > 50


def test_memory_coupon_does_not_pay_forfeited_final_memory():
    record = build_record(40, 0)
    terms = record["contract_terms"]
    scenario = {"spot_multiplier": 0.5, "rate": 0.01}
    maturity = date.fromisoformat(terms["maturity"])
    expected = terms["principal"] * math.exp(-0.01 * (maturity - date(2026, 10, 4)).days / 365)
    assert reference_pv(record, scenario) == pytest.approx(expected, abs=1e-9)


def test_libor_uses_three_calendar_months_after_two_business_days():
    fixing = date(2026, 10, 9)
    # Friday fixing -> Tuesday start, then three calendar months, 92 ACT/360 days.
    expected = math.expm1(0.025 * 92 / 365) / (92 / 360)
    assert libor_zero_vol(fixing, 0.025) == pytest.approx(expected, abs=1e-12)


def test_swap_forward_schedule_retains_month_end_drift_and_final_stub():
    rate = 0.015
    # Sequential six-month increments reach March 30, then a final one-day stub.
    annuity = 0.5 * math.exp(-rate * 183 / 365) + 0.5 * math.exp(-rate * 365 / 365)
    expected = (1 - math.exp(-rate * 366 / 365)) / annuity
    assert swap_zero_vol(date(2027, 3, 31), rate) == pytest.approx(expected, abs=1e-12)
