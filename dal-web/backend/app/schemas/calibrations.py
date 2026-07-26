"""Versioned wire contracts for the curve calibration workbench."""

from __future__ import annotations

from datetime import date, datetime
from typing import Annotated, Literal

from pydantic import (
    AfterValidator,
    BaseModel,
    ConfigDict,
    Field,
    JsonValue,
    model_validator,
)


class CalibrationWireModel(BaseModel):
    model_config = ConfigDict(extra="forbid", validate_default=True)


class FrozenCalibrationWireModel(CalibrationWireModel):
    model_config = ConfigDict(extra="forbid", validate_default=True, frozen=True)


FiniteFloat = Annotated[float, Field(allow_inf_nan=False)]
PositiveFiniteFloat = Annotated[float, Field(gt=0.0, allow_inf_nan=False)]
PositiveInt = Annotated[int, Field(gt=0)]
NonNegativeInt = Annotated[int, Field(ge=0)]
BoundedString = Annotated[str, Field(max_length=128)]
NonEmptyString = Annotated[str, Field(min_length=1, max_length=128)]
GeneratedName = Annotated[str, Field(min_length=1, max_length=160)]
EntityId = Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
Sha256Hex = Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
MatrixMetadataDimension = Annotated[int, Field(ge=0, le=200)]
LocationItem = str | int

MAX_TOTAL_FREE_PARAMETERS = 200
MAX_RESPONSE_BYTES = 1 << 20
MAX_MATERIALIZED_MATRIX_DIMENSION = 100

SolveMode = Literal["EXACT", "APPROXIMATE"]
JacobianMode = Literal["ANALYTIC", "BUMPED"]
CurveParameterization = Literal[
    "PIECEWISE_CONSTANT_FWD",
    "PIECEWISE_LINEAR_FWD",
    "ZERO_RATE",
    "LOG_DISCOUNT",
]
LogDfScheme = Literal["LOG_LINEAR", "LOG_CUBIC_NATURAL", "MIXED"]
KnotPolicy = Literal["INPUT", "INSTRUMENTS", "AUGMENTED"]
XccyNotionalMode = Literal["FIXED", "RESETTABLE", "MARK_TO_MARKET"]


def _validate_dal_datetime(value: datetime) -> datetime:
    if value.tzinfo is not None or value.microsecond != 0:
        raise ValueError("DAL date-times must be timezone-naive with whole seconds")
    return value


DalDateTime = Annotated[datetime, AfterValidator(_validate_dal_datetime)]


class InputKnotOriginDTO(FrozenCalibrationWireModel):
    kind: Literal["INPUT"]
    input_knot_index: NonNegativeInt


class InstrumentStartOriginDTO(FrozenCalibrationWireModel):
    kind: Literal["INSTRUMENT_START"]
    instrument_input_index: NonNegativeInt


class InstrumentEndOriginDTO(FrozenCalibrationWireModel):
    kind: Literal["INSTRUMENT_END"]
    instrument_input_index: NonNegativeInt


class SyntheticAnchorOriginDTO(FrozenCalibrationWireModel):
    kind: Literal["SYNTHETIC_ANCHOR"]


KnotOriginDTO = Annotated[
    InputKnotOriginDTO
    | InstrumentStartOriginDTO
    | InstrumentEndOriginDTO
    | SyntheticAnchorOriginDTO,
    Field(discriminator="kind"),
]
CandidateDisposition = Literal["ADDED", "DUPLICATE", "FILTERED_NOT_AFTER_TODAY"]
FreeParameterComponent = Literal[
    "right_forward", "left_forward", "zero_rate", "log_discount_factor"
]


class KnotCandidateDTO(FrozenCalibrationWireModel):
    ordinal: NonNegativeInt
    date: date
    origin: KnotOriginDTO
    disposition: CandidateDisposition
    resolved_index: NonNegativeInt | None

    @model_validator(mode="after")
    def _check_disposition(self) -> KnotCandidateDTO:
        has_index = self.resolved_index is not None
        if self.disposition == "FILTERED_NOT_AFTER_TODAY" and has_index:
            raise ValueError("filtered candidates require resolved_index=null")
        if self.disposition in {"ADDED", "DUPLICATE"} and not has_index:
            raise ValueError("added and duplicate candidates require a resolved index")
        return self


class ResolvedKnotNodeDTO(FrozenCalibrationWireModel):
    date: date
    origins: Annotated[tuple[KnotOriginDTO, ...], Field(min_length=1, max_length=300)]


class FreeParameterDTO(FrozenCalibrationWireModel):
    date: date
    component: FreeParameterComponent


class ResolvedKnotCountsDTO(FrozenCalibrationWireModel):
    submitted_knots: Annotated[int, Field(ge=0, le=100)]
    instrument_candidates: Annotated[int, Field(ge=0, le=200)]
    resolved_declared_nodes: Annotated[int, Field(ge=1, le=100)]
    storage_nodes: Annotated[int, Field(ge=1, le=100)]
    free_parameters: Annotated[int, Field(ge=1, le=200)]


