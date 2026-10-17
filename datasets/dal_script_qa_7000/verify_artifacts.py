import hashlib
import json
import math
import re

from generate import ROOT
from reference import SCENARIOS, reference_pv
from validate import unique_checks, validated_revision

FILE_NAMES = [
    "qa.jsonl",
    "index.md",
    "README.md",
    "validation.jsonl",
    "validation_summary.json",
    "generate.py",
    "reference.py",
    "validate.py",
    "price_one.py",
    "test_dataset.py",
    "verify_artifacts.py",
]

EXPECTED_TEST_COUNTS = {"qa_dataset_tests.log": 24, "qa_existing_tests.log": 101}


def read_jsonl(path):
    content = path.read_bytes()
    if not content.endswith(b"\n"):
        raise ValueError(f"JSONL must end with a newline: {path.name}")
    return [json.loads(line) for line in content.decode("utf-8").splitlines()]


def verify_summary(summary, qa_bytes):
    if summary["sha256_qa_jsonl"] != hashlib.sha256(qa_bytes).hexdigest():
        raise ValueError("Validation does not match the delivered dataset")
    expected = {
        "status": "passed",
        "record_count": 7000,
        "passed": 7000,
        "failed": 0,
        "family_count": 70,
        "stochastic_tree_and_compiled_runs": 14000,
        "zero_volatility_oracle_runs": 21000,
        "aad_tree_and_compiled_runs": 420,
        "failures": [],
    }
    for key, value in expected.items():
        if summary[key] != value:
            raise ValueError(f"Invalid validation summary: {key}")
    if set(summary["uniqueness_checks"].values()) != {"passed"}:
        raise ValueError("Uniqueness validation is incomplete")
    validated_revision(summary["source_revision"])


def verify_reference(record, result, scenario):
    if result["scenario"] != scenario["name"]:
        raise ValueError("Incorrect reference scenario")
    expected, actual, reported_error = (
        result[key] for key in ("expected_pv", "mc_pv", "absolute_error")
    )
    if not all(math.isfinite(value) for value in (expected, actual, reported_error)):
        raise ValueError("Non-finite reference result")
    error = abs(expected - actual)
    if not math.isclose(error, reported_error, rel_tol=1e-12, abs_tol=1e-12):
        raise ValueError("Incorrect reference error")
    if error > 2e-9 * max(1, abs(expected)):
        raise ValueError("Independent reference did not pass")
    if not math.isclose(
        expected, reference_pv(record, scenario), rel_tol=2e-12, abs_tol=1e-12
    ):
        raise ValueError("Independent reference no longer matches the contract")


def verify_mode_prices(result):
    tree, compiled = result["tree_pv"], result["compiled_pv"]
    if not all(math.isfinite(value) for value in (tree, compiled)):
        raise ValueError("Non-finite mode price")
    difference = abs(tree - compiled)
    if difference > 1e-9 * max(1, abs(tree), abs(compiled)):
        raise ValueError("Tree/compiled prices disagree")
    if not math.isclose(
        difference,
        result["tree_compiled_absolute_difference"],
        rel_tol=1e-12,
        abs_tol=1e-12,
    ):
        raise ValueError("Incorrect mode price difference")


def verify_aad(record, result):
    variant = (int(record["id"].split("-")[-1]) - 1) % 100
    modes = [False, True] if variant in (0, 49, 99) else []
    checks = result["aad_checks"]
    if [check["compiled"] for check in checks] != modes:
        raise ValueError("AAD sample coverage is incomplete")
    for check in checks:
        if not math.isfinite(check["pv"]) or check["finite_derivative_count"] < 1:
            raise ValueError("Invalid AAD sample")


def verify_record(record, result):
    expected = {"id": record["id"], "family": record["family"], "status": "passed"}
    if any(result[key] != value for key, value in expected.items()):
        raise ValueError("Per-record validation coverage is incomplete")
    references = result["zero_volatility_references"]
    if len(references) != len(SCENARIOS):
        raise ValueError("Per-record reference coverage is incomplete")
    for reference, scenario in zip(references, SCENARIOS):
        verify_reference(record, reference, scenario)
    verify_mode_prices(result)
    verify_aad(record, result)


def verify_results(records, results):
    if len(records) != 7000 or len(results) != 7000:
        raise ValueError("Expected 7000 records and 7000 validations")
    unique_checks(records)
    for record, result in zip(records, results):
        verify_record(record, result)
    if len({result["canonical_contract_fingerprint"] for result in results}) != 7000:
        raise ValueError("Duplicate normalized contracts")


def verify_index(records):
    index_text = (ROOT / "index.md").read_text(encoding="utf-8")
    links = re.findall(r"\[(DAL-QA-\d+)\]\(qa\.jsonl#L(\d+)\)", index_text)
    if links != [(r["id"], str(i)) for i, r in enumerate(records, 1)]:
        raise ValueError("Index links do not match JSONL record order")
    rows = [line for line in index_text.splitlines() if "| [DAL-QA-" in line]
    for line, record in zip(rows, records):
        cells = [cell.strip() for cell in line.split("|")[1:-1]]
        expected = [
            record["category"],
            record["product_type"],
            "、".join(record["features"]),
        ]
        if cells[2:] != expected:
            raise ValueError("Index classification does not match its record")
    return len(links)


def verify_markdown(markdown):
    text = markdown.read_text(encoding="utf-8")
    if not text.endswith("\n") or any(
        line.rstrip() != line for line in text.splitlines()
    ):
        raise ValueError(f"Markdown whitespace error: {markdown.name}")
    for link in re.findall(r"\[[^\]]+\]\(([^)]+)\)", text):
        if not (markdown.parent / link.split("#")[0]).is_file():
            raise ValueError(f"Broken local link: {link}")


def file_metadata(name):
    content = (ROOT / name).read_bytes()
    return {"bytes": len(content), "sha256": hashlib.sha256(content).hexdigest()}


def pytest_result(log_name):
    log = (ROOT.parents[1] / "build" / log_name).read_text()
    last_line = log.rstrip().split("\n")[-1]
    match = re.fullmatch(r"(\d+) passed in ([\d.]+)s", last_line)
    if not match:
        raise ValueError(f"No passing pytest result in {log_name}")
    if int(match.group(1)) != EXPECTED_TEST_COUNTS[log_name]:
        raise ValueError(f"Incomplete pytest run in {log_name}")
    return {"passed": int(match.group(1)), "seconds": float(match.group(2))}


def verify():
    records = read_jsonl(ROOT / "qa.jsonl")
    results = read_jsonl(ROOT / "validation.jsonl")
    summary = json.loads((ROOT / "validation_summary.json").read_text(encoding="utf-8"))
    verify_summary(summary, (ROOT / "qa.jsonl").read_bytes())
    verify_results(records, results)
    index_count = verify_index(records)
    for markdown in (ROOT / "README.md", ROOT / "index.md"):
        verify_markdown(markdown)
    manifest = {
        "status": "passed",
        "question_answer_count": len(records),
        "index_record_links": index_count,
        "matching_validation_records": len(results),
        "files": {name: file_metadata(name) for name in FILE_NAMES},
    }
    for log_name in ("qa_dataset_tests.log", "qa_existing_tests.log"):
        manifest[log_name] = pytest_result(log_name)
    (ROOT / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(manifest, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    verify()
