"""Admit previously accepted native binary64 jobs with matching provenance."""

import hashlib
import json
from pathlib import Path

from lsmc_storage_attestation import certified_source_inputs
from lsmc_storage_patch import CORE, ROWS, SOURCE_HASHES
from lsmc_study_build import compile_entry, digest
from lsmc_study_data import Job, parse_record
from lsmc_study_run import verify_image
from lsmc_study_store import require_complete


def require_reference_sources(native, current):
    prefixes = ("dal-cpp/dal/", "dal-cpp/externals/")
    left = {name: value for name, value in native.items() if name.startswith(prefixes)}
    right = {name: value for name, value in current.items() if name.startswith(prefixes)}
    if left != right or left.get(ROWS) != SOURCE_HASHES[ROWS]:
        raise ValueError("native binary64 reference requires identical complete production and dependency sources")


def compile_signature(source, build):
    source, build = Path(source), Path(build)
    database = json.loads((build / "compile_commands.json").read_text())
    command, _ = compile_entry(database, source / CORE)
    return [argument.replace(str(source), "<source>").replace(str(build), "<build>") for argument in command]


def selected_observations(root, manifest, jobs, anchor):
    native_jobs = tuple(Job(**row) for row in manifest["jobs"])
    attempts = require_complete(root, native_jobs)
    indexed = {job.key: (job, attempt) for job, attempt in zip(native_jobs, attempts)}
    if len(indexed) != len(native_jobs) or any(job.key not in indexed for job in jobs):
        raise ValueError("native reference lacks the exact selected storage-study jobs")
    return [(job, parse_record(indexed[job.key][1]["stdout"], job, anchor)) for job in jobs]


def load_native_reference(root, jobs, storage_image, attestation=None):
    root = Path(root)
    manifest = json.loads((root / "profile.json").read_text())
    image = manifest["context"]["image"]
    verify_image(image)
    require_reference_sources(certified_source_inputs(image, attestation), storage_image["source_sha256"])
    native_flags = compile_signature(image["source"], image["build"])
    storage_flags = compile_signature(storage_image["overlay"], storage_image["build"])
    if native_flags != storage_flags or image["anchor"] != storage_image["anchor"]:
        raise ValueError("native reference and storage image have different compile settings or path anchors")
    observations = selected_observations(root, manifest, jobs, image["anchor"])
    verify_image(image)
    keys = json.dumps(sorted(job.key for job in jobs), separators=(",", ":"))
    provenance = dict(manifest_sha256=digest(root / "profile.json"), executable_sha256=image["executable_sha256"],
                      selected_jobs=len(jobs), selected_keys_sha256=hashlib.sha256(keys.encode()).hexdigest(),
                      source_and_dependency_match=True, compile_signature=native_flags)
    provenance["attestation_sha256"] = digest(attestation) if attestation is not None else None
    return observations, provenance
