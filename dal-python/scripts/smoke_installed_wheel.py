#!/usr/bin/env python3
"""Install and smoke-test one cp39 dal-python wheel in a fresh environment."""

import argparse
import asyncio
from importlib import machinery, metadata
import json
import os
from pathlib import Path
import platform
import shutil
import site
import sys
import sysconfig


SEED_DISTRIBUTIONS = {"pip", "setuptools", "wheel"}


def canonical(path):
    return Path(path).resolve()


def contained_by(path, root):
    try:
        canonical(path).relative_to(canonical(root))
    except ValueError:
        return False
    return True


def validate_smoke_interpreter(implementation, version):
    observed = "%s %s.%s" % (implementation, version[0], version[1])
    if implementation != "CPython" or tuple(version[:2]) != (3, 9):
        raise ValueError("fresh wheel smoke requires CPython 3.9; observed %s" % observed)
    return observed


def validate_runtime_isolation(isolated, user_site_enabled, environment):
    if not isolated:
        raise ValueError("fresh wheel smoke must run Python in isolated mode")
    if user_site_enabled:
        raise ValueError("fresh wheel smoke must disable user site-packages")
    if environment.get("PYTHONPATH") or environment.get("PYTHONHOME"):
        raise ValueError("fresh wheel smoke must unset PYTHONPATH and PYTHONHOME")


def require_new_path(path, label):
    resolved = canonical(path)
    if resolved.exists():
        raise ValueError(
            "%s %r already exists; expected a newly created path" % (label, str(resolved))
        )
    return resolved


def require_path_in_roots(label, resolved, allowed):
    if not any(contained_by(resolved, root) for root in allowed):
        raise ValueError(
            "%s %r is outside fresh site-packages roots %r"
            % (label, str(resolved), [str(root) for root in allowed])
        )


def reject_path_in_roots(label, resolved, excluded):
    if any(contained_by(resolved, root) for root in excluded):
        raise ValueError(
            "%s %r is inside excluded source/test root %r"
            % (label, str(resolved), [str(root) for root in excluded])
        )


def validate_installed_path(label, observed, site_roots, excluded_roots):
    resolved = canonical(observed)
    allowed = [canonical(root) for root in site_roots]
    excluded = [canonical(root) for root in excluded_roots]
    require_path_in_roots(label, resolved, allowed)
    reject_path_in_roots(label, resolved, excluded)
    return resolved


def validate_distribution_inventory(names):
    normalized = {name.lower().replace("_", "-") for name in names}
    allowed = SEED_DISTRIBUTIONS | {"dal-python"}
    if "dal-python" not in normalized:
        raise ValueError("fresh environment does not contain dal-python")
    unexpected = sorted(normalized - allowed)
    if unexpected:
        raise ValueError(
            "fresh environment contains unexpected third-party distributions %r; allowed %r"
            % (unexpected, sorted(allowed))
        )
    return sorted(normalized)


def validate_versions(distribution_version, module_version):
    if distribution_version != module_version:
        raise ValueError(
            "dal-python distribution version %r does not equal dal.__version__ %r"
            % (distribution_version, module_version)
        )


def validate_native_suffix(native_path, extension_suffixes):
    if not any(str(native_path).endswith(suffix) for suffix in extension_suffixes):
        raise ValueError(
            "dal._dal path %r does not use an interpreter extension suffix from %r"
            % (str(native_path), extension_suffixes)
        )


def validate_work_directory(observed, expected):
    work_dir = canonical(observed)
    if work_dir != canonical(expected):
        raise ValueError(
            "fresh wheel smoke working directory %r does not equal %r"
            % (str(work_dir), str(canonical(expected)))
        )
    if any(work_dir.iterdir()):
        raise ValueError("fresh wheel smoke working directory %r is not empty" % str(work_dir))
    return work_dir


def validate_sys_path(entries, excluded_roots):
    excluded = [canonical(root) for root in excluded_roots]
    for entry in entries:
        if not entry:
            continue
        resolved = canonical(entry)
        if any(contained_by(resolved, root) for root in excluded):
            raise ValueError(
                "fresh wheel smoke sys.path entry %r is inside excluded root" % str(resolved)
            )


