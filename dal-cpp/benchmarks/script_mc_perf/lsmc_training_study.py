#!/usr/bin/env python3
"""Prepare address controls, run immutable profiles and summarize paired results."""

import argparse
import json
from pathlib import Path

from lsmc_study_build import build_control, digest
from lsmc_study_data import Job, parse_record, profile_definition, profile_jobs
from lsmc_study_run import dataset_lock, run_jobs, verify_image
from lsmc_study_store import prepare_dataset, require_complete, write_json
from lsmc_study_summary import summarize


def helper_identity():
    directory = Path(__file__).resolve().parent
    names = ("lsmc_training_study.py", "lsmc_study_data.py", "lsmc_study_run.py", "lsmc_study_store.py", "lsmc_study_support.py")
    return {name: digest(directory / name) for name in names}


def run_profile(arguments):
    identity = json.loads(arguments.image.read_text())
    definition = profile_definition(arguments.profile)
    if identity["anchor"] != definition["anchor"]:
        raise ValueError("control image anchor does not match the study profile")
    verify_image(identity)
    jobs = profile_jobs(arguments.profile, arguments.case, arguments.threads)
    context = dict(profile=arguments.profile, definition=definition, image=identity, helpers=helper_identity())
    context = json.loads(json.dumps(context))
    prepare_dataset(arguments.output, context, jobs)
    with dataset_lock(arguments.output):
        run_jobs(arguments.output, jobs, identity["anchor"], Path(identity["executable"]), arguments.timeout, arguments.retry_failed)
    verify_image(identity)
    if helper_identity() != context["helpers"]:
        raise ValueError("study helpers changed during execution")


def summarize_dataset(arguments):
    manifest = json.loads((arguments.dataset / "profile.json").read_text())
    context = manifest["context"]
    identity = context["image"]
    verify_image(identity)
    jobs = tuple(Job(**row) for row in manifest["jobs"])
    with dataset_lock(arguments.dataset):
        attempts = require_complete(arguments.dataset, jobs)
        observations = [(job, parse_record(attempt["stdout"], job, identity["anchor"])) for job, attempt in zip(jobs, attempts)]
    summary = summarize(observations)
    summary["diagnostic_only"] = context["definition"]["diagnostic_only"]
    summary["provenance"] = dict(manifest_sha256=digest(arguments.dataset / "profile.json"),
                                 executable_sha256=identity["executable_sha256"],
                                 analyzer_sha256=digest(Path(__file__).with_name("lsmc_study_summary.py")))
    if arguments.output.exists():
        raise FileExistsError("preserve the existing summary before writing another")
    write_json(arguments.output, summary)
    print(f"summarized {summary['samples']} verified observations into {arguments.output}")


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    commands = result.add_subparsers(dest="command", required=True)
    control = commands.add_parser("control", help="build controls from an isolated, freshly built static Release tree")
    for name in ("source", "build", "output"):
        control.add_argument("--" + name, required=True, type=Path)
    control.add_argument("--anchor", required=True, type=int)
    run = commands.add_parser("run", help="run or resume a prespecified profile on a verified control image")
    run.add_argument("--image", required=True, type=Path)
    run.add_argument("--profile", required=True, choices=("accuracy", "corner", "smoke", "calibration"))
    run.add_argument("--output", required=True, type=Path)
    run.add_argument("--case", action="append")
    run.add_argument("--threads", type=int, default=4, choices=range(1, 257), metavar="1..256")
    run.add_argument("--timeout", type=int, default=900)
    run.add_argument("--retry-failed", action="store_true")
    summary = commands.add_parser("summarize", help="validate a complete dataset and write paired uncertainty summaries")
    summary.add_argument("--dataset", required=True, type=Path)
    summary.add_argument("--output", required=True, type=Path)
    return result


def main():
    arguments = parser().parse_args()
    if arguments.command == "control":
        build_control(arguments.source, arguments.build, arguments.output, arguments.anchor)
    elif arguments.command == "run":
        if arguments.timeout <= 0:
            raise ValueError("timeout must be positive")
        run_profile(arguments)
    else:
        summarize_dataset(arguments)


if __name__ == "__main__":
    main()
