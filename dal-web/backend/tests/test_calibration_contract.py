from __future__ import annotations

import json
import threading
from dataclasses import FrozenInstanceError
from datetime import date, datetime
from types import SimpleNamespace
from unittest.mock import Mock

import pytest
from pydantic import ValidationError

from app.schemas.calibrations import (
    ExecutionSingleKnotIdentityDTO,
    FixingObservationDTO,
    JointBasisDeclarationDTO,
    JointCurveDeclarationDTO,
    MatrixDTO,
    NamedRangeDTO,
    SingleCurveDeclarationDTO,
    _validate_unique_fixings,
)
from app.services.calibrations import (
    CalibrationHttpError,
    FrozenJsonArray,
    FrozenJsonObject,
    NativeExecutionIdentityMismatchError,
    NativeSolverDidNotConvergeError,
    PersistedKnotPlanIntegrityError,
    SingleGatewayPreLockRequest,
    VerifiedSingleWorkerAdmissionEvidence,
    _check_required_xccy_fixings,
    _check_single_analytic_eligibility,
    _fail_native,
    _resolve_declaration_initial_guess,
    _terminalize_integrity_error,
    _validate_single_worker_admission_context,
    build_joint_admission_count_plan,
    calculate_quote_bump_preview,
    freeze_integrity_error_evidence,
    to_api_error_dto,
)
from app.services.dal_gateway import (
    DalGateway,
    GatewayCalibrationResult,
    _fallback_single_plan,
    _joint_parameter_axis,
    _native_joint_curve_payload,
    _native_single_curve_payload,
    _terminal_identity_from_curve_payload,
)


def test_wire_models_forbid_extra_and_validate_defaults():
    with pytest.raises(ValidationError):
        SingleCurveDeclarationDTO(
            curve_name="curve",
            target_collateral="OIS",
            target_tenor=None,
            calibrate_discount_curve=True,
            libor_basis="ACT_365F",
            parameterization="PIECEWISE_CONSTANT_FWD",
            log_df_scheme=None,
            knot_policy="INPUT",
            knot_dates=[date(2027, 1, 1)],
            base_curve_id=None,
            unexpected=True,
        )


def test_matrix_metadata_200_is_independent_from_values_100():
    metadata = MatrixDTO(
        availability="not_requested",
        shape=(100, 200),
        row_axis=[f"r{i}" for i in range(100)],
        column_axis=[f"p{i}" for i in range(200)],
        scaling="unscaled",
        residual_tolerance=None,
        values=None,
    )
    assert metadata.shape == (100, 200)

    with pytest.raises(ValidationError):
        MatrixDTO(
            availability="available",
            shape=(100, 200),
            row_axis=[f"r{i}" for i in range(100)],
            column_axis=[f"p{i}" for i in range(200)],
            scaling="unscaled",
            residual_tolerance=None,
            values=[[0.0] * 200 for _ in range(100)],
        )


def _joint_curve(parameterization: str, knots: int) -> JointCurveDeclarationDTO:
    scheme = "LOG_LINEAR" if parameterization in {"ZERO_RATE", "LOG_DISCOUNT"} else None
    return JointCurveDeclarationDTO.model_construct(
        curve_name="curve",
        calibrate_discount_curve=True,
        target_collateral="OIS",
        target_tenor=None,
        base_layered_over_discount=False,
        parameterization=parameterization,
        log_df_scheme=scheme,
        knot_dates=[date(2027, 1, 1)] * knots,
        instruments=[],
        smoothing_weight=None,
        initial_guess_per_node=[],
    )


def _joint_basis(parameterization: str, knots: int) -> JointBasisDeclarationDTO:
    scheme = "LOG_LINEAR" if parameterization in {"ZERO_RATE", "LOG_DISCOUNT"} else None
    return JointBasisDeclarationDTO.model_construct(
        curve_name="basis",
        parameterization=parameterization,
        log_df_scheme=scheme,
        knot_dates=[date(2027, 1, 1)] * knots,
        instruments=[],
        smoothing_weight=None,
        initial_guess_per_node=[],
    )


