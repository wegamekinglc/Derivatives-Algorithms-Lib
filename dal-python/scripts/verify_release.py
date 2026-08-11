#!/usr/bin/env python3
"""Validate the complete dal-python wheel set before a PyPI upload."""

from __future__ import annotations

import argparse
from collections import Counter
from email.parser import BytesParser
from hashlib import sha256
from http.client import HTTPSConnection
from pathlib import Path
import re
import sys
import tomllib
from urllib.parse import quote
from zipfile import ZipFile


PACKAGE_ROOT = Path(__file__).resolve().parents[1]
PYPROJECT = PACKAGE_ROOT / "pyproject.toml"
INIT = PACKAGE_ROOT / "src" / "dal" / "__init__.py"
VERSION_RE = re.compile(r'^__version__\s*=\s*["\']([^"\']+)["\']\s*$', re.MULTILINE)
LINUX_PLATFORM = "manylinux_2_28_x86_64"
WINDOWS_PLATFORM = "win_amd64"
PYTHON_TAG_RE = re.compile(r"cp[1-9][0-9]+")
MANYLINUX_PLATFORM_RE = re.compile(r"manylinux_([0-9]+)_([0-9]+)_x86_64")


def project_configuration() -> tuple[str, str, tuple[str, ...]]:
    with PYPROJECT.open("rb") as stream:
        config = tomllib.load(stream)
    project = config["project"]
    build_selectors = config["tool"]["cibuildwheel"]["build"]
    python_tags = tuple(selector.removesuffix("-*") for selector in build_selectors)
    return project["version"], project["requires-python"], python_tags


def source_version() -> str:
    match = VERSION_RE.search(INIT.read_text(encoding="utf-8"))
    if match is None:
        raise ValueError(f"{INIT} does not declare __version__")
    return match.group(1)


def validate_expected_python_tags(
    python_tags: tuple[str, ...], configured_python_tags: tuple[str, ...]
) -> tuple[str, ...]:
    if not python_tags:
        raise ValueError(
            "--expected-python: explicit value is empty; omit the option for the configured "
            "full set or supply a nonempty configured subset"
        )
    seen: set[str] = set()
    for python_tag in python_tags:
        if not python_tag or python_tag != python_tag.strip():
            raise ValueError(f"--expected-python: malformed selector {python_tag!r}")
        if PYTHON_TAG_RE.fullmatch(python_tag) is None:
            raise ValueError(f"--expected-python: malformed selector {python_tag!r}")
        if python_tag in seen:
            raise ValueError(f"--expected-python: duplicate selector {python_tag!r}")
        if python_tag not in configured_python_tags:
            configured = ", ".join(configured_python_tags)
            raise ValueError(
                f"--expected-python: selector {python_tag!r} is not configured; "
                f"configured tags are {configured}"
            )
        seen.add(python_tag)
    return python_tags


def wheel_parts(path: Path) -> tuple[str, str, str, str, str]:
    if path.suffix != ".whl":
        raise ValueError(f"not a wheel: {path}")
    parts = path.name[:-4].rsplit("-", 4)
    if len(parts) != 5:
        raise ValueError(f"invalid wheel filename: {path.name}")
    return tuple(parts)  # type: ignore[return-value]


def metadata_from_wheel(archive: ZipFile, suffix: str):
    matches = [name for name in archive.namelist() if name.endswith(suffix)]
    if len(matches) != 1:
        raise ValueError(f"wheel must contain exactly one {suffix}, found {matches}")
    return BytesParser().parsebytes(archive.read(matches[0]))


def platform_family(platform_tag: str) -> str:
    components = platform_tag.split(".")
    if not components or any(not component for component in components):
        raise ValueError(f"platform field {platform_tag!r} contains an empty component")
    if len(components) != len(set(components)):
        raise ValueError(f"platform field {platform_tag!r} contains duplicate components")
    if LINUX_PLATFORM in components:
        for component in components:
            match = MANYLINUX_PLATFORM_RE.fullmatch(component)
            if match is None:
                raise ValueError(
                    f"platform component {component!r} is not a PEP 600 x86-64 component"
                )
            glibc_version = tuple(int(part) for part in match.groups())
            if glibc_version > (2, 28):
                raise ValueError(
                    f"platform component {component!r} exceeds the manylinux 2.28 baseline"
                )
        return "linux"
    if components == [WINDOWS_PLATFORM]:
        return "windows"
    if WINDOWS_PLATFORM in components:
        raise ValueError(
            f"Windows platform must be exactly {WINDOWS_PLATFORM!r}, observed {platform_tag!r}"
        )
    raise ValueError(f"unsupported or unrepaired wheel platform tag: {platform_tag}")


