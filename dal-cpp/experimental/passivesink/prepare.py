#!/usr/bin/env python3
"""Create two new isolated worktrees for the native sink overlay experiment."""

import argparse
from pathlib import Path
import shutil
import subprocess  # nosec B404: git argv only, without shell interpretation


def git(repo, *arguments):
    return subprocess.check_output(["git", "-C", str(repo), *arguments], text=True).strip()  # nosemgrep # nosec B603


def dependencies(repo, commit):
    result = {}
    for name in ["eigen", "googletest", "rapidjson", "machinist"]:
        path = repo / "dal-cpp/externals" / name
        expected = git(repo, "rev-parse", f"{commit}:dal-cpp/externals/{name}")
        if git(path, "rev-parse", "HEAD") != expected or git(path, "status", "--porcelain"):
            raise RuntimeError(f"Dependency {name} must be clean and at the selected gitlink")
        result[name] = path
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--work-root", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()
    repo = args.repo.resolve(strict=True)
    commit = git(repo, "rev-parse", args.commit + "^{commit}")
    deps = dependencies(repo, commit)
    root = args.work_root.resolve()
    root.mkdir(parents=True, exist_ok=False)
    for side in ["base", "head"]:
        source = root / f"{side}-source"
        git(repo, "worktree", "add", "--detach", str(source), commit)
        shutil.copytree(Path(__file__).resolve().parent, source / "dal-cpp/experimental/passivesink", dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns("__pycache__"))
        for name, path in deps.items():
            destination = source / "dal-cpp/externals" / name
            destination.rmdir()
            destination.symlink_to(path, target_is_directory=True)
    git(root / "head-source", "apply", str(Path(__file__).with_name("native-sink.patch").resolve()))
    print(f"Prepared base and sink sources at {commit}; initialized dependencies are shared read-only.")


if __name__ == "__main__":
    main()
