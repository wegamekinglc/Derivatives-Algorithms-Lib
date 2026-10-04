import hashlib
import json
import re
from pathlib import Path

from generate import ROOT
from validate import unique_checks


def verify():
    qa_bytes = (ROOT / "qa.jsonl").read_bytes()
    if not qa_bytes.endswith(b"\n"):
        raise ValueError("JSONL must end with a newline")
    records = [json.loads(line) for line in qa_bytes.decode("utf-8").splitlines()]
    results = [json.loads(line) for line in (ROOT / "validation.jsonl").read_text(encoding="utf-8").splitlines()]
    summary = json.loads((ROOT / "validation_summary.json").read_text(encoding="utf-8"))
    if len(records) != 7000 or len(results) != 7000:
        raise ValueError("Expected 7000 records and 7000 validations")
    unique_checks(records)
    if summary["sha256_qa_jsonl"] != hashlib.sha256(qa_bytes).hexdigest():
        raise ValueError("Validation does not match the delivered dataset")
    if summary["status"] != "passed" or summary["failed"] != 0 or summary["passed"] != 7000:
        raise ValueError("Full engine validation has not passed")
    for record, result in zip(records, results):
        if record["id"] != result["id"] or result["status"] != "passed" or len(result["zero_volatility_references"]) != 3:
            raise ValueError("Per-record validation coverage is incomplete")
    if len({result["canonical_contract_fingerprint"] for result in results}) != 7000:
        raise ValueError("Duplicate normalized contracts")
    index_text = (ROOT / "index.md").read_text(encoding="utf-8")
    links = re.findall(r"\[(DAL-QA-\d+)\]\(qa\.jsonl#L(\d+)\)", index_text)
    if links != [(r["id"], str(i)) for i, r in enumerate(records, 1)]:
        raise ValueError("Index links do not match JSONL record order")
    for markdown in (ROOT / "README.md", ROOT / "index.md"):
        text = markdown.read_text(encoding="utf-8")
        if not text.endswith("\n") or any(line.rstrip() != line for line in text.splitlines()):
            raise ValueError(f"Markdown whitespace error: {markdown.name}")
        for link in re.findall(r"\[[^\]]+\]\(([^)]+)\)", text):
            if not (markdown.parent / link.split("#")[0]).is_file():
                raise ValueError(f"Broken local link: {link}")
    files = ["qa.jsonl", "index.md", "README.md", "validation.jsonl", "validation_summary.json", "generate.py", "reference.py", "validate.py", "price_one.py", "test_dataset.py", "verify_artifacts.py"]
    manifest = {"status": "passed", "question_answer_count": len(records), "index_record_links": len(links), "matching_validation_records": len(results),
                "files": {name: {"bytes": (ROOT / name).stat().st_size, "sha256": hashlib.sha256((ROOT / name).read_bytes()).hexdigest()} for name in files}}
    build_dir = ROOT.parents[1] / "build"
    for log_name in ("qa_dataset_tests.log", "qa_existing_tests.log"):
        log = (build_dir / log_name).read_text()
        match = re.search(r"(\d+) passed in ([\d.]+)s", log)
        if not match:
            raise ValueError(f"No passing pytest result in {log_name}")
        manifest[log_name] = {"passed": int(match.group(1)), "seconds": float(match.group(2))}
    (ROOT / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    verify()