class ResolvedSingleKnotPlanDTO(FrozenCalibrationWireModel):
    planner_version: Literal[1]
    requested_policy: KnotPolicy
    execution_policy: Literal["INPUT"]
    submitted_knot_dates: Annotated[tuple[date, ...], Field(max_length=100)]
    candidate_trace: Annotated[tuple[KnotCandidateDTO, ...], Field(max_length=300)]
    resolved_declared_nodes: Annotated[
        tuple[ResolvedKnotNodeDTO, ...], Field(min_length=1, max_length=100)
    ]
    storage_nodes: Annotated[tuple[ResolvedKnotNodeDTO, ...], Field(min_length=1, max_length=100)]
    free_parameters: Annotated[tuple[FreeParameterDTO, ...], Field(min_length=1, max_length=200)]
    anchor_added: bool
    counts: ResolvedKnotCountsDTO

    @model_validator(mode="after")
    def _check_plan(self) -> ResolvedSingleKnotPlanDTO:
        if [candidate.ordinal for candidate in self.candidate_trace] != list(
            range(len(self.candidate_trace))
        ):
            raise ValueError("candidate ordinals must be consecutive")
        for nodes, label in (
            (self.resolved_declared_nodes, "resolved"),
            (self.storage_nodes, "storage"),
        ):
            dates = [node.date for node in nodes]
            if dates != sorted(set(dates)):
                raise ValueError(f"{label} dates must be strictly increasing")
        expected = (
            len(self.submitted_knot_dates),
            sum(
                candidate.origin.kind in {"INSTRUMENT_START", "INSTRUMENT_END"}
                for candidate in self.candidate_trace
            ),
            len(self.resolved_declared_nodes),
            len(self.storage_nodes),
            len(self.free_parameters),
        )
        actual = (
            self.counts.submitted_knots,
            self.counts.instrument_candidates,
            self.counts.resolved_declared_nodes,
            self.counts.storage_nodes,
            self.counts.free_parameters,
        )
        if expected != actual:
            raise ValueError("resolved knot plan counts do not match its arrays")
        if self.anchor_added != any(
            origin.kind == "SYNTHETIC_ANCHOR"
            for node in self.storage_nodes
            for origin in node.origins
        ):
            raise ValueError("anchor_added does not match storage-node origins")
        return self


class ExecutionSingleKnotCountsDTO(FrozenCalibrationWireModel):
    resolved_declared_nodes: Annotated[int, Field(ge=1, le=100)]
    storage_nodes: Annotated[int, Field(ge=1, le=100)]
    free_parameters: Annotated[int, Field(ge=1, le=200)]


class ExecutionSingleKnotIdentityDTO(FrozenCalibrationWireModel):
    identity_version: Literal[1]
    execution_policy: Literal["INPUT"]
    today: date
    parameterization: CurveParameterization
    log_df_scheme: LogDfScheme | None
    resolved_declared_dates: Annotated[tuple[date, ...], Field(min_length=1, max_length=100)]
    storage_dates: Annotated[tuple[date, ...], Field(min_length=1, max_length=100)]
    free_parameters: Annotated[tuple[FreeParameterDTO, ...], Field(min_length=1, max_length=200)]
    counts: ExecutionSingleKnotCountsDTO

    @model_validator(mode="after")
    def _check_identity(self) -> ExecutionSingleKnotIdentityDTO:
        expects_scheme = self.parameterization in {"ZERO_RATE", "LOG_DISCOUNT"}
        if expects_scheme != (self.log_df_scheme is not None):
            raise ValueError("log_df_scheme does not match parameterization")
        if list(self.resolved_declared_dates) != sorted(set(self.resolved_declared_dates)):
            raise ValueError("resolved declared dates must be strictly increasing")
        if list(self.storage_dates) != sorted(set(self.storage_dates)):
            raise ValueError("storage dates must be strictly increasing")
        expected = (
            len(self.resolved_declared_dates),
            len(self.storage_dates),
            len(self.free_parameters),
        )
        actual = (
            self.counts.resolved_declared_nodes,
            self.counts.storage_nodes,
            self.counts.free_parameters,
        )
        if expected != actual:
            raise ValueError("execution identity counts do not match its arrays")
        return self


class SingleCalibrationSolverDTO(CalibrationWireModel):
    solve_mode: SolveMode = "EXACT"
    smoothing_weight: PositiveFiniteFloat = 1.0
    tolerance: PositiveFiniteFloat = 1e-8
    fit_tolerance: PositiveFiniteFloat = 1e-6
    initial_guess: FiniteFloat = 0.05
    max_evaluations: PositiveInt = 200
    max_restarts: PositiveInt = 20


class StagedXccyCalibrationSolverDTO(CalibrationWireModel):
    solve_mode: SolveMode = "EXACT"
    smoothing_weight: PositiveFiniteFloat = 1.0
    tolerance: PositiveFiniteFloat = 1e-10
    fit_tolerance: PositiveFiniteFloat = 1e-6
    initial_guess: FiniteFloat = 0.0
    max_evaluations: PositiveInt = 200
    max_restarts: PositiveInt = 20


class JointXccyCalibrationSolverDTO(CalibrationWireModel):
    solve_mode: SolveMode = "EXACT"
    smoothing_weight: PositiveFiniteFloat = 1.0
    tolerance: PositiveFiniteFloat = 1e-8
    fit_tolerance: PositiveFiniteFloat = 1e-6
    initial_guess: FiniteFloat = 0.0
    max_evaluations: PositiveInt = 200
    max_restarts: PositiveInt = 20


