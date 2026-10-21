#!/usr/bin/env python3
"""Build the pinned InFine native comparison worker in a new output directory."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

from dal_comparisons.infine import (
    ASSEMBLY,
    BUILD_SCHEMA,
    ENVIRONMENT,
    NATIVE_ROOT,
    REVISION,
    SDK_VERSION,
    adapter_hashes,
    binary_files,
    sha256,
)
from dal_benchmarks.harness import require


def git(source, *args):
    return subprocess.check_output(["git", "-C", str(source), *args], text=True).strip()


def verify_source(source):
    require(
        git(source, "rev-parse", "HEAD") == REVISION, "InFine source revision mismatch"
    )
    require(
        not git(source, "status", "--porcelain", "--untracked-files=normal"),
        "InFine source checkout must be clean",
    )


def runtime_files(dotnet):
    lines = subprocess.check_output(
        [str(dotnet), "--list-runtimes"], text=True
    ).splitlines()
    entries = [line.split(maxsplit=2) for line in lines]
    candidates = [
        entry
        for entry in entries
        if len(entry) == 3
        and entry[0] == "Microsoft.NETCore.App"
        and entry[1].startswith("8.")
        and all(part.isdigit() for part in entry[1].split("."))
    ]
    require(bool(candidates), "InFine requires the .NET 8 runtime")
    _, version, parent = max(
        candidates, key=lambda fields: tuple(map(int, fields[1].split(".")))
    )
    directory = Path(parent.strip("[]")) / version
    paths = {
        dotnet,
        *directory.glob("*.dll"),
        *directory.glob("*.so"),
        *directory.glob("*.dylib"),
    }
    return version, {str(path): sha256(path) for path in sorted(paths)}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dotnet", default="dotnet")
    args = parser.parse_args(argv)
    source, output = args.source.resolve(), args.output.resolve()
    executable = shutil.which(args.dotnet)
    require(executable is not None, "dotnet SDK missing")
    dotnet = Path(executable).resolve()
    verify_source(source)
    hashes = adapter_hashes()
    require("packages.lock.json" in hashes, "InFine NuGet lock missing")
    output.mkdir(parents=True, exist_ok=False)
    (output / "global.json").write_text(
        json.dumps({"sdk": {"version": SDK_VERSION, "rollForward": "disable"}}) + "\n"
    )
    command = [
        str(dotnet),
        "publish",
        str(NATIVE_ROOT / "Dal.Infine.Benchmarks.csproj"),
        "--configuration",
        "Release",
        "--artifacts-path",
        str(output / "artifacts"),
        "--output",
        str(output / "bin"),
        "--disable-build-servers",
        f"-p:QflSource={source}",
    ]
    with (output / "build.log").open("w", encoding="utf-8") as log:
        subprocess.run(
            command,
            shell=False,
            check=True,
            stdout=log,
            stderr=subprocess.STDOUT,
            cwd=output,
            env=dict(os.environ, **ENVIRONMENT),
            timeout=600,
        )
    verify_source(source)
    require(hashes == adapter_hashes(), "InFine adapter changed during build")
    require((output / "bin" / ASSEMBLY).is_file(), "InFine native worker missing")
    runtime, runtime_identity = runtime_files(dotnet)
    manifest = {
        "schema": BUILD_SCHEMA,
        "revision": REVISION,
        "configuration": "Release",
        "dotnet": str(dotnet),
        "sdk": subprocess.check_output(
            [str(dotnet), "--version"], cwd=output, text=True
        ).strip(),
        "runtime": runtime,
        "runtime_files": runtime_identity,
        "adapter_hashes": hashes,
        "files": {path.name: sha256(path) for path in binary_files(output / "bin")},
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Built InFine {REVISION} with .NET {manifest['sdk']}: {output}")


if __name__ == "__main__":
    main()
