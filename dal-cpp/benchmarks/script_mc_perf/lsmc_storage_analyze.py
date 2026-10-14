#!/usr/bin/env python3
"""Validate complete storage data and paired native references before analysis."""

import argparse
import json
from pathlib import Path

from lsmc_storage_data import parse_storage_record, storage_definition, storage_jobs
from lsmc_storage_reference import load_native_reference
from lsmc_storage_run import verify_storage_image
from lsmc_storage_summary import summarize_storage
from lsmc_study_build import digest
from lsmc_study_run import dataset_lock
from lsmc_study_store import require_complete, write_json


def load_storage_dataset(root, helpers):
    manifest = json.loads((root / "profile.json").read_text())
    context = manifest["context"]
    image = context["image"]
    verify_storage_image(image)
    expected = {name: digest(helpers / name) for name in context["helpers"]}
    if expected != context["helpers"] or image["storage_bits"] != 32:
        raise ValueError("storage analysis requires the original execution helpers and binary32 observations")
    jobs = storage_jobs(context["profile"])
    definition = json.loads(json.dumps(storage_definition(context["profile"])))
    if manifest["jobs"] != [job.__dict__ for job in jobs] or context["definition"] != definition:
        raise ValueError("storage dataset differs from the complete prespecified profile")
    with dataset_lock(root):
        attempts = require_complete(root, jobs)
        observations = [(job, parse_storage_record(attempt["stdout"], job, image["anchor"], 32))
                        for job, attempt in zip(jobs, attempts)]
    return observations, image, jobs


def analyze(dataset, reference, helpers, output):
    if output.exists():
        raise FileExistsError("preserve the existing storage summary before writing another")
    observations, image, jobs = load_storage_dataset(dataset, helpers)
    native, provenance = load_native_reference(reference, jobs, image)
    summary = summarize_storage(observations, native)
    summary["provenance"] = dict(native_reference=provenance, storage_executable_sha256=image["executable_sha256"],
                                 storage_manifest_sha256=digest(dataset / "profile.json"),
                                 analyzer_sha256=digest(Path(__file__).with_name("lsmc_storage_summary.py")))
    verify_storage_image(image)
    write_json(output, summary)
    print(f"summarized {len(jobs)} verified binary32/native64 pairs: {output}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("dataset", "native-reference", "execution-helpers", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    arguments = parser.parse_args()
    analyze(arguments.dataset, arguments.native_reference, arguments.execution_helpers, arguments.output)


if __name__ == "__main__":
    main()
