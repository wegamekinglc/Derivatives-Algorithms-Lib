"""The one and only integration surface with the DAL library.

The web backend talks to the Derivatives Algorithms Library exclusively through
its Python public API -- the compiled ``dal`` package (the dal-python pybind11
bindings; see ``dal-python/src/bindings/value.cpp``).  No router, schema or
service module imports ``dal`` directly -- they all go through :class:`DalGateway`.
"""

from __future__ import annotations

import threading
from dataclasses import dataclass
from typing import Any

from app.native_runtime import load_native_dal

dal = load_native_dal()


@dataclass(frozen=True)
class ValuationRequest:
    """Normalised inputs for a Monte Carlo valuation."""

    event_dates: list[Any]
    events: list[str]
    model_kind: str
    model_params: dict[str, Any]
    num_paths: int = 1 << 16
    method: str = "sobol"
    use_brownian_bridge: bool = False
    enable_aad: bool = True
    smooth: float = 0.01
    evaluation_date: tuple[int, int, int] | None = None


class DalGateway:
    """Thin, thread-safe adapter over the DAL public Python API."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._dal = dal

    @property
    def is_native(self) -> bool:
        return True

    @property
    def backend_name(self) -> str:
        return "dal"

    # -- primitive constructors ------------------------------------------

    def make_date(self, year: int, month: int, day: int) -> Any:
        return self._dal.Date_(year, month, day)

    def _to_cell(self, value: Any) -> Any:
        """Wrap a python value as a ``dal.Cell_`` unless it already is one."""
        cell_cls = self._dal.Cell_
        if isinstance(value, cell_cls):
            return value
        return cell_cls(value)

    def set_evaluation_date(self, year: int, month: int, day: int) -> None:
        self._dal.EvaluationDate_Set(self.make_date(year, month, day))

    def get_evaluation_date(self) -> str:
        return repr(self._dal.EvaluationDate_Get())

    # -- product / model -------------------------------------------------

    def _coerce_event_date(self, raw: Any) -> Any:
        """Convert a JSON-friendly event-date token into a ``Cell_``.

        Accepted forms:
        * ``{"date": "YYYY-MM-DD"}``  -> Cell_ wrapping a Date_
        * a plain string label/schedule (e.g. ``"STRIKE"`` or
          ``"START: ... END: ... FREQ: 1W"``) -> Cell_ wrapping the string
        """
        if isinstance(raw, dict) and "date" in raw:
            y, m, d = (int(x) for x in str(raw["date"]).split("-"))
            return self._to_cell(self.make_date(y, m, d))
        return self._to_cell(str(raw))

    def build_product(self, event_dates: list[Any], events: list[str]) -> Any:
        cells = [self._coerce_event_date(d) for d in event_dates]
        return self._dal.Product_New(cells, [str(e) for e in events])

    def debug_product(self, event_dates: list[Any], events: list[str]) -> str:
        product = self.build_product(event_dates, events)
        return self._dal.Product_Debug(product)

    def build_model(self, kind: str, params: dict[str, Any]) -> Any:
        if kind == "BSModelData_":
            return self._dal.BSModelData_New(
                float(params["spot"]),
                float(params["vol"]),
                float(params["rate"]),
                float(params["div"]),
            )
        if kind == "DupireModelData_":
            spots = [float(x) for x in params["spots"]]
            times = [float(x) for x in params["times"]]
            vols = self._build_matrix(spots, times, params["vols"])
            return self._dal.DupireModelData_New(
                float(params["spot"]),
                float(params["rate"]),
                float(params["repo"]),
                spots,
                times,
                vols,
            )
        raise ValueError(f"Unsupported model kind: {kind!r}")

    def _build_matrix(self, spots: list[float], times: list[float], vols: Any) -> Any:
        """Build a ``dal.DoubleMatrix_`` from a spots-by-times nested sequence."""
        matrix_cls = getattr(self._dal, "DoubleMatrix_", None)
        n_rows, n_cols = len(spots), len(times)
        if len(vols) != n_rows or any(len(row) != n_cols for row in vols):
            raise ValueError("Dupire vols must be a rectangular matrix matching spots x times")

        rows = [[float(value) for value in row] for row in vols]
        if matrix_cls is None:
            return rows
        return matrix_cls(rows)

    # -- valuation -------------------------------------------------------

    def value(self, request: ValuationRequest) -> dict[str, float]:
        with self._lock:
            if request.evaluation_date is not None:
                self.set_evaluation_date(*request.evaluation_date)
            product = self.build_product(request.event_dates, request.events)
            model = self.build_model(request.model_kind, request.model_params)
            raw = self._dal.MonteCarlo_Value(
                product,
                model,
                int(request.num_paths),
                str(request.method),
                bool(request.use_brownian_bridge),
                bool(request.enable_aad),
                float(request.smooth),
            )
            return {str(k): float(v) for k, v in dict(raw).items()}


# Process-wide singleton stored in a mutable container so get_gateway()
# does not need a `global` statement.
_gateway_box: list[DalGateway | None] = [None]
_gateway_lock = threading.Lock()


def get_gateway() -> DalGateway:
    """Return a process-wide :class:`DalGateway` singleton."""
    if _gateway_box[0] is None:
        with _gateway_lock:
            if _gateway_box[0] is None:
                _gateway_box[0] = DalGateway()
    return _gateway_box[0]
