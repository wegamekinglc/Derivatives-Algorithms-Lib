"""Build an isolated static control image with two replacement objects."""

import difflib
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess  # nosec B404: trusted local compiler/build tools, without a shell

from lsmc_study_support import CONTROL_MACRO, controlled_source


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def program(name):
    executable = shutil.which(name)
    if executable is None:
        raise ValueError(f"missing required local build tool: {name}")
    return str(Path(executable).resolve())


def cache_value(build, name):
    values = [line.split("=", 1)[1] for line in (build / "CMakeCache.txt").read_text().splitlines()
              if line.startswith(name + ":") and "=" in line]
    if len(values) != 1:
        raise ValueError(f"missing or ambiguous CMake cache value: {name}")
    return values[0]


def validate_compile(command):
    if command.count("-c") != 1 or command.count("-o") != 1 or any(CONTROL_MACRO in arg for arg in command):
        raise ValueError("unrecognized or already controlled compile command")


def compile_entry(database, source):
    matches = [row for row in database if Path(row["file"]).resolve() == source.resolve()]
    if len(matches) != 1:
        raise ValueError(f"expected one compile command for {source}")
    row = matches[0]
    command = row.get("arguments") or shlex.split(row["command"])
    validate_compile(command)
    return command, Path(row["directory"])


def replacement_compile(command, source, output, anchor):
    command = list(command)
    command[command.index("-c") + 1] = str(source)
    command[command.index("-o") + 1] = str(output)
    command.insert(1, f"-D{CONTROL_MACRO}={anchor}")
    if "-MF" in command:
        command[command.index("-MF") + 1] = str(output.with_suffix(".d"))
    if "-MT" in command:
        command[command.index("-MT") + 1] = str(output)
    return command


def unsafe_link_option(argument):
    forbidden = ("whole-archive", "allow-multiple-definition", "muldefs")
    return argument.startswith("@") or any(flag in argument for flag in forbidden)


def link_arguments(command, output):
    if command.count("-o") != 1 or any(unsafe_link_option(arg) for arg in command):
        raise ValueError("unrecognized static link command")
    arguments = [arg for arg in command if not arg.startswith("-Wl,--dependency-file=")]
    position = arguments.index("-o") + 1
    if position >= len(arguments):
        raise ValueError("missing link output")
    arguments[position] = str(output)
    return arguments


def linked_input(argument, directory, old_sample, new_sample, control):
    path = (directory / argument).resolve()
    if path == old_sample:
        return [str(new_sample)], []
    if path.name == "libdal_cpp.a":
        return [str(control), argument], [path]
    return [argument], [path] if path.suffix in (".o", ".a") else []


def replacement_link(command, directory, old_sample, new_sample, control, output):
    command = link_arguments(command, output)
    paths = [(directory / arg).resolve() for arg in command]
    if paths.count(old_sample) != 1 or sum(path.name == "libdal_cpp.a" for path in paths) != 1:
        raise ValueError("expected one sample object and one static DAL library")
    arguments, reused = [], []
    for argument in command:
        replacement, inputs = linked_input(argument, directory, old_sample, new_sample, control)
        arguments.extend(replacement)
        reused.extend(inputs)
    return arguments, reused


def run_logged(command, directory, log):
    with log.open("w") as output:
        # Commands come from the owned CMake build or fixed Git/build operations.
        result = subprocess.run(command, cwd=directory, stdout=output, stderr=subprocess.STDOUT, shell=False)  # nosemgrep  # nosec B603
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}); retained log: {log}")


def tracked_files(source, *paths):
    return subprocess.check_output([program("git"), "ls-files", "--", *paths], cwd=source, text=True, shell=False).splitlines()  # nosemgrep  # nosec B603


def dependency_sources(source):
    names = []
    for dependency, include in (("eigen", "Eigen"), ("rapidjson", "include")):
        relative = Path("dal-cpp/externals") / dependency
        names.extend((relative / name).as_posix() for name in tracked_files(source / relative, include))
    return names


def source_inputs(source):
    configuration = ("CMakeLists.txt", "CMakePresets.json", "dal-cpp/CMakeLists.txt", "dal-cpp/benchmarks/CMakeLists.txt")
    names = tracked_files(source, "dal-cpp/dal", *configuration)
    names += dependency_sources(source)
    benchmark = source / "dal-cpp/benchmarks/script_mc_perf"
    names += [path.relative_to(source).as_posix() for path in benchmark.iterdir()
              if path.suffix in (".cpp", ".hpp") or path.name == "CMakeLists.txt"]
    return {name: digest(source / name) for name in names}


