"""Regenerate the committed DAL Web OpenAPI snapshot."""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    backend = root / "dal-web" / "backend"
    sys.path.insert(0, str(backend))
    os.environ["DAL_WEB_STORE"] = "memory"
    os.environ["WEBUI_SEED_DEMO"] = "0"

    from tests.fake_dal import build_fake_dal

    sys.modules["dal"] = build_fake_dal()

    from app.main import create_app

    target = backend / "openapi" / "dal-web.openapi.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(
        json.dumps(
            create_app().openapi(),
            ensure_ascii=False,
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    print(target.relative_to(root))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
