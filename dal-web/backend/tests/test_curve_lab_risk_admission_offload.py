"""Risk admission event-loop and reservation compatibility regressions."""

from __future__ import annotations

import asyncio
import threading

import pytest
from httpx import ASGITransport, AsyncClient

from tests.test_curve_lab_risk_api import _publish_version, _request


def _prepared_admission(client, monkeypatch):
    from app.services.dal_gateway import get_gateway
    from app.services.store import get_store

    _, version = _publish_version(client)
    gateway = get_gateway()
    monkeypatch.setattr(
        gateway,
        "curve_lab_required_historical_fixings",
        lambda *_args, **_kwargs: [],
    )
    request = _request(version["id"])
    request["measures"] = ["PV"]
    request["sensitivity_layers"] = []
    return get_store(), gateway, request


def test_risk_admission_preflight_runs_off_request_loop_while_heartbeat_advances(
    client,
    monkeypatch,
) -> None:
    import app.services.curve_risk as curve_risk
    from app.services.dal_gateway import get_gateway

    _, version = _publish_version(client)
    blocked = threading.Event()
    release = threading.Event()
    preflight_thread_ids: list[int] = []

    def blocking_preflight(*_args, **_kwargs):
        preflight_thread_ids.append(threading.get_ident())
        blocked.set()
        release.wait(timeout=1)
        blocked.clear()
        return []

    monkeypatch.setattr(
        get_gateway(),
        "curve_lab_required_historical_fixings",
        blocking_preflight,
    )

    class CapturingReservation:
        submitted: tuple[object, ...] | None = None

        def submit(self, function, /, *args):
            self.submitted = (function, *args)

        def cancel(self) -> None:
            pass

    reservation = CapturingReservation()
    monkeypatch.setattr(curve_risk, "_reserve_job", lambda: reservation)
    request = _request(version["id"])
    request["measures"] = ["PV"]
    request["sensitivity_layers"] = []

    async def exercise_route():
        heartbeat_count = 0
        request_loop_thread_id = threading.get_ident()
        transport = ASGITransport(app=client.app)
        async with AsyncClient(transport=transport, base_url="http://testserver") as async_client:
            request_task = asyncio.create_task(
                async_client.post("/api/curve-lab/risk-runs", json=request)
            )
            while not blocked.is_set() and not request_task.done():
                await asyncio.sleep(0)
            while blocked.is_set():
                heartbeat_count += 1
                release.set()
                await asyncio.sleep(0)
            response = await request_task
        return response, heartbeat_count, request_loop_thread_id

    response, heartbeat_count, request_loop_thread_id = asyncio.run(exercise_route())

    assert response.status_code == 202, response.text
    assert response.json()["state"] == "QUEUED"
    assert heartbeat_count >= 1
    assert len(preflight_thread_ids) == 1
    assert preflight_thread_ids[0] != request_loop_thread_id
    assert reservation.submitted is not None


def test_risk_admission_preserves_reserve_publish_submit_order_and_terminal_audit(
    client,
    monkeypatch,
) -> None:
    import app.services.curve_risk as curve_risk

    store, gateway, request = _prepared_admission(client, monkeypatch)
    events: list[str] = []
    publications: list[tuple[dict, list[dict]]] = []
    audits: list[dict] = []
    original_publish = store.publish_curve_lab_risk_run
    original_audit = store.add_curve_lab_audit_event

    def publish(record, matrices):
        events.append(f"publish:{record['state']}")
        publications.append((dict(record), list(matrices)))
        return original_publish(record, matrices)

    def add_audit(record):
        audits.append(dict(record))
        original_audit(record)

    monkeypatch.setattr(store, "publish_curve_lab_risk_run", publish)
    monkeypatch.setattr(store, "add_curve_lab_audit_event", add_audit)

    def price(_document, trades, _evaluation_time, base_currency, **_kwargs):
        return [
            {
                "trade_id": trade["trade_id"],
                "instrument_type": trade["instrument_type"],
                "succeeded": True,
                "pv": "100",
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": [],
                "error": "",
            }
            for trade in trades
        ]

    monkeypatch.setattr(gateway, "price_curve_lab_trades", price)

    class InlineReservation:
        def submit(self, function, /, *args):
            events.append("submit")
            function(*args)

        def cancel(self) -> None:
            pytest.fail("successful admission must not cancel its reservation")

    def reserve():
        events.append("reserve")
        return InlineReservation()

    monkeypatch.setattr(curve_risk, "_reserve_job", reserve)

    response = client.post("/api/curve-lab/risk-runs", json=request)

    assert response.status_code == 202, response.text
    admitted = response.json()
    assert admitted["state"] == "QUEUED"
    assert events[:3] == ["reserve", "publish:QUEUED", "submit"]
    assert [record["state"] for record, _ in publications] == [
        "QUEUED",
        "RUNNING",
        "SUCCEEDED",
    ]
    assert publications[0][1] == []
    completed = store.get_curve_lab_risk_run(admitted["id"])
    assert completed["state"] == "SUCCEEDED"
    assert completed["result"] is not None
    assert completed["error"] is None
    assert completed["finished_at"] is not None
    assert len(audits) == 1
    assert audits[0]["action"] == "RISK_RUN_SUCCEEDED"
    assert audits[0]["target_type"] == "curve_risk_run"
    assert audits[0]["target_id"] == admitted["id"]
    assert audits[0]["outcome"] == "SUCCEEDED"
    assert audits[0]["details"] == {"estimated_work": completed["estimated_work"]}