def require_source_inputs(source, inputs):
    if any(digest(source / name) != value for name, value in inputs.items()):
        raise ValueError("source changed during control preparation")


def control_commands(source, build, destination, anchor):
    database = json.loads((build / "compile_commands.json").read_text())
    core = source / "dal-cpp/dal/script/lsmc.cpp"
    sample = source / "dal-cpp/benchmarks/script_mc_perf/lsmctrainingstudy.cpp"
    core_command, core_directory = compile_entry(database, core)
    sample_command, sample_directory = compile_entry(database, sample)
    old_sample = (sample_directory / sample_command[sample_command.index("-o") + 1]).resolve()
    core_object = destination / "lsmc-control.o"
    sample_object = destination / "sample-control.o"
    compile_commands = (
        (replacement_compile(core_command, destination / "lsmc.cpp", core_object, anchor), core_directory),
        (replacement_compile(sample_command, destination / "lsmctrainingstudy.cpp", sample_object, anchor), sample_directory),
    )
    link_directory = build / "dal-cpp/benchmarks/script_mc_perf"
    link = shlex.split((link_directory / "CMakeFiles/script_mc_perf.dir/link.txt").read_text())
    output = destination / "script_mc_perf"
    link_command, reused = replacement_link(link, link_directory, old_sample, sample_object, core_object, output)
    return compile_commands, (link_command, link_directory), reused


def validate_build(build):
    if cache_value(build, "BUILD_SHARED_LIBS").upper() != "OFF" or cache_value(build, "CMAKE_GENERATOR") != "Unix Makefiles":
        raise ValueError("control preparation requires a static Unix Makefiles build")
    if cache_value(build, "CMAKE_BUILD_TYPE") != "Release":
        raise ValueError("control preparation requires Release")


def build_control(source, build, destination, anchor):
    source, build, destination = (Path(path).resolve() for path in (source, build, destination))
    validate_build(build)
    original = (source / "dal-cpp/dal/script/lsmc.cpp").read_bytes().decode()
    patched = controlled_source(original, anchor)
    inputs = source_inputs(source)
    stock_binary = build / "dal-cpp/benchmarks/script_mc_perf/script_mc_perf"
    stock_identity = digest(stock_binary)
    destination.mkdir(parents=True, exist_ok=False)
    (destination / "lsmc.cpp").write_text(patched)
    for name in ("lsmctrainingstudy.cpp", "lsmctrainingstudy.hpp"):
        data = (source / "dal-cpp/benchmarks/script_mc_perf" / name).read_bytes()
        (destination / name).write_bytes(data)
    patch = "".join(difflib.unified_diff(original.splitlines(keepends=True), patched.splitlines(keepends=True),
                                       fromfile="stock/dal-cpp/dal/script/lsmc.cpp", tofile="control/lsmc.cpp"))
    (destination / "address-control.patch").write_text(patch)
    compile_commands, link, reused = control_commands(source, build, destination, anchor)
    reused_identity = {str(path): digest(path) for path in reused}
    commands = []
    for index, (command, directory) in enumerate((*compile_commands, link)):
        run_logged(command, directory, destination / f"build-{index}.log")
        commands.append(dict(command=command, directory=str(directory)))
        require_source_inputs(source, inputs)
    if digest(stock_binary) != stock_identity or any(digest(path) != value for path, value in reused_identity.items()):
        raise ValueError("stock binary or reused objects changed during preparation")
    executable = destination / "script_mc_perf"
    receipt = dict(anchor=anchor, source=str(source), build=str(build), stock_binary=str(stock_binary),
                   stock_sha256=stock_identity, executable=str(executable), executable_sha256=digest(executable),
                   source_sha256=inputs, reused_sha256=reused_identity, commands=commands,
                   patch_sha256=digest(destination / "address-control.patch"),
                   controlled_source_sha256=digest(destination / "lsmc.cpp"),
                   compiler=subprocess.check_output([compile_commands[0][0][0], "--version"], text=True, shell=False))  # nosemgrep  # nosec B603
    (destination / "identity.json").write_text(json.dumps(receipt, indent=2))
    return receipt
