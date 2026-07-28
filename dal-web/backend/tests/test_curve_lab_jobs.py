from __future__ import annotations

import threading
from concurrent.futures import Future
from datetime import UTC, datetime, timedelta

import pytest

from app.services.curve_lab_jobs import (
    BoundedCurveLabExecutor,
    CurveLabQueueFullError,
    deadline_expired,
    new_deadline,
)


def test_bounded_executor_rejects_above_two_running_and_one_hundred_queued() -> None:
    jobs = BoundedCurveLabExecutor(max_workers=2, max_queued=100)
    entered = threading.Barrier(3)
    release = threading.Event()
    futures: list[Future[None]] = []

    def blocked() -> None:
        entered.wait(timeout=5)
        assert release.wait(timeout=5)

    try:
        for _ in range(2):
            reservation = jobs.reserve()
            futures.append(reservation.submit(blocked))
        entered.wait(timeout=5)

        for _ in range(100):
            reservation = jobs.reserve()
            futures.append(reservation.submit(lambda: None))

        with pytest.raises(CurveLabQueueFullError):
            jobs.reserve()
    finally:
        release.set()
        for future in futures:
            future.result(timeout=5)
        jobs.shutdown()


def test_cancelled_reservation_releases_capacity() -> None:
    jobs = BoundedCurveLabExecutor(max_workers=1, max_queued=0)
    reservation = jobs.reserve()

    with pytest.raises(CurveLabQueueFullError):
        jobs.reserve()

    reservation.cancel()
    replacement = jobs.reserve()
    replacement.cancel()
    jobs.shutdown()


def test_soft_deadline_is_fifteen_minutes_and_uses_persisted_timestamp() -> None:
    created_at = datetime(2026, 7, 28, 12, 0, tzinfo=UTC)
    deadline = new_deadline(created_at)

    assert deadline == "2026-07-28T12:15:00Z"
    assert deadline_expired(deadline, now=created_at + timedelta(minutes=15))
    assert not deadline_expired(
        deadline,
        now=created_at + timedelta(minutes=15) - timedelta(microseconds=1),
    )