def test_joint_admission_count_plan_visits_all_and_keeps_first_overflow():
    plan = build_joint_admission_count_plan(
        domestic=(_joint_curve("PIECEWISE_LINEAR_FWD", 100),),
        foreign=(_joint_curve("PIECEWISE_CONSTANT_FWD", 1),),
        basis=_joint_basis("PIECEWISE_CONSTANT_FWD", 1),
    )

    assert plan.total_free_parameters == 202
    assert [item.cumulative_after for item in plan.declarations] == [200, 201, 202]
    assert plan.first_overflowing_declaration is plan.declarations[1]
    assert plan.first_overflowing_declaration.group == "foreign"


def test_quote_bump_uses_solver_scaled_formula_exactly():
    inverse = MatrixDTO(
        availability="available",
        shape=(2, 1),
        row_axis=["p0", "p1"],
        column_axis=["residual:" + "a" * 32],
        scaling="solver_scaled",
        residual_tolerance=0.25,
        values=[[2.0], [-3.0]],
    )

    preview = calculate_quote_bump_preview(inverse, 0, 0.01)

    assert [delta.value for delta in preview.delta_parameters] == [0.08, -0.12]
    assert preview.formula == ("delta_x = effective_inverse * delta_quote / residual_tolerance")


def test_frozen_integrity_evidence_is_deep_and_canonical():
    location: list[str | int] = ["body", "integrity", 0]
    context = {
        "empty_array": [],
        "empty_object": {},
        "nested": {"array": [{"leaf": "before"}]},
    }
    frozen = freeze_integrity_error_evidence(
        "PERSISTED_EXPECTED_EXECUTION_IDENTITY_HASH_MISMATCH",
        "persisted expected single-knot execution identity failed canonical hash verification",
        location,
        context,
    )

    location.append("changed")
    context["nested"]["array"][0]["leaf"] = "after"
    assert isinstance(frozen.context, FrozenJsonObject)
    entries = dict(frozen.context.entries)
    assert isinstance(entries["empty_array"], FrozenJsonArray)
    assert isinstance(entries["empty_object"], FrozenJsonObject)
    assert json.loads(frozen.canonical_error_utf8)["context"]["nested"]["array"][0] == {
        "leaf": "before"
    }
    with pytest.raises(FrozenInstanceError):
        frozen.message = "changed"

    first = to_api_error_dto(frozen)
    first.context["nested"]["array"][0]["leaf"] = "wire-only"
    second = to_api_error_dto(frozen)
    assert second.context["nested"]["array"][0]["leaf"] == "before"


def test_integrity_terminalization_passes_the_frozen_canonical_bytes_verbatim():
    frozen = freeze_integrity_error_evidence(
        "PERSISTED_KNOT_PLAN_HASH_MISMATCH",
        "persisted single-knot plan failed canonical hash verification",
        None,
        {"stored_plan_hash": "before", "actual_plan_hash": "after"},
    )
    store = Mock()

    _terminalize_integrity_error(
        store,
        "a" * 32,
        PersistedKnotPlanIntegrityError(frozen),
        "plan",
    )

    passed = store.fail_knot_plan_integrity.call_args.args[2]
    assert passed is frozen.canonical_error_utf8
    assert store.fail_expected_execution_identity_integrity.call_count == 0


def test_fallback_planner_maps_submitted_trace_to_final_canonical_indices():
    request = SimpleNamespace(
        today=date(2026, 1, 2),
        declaration=SimpleNamespace(
            knot_policy="INSTRUMENTS",
            knot_dates=[],
            parameterization="PIECEWISE_CONSTANT_FWD",
        ),
        instruments=[
            SimpleNamespace(start=date(2028, 1, 2), maturity=date(2029, 1, 2)),
            SimpleNamespace(start=date(2027, 1, 2), maturity=date(2028, 1, 2)),
        ],
    )

    plan = _fallback_single_plan(request)

    assert [item.date for item in plan.resolved_declared_nodes] == [
        date(2027, 1, 2),
        date(2028, 1, 2),
        date(2029, 1, 2),
    ]
    assert [item.resolved_index for item in plan.candidate_trace] == [1, 2, 0, 1]
    assert [item.disposition for item in plan.candidate_trace] == [
        "ADDED",
        "ADDED",
        "ADDED",
        "DUPLICATE",
    ]


