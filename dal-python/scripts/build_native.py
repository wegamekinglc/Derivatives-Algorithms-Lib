#!/usr/bin/env python3
"""Build the portable native DAL install consumed by cibuildwheel."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess


PACKAGE_ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = PACKAGE_ROOT.parent


def checked_directory(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    if resolved == Path(resolved.anchor) or resolved in {PACKAGE_ROOT, REPOSITORY_ROOT}:
        raise ValueError(f"{label} must be a dedicated build directory, got {resolved}")
    resolved.mkdir(parents=True, exist_ok=True)
    return resolved


def run(command: list[str]) -> None:
    print("+", subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, check=True)


def find_public_config(install_dir: Path) -> Path:
    """Find the installed DAL public CMake package on Unix and Windows layouts."""
    relative = Path("cmake") / "dal-public" / "dal-publicConfig.cmake"
    candidates = tuple(install_dir / lib_dir / relative for lib_dir in ("lib", "lib64"))
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    expected = ", ".join(str(candidate) for candidate in candidates)
    raise RuntimeError(f"native install is missing a DAL public CMake package; checked {expected}")


def build_native(build_dir: Path, install_dir: Path) -> None:
    build_dir = checked_directory(build_dir, "build directory")
    install_dir = checked_directory(install_dir, "install directory")

    configure = [
        "cmake",
        "-S",
        str(REPOSITORY_ROOT),
        "-B",
        str(build_dir),
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={install_dir}",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DDAL_BUILD_PUBLIC=ON",
        "-DDAL_BUILD_PYTHON=OFF",
        "-DDAL_BUILD_EXCEL=OFF",
        "-DDAL_CPP_BUILD_TESTS=OFF",
        "-DDAL_CPP_BUILD_EXAMPLES=OFF",
        "-DDAL_CPP_BUILD_BENCHMARKS=OFF",
        "-DDAL_PUBLIC_BUILD_TESTS=OFF",
        "-DDAL_ENABLE_NATIVE_ARCH=OFF",
        "-DDAL_USE_XAD_AAD=OFF",
        "-DDAL_USE_CODIPACK_AAD=OFF",
        "-DDAL_USE_ADEPT_AAD=OFF",
    ]
    if os.name == "nt":
        configure.extend(["-DMSVC_RUNTIME=static", "-DXAD_STATIC_MSVC_RUNTIME=ON"])

    run(configure)
    run(
        [
            "cmake",
            "--build",
            str(build_dir),
            "--target",
            "dal_public",
            "--parallel",
            str(min(os.cpu_count() or 2, 4)),
        ]
    )
    run(["cmake", "--install", str(build_dir), "--prefix", str(install_dir)])

    find_public_config(install_dir)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--install-dir", required=True, type=Path)
    args = parser.parse_args()
    build_native(args.build_dir, args.install_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