class NormalizedCalibrationSolverDTO(CalibrationWireModel):
    solve_mode: SolveMode
    smoothing_weight: PositiveFiniteFloat
    tolerance: PositiveFiniteFloat
    fit_tolerance: PositiveFiniteFloat
    initial_guess: FiniteFloat
    max_evaluations: PositiveInt
    max_restarts: PositiveInt


class CalibrationOptionsDTO(CalibrationWireModel):
    jacobian_mode: JacobianMode = "ANALYTIC"
    include_jacobian: bool = False
    include_effective_inverse: bool = False


class NormalizedCalibrationOptionsDTO(CalibrationWireModel):
    jacobian_mode: JacobianMode
    include_jacobian: bool
    include_effective_inverse: bool


class CurrencyPairDTO(CalibrationWireModel):
    domestic: NonEmptyString
    foreign: NonEmptyString

    @model_validator(mode="after")
    def _different_currencies(self) -> CurrencyPairDTO:
        if self.domestic == self.foreign:
            raise ValueError("currency pair members must differ")
        return self


class RateLegConventionDTO(CalibrationWireModel):
    payment_frequency: NonEmptyString
    day_basis: NonEmptyString
    payment_lag: int
    business_day_convention: NonEmptyString
    payment_convention: NonEmptyString
    accrual_holidays: BoundedString
    payment_holidays: BoundedString
    end_of_month: bool


class RateIndexConventionDTO(CalibrationWireModel):
    spot_lag: int
    fixing_lag: int
    use_projection_curve: bool
    forecast_tenor: NonEmptyString
    day_basis: NonEmptyString
    business_day_convention: NonEmptyString
    fixing_holidays: BoundedString
    accrual_holidays: BoundedString
    end_of_month: bool
    collateral: NonEmptyString


class CrossCurrencyConventionDTO(CalibrationWireModel):
    initial_notional_exchange: bool
    final_notional_exchange: bool
    spread_on_foreign_leg: bool
    domestic_index: RateIndexConventionDTO
    domestic_leg: RateLegConventionDTO
    foreign_index: RateIndexConventionDTO
    foreign_leg: RateLegConventionDTO


class FxResetConventionDTO(CalibrationWireModel):
    fixing_lag: Annotated[int, Field(ge=-1)]
    fixing_holidays: BoundedString
    fixing_convention: NonEmptyString
    fixing_hour: Annotated[int, Field(ge=-1, le=23)]
    fixing_minute: Annotated[int, Field(ge=-1, le=59)]


class FixingIdentityDTO(CalibrationWireModel):
    index_name: BoundedString
    fixing_hour: Annotated[int, Field(ge=-1, le=23)]
    fixing_minute: Annotated[int, Field(ge=-1, le=59)]


class XccySwapConfigDTO(CalibrationWireModel):
    pair: CurrencyPairDTO
    domestic_notional: PositiveFiniteFloat
    foreign_notional: PositiveFiniteFloat
    convention: CrossCurrencyConventionDTO
    notional_mode: XccyNotionalMode
    fx_reset: FxResetConventionDTO
    domestic_rate_fixing: FixingIdentityDTO
    foreign_rate_fixing: FixingIdentityDTO

    @model_validator(mode="after")
    def _check_reset_configuration(self) -> XccySwapConfigDTO:
        if self.notional_mode == "FIXED":
            return self
        if (
            self.fx_reset.fixing_lag < 0
            or self.fx_reset.fixing_hour < 0
            or self.fx_reset.fixing_minute < 0
        ):
            raise ValueError("resettable notionals require a complete FX reset convention")
        for fixing in (self.domestic_rate_fixing, self.foreign_rate_fixing):
            if not fixing.index_name or fixing.fixing_hour < 0 or fixing.fixing_minute < 0:
                raise ValueError("resettable notionals require complete rate-fixing identities")
        return self


class FixingObservationDTO(CalibrationWireModel):
    index_name: NonEmptyString
    timestamp: DalDateTime
    value: PositiveFiniteFloat


class InstrumentBaseDTO(CalibrationWireModel):
    label: NonEmptyString
    trade_date: date
    start: date
    maturity: date
    market_rate: FiniteFloat

    @model_validator(mode="after")
    def _check_dates(self) -> InstrumentBaseDTO:
        if self.start >= self.maturity:
            raise ValueError("instrument start must be before maturity")
        return self


class DepositInstrumentDTO(InstrumentBaseDTO):
    kind: Literal["DEPOSIT"]
    index: RateIndexConventionDTO


class FraInstrumentDTO(InstrumentBaseDTO):
    kind: Literal["FRA"]
    index: RateIndexConventionDTO


class FutureInstrumentDTO(InstrumentBaseDTO):
    kind: Literal["FUTURE"]
    index: RateIndexConventionDTO
    convexity_adjustment: FiniteFloat = 0.0


class SwapInstrumentDTO(InstrumentBaseDTO):
    kind: Literal["SWAP"]
    fixed_leg: RateLegConventionDTO
    float_index: RateIndexConventionDTO
    float_leg: RateLegConventionDTO


class OisSwapInstrumentDTO(InstrumentBaseDTO):
    kind: Literal["OIS_SWAP"]
    fixed_leg: RateLegConventionDTO
    overnight_index: RateIndexConventionDTO
    float_leg: RateLegConventionDTO


class BasisSwapInstrumentDTO(InstrumentBaseDTO):
    kind: Literal["BASIS_SWAP"]
    spread_index: RateIndexConventionDTO
    spread_leg: RateLegConventionDTO
    reference_index: RateIndexConventionDTO
    reference_leg: RateLegConventionDTO


