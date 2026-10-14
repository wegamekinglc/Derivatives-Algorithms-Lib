#!/usr/bin/env python3
"""Build a complete, isolated binary32 or binary64 training-storage image."""

import argparse
import difflib
from pathlib import Path

from lsmc_storage_patch import SOURCE_HASHES, storage_sources
from lsmc_study_build import digest, require_source_inputs, source_inputs
from lsmc_study_prepare import build_stock, clone_inputs, dependency_pins, source_identity, validate_destination
from lsmc_study_store import write_json
from lsmc_study_support import count


def apply_overlay(source, bits, anchor, output):
    original = {name: (source / name).read_text() for name in SOURCE_HASHES}
    patched = storage_sources(original, bits, anchor)
    changes = []
    for name, content in patched.items():
        changes.extend(difflib.unified_diff(original[name].splitlines(keepends=True), content.splitlines(keepends=True),
                                           fromfile="stock/" + name, tofile="storage/" + name))
        (source / name).write_text(content)
    patch = output / "storage-overlay.patch"
    patch.write_text("".join(changes))
    return patch


def prepare_storage(source, output, bits, anchor, jobs):
    count(anchor, "training anchor")
    count(jobs, "build jobs", maximum=256)
    if type(bits) is not int or bits not in (32, 64):
        raise ValueError("storage width must be 32 or 64")
    source = Path(source).resolve()
    output = validate_destination(source, output)
    revision, pins = source_identity(source), dependency_pins(source)
    inputs = source_inputs(source)
    output.mkdir(parents=True, exist_ok=False)
    write_json(output / "preparation-inputs.json", dict(source=str(source), revision=revision, dependencies=pins,
                                                       storage_bits=bits, anchor=anchor, source_sha256=inputs))
    overlay = clone_inputs(source, output, revision, pins)
    patch = apply_overlay(overlay, bits, anchor, output)
    overlay_inputs = source_inputs(overlay)
    build, commands = build_stock(overlay, output, jobs)
    require_source_inputs(overlay, overlay_inputs)
    require_source_inputs(source, inputs)
    if source_identity(source) != revision or dependency_pins(source) != pins:
        raise ValueError("supplied source or dependencies changed during storage preparation")
    executable = build / "dal-cpp/benchmarks/script_mc_perf/script_mc_perf"
    identity = dict(storage_bits=bits, anchor=anchor, source=str(source), source_sha256=inputs,
                    overlay=str(overlay), overlay_sha256=overlay_inputs,
                    executable=str(executable), executable_sha256=digest(executable),
                    patch=str(patch), patch_sha256=digest(patch), revision=revision, dependencies=pins,
                    commands=commands, cache_sha256=digest(build / "CMakeCache.txt"),
                    compile_database_sha256=digest(build / "compile_commands.json"), build=str(build))
    write_json(output / "prepared-storage-image.json", identity)
    print(f"prepared complete {bits}-bit storage image: {output / 'prepared-storage-image.json'}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--bits", type=int, choices=(32, 64), required=True)
    parser.add_argument("--anchor", type=int, required=True)
    parser.add_argument("--jobs", type=int, default=4)
    arguments = parser.parse_args()
    prepare_storage(arguments.source, arguments.output, arguments.bits, arguments.anchor, arguments.jobs)


if __name__ == "__main__":
    main()
