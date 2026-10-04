import argparse
import json

import dal

from generate import ROOT
from validate import native_model, native_product, native_valuation


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--id", default="DAL-QA-00001")
    parser.add_argument("--paths", type=int)
    parser.add_argument("--aad", action="store_true")
    parser.add_argument("--tree", action="store_true")
    options = parser.parse_args()
    with (ROOT / "qa.jsonl").open(encoding="utf-8") as stream:
        record = next((record for line in stream if (record := json.loads(line))["id"] == options.id), None)
    if record is None:
        parser.error(f"Unknown record ID: {options.id}")
    settings = record["pricing_context"]["recommended_simulation"].copy()
    default_paths = settings.pop("n_paths")
    settings.update(compiled=not options.tree, enable_aad=options.aad)
    result = dal.MonteCarlo_ValueWithSettings(native_product(record), native_model(record), options.paths or default_paths,
                                            valuation=native_valuation(record), simulation=dal.MonteCarloSettings_(**settings))
    print(json.dumps({"id": record["id"], "n_paths": options.paths or default_paths, "result": result}, ensure_ascii=False, indent=2, allow_nan=False))


if __name__ == "__main__":
    main()
