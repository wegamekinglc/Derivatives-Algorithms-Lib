"""Native-only DAL runtime preflight tests."""

from __future__ import annotations

import importlib
import types

import pytest

from app.native_runtime import NativeDalUnavailableError, load_native_dal


def test_native_preflight_reports_install_command(monkeypatch):
    def missing(_name: str):
        raise ModuleNotFoundError("No module named 'dal'")

    monkeypatch.setattr(importlib, "import_module", missing)

    with pytest.raises(NativeDalUnavailableError) as error:
        load_native_dal()

    message = str(error.value)
    assert "Native DAL Python package is required" in message  # nosec B101
    assert "uv pip install ../../dal-python" in message  # nosec B101
    assert (  # nosec B101
        "--config-settings=cmake.define.DAL_INSTALL_PREFIX="
        "/absolute/path/to/build/stage/<platform-preset>"
    ) in message
    assert "Release-linux" not in message  # nosec B101
    assert "--no-build-isolation" not in message  # nosec B101
    assert "../../docs/installation.md#install-the-native-package" in message  # nosec B101
    assert "No module named 'dal'" in message  # nosec B101


def test_native_preflight_rejects_incomplete_module(monkeypatch):
    incomplete = types.ModuleType("dal")
    monkeypatch.setattr(importlib, "import_module", lambda _name: incomplete)

    with pytest.raises(NativeDalUnavailableError, match="missing required symbols"):
        load_native_dal()