def test_single_analytic_issue_is_mapped_back_to_submitted_instrument():
    request = SimpleNamespace(
        today=date(2026, 1, 2),
        options=SimpleNamespace(jacobian_mode="ANALYTIC"),
        instruments=[
            SimpleNamespace(
                kind="DEPOSIT",
                start=date(2028, 1, 2),
                maturity=date(2029, 1, 2),
                trade_date=date(2026, 1, 1),
            ),
            SimpleNamespace(
                kind="DEPOSIT",
                start=date(2027, 1, 2),
                maturity=date(2028, 1, 2),
                trade_date=date(2026, 1, 2),
            ),
        ],
    )
    issue = SimpleNamespace(
        reason=SimpleNamespace(name="TRADE_DATE_MISMATCH"),
        instrument_index=1,
        reset_index=-1,
        native_message="diagnostic only",
    )
    report = SimpleNamespace(eligible=False, issues=(issue,))

    with pytest.raises(CalibrationHttpError) as captured:
        _check_single_analytic_eligibility(
            request, report, ("Deposit", "Deposit")
        )

    assert captured.value.error.location == ["body", "instruments", 0, "trade_date"]
    assert captured.value.error.context["input_index"] == 0
    assert captured.value.error.context["calibration_index"] == 1


def test_unbounded_planner_carrier_preserves_101st_storage_offender():
    today = date(2026, 1, 2)
    first = date(2027, 1, 2)
    request = SimpleNamespace(
        today=today,
        declaration=SimpleNamespace(
            knot_policy="INPUT",
            knot_dates=[first + date.resolution * index for index in range(100)],
            parameterization="ZERO_RATE",
        ),
        instruments=[],
    )

    plan = _fallback_single_plan(request)

    assert plan.counts.resolved_declared_nodes == 100
    assert plan.counts.storage_nodes == 101
    assert plan.candidate_trace[99].ordinal == 99
    assert plan.candidate_trace[99].origin.input_knot_index == 99


def test_log_discount_scalar_seed_uses_dated_raw_parameter_units():
    anchor = date(2026, 1, 1)

    resolved = _resolve_declaration_initial_guess(
        values=(),
        scalar=0.04,
        free_count=2,
        parameterization="LOG_DISCOUNT",
        anchor=anchor,
        knot_dates=(
            anchor + date.resolution * 365,
            anchor + date.resolution * 730,
        ),
        day_basis="ACT_365F",
        location=["body", "basis", "initial_guess_per_node"],
    )

    assert resolved == pytest.approx([-0.04, -0.08])


def test_terminal_identity_reads_zero_rate_storage_from_concrete_curve():
    anchor = date(2026, 1, 1)
    future = date(2027, 1, 1)
    inspected = ExecutionSingleKnotIdentityDTO(
        identity_version=1,
        execution_policy="INPUT",
        today=anchor,
        parameterization="ZERO_RATE",
        log_df_scheme="LOG_LINEAR",
        resolved_declared_dates=(future,),
        storage_dates=(anchor, future),
        free_parameters=({"date": future, "component": "zero_rate"},),
        counts={
            "resolved_declared_nodes": 1,
            "storage_nodes": 2,
            "free_parameters": 1,
        },
    )

    terminal = _terminal_identity_from_curve_payload(
        inspected,
        {
            "parameterization": "ZERO_RATE",
            "anchor_date": anchor,
            "node_dates": [future],
        },
    )

    assert terminal.storage_dates == (anchor, future)
    assert terminal.counts.storage_nodes == 2


class _NativeDate:
    def __init__(self, value: date) -> None:
        self.value = value

    def __repr__(self) -> str:
        return self.value.isoformat()


class _NativeScheme:
    def __init__(self, name: str) -> None:
        self.name = name