def validate_runtime_roots(args):
    environment_root = canonical(args.environment)
    if (
        canonical(sys.prefix) != environment_root
        or canonical(sys.base_prefix) == environment_root
    ):
        raise ValueError(
            "fresh wheel smoke prefix %r is not the requested isolated environment %r"
            % (str(canonical(sys.prefix)), str(environment_root))
        )
    work_dir = validate_work_directory(Path.cwd(), args.work_dir)

    excluded_roots = [canonical(path) for path in args.exclude]
    validate_sys_path(sys.path, excluded_roots)

    site_roots = {
        canonical(sysconfig.get_path(name)) for name in ("purelib", "platlib")
    }
    return environment_root, work_dir, excluded_roots, site_roots


def validate_date_operation(dal):
    value = dal.Date_(2024, 1, 2)
    observed_date = (dal.Year(value), dal.Month(value), dal.Day(value))
    if observed_date != (2024, 1, 2):
        raise ValueError("DAL Date_ smoke returned %r" % (observed_date,))


def installed_runtime(site_roots, excluded_roots):
    distribution = metadata.distribution("dal-python")
    distribution_root = validate_installed_path(
        "dal-python distribution root",
        distribution.locate_file(""),
        site_roots,
        excluded_roots,
    )

    import dal
    import dal._dal as native

    package_path = validate_installed_path(
        "dal.__file__", dal.__file__, site_roots, excluded_roots
    )
    native_path = validate_installed_path(
        "dal._dal.__file__", native.__file__, site_roots, excluded_roots
    )
    distribution_version = metadata.version("dal-python")
    validate_versions(distribution_version, dal.__version__)
    validate_native_suffix(native_path, machinery.EXTENSION_SUFFIXES)

    inventory = validate_distribution_inventory(
        distribution.metadata.get("Name")
        for distribution in metadata.distributions()
        if distribution.metadata.get("Name")
    )
    validate_date_operation(dal)
    return {
        "dal_file": package_path,
        "native_file": native_path,
        "distribution_root": distribution_root,
        "distribution_version": distribution_version,
        "module_version": dal.__version__,
        "distributions": inventory,
    }


def runtime_evidence(args, environment_root, work_dir, site_roots, runtime):
    return {
        "environment": str(environment_root),
        "python": sys.version,
        "wheel": str(canonical(args.wheel)),
        "sys_prefix": str(canonical(sys.prefix)),
        "work_dir": str(work_dir),
        "site_packages": sorted(str(root) for root in site_roots),
        "sys_path": [str(canonical(entry)) for entry in sys.path if entry],
        "dal_file": str(runtime["dal_file"]),
        "native_file": str(runtime["native_file"]),
        "distribution_root": str(runtime["distribution_root"]),
        "distribution_version": runtime["distribution_version"],
        "module_version": runtime["module_version"],
        "distributions": runtime["distributions"],
    }


def validate_runtime(args):
    validate_smoke_interpreter(platform.python_implementation(), sys.version_info)
    validate_runtime_isolation(sys.flags.isolated, site.ENABLE_USER_SITE, os.environ)
    environment_root, work_dir, excluded_roots, site_roots = validate_runtime_roots(
        args
    )
    runtime = installed_runtime(site_roots, excluded_roots)
    evidence = runtime_evidence(args, environment_root, work_dir, site_roots, runtime)
    print(json.dumps(evidence, indent=2, sort_keys=True))


def environment_python(environment):
    if os.name == "nt":
        return environment / "Scripts" / "python.exe"
    return environment / "bin" / "python"


def uv_executable():
    executable = shutil.which("uv")
    if executable is None:
        raise ValueError("cp39 fresh smoke requires uv to create an isolated environment")
    executable = canonical(executable)
    if executable.name.lower() not in ("uv", "uv.exe"):
        raise ValueError("unexpected uv executable %r" % str(executable))
    return executable