class XccySwapInstrumentDTO(InstrumentBaseDTO):
    kind: Literal["XCCY_SWAP"]
    config: XccySwapConfigDTO


RateInstrumentDTO = Annotated[
    DepositInstrumentDTO
    | FraInstrumentDTO
    | FutureInstrumentDTO
    | SwapInstrumentDTO
    | OisSwapInstrumentDTO
    | BasisSwapInstrumentDTO,
    Field(discriminator="kind"),
]
CalibrationInstrumentDTO = Annotated[
    DepositInstrumentDTO
    | FraInstrumentDTO
    | FutureInstrumentDTO
    | SwapInstrumentDTO
    | OisSwapInstrumentDTO
    | BasisSwapInstrumentDTO
    | XccySwapInstrumentDTO,
    Field(discriminator="kind"),
]


def _validate_representation(
    parameterization: CurveParameterization, scheme: LogDfScheme | None
) -> None:
    expects_scheme = parameterization in {"ZERO_RATE", "LOG_DISCOUNT"}
    if expects_scheme != (scheme is not None):
        raise ValueError("log_df_scheme does not match parameterization")


class SingleCurveDeclarationDTO(CalibrationWireModel):
    curve_name: NonEmptyString
    target_collateral: NonEmptyString
    target_tenor: NonEmptyString | None
    calibrate_discount_curve: bool
    libor_basis: NonEmptyString
    parameterization: CurveParameterization
    log_df_scheme: LogDfScheme | None
    knot_policy: KnotPolicy
    knot_dates: Annotated[list[date], Field(max_length=100)]
    base_curve_id: EntityId | None
    discount_curve_ids: Annotated[
        dict[NonEmptyString, EntityId], Field(default_factory=dict, max_length=16)
    ]
    forward_curve_ids: Annotated[
        dict[NonEmptyString, EntityId], Field(default_factory=dict, max_length=16)
    ]
    initial_guess_per_node: Annotated[
        list[FiniteFloat], Field(default_factory=list, max_length=200)
    ]

    @model_validator(mode="after")
    def _check_declaration(self) -> SingleCurveDeclarationDTO:
        _validate_representation(self.parameterization, self.log_df_scheme)
        if self.calibrate_discount_curve != (self.target_tenor is None):
            raise ValueError("target_tenor must be null only for discount declarations")
        if self.knot_dates and self.knot_dates != sorted(set(self.knot_dates)):
            raise ValueError("knot dates must be strictly increasing")
        return self


class SingleCalibrationRequest(CalibrationWireModel):
    schema_version: Literal[1]
    name: NonEmptyString
    today: date
    currency: NonEmptyString
    declaration: SingleCurveDeclarationDTO
    instruments: Annotated[list[RateInstrumentDTO], Field(min_length=1, max_length=100)]
    solver: SingleCalibrationSolverDTO = Field(default_factory=SingleCalibrationSolverDTO)
    options: CalibrationOptionsDTO = Field(default_factory=CalibrationOptionsDTO)

    @model_validator(mode="after")
    def _check_knots_against_today(self) -> SingleCalibrationRequest:
        declaration = self.declaration
        if not declaration.knot_dates:
            return self
        if (
            declaration.parameterization == "LOG_DISCOUNT"
            and declaration.knot_policy != "INSTRUMENTS"
        ):
            if declaration.knot_dates[0] != self.today:
                raise ValueError("LOG_DISCOUNT submitted knots must start at today")
            future = declaration.knot_dates[1:]
        else:
            future = declaration.knot_dates
        if any(knot <= self.today for knot in future):
            raise ValueError("submitted free knots must be after today")
        return self


class StagedCurveBlockDTO(CalibrationWireModel):
    name: NonEmptyString
    currency: NonEmptyString
    libor_basis: NonEmptyString
    discount_curve_ids: Annotated[dict[NonEmptyString, EntityId], Field(max_length=16)]
    forward_curve_ids: Annotated[dict[NonEmptyString, EntityId], Field(max_length=16)]


class StagedBasisDeclarationDTO(CalibrationWireModel):
    curve_name: NonEmptyString
    knot_dates: Annotated[list[date], Field(min_length=1, max_length=100)]
    instruments: Annotated[list[XccySwapInstrumentDTO], Field(min_length=1, max_length=100)]
    initial_guess_per_node: Annotated[
        list[FiniteFloat], Field(default_factory=list, max_length=100)
    ]


class StagedXccyCalibrationRequest(CalibrationWireModel):
    schema_version: Literal[1]
    name: NonEmptyString
    valuation_time: DalDateTime
    pair: CurrencyPairDTO
    collateral_currency: NonEmptyString
    fx_spot: PositiveFiniteFloat
    fx_forward_collateral: NonEmptyString
    domestic_curve_block: StagedCurveBlockDTO
    foreign_curve_block: StagedCurveBlockDTO
    basis: StagedBasisDeclarationDTO
    fixings: list[FixingObservationDTO]
    solver: StagedXccyCalibrationSolverDTO = Field(default_factory=StagedXccyCalibrationSolverDTO)
    options: CalibrationOptionsDTO = Field(default_factory=CalibrationOptionsDTO)

    @model_validator(mode="after")
    def _check_staged(self) -> StagedXccyCalibrationRequest:
        if self.domestic_curve_block.currency != self.pair.domestic:
            raise ValueError("domestic block currency must match the pair")
        if self.foreign_curve_block.currency != self.pair.foreign:
            raise ValueError("foreign block currency must match the pair")
        if self.collateral_currency != self.pair.domestic:
            raise ValueError("staged XCCY supports domestic collateral only")
        if self.basis.knot_dates != sorted(set(self.basis.knot_dates)):
            raise ValueError("basis knot dates must be strictly increasing")
        if any(knot <= self.valuation_time.date() for knot in self.basis.knot_dates):
            raise ValueError("basis knot dates must be after valuation date")
        _validate_unique_fixings(self.fixings)
        return self