def test_risk_admission_publication_failure_restores_capacity_without_side_effects(
    client,
    monkeypatch,
) -> None:
    import app.services.curve_risk as curve_risk
    from app.services.curve_lab_jobs import BoundedCurveLabExecutor, CurveLabQueueFullError
    from app.services.store import NotFoundError

    store, _gateway, request = _prepared_admission(client, monkeypatch)
    jobs = BoundedCurveLabExecutor(max_workers=1, max_queued=0)
    captured_run_ids: list[str] = []
    audits: list[dict] = []
    submit_calls = 0

    class PublicationError(RuntimeError):
        pass

    expected_error = PublicationError("publish failed")

    def fail_publish(record, _matrices):
        captured_run_ids.append(record["id"])
        raise expected_error

    def submit(*_args, **_kwargs):
        nonlocal submit_calls
        submit_calls += 1

    monkeypatch.setattr(store, "publish_curve_lab_risk_run", fail_publish)
    monkeypatch.setattr(store, "add_curve_lab_audit_event", lambda record: audits.append(record))
    monkeypatch.setattr(jobs, "_submit", submit)
    monkeypatch.setattr(curve_risk, "_reserve_job", jobs.reserve)

    try:
        with pytest.raises(PublicationError) as caught:
            client.post("/api/curve-lab/risk-runs", json=request)

        assert caught.value is expected_error
        assert len(captured_run_ids) == 1
        assert submit_calls == 0
        assert audits == []
        with pytest.raises(NotFoundError):
            store.get_curve_lab_risk_run(captured_run_ids[0])
        replacement = jobs.reserve()
        with pytest.raises(CurveLabQueueFullError):
            jobs.reserve()
        replacement.cancel()
    finally:
        jobs.shutdown()


def test_risk_admission_submission_failure_keeps_queued_row_and_releases_one_slot(
    client,
    monkeypatch,
) -> None:
    import app.services.curve_risk as curve_risk
    from app.services.curve_lab_jobs import BoundedCurveLabExecutor, CurveLabQueueFullError

    store, _gateway, request = _prepared_admission(client, monkeypatch)
    jobs = BoundedCurveLabExecutor(max_workers=1, max_queued=0)
    publications: list[tuple[dict, list[dict]]] = []
    audits: list[dict] = []
    original_publish = store.publish_curve_lab_risk_run
    submit_calls = 0

    class SubmissionError(RuntimeError):
        pass

    expected_error = SubmissionError("submit failed")

    def publish(record, matrices):
        publications.append((dict(record), list(matrices)))
        return original_publish(record, matrices)

    def fail_submit(*_args, **_kwargs):
        nonlocal submit_calls
        submit_calls += 1
        raise expected_error

    monkeypatch.setattr(store, "publish_curve_lab_risk_run", publish)
    monkeypatch.setattr(store, "add_curve_lab_audit_event", lambda record: audits.append(record))
    monkeypatch.setattr(jobs, "_submit", fail_submit)
    monkeypatch.setattr(curve_risk, "_reserve_job", jobs.reserve)

    try:
        with pytest.raises(SubmissionError) as caught:
            client.post("/api/curve-lab/risk-runs", json=request)

        assert caught.value is expected_error
        assert submit_calls == 1
        assert len(publications) == 1
        published, matrices = publications[0]
        assert matrices == []
        assert published["state"] == "QUEUED"
        assert published["result"] is None
        assert published["error"] is None
        assert published["finished_at"] is None
        stored = store.get_curve_lab_risk_run(published["id"])
        assert stored["state"] == "QUEUED"
        assert stored["result"] is None
        assert stored["error"] is None
        assert stored["finished_at"] is None
        assert audits == []
        replacement = jobs.reserve()
        with pytest.raises(CurveLabQueueFullError):
            jobs.reserve()
        replacement.cancel()
    finally:
        jobs.shutdown()
