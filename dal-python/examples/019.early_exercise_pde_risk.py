"""Owning finite-scale PDE risks with an explicit boundary PARTIAL case."""

import dal

from float_format import format_float


def valuation(style, spot=100.0):
    today, maturity = dal.Date_(2026, 10, 18), dal.Date_(2027, 10, 18)
    dates = [dal.Date_(2027, 3, 4), maturity] if style == "BERMUDAN" else []
    contract = dal.EarlyExercisePdeContract_(today, maturity, 100.0, "PUT", style, exercise_dates=dates)
    market = dal.EarlyExercisePdeMarket_(spot, 0.2, [dal.EarlyExercisePdeCarrySegment_(maturity, 0.05, 0.0)])
    return dal.EarlyExercisePdeRequest_(contract, market, settings=dal.EarlyExercisePdeSettings_(upper=400.0))


def display(label, result):
    print(f"{label}  {result.status}  {result.method}")
    if not result.price_available:
        raise RuntimeError("example base price unavailable")
    print(f"Price per unit underlying: {format_float(result.price)}")
    print(f"{'Risk':<20}{'Value':<14}{'Status':<46}Unit")
    print("-" * 125)
    for row in result.risks:
        available = row.status in ("AVAILABLE", "AVAILABLE_EXERCISE_REGION")
        if available != (row.value is not None):
            raise RuntimeError(f"availability/value mismatch: {row.risk_id}")
        value = format_float(row.value) if available else "unavailable"
        print(f"{row.risk_id:<20}{value:<14}{row.status:<46}{row.unit}")
    print("Gamma: FINITE_SCALE_MODEL_APPROXIMATION; carry: NATIVE_MARKET_PARAMETER_PARTIAL\n")


def main():
    for style in ("BERMUDAN", "AMERICAN"):
        result = dal.EarlyExercisePdeRiskResult_New(dal.EarlyExercisePdeRiskRequest_(valuation(style)))
        if not result.is_complete:
            raise RuntimeError(f"smooth {style} example must be complete")
        display(style, result)
    price = dal.EarlyExercisePdeResult_New(valuation("AMERICAN"))
    if not price.is_success:
        raise RuntimeError("boundary control price failed")
    region = price.exercise_regions[price.exercise_levels[0].region_offset]
    if region.right_clipped:
        raise RuntimeError("boundary control edge is clipped")
    spot = 0.5 * (price.grid[region.last_node] + price.grid[region.last_node + 1])
    result = dal.EarlyExercisePdeRiskResult_New(dal.EarlyExercisePdeRiskRequest_(valuation("AMERICAN", spot)))
    if result.status != "PARTIAL":
        raise RuntimeError("boundary example must be PARTIAL")
    if result.risks[0].status != "UNAVAILABLE_NEAR_EXERCISE_BOUNDARY":
        raise RuntimeError("boundary Delta must be unavailable near exercise boundary")
    display("Boundary", result)


if __name__ == "__main__":
    main()