class JointCurveDeclarationDTO(CalibrationWireModel):
    curve_name: NonEmptyString
    calibrate_discount_curve: bool
    target_collateral: NonEmptyString
    target_tenor: NonEmptyString | None
    base_layered_over_discount: bool
    parameterization: CurveParameterization
    log_df_scheme: LogDfScheme | None
    knot_dates: Annotated[list[date], Field(min_length=1, max_length=100)]
    instruments: Annotated[list[RateInstrumentDTO], Field(min_length=1, max_length=100)]
    smoothing_weight: PositiveFiniteFloat | None = None
    initial_guess_per_node: Annotated[
        list[FiniteFloat], Field(default_factory=list, max_length=200)
    ]

    @model_validator(mode="after")
    def _check_joint_declaration(self) -> JointCurveDeclarationDTO:
        _validate_representation(self.parameterization, self.log_df_scheme)
        if self.calibrate_discount_curve != (self.target_tenor is None):
            raise ValueError("target_tenor must be null only for discount declarations")
        if self.calibrate_discount_curve and self.base_layered_over_discount:
            raise ValueError("discount declarations cannot be layered over discount")
        if self.knot_dates != sorted(set(self.knot_dates)):
            raise ValueError("knot dates must be strictly increasing")
        return self


class JointCurrencyCurveDTO(CalibrationWireModel):
    currency: NonEmptyString
    libor_basis: NonEmptyString
    declarations: Annotated[list[JointCurveDeclarationDTO], Field(min_length=1, max_length=16)]


class JointBasisDeclarationDTO(CalibrationWireModel):
    curve_name: NonEmptyString
    parameterization: CurveParameterization
    log_df_scheme: LogDfScheme | None
    knot_dates: Annotated[list[date], Field(min_length=1, max_length=100)]
    instruments: Annotated[list[XccySwapInstrumentDTO], Field(min_length=1, max_length=100)]
    smoothing_weight: PositiveFiniteFloat | None = None
    initial_guess_per_node: Annotated[
        list[FiniteFloat], Field(default_factory=list, max_length=200)
    ]

    @model_validator(mode="after")
    def _check_joint_basis(self) -> JointBasisDeclarationDTO:
        _validate_representation(self.parameterization, self.log_df_scheme)
        if self.knot_dates != sorted(set(self.knot_dates)):
            raise ValueError("basis knot dates must be strictly increasing")
        return self


class JointXccyCalibrationRequest(CalibrationWireModel):
    model_config = ConfigDict(
        extra="forbid",
        validate_default=True,
        json_schema_extra={"x-dal-max-total-free-parameters": MAX_TOTAL_FREE_PARAMETERS},
    )

    schema_version: Literal[1]
    name: NonEmptyString
    valuation_time: DalDateTime
    pair: CurrencyPairDTO
    collateral_currency: NonEmptyString
    fx_spot: PositiveFiniteFloat
    domestic: JointCurrencyCurveDTO
    foreign: JointCurrencyCurveDTO
    basis: JointBasisDeclarationDTO
    fixings: list[FixingObservationDTO]
    solver: JointXccyCalibrationSolverDTO = Field(default_factory=JointXccyCalibrationSolverDTO)
    options: CalibrationOptionsDTO = Field(default_factory=CalibrationOptionsDTO)

    @model_validator(mode="after")
    def _check_joint(self) -> JointXccyCalibrationRequest:
        if self.domestic.currency != self.pair.domestic:
            raise ValueError("domestic currency must match the pair")
        if self.foreign.currency != self.pair.foreign:
            raise ValueError("foreign currency must match the pair")
        if self.collateral_currency != self.pair.domestic:
            raise ValueError("joint XCCY supports domestic collateral only")
        if len(self.domestic.declarations) + len(self.foreign.declarations) + 1 > 16:
            raise ValueError("joint XCCY supports at most 16 total declarations")
        total_instruments = sum(
            len(declaration.instruments)
            for declaration in (*self.domestic.declarations, *self.foreign.declarations)
        ) + len(self.basis.instruments)
        if total_instruments > 100:
            raise ValueError("joint XCCY supports at most 100 total instruments")
        for group in (self.domestic, self.foreign):
            if not any(declaration.calibrate_discount_curve for declaration in group.declarations):
                raise ValueError("each currency requires a discount declaration")
            collateral_slots = [
                declaration.target_collateral
                for declaration in group.declarations
                if declaration.calibrate_discount_curve
            ]
            tenor_slots = [
                declaration.target_tenor
                for declaration in group.declarations
                if not declaration.calibrate_discount_curve
            ]
            if len(collateral_slots) != len(set(collateral_slots)):
                raise ValueError("discount collateral slots must be unique")
            if len(tenor_slots) != len(set(tenor_slots)):
                raise ValueError("forward tenor slots must be unique")
        _validate_unique_fixings(self.fixings)
        return self


