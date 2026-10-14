"""Explicit byte-identical rebuild proof for older reference manifests."""

import json
from pathlib import Path

from lsmc_study_build import digest, require_source_inputs


def require_object_pairs(image, entry):
    pairs = entry["identical_controlled_objects"]
    steps = image["commands"][:2]
    if len(pairs) != 2 or len(steps) != 2:
        raise ValueError("reference attestation requires both controlled objects")
    for pair, step in zip(pairs, steps):
        command = step["command"]
        original = command[command.index("-o") + 1]
        if pair["original"] != original or digest(original) != pair["sha256"] or digest(pair["fresh"]) != pair["sha256"]:
            raise ValueError("reference controlled-object rebuild is not byte-identical")


def certified_entry(image, proof):
    matches = [entry for entry in proof["images"] if entry["anchor"] == image["anchor"] and
               entry["executable_sha256"] == image["executable_sha256"]]
    if len(matches) != 1:
        raise ValueError("reference attestation does not identify exactly this controlled image")
    entry = matches[0]
    if entry["stock_sha256"] != image["stock_sha256"] or digest(entry["fresh_stock"]) != image["stock_sha256"]:
        raise ValueError("reference fresh stock image is not byte-identical")
    if digest(entry["fresh_executable"]) != image["executable_sha256"]:
        raise ValueError("reference relinked controlled image is not byte-identical")
    return entry


def require_native_archive(image, proof):
    archive = proof["identical_native_archive"]
    linked = [(name, value) for name, value in image["reused_sha256"].items() if Path(name).name == "libdal_cpp.a"]
    if linked != [(archive["original"], archive["sha256"])]:
        raise ValueError("reference rebuild must certify the original linked native archive")
    if any(digest(archive[name]) != archive["sha256"] for name in ("original", "fresh")):
        raise ValueError("reference rebuilt native archive is not byte-identical")


def require_rebuild_inputs(proof, path):
    configure = proof["commands"][0]
    build = Path(configure[configure.index("-B") + 1])
    identities = {build / "CMakeCache.txt": proof["cache_sha256"],
                  build / "compile_commands.json": proof["compile_database_sha256"],
                  Path(path).parent / "collector.py": proof["collector_sha256"]}
    if any(digest(name) != expected for name, expected in identities.items()):
        raise ValueError("reference rebuild configuration or collector changed")


def certified_source_inputs(image, path):
    if path is None:
        return image["source_sha256"]
    proof = json.loads(Path(path).read_text())
    if proof.get("byte_identical_stock_and_controlled_objects") is not True or proof["source"] != image["source"]:
        raise ValueError("reference attestation does not certify this source")
    require_source_inputs(Path(proof["source"]), proof["source_sha256"])
    require_native_archive(image, proof)
    require_rebuild_inputs(proof, path)
    require_object_pairs(image, certified_entry(image, proof))
    if any(proof["source_sha256"].get(name) != value for name, value in image["source_sha256"].items()):
        raise ValueError("reference attestation contradicts the original source manifest")
    return proof["source_sha256"]