def _native_zero_curve(scheme: str) -> SimpleNamespace:
    return SimpleNamespace(
        node_dates=[_NativeDate(date(2027, 1, 2))],
        zero_rates=[0.02],
        day_count="ACT_365F",
        log_df_scheme=_NativeScheme(scheme),
    )


@pytest.mark.parametrize("payload_kind", ("single", "joint"))
def test_native_curve_payload_rejects_actual_scheme_mismatch(
    payload_kind: str,
) -> None:
    """D6 — response/persistence provenance is the concrete native getter."""
    declaration = SimpleNamespace(
        curve_name="usd_zero",
        calibrate_discount_curve=True,
        target_collateral="OIS",
        target_tenor=None,
        parameterization="ZERO_RATE",
        log_df_scheme="LOG_LINEAR",
        base_curve_id=None,
    )
    curve = _native_zero_curve("MIXED")

    with pytest.raises(RuntimeError, match="native curve log-DF scheme"):
        if payload_kind == "single":
            _native_single_curve_payload(
                SimpleNamespace(
                    declaration=declaration,
                    currency="USD",
                    today=date(2026, 1, 2),
                ),
                Mock(),
                curve,
            )
        else:
            _native_joint_curve_payload(
                declaration,
                currency="USD",
                collateral_currency="USD",
                anchor_date=date(2026, 1, 2),
                curve=curve,
                role="discount",
            )


def test_joint_parameter_axis_uses_native_range_and_curve_layout_dates():
    request = SimpleNamespace(
        domestic=SimpleNamespace(
            declarations=[
                SimpleNamespace(
                    parameterization="PIECEWISE_LINEAR_FWD",
                    knot_dates=[date(2027, 1, 2)],
                )
            ]
        ),
        foreign=SimpleNamespace(
            declarations=[
                SimpleNamespace(
                    parameterization="ZERO_RATE",
                    knot_dates=[date(2027, 6, 2)],
                )
            ]
        ),
        basis=SimpleNamespace(
            parameterization="LOG_DISCOUNT",
            knot_dates=[date(2028, 1, 2)],
        ),
    )
    ranges = [
        NamedRangeDTO(name="domestic:usd", offset=0, size=2),
        NamedRangeDTO(name="foreign:eur", offset=2, size=1),
        NamedRangeDTO(name="basis:basis", offset=3, size=1),
    ]

    assert _joint_parameter_axis(request, ranges) == [
        "parameter:domestic:usd:2027-01-02:left_forward",
        "parameter:domestic:usd:2027-01-02:right_forward",
        "parameter:foreign:eur:2027-06-02:zero_rate",
        "parameter:basis:basis:2028-01-02:log_discount_factor",
    ]


def test_missing_historical_fixing_is_a_pre_admission_submitted_error():
    instrument = SimpleNamespace(
        config=SimpleNamespace(
            domestic_rate_fixing=SimpleNamespace(index_name="USD-SOFR"),
            foreign_rate_fixing=SimpleNamespace(index_name="EUR-ESTR"),
        )
    )
    request = SimpleNamespace(
        fixings=[],
        basis=SimpleNamespace(instruments=[instrument]),
    )
    required = [
        SimpleNamespace(
            instrument_index=0,
            index_name="EUR-ESTR",
            timestamp=datetime.fromisoformat("2025-12-31T11:00:00"),
        )
    ]

    with pytest.raises(CalibrationHttpError) as captured:
        _check_required_xccy_fixings(request, required)

    assert captured.value.error.code == "MISSING_FIXING"
    assert captured.value.error.location == [
        "body",
        "basis",
        "instruments",
        0,
        "config",
        "foreign_rate_fixing",
    ]
    assert captured.value.error.context == {
        "index_name": "EUR-ESTR",
        "timestamp": "2025-12-31T11:00:00",
    }


def test_reciprocal_fx_observations_must_be_consistent():
    timestamp = datetime.fromisoformat("2025-12-31T11:00:00")
    observations = [
        FixingObservationDTO(
            index_name="FX[EUR/USD]",
            timestamp=timestamp,
            value=1.25,
        ),
        FixingObservationDTO(
            index_name="FX[USD/EUR]",
            timestamp=timestamp,
            value=0.81,
        ),
    ]

    with pytest.raises(ValueError, match="reciprocal FX"):
        _validate_unique_fixings(observations)