def _validate_unique_fixings(fixings: list[FixingObservationDTO]) -> None:
    keys = [(fixing.index_name, fixing.timestamp) for fixing in fixings]
    if len(keys) != len(set(keys)):
        raise ValueError("fixing observations must have unique index/timestamp keys")
    values = {
        (fixing.index_name, fixing.timestamp): fixing.value
        for fixing in fixings
    }
    for (index_name, timestamp), value in values.items():
        if not index_name.startswith("FX[") or not index_name.endswith("]"):
            continue
        pair = index_name[3:-1]
        if pair.count("/") != 1:
            continue
        numerator, denominator = pair.split("/")
        if not numerator or not denominator:
            continue
        reverse = values.get((f"FX[{denominator}/{numerator}]", timestamp))
        if reverse is not None and abs(value * reverse - 1.0) > 1.0e-10:
            raise ValueError(
                "reciprocal FX fixing observations must be mutually consistent"
            )


class CurveTargetDTO(CalibrationWireModel):
    collateral: NonEmptyString
    tenor: NonEmptyString | None


class PwcParametersDTO(CalibrationWireModel):
    right_forwards: Annotated[list[FiniteFloat], Field(min_length=1, max_length=100)]


class PwlfParametersDTO(CalibrationWireModel):
    left_forwards: Annotated[list[FiniteFloat], Field(min_length=1, max_length=100)]
    right_forwards: Annotated[list[FiniteFloat], Field(min_length=1, max_length=100)]


class ZeroRateParametersDTO(CalibrationWireModel):
    zero_rates: Annotated[list[FiniteFloat], Field(min_length=1, max_length=99)]


class LogDiscountParametersDTO(CalibrationWireModel):
    log_discount_factors: Annotated[list[FiniteFloat], Field(min_length=2, max_length=100)]


class CurveReconstructionBaseDTO(CalibrationWireModel):
    dto_version: Literal[1]
    id: EntityId
    name: NonEmptyString
    currency: NonEmptyString
    role: Literal["discount", "forward", "basis", "base"]
    target: CurveTargetDTO
    anchor_date: date
    base_curve_id: EntityId | None
    base: CurveReconstructionDTO | None
    source_run_id: EntityId

    @model_validator(mode="after")
    def _check_base(self) -> CurveReconstructionBaseDTO:
        if (self.base_curve_id is None) != (self.base is None):
            raise ValueError("base_curve_id and base must be null or non-null together")
        if self.base is not None:
            if self.base.id != self.base_curve_id or self.base.currency != self.currency:
                raise ValueError("expanded base must match id and currency")
            seen = {self.id}
            current = self.base
            depth = 1
            while current is not None:
                if current.id in seen:
                    raise ValueError("curve base chain contains a cycle")
                seen.add(current.id)
                depth += 1
                if depth > 8:
                    raise ValueError("curve base chain exceeds depth 8")
                current = current.base
        return self


class PwcCurveReconstructionDTO(CurveReconstructionBaseDTO):
    parameterization: Literal["PIECEWISE_CONSTANT_FWD"]
    day_count: Literal["ACT_365F"]
    log_df_scheme: None
    node_dates: Annotated[list[date], Field(min_length=1, max_length=100)]
    parameters: PwcParametersDTO

    @model_validator(mode="after")
    def _check_pwc(self) -> PwcCurveReconstructionDTO:
        _validate_node_dates(self.node_dates)
        if len(self.node_dates) != len(self.parameters.right_forwards):
            raise ValueError("PWC dates and right forwards must have equal length")
        return self


class PwlfCurveReconstructionDTO(CurveReconstructionBaseDTO):
    parameterization: Literal["PIECEWISE_LINEAR_FWD"]
    day_count: Literal["ACT_365F"]
    log_df_scheme: None
    node_dates: Annotated[list[date], Field(min_length=1, max_length=100)]
    parameters: PwlfParametersDTO

    @model_validator(mode="after")
    def _check_pwlf(self) -> PwlfCurveReconstructionDTO:
        _validate_node_dates(self.node_dates)
        lengths = (
            len(self.node_dates),
            len(self.parameters.left_forwards),
            len(self.parameters.right_forwards),
        )
        if len(set(lengths)) != 1:
            raise ValueError("PWLF dates and forward arrays must have equal length")
        return self


class ZeroRateCurveReconstructionDTO(CurveReconstructionBaseDTO):
    parameterization: Literal["ZERO_RATE"]
    day_count: NonEmptyString
    log_df_scheme: LogDfScheme
    node_dates: Annotated[list[date], Field(min_length=1, max_length=99)]
    parameters: ZeroRateParametersDTO

    @model_validator(mode="after")
    def _check_zero(self) -> ZeroRateCurveReconstructionDTO:
        _validate_node_dates(self.node_dates)
        if any(node <= self.anchor_date for node in self.node_dates):
            raise ValueError("ZERO_RATE node dates must be after the anchor")
        if len(self.node_dates) != len(self.parameters.zero_rates):
            raise ValueError("ZERO_RATE dates and rates must have equal length")
        return self


