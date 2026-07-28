"""Regenerate the committed DAL Web OpenAPI snapshot."""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path

BACKEND_ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(BACKEND_ROOT))
os.environ["DAL_WEB_DB_URL"] = "sqlite:///:memory:"
os.environ["WEBUI_SEED_DEMO"] = "0"

try:
    import dal  # noqa: F401
except ModuleNotFoundError:
    from tests.fake_dal import build_fake_dal

    sys.modules["dal"] = build_fake_dal()

from app.main import create_app  # noqa: E402


def main() -> None:
    target = BACKEND_ROOT / "openapi" / "dal-web.openapi.json"
    document = create_app().openapi()
    target.write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