def test_native_non_convergence_has_a_stable_terminal_error_domain():
    store = Mock()
    error = NativeSolverDidNotConvergeError(
        max_abs_residual=None,
        rms_residual=None,
        evaluations=1,
        max_evaluations=1,
    )

    _fail_native(store, "a" * 32, error)

    payload = store.fail_calibration.call_args.kwargs["error_payload"]
    assert payload == {
        "code": "SOLVER_DID_NOT_CONVERGE",
        "message": "Calibration solver did not converge",
        "location": None,
        "context": {
            "max_abs_residual": None,
            "rms_residual": None,
            "evaluations": 1,
            "max_evaluations": 1,
        },
    }


def test_unknown_native_failure_exposes_only_a_sanitized_native_message():
    store = Mock()

    _fail_native(store, "b" * 32, RuntimeError("secret /tmp/book.db"))

    payload = store.fail_calibration.call_args.kwargs["error_payload"]
    assert payload["code"] == "NATIVE_CALIBRATION_FAILED"
    assert payload["context"] == {
        "native_message": "RuntimeError: native calibration failed"
    }


def test_gateway_translates_only_the_private_native_convergence_type(
    monkeypatch,
):
    class NativeConvergenceError(RuntimeError):
        pass

    gateway = DalGateway()
    monkeypatch.setattr(
        gateway._dal,
        "_dal",
        SimpleNamespace(_CalibrationConvergenceError=NativeConvergenceError),
        raising=False,
    )

    def exhaust() -> None:
        raise NativeConvergenceError("native detail")

    request = SimpleNamespace(
        solver=SimpleNamespace(max_evaluations=7)
    )
    with pytest.raises(NativeSolverDidNotConvergeError) as captured:
        gateway._call_native_calibration(exhaust, request)

    assert captured.value.diagnostics["evaluations"] == 7


def test_worker_bounded_evidence_is_validated_against_request_context():
    today = date(2026, 1, 2)
    maturity = date(2027, 1, 2)
    request = SimpleNamespace(
        today=today,
        declaration=SimpleNamespace(
            knot_policy="INPUT",
            knot_dates=[maturity],
            parameterization="PIECEWISE_CONSTANT_FWD",
            log_df_scheme=None,
        ),
        instruments=[],
    )
    plan = _fallback_single_plan(request).to_bounded_dto()
    mismatched = ExecutionSingleKnotIdentityDTO(
        identity_version=1,
        execution_policy="INPUT",
        today=today,
        parameterization="ZERO_RATE",
        log_df_scheme="LOG_LINEAR",
        resolved_declared_dates=(maturity,),
        storage_dates=(today, maturity),
        free_parameters=(
            {"date": maturity, "component": "zero_rate"},
        ),
        counts={
            "resolved_declared_nodes": 1,
            "storage_nodes": 2,
            "free_parameters": 1,
        },
    )

    with pytest.raises(
        ValueError, match="expected execution identity does not match"
    ):
        _validate_single_worker_admission_context(
            SimpleNamespace(request=request),
            plan,
            mismatched,
        )


