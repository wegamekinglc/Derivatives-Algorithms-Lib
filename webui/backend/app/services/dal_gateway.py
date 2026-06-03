"""The one and only integration surface with the DAL library.

Per the project requirement, the web backend talks to the Derivatives
Algorithms Library **exclusively through its Python public API** (the symbols
exported by the ``dal`` package: ``Date_``, ``Cell_``, ``EvaluationDate_Set``,
``Product_New``, ``BSModelData_New``, ``DupireModelData_New``,
``MonteCarlo_Value`` ...).  No router, schema or service module imports ``dal``
directly -- they all go through :class:`DalGateway`.

Backend selection
-----------------
* ``DAL_MODULE`` (default ``"dal"``) -- the importable module that provides the
  public API.  In a real deployment this is the compiled SWIG extension.
* If that import fails and ``DAL_REQUIRE_NATIVE`` is not set to a truthy value,
  the gateway falls back to :mod:`app.services.dal_stub`, which re-implements
  the same public API in pure Python for local development and CI.
"""

from __future__ import annotations

import importlib
import os
import threading
from dataclasses import dataclass
from typing import Any, Dict, List, Optional

_TRUTHY = {"1", "true", "yes", "on"}


def _is_truthy(value: Optional[str]) -> bool:
    return value is not None and value.strip().lower() in _TRUTHY


@dataclass(frozen=True)
class ValuationRequest:
    """Normalised inputs for a Monte Carlo valuation."""

    event_dates: List[Any]
    events: List[str]
    model_kind: str
    model_params: Dict[str, Any]
    num_paths: int = 1 << 16
    method: str = "sobol"
    use_brownian_bridge: bool = False
    enable_aad: bool = True
    smooth: float = 0.01
    evaluation_date: Optional[tuple[int, int, int]] = None


class DalGateway:
    """Thin, thread-safe adapter over the DAL public Python API."""

    def __init__(self, module_name: Optional[str] = None) -> None:
        self._lock = threading.Lock()
        self._module_name = module_name or os.environ.get("DAL_MODULE", "dal")
        self._dal, self._is_native = self._load_module(self._module_name)

    # -- module loading ---------------------------------------------------

    @staticmethod
    def _load_module(module_name: str):
        try:
            module = importlib.import_module(module_name)
            # The pure-python development fallback is never considered native.
            is_native = not module.__name__.endswith("dal_stub")
            return module, is_native
        except Exception as native_error:  # noqa: BLE001 - report below
            if _is_truthy(os.environ.get("DAL_REQUIRE_NATIVE")):
                raise RuntimeError(
                    f"Could not import native DAL module '{module_name}': {native_error}. "
                    "Build the C++ library and SWIG Python bindings, or unset "
                    "DAL_REQUIRE_NATIVE to use the development stub."
                ) from native_error
            from app.services import dal_stub

            return dal_stub, False

    @property
    def is_native(self) -> bool:
        """True when bound to the compiled DAL extension, False for the stub."""
        return self._is_native

    @property
    def backend_name(self) -> str:
        return self._module_name if self._is_native else "dal_stub"

    # -- primitive constructors ------------------------------------------

    def make_date(self, year: int, month: int, day: int):
        return self._dal.Date_(year, month, day)

    def _to_cell(self, value: Any):
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

    def _coerce_event_date(self, raw: Any):
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

    def build_product(self, event_dates: List[Any], events: List[str]):
        cells = [self._coerce_event_date(d) for d in event_dates]
        return self._dal.Product_New(cells, [str(e) for e in events])

    def debug_product(self, event_dates: List[Any], events: List[str]) -> str:
        product = self.build_product(event_dates, events)
        return self._dal.Product_Debug(product)

    def build_model(self, kind: str, params: Dict[str, Any]):
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

    def _build_matrix(self, spots: List[float], times: List[float], vols: Any):
        """Build a ``dal.DoubleMatrix_`` (rows=spots, cols=times) from a flat 2D list.

        The native SWIG matrix binding exposes constructor fill and read access,
        but not element mutation.  Native Dupire support is therefore limited to
        flat volatility surfaces until the public binding grows a setter.
        """
        matrix_cls = getattr(self._dal, "DoubleMatrix_", None)
        if matrix_cls is None:
            return vols

        n_rows, n_cols = len(spots), len(times)
        if len(vols) != n_rows or any(len(row) != n_cols for row in vols):
            raise ValueError("Dupire vols must be a rectangular matrix matching spots x times")

        flat_vol = float(vols[0][0])
        if any(float(vols[i][j]) != flat_vol for i in range(n_rows) for j in range(n_cols)):
            raise ValueError("Native DAL Python bindings currently support only flat Dupire volatility surfaces")
        return matrix_cls(n_rows, n_cols, flat_vol)

    # -- valuation -------------------------------------------------------

    def value(self, request: ValuationRequest) -> Dict[str, float]:
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


_gateway: Optional[DalGateway] = None
_gateway_lock = threading.Lock()


def get_gateway() -> DalGateway:
    """Return a process-wide :class:`DalGateway` singleton."""
    global _gateway
    if _gateway is None:
        with _gateway_lock:
            if _gateway is None:
                _gateway = DalGateway()
    return _gateway
