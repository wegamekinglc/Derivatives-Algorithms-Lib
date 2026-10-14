#!/usr/bin/env python3
"""Build a committed study in an owned clone, leaving supplied inputs untouched."""

import argparse
from pathlib import Path
import subprocess  # nosec B404: fixed local Git/CMake tools, without a shell

from lsmc_study_build import build_control, digest, program, run_logged, source_inputs, require_source_inputs
from lsmc_study_store import write_json
from lsmc_study_support import count

DEPENDENCIES = ("eigen", "googletest", "machinist", "rapidjson")


def git(source, *arguments):
    return subprocess.check_output([program("git"), *arguments], cwd=source, text=True, shell=False).strip()  # nosemgrep  # nosec B603


def source_identity(source):
    if git(source, "status", "--porcelain", "--untracked-files=no"):
        raise ValueError("study preparation requires committed, clean tracked inputs")
    untracked = git(source, "ls-files", "--others", "--exclude-standard", "--", "dal-cpp/benchmarks/script_mc_perf")
    if untracked:
        raise ValueError("untracked study inputs would be omitted from the isolated clone")
    return dict(commit=git(source, "rev-parse", "HEAD"), tree=git(source, "rev-parse", "HEAD^{tree}"))


def validate_destination(source, output):
    source, output = Path(source).resolve(), Path(output).resolve()
    if output.exists() or source == output or source in output.parents or output in source.parents:
        raise ValueError("output must be a new directory outside the supplied source")
    return output


def validate_dependency(source, pin):
    if git(source, "rev-parse", "HEAD") != pin:
        raise ValueError(f"dependency does not match its committed pin: {source}")
    if git(source, "status", "--porcelain", "--untracked-files=all"):
        raise ValueError(f"dependency must be clean: {source}")


def dependency_pins(source):
    result = {}
    for name in DEPENDENCIES:
        relative = "dal-cpp/externals/" + name
        entry = git(source, "ls-files", "--stage", "--", relative).split()
        if len(entry) != 4 or entry[0] != "160000" or entry[3] != relative:
            raise ValueError(f"missing committed dependency pin: {relative}")
        validate_dependency(source / relative, entry[1])
        result[relative] = entry[1]
    return result


def clone_inputs(source, root, identity, pins):
    stock = root / "stock-source"
    run_logged([program("git"), "clone", "--no-local", "--no-checkout", str(source), str(stock)], root, root / "clone.log")
    run_logged([program("git"), "checkout", "--detach", identity["commit"]], stock, root / "checkout.log")
    for relative, pin in pins.items():
        name = Path(relative).name
        target = stock / relative
        run_logged([program("git"), "clone", "--no-local", str(source / relative), str(target)], root, root / f"clone-{name}.log")
        run_logged([program("git"), "checkout", "--detach", pin], target, root / f"checkout-{name}.log")
        validate_dependency(target, pin)
    return stock


def build_stock(stock, root, jobs):
    build = root / "stock-build"
    configure = [program("cmake"), "--preset", "Release-linux", "-G", "Unix Makefiles", "-S", str(stock), "-B", str(build),
                 "-DBUILD_SHARED_LIBS=OFF", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DCMAKE_CXX_COMPILER_LAUNCHER=",
                 "-DDAL_CPP_BUILD_BENCHMARKS=ON", "-DDAL_CPP_BUILD_TESTS=OFF", "-DDAL_CPP_BUILD_EXAMPLES=OFF",
                 "-DDAL_BUILD_PUBLIC=OFF", "-DDAL_BUILD_PYTHON=OFF", "-DDAL_BUILD_EXCEL=OFF",
                 "-DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF", "-DDAL_USE_EIGEN=ON", "-DDAL_ENABLE_AAD_PROFILING=OFF"]
    compile_command = [program("cmake"), "--build", str(build), "--target", "script_mc_perf", "--parallel", str(jobs)]
    inputs = source_inputs(stock)
    for name, command in (("configure", configure), ("compile", compile_command)):
        run_logged(command, root, root / (name + ".log"))
        require_source_inputs(stock, inputs)
    return build, [configure, compile_command]


def prepare(source, output, anchor, jobs):
    count(anchor, "training anchor")
    count(jobs, "build jobs", maximum=256)
    source = Path(source).resolve()
    output = validate_destination(source, output)
    identity = source_identity(source)
    pins = dependency_pins(source)
    output.mkdir(parents=True, exist_ok=False)
    write_json(output / "preparation-inputs.json", dict(source=str(source), revision=identity, dependencies=pins, anchor=anchor))
    stock = clone_inputs(source, output, identity, pins)
    build, commands = build_stock(stock, output, jobs)
    if source_identity(source) != identity or dependency_pins(source) != pins:
        raise ValueError("supplied source or dependencies changed during preparation")
    if source_identity(stock) != identity:
        raise ValueError("isolated source revision differs from the supplied source")
    control = build_control(stock, build, output / ("control-" + str(anchor)), anchor)
    control["preparation"] = dict(revision=identity, dependencies=pins, commands=commands,
                                  cache_sha256=digest(build / "CMakeCache.txt"),
                                  compile_database_sha256=digest(build / "compile_commands.json"),
                                  cmake=subprocess.check_output([program("cmake"), "--version"], text=True, shell=False))  # nosemgrep  # nosec B603
    write_json(output / "prepared-image.json", control)
    print(f"prepared immutable stock/control image: {output / 'prepared-image.json'}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--anchor", required=True, type=int)
    parser.add_argument("--jobs", default=4, type=int)
    arguments = parser.parse_args()
    prepare(arguments.source, arguments.output, arguments.anchor, arguments.jobs)


if __name__ == "__main__":
    main()