def test_gateway_compares_terminal_identity_to_exact_verified_carrier(
    monkeypatch,
):
    today = date(2026, 1, 2)
    maturity = date(2027, 1, 2)
    request = SimpleNamespace(
        today=today,
        declaration=SimpleNamespace(
            knot_policy="INPUT",
            knot_dates=[maturity],
            parameterization="PIECEWISE_CONSTANT_FWD",
            log_df_scheme=None,
        ),
        instruments=[],
    )
    plan = _fallback_single_plan(request).to_bounded_dto()
    expected = ExecutionSingleKnotIdentityDTO(
        identity_version=1,
        execution_policy="INPUT",
        today=today,
        parameterization="PIECEWISE_CONSTANT_FWD",
        log_df_scheme=None,
        resolved_declared_dates=(maturity,),
        storage_dates=(maturity,),
        free_parameters=(
            {"date": maturity, "component": "right_forward"},
        ),
        counts={
            "resolved_declared_nodes": 1,
            "storage_nodes": 1,
            "free_parameters": 1,
        },
    )
    mismatched = expected.model_copy(
        update={"execution_policy": "INSTRUMENTS"}
    )
    evidence = VerifiedSingleWorkerAdmissionEvidence(
        resolved_knot_plan=plan,
        resolved_knot_plan_hash="plan-hash",
        expected_execution_identity=expected,
        expected_execution_identity_hash="identity-hash",
    )
    gateway = DalGateway()
    monkeypatch.setattr(
        gateway, "_build_single_execution_spec", lambda _verified: None
    )
    monkeypatch.setattr(
        gateway,
        "_calibrate_single_verified",
        lambda *_args: SimpleNamespace(
            actual_execution_identity=mismatched,
            actual_jacobian_mode="ANALYTIC",
        ),
    )

    with pytest.raises(
        NativeExecutionIdentityMismatchError
    ) as captured:
        gateway.calibrate_single(
            SingleGatewayPreLockRequest(request, {}),
            lambda _acquired_at: None,
            lambda _pre_lock: evidence,
            lambda _actual: None,
        )

    assert captured.value.comparison_stage == "post_solve_storage"
    assert captured.value.expected is expected
    assert captured.value.actual is mismatched


def test_gateway_holds_continuous_calibration_lock_through_evidence_and_solve(
    monkeypatch,
):
    today = date(2026, 1, 2)
    maturity = date(2027, 1, 2)
    request = SimpleNamespace(
        today=today,
        declaration=SimpleNamespace(
            knot_policy="INPUT",
            knot_dates=[maturity],
            parameterization="PIECEWISE_CONSTANT_FWD",
            log_df_scheme=None,
        ),
        instruments=[],
    )
    plan = _fallback_single_plan(request).to_bounded_dto()
    identity = ExecutionSingleKnotIdentityDTO(
        identity_version=1,
        execution_policy="INPUT",
        today=today,
        parameterization="PIECEWISE_CONSTANT_FWD",
        log_df_scheme=None,
        resolved_declared_dates=(maturity,),
        storage_dates=(maturity,),
        free_parameters=(
            {"date": maturity, "component": "right_forward"},
        ),
        counts={
            "resolved_declared_nodes": 1,
            "storage_nodes": 1,
            "free_parameters": 1,
        },
    )
    evidence = VerifiedSingleWorkerAdmissionEvidence(
        plan,
        "plan-hash",
        identity,
        "identity-hash",
    )
    gateway = DalGateway()
    entered_verify = threading.Event()
    release_verify = threading.Event()
    events: list[str] = []
    failures: list[BaseException] = []
    monkeypatch.setattr(
        gateway, "_build_single_execution_spec", lambda _verified: None
    )

    def solve(*_args):
        events.append("solve")
        return GatewayCalibrationResult(
            "ANALYTIC",
            identity,
            (),
            (),
            SimpleNamespace(),
            None,
            SimpleNamespace(),
            SimpleNamespace(),
            SimpleNamespace(),
            0.0,
        )

    monkeypatch.setattr(gateway, "_calibrate_single_verified", solve)

    def verify(_pre_lock):
        events.append("verify")
        entered_verify.set()
        assert release_verify.wait(timeout=5)
        return evidence

    def run() -> None:
        try:
            gateway.calibrate_single(
                SingleGatewayPreLockRequest(request, {}),
                lambda _acquired_at: events.append("lock"),
                verify,
                lambda _actual: events.append("identity"),
            )
        except BaseException as exc:
            failures.append(exc)

    worker = threading.Thread(target=run)
    worker.start()
    assert entered_verify.wait(timeout=5)
    assert not gateway._calibration_lock.acquire(blocking=False)
    assert gateway.health_snapshot().backend == "dal"
    release_verify.set()
    worker.join(timeout=5)

    assert not worker.is_alive()
    assert failures == []
    assert events == ["lock", "verify", "identity", "solve"]
