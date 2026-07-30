"""Bounded admission and soft deadlines for asynchronous Curve Lab work."""

from __future__ import annotations

from collections.abc import Callable
from concurrent.futures import Future, ThreadPoolExecutor
from datetime import UTC, datetime, timedelta
from threading import BoundedSemaphore, Lock
from typing import TypeVar

_T = TypeVar("_T")
_SOFT_DEADLINE = timedelta(minutes=15)


class CurveLabQueueFullError(RuntimeError):
    """Raised before persistence when the approved worker capacity is exhausted."""


class _Reservation:
    def __init__(self, owner: BoundedCurveLabExecutor) -> None:
        self._owner = owner
        self._lock = Lock()
        self._consumed = False

    def submit(
        self,
        function: Callable[..., _T],
        /,
        *args: object,
        **kwargs: object,
    ) -> Future[_T]:
        with self._lock:
            if self._consumed:
                raise RuntimeError("Curve Lab job reservation was already consumed")
            self._consumed = True
        try:
            return self._owner._submit(function, *args, **kwargs)
        except Exception:
            self._owner._release()
            raise

    def cancel(self) -> None:
        with self._lock:
            if self._consumed:
                return
            self._consumed = True
        self._owner._release()


class BoundedCurveLabExecutor:
    """Two running jobs plus a fail-closed bounded waiting queue."""

    def __init__(
        self,
        *,
        max_workers: int = 2,
        max_queued: int = 100,
        thread_name_prefix: str = "curve-lab",
    ) -> None:
        if max_workers <= 0 or max_queued < 0:
            raise ValueError("Curve Lab worker and queue limits must be non-negative")
        self._capacity = BoundedSemaphore(max_workers + max_queued)
        self._executor = ThreadPoolExecutor(
            max_workers=max_workers,
            thread_name_prefix=thread_name_prefix,
        )

    def reserve(self) -> _Reservation:
        if not self._capacity.acquire(blocking=False):
            raise CurveLabQueueFullError("Curve Lab worker queue is full")
        return _Reservation(self)

    def _submit(
        self,
        function: Callable[..., _T],
        /,
        *args: object,
        **kwargs: object,
    ) -> Future[_T]:
        def guarded() -> _T:
            try:
                return function(*args, **kwargs)
            finally:
                self._release()

        return self._executor.submit(guarded)

    def _release(self) -> None:
        self._capacity.release()

    def shutdown(self) -> None:
        self._executor.shutdown(wait=True)


CURVE_LAB_JOBS = BoundedCurveLabExecutor()


def _timestamp(value: datetime) -> str:
    return value.astimezone(UTC).isoformat().replace("+00:00", "Z")


def new_deadline(created_at: datetime | None = None) -> str:
    return _timestamp((created_at or datetime.now(UTC)) + _SOFT_DEADLINE)


def deadline_expired(deadline_at: str, *, now: datetime | None = None) -> bool:
    deadline = datetime.fromisoformat(deadline_at.replace("Z", "+00:00"))
    return (now or datetime.now(UTC)) >= deadline


def soft_deadline_error(record: dict) -> dict:
    """Return the stable public error for a persisted Curve Lab deadline."""

    return {
        "code": "SOFT_DEADLINE_EXCEEDED",
        "message": "Curve Lab work exceeded its persisted soft deadline.",
        "field": "deadline_at",
        "value": record["deadline_at"],
        "resource_id": record["id"],
        "details": {},
    }
