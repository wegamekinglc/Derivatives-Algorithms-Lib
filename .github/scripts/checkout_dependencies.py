#!/usr/bin/env python3

import argparse
from pathlib import Path
import re
import subprocess


EIGEN_PATH = "dal-cpp/externals/eigen"
EIGEN_PRIMARY = "https://gitlab.com/libeigen/eigen.git"
EIGEN_MIRROR = "https://github.com/eigen-mirror/eigen.git"


def git(root, *args, capture=False):
    result = subprocess.run(
        ["git", "-C", str(root), *args], check=True, text=True,
        stdout=subprocess.PIPE if capture else None,
    )
    return result.stdout if capture else ""


def eigen_pin(root):
    fields = git(root, "ls-tree", "HEAD", "--", EIGEN_PATH, capture=True).split()
    if (len(fields) != 4 or fields[0:2] != ["160000", "commit"]
            or not re.fullmatch(r"[0-9a-f]{40}", fields[2]) or fields[3] != EIGEN_PATH):
        raise ValueError("Eigen must be pinned by a valid Git submodule commit")
    return fields[2]


def checkout_eigen(root):
    pin = eigen_pin(root)
    destination = root / EIGEN_PATH
    destination.mkdir(parents=True, exist_ok=True)
    if not (destination / ".git").exists():
        git(destination, "init")
    for remote in (EIGEN_PRIMARY, EIGEN_MIRROR):
        try:
            git(destination, "fetch", "--depth=1", remote, pin)
            break
        except subprocess.CalledProcessError:
            if remote == EIGEN_MIRROR:
                raise
            print("Eigen primary download failed; retrying the same pinned commit from its mirror", flush=True)
    git(destination, "checkout", "--detach", pin)
    if git(destination, "rev-parse", "HEAD", capture=True).strip() != pin:
        raise RuntimeError("Eigen checkout does not match the requested submodule commit")
    print(f"Verified Eigen commit {pin}", flush=True)


def checkout_dependencies(root):
    git(root, "submodule", "sync", "--recursive")
    git(root, "-c", "url.https://github.com/.insteadOf=git@github.com:",
        "-c", "submodule.dal-cpp/externals/eigen.update=none",
        "submodule", "update", "--init", "--recursive")
    checkout_eigen(root)


def main():
    parser = argparse.ArgumentParser(description="Check out pinned CI dependencies with an Eigen download fallback")
    parser.add_argument("--worktree", type=Path, default=Path("."))
    args = parser.parse_args()
    checkout_dependencies(args.worktree.resolve())


if __name__ == "__main__":
    main()