class LogDiscountCurveReconstructionDTO(CurveReconstructionBaseDTO):
    parameterization: Literal["LOG_DISCOUNT"]
    day_count: NonEmptyString
    log_df_scheme: LogDfScheme
    node_dates: Annotated[list[date], Field(min_length=2, max_length=100)]
    parameters: LogDiscountParametersDTO

    @model_validator(mode="after")
    def _check_log_discount(self) -> LogDiscountCurveReconstructionDTO:
        _validate_node_dates(self.node_dates)
        if self.node_dates[0] != self.anchor_date:
            raise ValueError("LOG_DISCOUNT storage starts at the anchor")
        if len(self.node_dates) != len(self.parameters.log_discount_factors):
            raise ValueError("LOG_DISCOUNT dates and values must have equal length")
        if self.parameters.log_discount_factors[0] != 0.0:
            raise ValueError("LOG_DISCOUNT anchor value must be zero")
        return self


CurveReconstructionDTO = Annotated[
    PwcCurveReconstructionDTO
    | PwlfCurveReconstructionDTO
    | ZeroRateCurveReconstructionDTO
    | LogDiscountCurveReconstructionDTO,
    Field(discriminator="parameterization"),
]


def _validate_node_dates(values: list[date]) -> None:
    if values != sorted(set(values)):
        raise ValueError("curve node dates must be strictly increasing")


class SolverDiagnosticsDTO(CalibrationWireModel):
    status: Literal["converged", "approximate_fit"]
    solve_mode: SolveMode
    used_approximate_fit: bool
    tolerance: PositiveFiniteFloat
    fit_tolerance: PositiveFiniteFloat
    max_abs_residual: Annotated[FiniteFloat, Field(ge=0.0)]
    rms_residual: Annotated[FiniteFloat, Field(ge=0.0)]
    evaluations: NonNegativeInt | None


class InstrumentDiagnosticDTO(CalibrationWireModel):
    instrument_id: EntityId
    group: GeneratedName
    calibration_index: NonNegativeInt
    market_rate: FiniteFloat
    model_rate: FiniteFloat
    residual: FiniteFloat


class FxForwardDTO(CalibrationWireModel):
    pair: CurrencyPairDTO
    dates: Annotated[list[date], Field(min_length=1, max_length=100)]
    forwards: Annotated[list[FiniteFloat], Field(min_length=1, max_length=100)]

    @model_validator(mode="after")
    def _check_fx_forwards(self) -> FxForwardDTO:
        if len(self.dates) != len(self.forwards):
            raise ValueError("FX forward dates and values must have equal length")
        _validate_node_dates(self.dates)
        return self


class NamedRangeDTO(CalibrationWireModel):
    name: GeneratedName
    offset: NonNegativeInt
    size: PositiveInt


class NamedRangesDTO(CalibrationWireModel):
    parameters: list[NamedRangeDTO]
    residuals: list[NamedRangeDTO]


class MatrixDTO(CalibrationWireModel):
    availability: Literal["available", "not_requested", "not_available_for_mode"]
    shape: tuple[MatrixMetadataDimension, MatrixMetadataDimension]
    row_axis: Annotated[list[str], Field(max_length=200)]
    column_axis: Annotated[list[str], Field(max_length=200)]
    scaling: Literal["unscaled", "solver_scaled"]
    residual_tolerance: PositiveFiniteFloat | None
    values: (
        Annotated[list[Annotated[list[FiniteFloat], Field(max_length=100)]], Field(max_length=100)]
        | None
    )

    @model_validator(mode="after")
    def _check_matrix(self) -> MatrixDTO:
        rows, columns = self.shape
        if len(self.row_axis) != rows or len(self.column_axis) != columns:
            raise ValueError("matrix axes must match metadata shape")
        if (self.availability == "available") != (self.values is not None):
            raise ValueError("only available matrices carry values")
        if self.values is not None:
            if rows > 100 or columns > 100:
                raise ValueError("materialized matrix values are limited to 100x100")
            if len(self.values) != rows or any(len(row) != columns for row in self.values):
                raise ValueError("matrix values must be rectangular and match shape")
        if self.scaling == "unscaled" and self.residual_tolerance is not None:
            raise ValueError("unscaled matrices require null residual tolerance")
        if self.scaling == "solver_scaled" and self.residual_tolerance is None:
            raise ValueError("solver-scaled matrices require residual tolerance")
        return self


class ParameterDeltaDTO(CalibrationWireModel):
    axis: str
    value: FiniteFloat


class QuoteBumpPreviewDTO(CalibrationWireModel):
    residual_index: NonNegativeInt
    instrument_id: EntityId
    quote_bump: FiniteFloat
    residual_tolerance: PositiveFiniteFloat
    delta_parameters: list[ParameterDeltaDTO]
    formula: Literal["delta_x = effective_inverse * delta_quote / residual_tolerance"]


class CalibrationTimingsDTO(CalibrationWireModel):
    native_solve_ms: Annotated[FiniteFloat, Field(ge=0.0)] | None
    serialization_ms: Annotated[FiniteFloat, Field(ge=0.0)] | None


class ApiErrorDTO(CalibrationWireModel):
    code: NonEmptyString
    message: str
    location: list[LocationItem] | None
    context: dict[str, JsonValue]


class ApiErrorResponse(CalibrationWireModel):
    error: ApiErrorDTO


