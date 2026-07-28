"""Process-isolated Curve Lab fixing admission against the compiled DAL binding."""

from __future__ import annotations

import argparse
import json
import os

from fastapi.testclient import TestClient
from test_curve_lab_risk_api import (
    _historical_fra_trade,
    _publish_version,
    _request,
    _wait_for_job,
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--db", required=True)
    args = parser.parse_args()
    os.environ.pop("DAL_WEB_STORE", None)
    os.environ["DAL_WEB_DB_URL"] = f"sqlite:///{args.db}"
    os.environ["WEBUI_SEED_DEMO"] = "0"

    from app.main import create_app

    with TestClient(create_app()) as client:
        _, version = _publish_version(client)
        result: list[dict[str, object]] = []
        for snapshot_id, fixing_time in (
            ("compiled-utc-z", "2026-01-14T11:00:00Z"),
            ("compiled-utc-offset", "2026-01-14T12:00:00+01:00"),
            ("compiled-utc-naive", "2026-01-14T11:00:00"),
        ):
            snapshot = client.post(
                "/api/curve-lab/fixing-snapshots",
                json={
                    "id": snapshot_id,
                    "observations": [
                        {
                            "index_name": "USD-SOFR",
                            "fixing_time": fixing_time,
                            "kind": "RATE",
                            "units": "DECIMAL_RATE",
                            "value": "0.04",
                        }
                    ],
                },
            )
            snapshot.raise_for_status()
            request = _request(version["id"])
            request["fixing_snapshot_id"] = snapshot_id
            request["measures"] = ["PV"]
            request["target"]["trades"] = [_historical_fra_trade()]
            response = client.post("/api/curve-lab/risk-runs", json=request)
            row: dict[str, object] = {
                "snapshot_id": snapshot_id,
                "status_code": response.status_code,
                "body": response.json(),
            }
            if response.status_code == 202:
                row["terminal"] = _wait_for_job(
                    client,
                    "risk-runs",
                    response.json()["id"],
                    {"SUCCEEDED", "FAILED", "TIMED_OUT"},
                )
            result.append(row)

    print(json.dumps(result, separators=(",", ":"), sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