def validate_wheel_filename(
    path: Path, expected_version: str, expected_requires_python: str
) -> tuple[str, str, str, str]:
    distribution, version, python_tag, abi_tag, platform_tag = wheel_parts(path)
    if distribution != "dal_python":
        raise ValueError(f"{path.name}: unexpected distribution {distribution}")
    if version != expected_version:
        raise ValueError(f"{path.name}: version {version} != {expected_version}")
    if PYTHON_TAG_RE.fullmatch(python_tag) is None or PYTHON_TAG_RE.fullmatch(abi_tag) is None:
        raise ValueError(
            f"{path.name}: Python and ABI fields must each contain one single CPython tag; "
            f"observed {python_tag!r} and {abi_tag!r}"
        )
    if abi_tag != python_tag:
        raise ValueError(f"{path.name}: ABI {abi_tag} does not match {python_tag}")
    family = platform_family(platform_tag)
    return python_tag, abi_tag, platform_tag, family


def validate_native_extension(path: Path, names: list[str], family: str) -> None:
    extension_suffix = ".pyd" if family == "windows" else ".so"
    extensions = [
        name for name in names if name.startswith("dal/_dal.") and name.endswith(extension_suffix)
    ]
    if len(extensions) != 1:
        raise ValueError(f"{path.name}: expected one compiled _dal extension, found {extensions}")


def normalized_requires_python(value: str) -> frozenset[str]:
    return frozenset(specifier.strip() for specifier in value.split(","))


def single_header(path: Path, metadata, key: str) -> str:
    values = metadata.get_all(key, [])
    if len(values) != 1:
        raise ValueError(f"{path.name}: METADATA has {len(values)} {key} headers; expected exactly 1")
    return values[0]


def validate_package_metadata(
    path: Path, metadata, expected_version: str, expected_requires_python: str
) -> None:
    expected_metadata = {
        "Name": "dal-python",
        "Version": expected_version,
        "License-Expression": "MIT",
        "Requires-Python": expected_requires_python,
    }
    for key, expected in expected_metadata.items():
        actual = single_header(path, metadata, key)
        equivalent = actual == expected
        if key == "Requires-Python":
            actual_specifiers = tuple(specifier.strip() for specifier in actual.split(","))
            expected_specifiers = normalized_requires_python(expected)
            equivalent = (
                len(actual_specifiers) == len(expected_specifiers)
                and len(set(actual_specifiers)) == len(actual_specifiers)
                and set(actual_specifiers) == expected_specifiers
            )
        if not equivalent:
            raise ValueError(f"{path.name}: {key}={actual!r}, expected {expected!r}")
    if not str(metadata.get_payload()).strip():
        raise ValueError(f"{path.name}: package description is empty")


def validate_wheel_metadata(
    path: Path, wheel_metadata, python_tag: str, abi_tag: str, platform_tag: str
) -> None:
    wheel_versions = wheel_metadata.get_all("Wheel-Version", [])
    if len(wheel_versions) != 1:
        raise ValueError(
            f"{path.name}: WHEEL has {len(wheel_versions)} Wheel-Version headers; expected exactly 1"
        )
    purelib_values = wheel_metadata.get_all("Root-Is-Purelib", [])
    if len(purelib_values) != 1:
        raise ValueError(
            f"{path.name}: WHEEL has {len(purelib_values)} Root-Is-Purelib headers; expected exactly 1"
        )
    if purelib_values[0] != "false":
        raise ValueError(f"{path.name}: native wheel is incorrectly marked pure")
    filename_tags = [
        f"{python_tag}-{abi_tag}-{platform}" for platform in platform_tag.split(".")
    ]
    wheel_tags = wheel_metadata.get_all("Tag", [])
    if Counter(wheel_tags) != Counter(filename_tags):
        raise ValueError(
            f"{path.name}: WHEEL tags {wheel_tags!r} must equal expanded filename tags "
            f"{filename_tags!r} in contents and cardinality"
        )


