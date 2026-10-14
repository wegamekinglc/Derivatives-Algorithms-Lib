"""Verify complete-build images and run storage-specific retained datasets."""

from functools import partial
import json
from pathlib import Path

from lsmc_storage_data import parse_storage_record, storage_definition, storage_jobs
from lsmc_study_build import digest, require_source_inputs
from lsmc_study_run import dataset_lock, run_jobs
from lsmc_study_store import prepare_dataset

HELPERS = ("lsmc_storage_study.py", "lsmc_storage_run.py", "lsmc_storage_data.py", "lsmc_study_run.py", "lsmc_study_data.py",
           "lsmc_study_store.py", "lsmc_study_build.py", "lsmc_study_support.py")


def helper_identity():
    root = Path(__file__).resolve().parent
    return {name: digest(root / name) for name in HELPERS}


def verify_storage_image(image):
    require_source_inputs(Path(image["source"]), image["source_sha256"])
    require_source_inputs(Path(image["overlay"]), image["overlay_sha256"])
    build = Path(image["build"])
    identities = {image["executable"]: image["executable_sha256"], image["patch"]: image["patch_sha256"],
                  build / "CMakeCache.txt": image["cache_sha256"],
                  build / "compile_commands.json": image["compile_database_sha256"]}
    if any(digest(path) != expected for path, expected in identities.items()):
        raise ValueError("storage executable, overlay patch or build configuration changed")
    if type(image["storage_bits"]) is not int or image["storage_bits"] not in (32, 64):
        raise ValueError("storage image width must be 32 or 64")


def run_storage_profile(image_path, profile, output, timeout=900, retry=False):
    image = json.loads(Path(image_path).read_text())
    definition = storage_definition(profile)
    if image["anchor"] != definition["anchor"] or timeout <= 0:
        raise ValueError("storage profile anchor must match its image and timeout must be positive")
    verify_storage_image(image)
    jobs = storage_jobs(profile)
    context = dict(profile=profile, definition=definition, image=image, helpers=helper_identity())
    context = json.loads(json.dumps(context))
    prepare_dataset(output, context, jobs)
    parser = partial(parse_storage_record, bits=image["storage_bits"])
    with dataset_lock(output):
        run_jobs(output, jobs, image["anchor"], Path(image["executable"]), timeout, retry, parser=parser)
    verify_storage_image(image)
    if helper_identity() != context["helpers"]:
        raise ValueError("storage execution helpers changed during sampling")
