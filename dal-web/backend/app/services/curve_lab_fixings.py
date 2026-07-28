"""Canonical UTC conversion for Curve Lab fixing instants."""

from __future__ import annotations

from datetime import UTC, datetime


def canonical_utc_datetime(value: datetime | str) -> datetime:
    parsed = (
        value
        if isinstance(value, datetime)
        else datetime.fromisoformat(value.replace("Z", "+00:00"))
    )
    if parsed.tzinfo is None:
        return parsed.replace(tzinfo=UTC)
    return parsed.astimezone(UTC)


def canonical_utc_timestamp(value: datetime | str) -> str:
    return canonical_utc_datetime(value).isoformat().replace("+00:00", "Z")