def validate_wheel(
    path: Path, expected_version: str, expected_requires_python: str
) -> tuple[str, str, str]:
    python_tag, abi_tag, platform_tag, family = validate_wheel_filename(
        path, expected_version, expected_requires_python
    )

    with ZipFile(path) as archive:
        names = archive.namelist()
        validate_native_extension(path, names, family)
        metadata = metadata_from_wheel(archive, "/METADATA")
        wheel_metadata = metadata_from_wheel(archive, "/WHEEL")
        validate_package_metadata(path, metadata, expected_version, expected_requires_python)
        validate_wheel_metadata(path, wheel_metadata, python_tag, abi_tag, platform_tag)

    digest = sha256(path.read_bytes()).hexdigest()
    return python_tag, family, digest


def ensure_version_is_new_on_pypi(version: str) -> None:
    path = f"/pypi/dal-python/{quote(version, safe='')}/json"
    connection = HTTPSConnection("pypi.org", timeout=20)
    try:
        connection.request("GET", path, headers={"Accept": "application/json"})
        response = connection.getresponse()
        response.read()
    finally:
        connection.close()
    if response.status == 404:
        return
    if response.status != 200:
        raise OSError(f"PyPI version check failed with HTTP {response.status}")
    raise ValueError(f"dal-python {version} already exists on PyPI and cannot be overwritten")


def validate_versions(tag: str | None = None, check_pypi: bool = False) -> tuple[str, str, tuple[str, ...]]:
    version, requires_python, configured_python_tags = project_configuration()
    declared_source_version = source_version()
    if declared_source_version != version:
        raise ValueError(
            f"dal.__version__={declared_source_version} does not match project version={version}"
        )
    if tag is not None and tag != f"dal-python-v{version}":
        raise ValueError(f"release tag {tag!r} must equal dal-python-v{version}")
    if check_pypi:
        ensure_version_is_new_on_pypi(version)
    return version, requires_python, configured_python_tags


def validate_release(
    dist_dir: Path,
    expected_python_tags: tuple[str, ...] | None = None,
    tag: str | None = None,
    check_pypi: bool = False,
) -> list[str]:
    version, requires_python, configured_python_tags = validate_versions(tag, check_pypi)

    python_tags = (
        configured_python_tags
        if expected_python_tags is None
        else validate_expected_python_tags(expected_python_tags, configured_python_tags)
    )
    wheels = sorted(dist_dir.glob("*.whl"))
    if not wheels:
        raise ValueError(f"no wheels found under {dist_dir}")

    actual: set[tuple[str, str]] = set()
    manifest: list[str] = []
    for wheel in wheels:
        python_tag, family, digest = validate_wheel(wheel, version, requires_python)
        target = (python_tag, family)
        if target in actual:
            raise ValueError(f"duplicate wheel target {target}")
        actual.add(target)
        manifest.append(f"{digest}  {wheel.name}")

    expected = {(python_tag, family) for python_tag in python_tags for family in ("linux", "windows")}
    if actual != expected:
        raise ValueError(
            f"wheel matrix mismatch: missing={sorted(expected - actual)}, "
            f"unexpected={sorted(actual - expected)}"
        )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dist-dir", type=Path)
    parser.add_argument("--expected-python", help="comma-separated CPython tags")
    parser.add_argument("--tag")
    parser.add_argument("--check-pypi", action="store_true")
    parser.add_argument("--version-only", action="store_true")
    args = parser.parse_args()
    expected_python = (
        tuple(args.expected_python.split(",")) if args.expected_python is not None else None
    )
    try:
        if args.version_only:
            version, _, _ = validate_versions(args.tag, args.check_pypi)
            print(version)
            return 0
        if args.dist_dir is None:
            parser.error("--dist-dir is required unless --version-only is used")
        manifest = validate_release(args.dist_dir, expected_python, args.tag, args.check_pypi)
    except (OSError, ValueError) as error:
        print(f"release verification failed: {error}", file=sys.stderr)
        return 1
    print("\n".join(manifest))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
