"""Explicit test-only gateway that enables synthetic calibration fixtures."""

from app.services.dal_gateway import DalGateway as NativeDalGateway


class FakeDalGateway(NativeDalGateway):
    """Keep fixture-only calibration fallbacks out of the production gateway policy."""

    def _require_test_double_fallback(self, capability: str) -> None:
        del capability
