#!/usr/bin/env python3
"""Install the private DAL-469 toolchain, retaining required and optional failures."""

import os
from pathlib import Path
import sys

from diagnose_dal469 import require, run_command, write_json


ORIGINAL_VERSION = "14.2.0"
ORIGINAL_PACKAGE = "14.2.0-4ubuntu2~24.04.1"
COMPILERS = ("gcc-14", "g++-14")


class SetupFailure(RuntimeError):
    def __init__(self, name, returncode):
        super().__init__(f"{name} failed (exit {returncode}); see retained setup logs")
        self.returncode = returncode


def checked_command(argv, output, name):
    status = run_command(argv, output / f"{name}.log")
    if status:
        raise SetupFailure(name, status)


def compiler_versions(output, label):
    versions = {}
    for compiler in COMPILERS:
        log = output / f"{label}-{compiler}.log"
        status = run_command([compiler, "-dumpfullversion", "-dumpversion"], log)
        versions[compiler] = log.read_text().strip() if status == 0 else None
    return versions


def original_packages_available(output):
    checked_command(["apt-cache", "madison", *COMPILERS], output, "apt-compiler-versions")
    available = {
        fields[0].strip() for line in (output / "apt-compiler-versions.log").read_text().splitlines()
        if len(fields := line.split("|")) >= 2 and fields[1].strip() == ORIGINAL_PACKAGE
    }
    return all(compiler in available for compiler in COMPILERS)


def install_compilers(output, state):
    exact = original_packages_available(output)
    state["selection"] = "install-original" if exact else "available-gcc-14"
    if not exact:
        state["fallback_reason"] = f"Original compiler package {ORIGINAL_PACKAGE} unavailable in refreshed index"
        print(f"::warning::{state['fallback_reason']}; actual compiler versions will be retained.")
    packages = [f"{compiler}={ORIGINAL_PACKAGE}" if exact else compiler for compiler in COMPILERS]
    # Installation/network failures are fatal; only version availability selects fallback.
    checked_command(["sudo", "apt-get", "install", "-y", *packages], output, "apt-install-compilers")


def verify_compilers(versions):
    values = tuple(versions.values())
    require(all(value and value.startswith("14.") for value in values) and len(set(values)) == 1,
            f"GCC/G++ 14 verification failed: {versions}")


def select_compilers(output, state):
    state["before"] = compiler_versions(output, "before")
    if all(value == ORIGINAL_VERSION for value in state["before"].values()):
        state["selection"] = "installed-original"
    else:
        install_compilers(output, state)
    state["actual"] = compiler_versions(output, "actual")
    verify_compilers(state["actual"])
    state["differences_from_original"] = {
        compiler: {"expected": ORIGINAL_VERSION, "actual": value}
        for compiler, value in state["actual"].items() if value != ORIGINAL_VERSION
    }


def install_optional_profilers(output):
    capabilities = {}
    for tool, package in (("gdb", "gdb"), ("perf", "linux-tools-generic")):
        install_status = run_command(["sudo", "apt-get", "install", "-y", package],
                                     output / f"apt-install-{tool}.log")
        version_status = run_command([tool, "--version"], output / f"version-{tool}.log")
        capabilities[tool] = {"install_status": install_status, "version_status": version_status,
                              "available": version_status == 0}
        if install_status or version_status:
            print(f"::warning::Optional {tool}: install exit {install_status}, version exit {version_status}; "
                  "capability gap retained, required setup continues.")
    return capabilities


def main():
    output = Path.cwd() / "diagnostic-results/setup"
    state = {"original_ci_exit_status": 1, "toolchain": {"requested_package": ORIGINAL_PACKAGE}}
    status = 0
    try:
        require(os.environ.get("GITHUB_EVENT_NAME") == "workflow_dispatch"
                and os.environ.get("DAL469_MODE") == "dal-469", "requires manual dal-469 mode")
        checked_command(["sudo", "apt-get", "update", "-o", "APT::Update::Error-Mode=any"], output, "apt-update")
        checked_command(["apt-cache", "policy", *COMPILERS], output, "apt-policy")
        select_compilers(output, state["toolchain"])
        checked_command(["sudo", "apt-get", "install", "-y", "make", "time"], output, "apt-install-required")
        checked_command(["make", "--version"], output, "version-make")
        checked_command(["/usr/bin/time", "--version"], output, "version-time")
        state["required_status"] = 0
        state["optional"] = install_optional_profilers(output)
    except (OSError, ValueError, RuntimeError) as error:
        status = error.returncode if isinstance(error, SetupFailure) else 1
        state["required_status"] = status
        state["error"] = str(error)
        print(error, file=sys.stderr)
    state["returncode"] = status
    write_json(output / "status.json", state)
    return status


if __name__ == "__main__":
    raise SystemExit(main())