def executable_path(executable):
    path = Path(executable)
    if not path.is_absolute():
        raise ValueError("process executable must be an absolute path: %r" % str(path))
    return path


async def wait_for_process(executable, arguments, cwd, environment):
    process = await asyncio.create_subprocess_exec(
        str(executable_path(executable)),
        *(str(argument) for argument in arguments),
        cwd=None if cwd is None else str(canonical(cwd)),
        env=environment,
    )
    return await process.wait()


def run_process(executable, arguments, *, cwd=None, environment=None):
    returncode = asyncio.run(wait_for_process(executable, arguments, cwd, environment))
    if returncode:
        raise OSError(
            "command %r exited with status %s" % (str(executable), returncode)
        )


def create_environment(environment, interpreter):
    run_process(
        uv_executable(),
        (
            "venv",
            "--no-project",
            "--python",
            str(interpreter),
            str(environment),
        ),
    )


def install_wheel(python, wheel):
    run_process(
        uv_executable(),
        (
            "pip",
            "install",
            "--python",
            str(python),
            "--no-config",
            str(wheel),
        ),
    )


def resolve_smoke_wheel(wheel, wheelhouse):
    if wheel is None:
        candidates = sorted(canonical(wheelhouse).glob("*-cp39-cp39-*.whl"))
        if len(candidates) != 1:
            raise ValueError(
                "cp39 fresh smoke expected exactly one wheel under %s, found %r"
                % (canonical(wheelhouse), [path.name for path in candidates])
            )
        wheel = candidates[0]
    resolved = canonical(wheel)
    if not resolved.is_file():
        raise ValueError("cp39 fresh smoke wheel does not exist: %s" % resolved)
    if "-cp39-cp39-" not in resolved.name or resolved.suffix != ".whl":
        raise ValueError(
            "cp39 fresh smoke requires a cp39-cp39 wheel; observed %s" % resolved.name
        )
    return resolved


def new_smoke_paths(args):
    environment = require_new_path(args.environment, "target environment")
    work_dir = require_new_path(args.work_dir, "working directory")
    excluded = [canonical(path) for path in args.exclude]
    for label, path in (
        ("target environment", environment),
        ("working directory", work_dir),
    ):
        if any(contained_by(path, root) for root in excluded):
            raise ValueError(
                "%s %r must be outside excluded roots" % (label, str(path))
            )
    return environment, work_dir, excluded


def verification_arguments(wheel, environment, work_dir, excluded):
    arguments = [
        "-I",
        str(canonical(__file__)),
        "--verify",
        "--wheel",
        str(wheel),
        "--environment",
        str(environment),
        "--work-dir",
        str(work_dir),
    ]
    for path in excluded:
        arguments.extend(("--exclude", str(path)))
    return arguments


def isolated_process_environment():
    environment = os.environ.copy()
    environment.pop("PYTHONPATH", None)
    environment.pop("PYTHONHOME", None)
    environment["PYTHONNOUSERSITE"] = "1"
    return environment


def create_and_run(args):
    validate_smoke_interpreter(platform.python_implementation(), sys.version_info)
    wheel = resolve_smoke_wheel(args.wheel, args.wheelhouse)
    environment, work_dir, excluded = new_smoke_paths(args)
    create_environment(environment, Path(sys.executable))
    work_dir.mkdir(parents=True)
    python = environment_python(environment)
    install_wheel(python, wheel)
    run_process(
        python,
        verification_arguments(wheel, environment, work_dir, excluded),
        cwd=work_dir,
        environment=isolated_process_environment(),
    )


def main():
    parser = argparse.ArgumentParser()
    wheel_source = parser.add_mutually_exclusive_group(required=True)
    wheel_source.add_argument("--wheel", type=Path)
    wheel_source.add_argument("--wheelhouse", type=Path)
    parser.add_argument("--environment", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--exclude", type=Path, action="append", default=[])
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    try:
        if args.verify:
            validate_runtime(args)
        else:
            create_and_run(args)
    except (OSError, ValueError) as error:
        print("cp39 fresh smoke failed: %s" % error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