class CalibrationRunCommonDTO(CalibrationWireModel):
    id: EntityId
    kind: Literal["single", "xccy_staged", "xccy_joint"]
    name: NonEmptyString
    schema_version: Literal[1]
    created_at: datetime
    started_at: datetime | None
    finished_at: datetime | None
    backend: NonEmptyString
    is_native: bool
    solver: NormalizedCalibrationSolverDTO
    options: NormalizedCalibrationOptionsDTO
    requested_jacobian_mode: JacobianMode
    resolved_knot_plan: ResolvedSingleKnotPlanDTO | None
    resolved_knot_plan_hash: Sha256Hex | None
    expected_execution_identity: ExecutionSingleKnotIdentityDTO | None
    expected_execution_identity_hash: Sha256Hex | None
    actual_execution_identity: ExecutionSingleKnotIdentityDTO | None
    actual_execution_identity_hash: Sha256Hex | None
    timings: CalibrationTimingsDTO

    @model_validator(mode="after")
    def _check_evidence_domains(self) -> CalibrationRunCommonDTO:
        plan_values = (
            self.resolved_knot_plan,
            self.resolved_knot_plan_hash,
            self.expected_execution_identity,
            self.expected_execution_identity_hash,
        )
        if self.kind == "single" and any(value is None for value in plan_values):
            raise ValueError("single runs require admission plan and expected identity evidence")
        if self.kind != "single" and any(value is not None for value in plan_values):
            raise ValueError("staged and joint runs cannot carry single admission evidence")
        if (self.actual_execution_identity is None) != (
            self.actual_execution_identity_hash is None
        ):
            raise ValueError("actual execution identity and hash must appear together")
        if self.kind != "single" and self.actual_execution_identity is not None:
            raise ValueError("only single runs carry actual execution identity")
        return self


class RunningCalibrationRunResponse(CalibrationRunCommonDTO):
    status: Literal["running"]
    phase: Literal["queued", "solving", "serializing", "persisting"]
    actual_jacobian_mode: None
    solver_diagnostics: None
    curves: list[CurveReconstructionDTO]
    instrument_diagnostics: list[InstrumentDiagnosticDTO]
    fx_forwards: None
    named_ranges: None
    jacobian: None
    effective_inverse: None
    quote_bump_preview: None
    error: None

    @model_validator(mode="after")
    def _empty_outputs(self) -> RunningCalibrationRunResponse:
        if self.curves or self.instrument_diagnostics:
            raise ValueError("running runs cannot carry outputs")
        return self


class CompletedCalibrationRunResponse(CalibrationRunCommonDTO):
    status: Literal["completed"]
    phase: Literal["finished"]
    actual_jacobian_mode: JacobianMode
    solver_diagnostics: SolverDiagnosticsDTO
    curves: Annotated[list[CurveReconstructionDTO], Field(min_length=1)]
    instrument_diagnostics: Annotated[
        list[InstrumentDiagnosticDTO], Field(min_length=1, max_length=100)
    ]
    fx_forwards: FxForwardDTO | None
    named_ranges: NamedRangesDTO
    jacobian: MatrixDTO
    effective_inverse: MatrixDTO
    quote_bump_preview: QuoteBumpPreviewDTO | None
    error: None

    @model_validator(mode="after")
    def _check_completed(self) -> CompletedCalibrationRunResponse:
        if self.actual_jacobian_mode != self.requested_jacobian_mode:
            raise ValueError("actual Jacobian mode must equal requested mode")
        if self.kind == "single" and self.fx_forwards is not None:
            raise ValueError("single runs cannot carry FX forwards")
        if self.kind != "single" and self.fx_forwards is None:
            raise ValueError("XCCY runs require FX forwards")
        return self


class FailedCalibrationRunResponse(CalibrationRunCommonDTO):
    status: Literal["failed"]
    phase: Literal["finished"]
    actual_jacobian_mode: JacobianMode | None
    solver_diagnostics: None
    curves: list[CurveReconstructionDTO]
    instrument_diagnostics: list[InstrumentDiagnosticDTO]
    fx_forwards: None
    named_ranges: None
    jacobian: None
    effective_inverse: None
    quote_bump_preview: None
    error: ApiErrorDTO

    @model_validator(mode="after")
    def _empty_outputs(self) -> FailedCalibrationRunResponse:
        if self.curves or self.instrument_diagnostics:
            raise ValueError("failed runs cannot carry output rows")
        return self


CalibrationRunResponse = Annotated[
    RunningCalibrationRunResponse | CompletedCalibrationRunResponse | FailedCalibrationRunResponse,
    Field(discriminator="status"),
]


class QuoteBumpQueryDTO(CalibrationWireModel):
    quote_bump_index: NonNegativeInt | None = None
    quote_bump_size: Annotated[float, Field(ge=-0.01, le=0.01, allow_inf_nan=False)] | None = None

    @model_validator(mode="after")
    def _check_pair(self) -> QuoteBumpQueryDTO:
        if (self.quote_bump_index is None) != (self.quote_bump_size is None):
            raise ValueError("quote bump index and size must be supplied together")
        if self.quote_bump_size == 0.0:
            raise ValueError("quote bump size must be non-zero")
        return self


CurveReconstructionBaseDTO.model_rebuild()
PwcCurveReconstructionDTO.model_rebuild()
PwlfCurveReconstructionDTO.model_rebuild()
ZeroRateCurveReconstructionDTO.model_rebuild()
LogDiscountCurveReconstructionDTO.model_rebuild()
