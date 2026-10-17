#!/usr/bin/env python3
"""Move the historical single-process timing assertion to the paired benchmark gate."""

import argparse
import difflib
import hashlib
import json
from pathlib import Path


SOURCE_PATH = Path("dal-cpp/benchmarks/rate_risk_perf/genericjointbenchmarks.cpp")
LEGACY_MESSAGE = "Generic joint steady-state overhead exceeds 20 percent"
LEGACY_ASSERTION = f'            REQUIRE(overhead <= 20.0, "{LEGACY_MESSAGE}");\n'


def prepare_baseline(source_root: Path, evidence_dir: Path) -> None:
    source_root = source_root.resolve()
    source = (source_root / SOURCE_PATH).resolve()
    if not source.is_relative_to(source_root):
        raise ValueError("Benchmark source escapes the baseline source root")
    original = source.read_bytes()
    text = original.decode("utf-8")
    if text.count(LEGACY_MESSAGE) != text.count(LEGACY_ASSERTION) or text.count(LEGACY_ASSERTION) > 1:
        raise ValueError("Unrecognized historical timing assertion; baseline was not edited")
    prepared = text.replace(LEGACY_ASSERTION, "").encode("utf-8")
    evidence_dir.mkdir(parents=True, exist_ok=True)
    patch = "".join(difflib.unified_diff(
        text.splitlines(keepends=True), prepared.decode("utf-8").splitlines(keepends=True),
        fromfile=f"a/{SOURCE_PATH.as_posix()}", tofile=f"b/{SOURCE_PATH.as_posix()}",
    ))
    (evidence_dir / "timing-assertion.patch").write_text(patch, encoding="utf-8")
    manifest = {
        "source": SOURCE_PATH.as_posix(),
        "adjusted": original != prepared,
        "original_sha256": hashlib.sha256(original).hexdigest(),
        "prepared_sha256": hashlib.sha256(prepared).hexdigest(),
    }
    (evidence_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    if original != prepared:
        source.write_bytes(prepared)
    print(json.dumps(manifest), flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    prepare_baseline(args.source_root, args.evidence_dir)


if __name__ == "__main__":
    main()
