"""Fixed-plan Dupire quote Greeks from the installed dal package."""

import dal

from float_format import format_float


def main():
    base = dal.BSModelData_New(100.0, 0.20, 0.05, 0.02)
    inputs = dal.DupireRiskInputs_(
        quote_strikes=[75.0, 105.0, 135.0],
        quote_maturities=[0.4, 1.2],
        quote_spreads=dal.DoubleMatrix_([[0.008, 0.012], [0.0, 0.004], [0.006, 0.009]]),
        inclusion_spots=list(range(60, 141, 10)),
        max_spot_spacing=10.0,
        inclusion_times=[0.5, 1.0],
        max_time_spacing=0.5,
    )
    calibration = dal.DupireCalibration_New(base, inputs)
    plan = dal.DupireQuoteSecondOrderPlan_New(
        calibration, strike=105.0, maturity=1.0, underlying_id="EQ", currency="USD", grid=(0.8, 81, 64)
    )

    axis = plan.input_axis
    e3 = [0, 0, 0, 1, 0, 0]
    e2 = [0, 0, 1, 0, 0, 0]
    mixed = [0.5, -0.25, -0.5, 1.0, 0.25, -0.5]
    single = dal.DupireQuoteSecondOrderResult_New(plan, input_axis=axis, directions=dal.DoubleMatrix_([e3]))
    risk = dal.DupireQuoteSecondOrderResult_New(plan, input_axis=axis, directions=dal.DoubleMatrix_([e3, e2, mixed]))
    products = risk.hessian_products

    print("Fixed-plan Dupire quote Greeks")
    print(f"PV (USD) {risk.value:.8f}   Quote unit {risk.quote_unit}")
    print(f"One direction {single.method}\nThree directions {risk.method}\n")
    print(f"{'Quote':<10}{'Strike':>8}{'Years':>8}{'Vega':>16}{'H e3':>16}{'H e2':>16}{'H mixed':>16}")
    for col, ((quote_id, strike, maturity), vega) in enumerate(zip(risk.input_axis, risk.gradient)):
        print(
            f"{quote_id:<10}{strike:>8.0f}{maturity:>8.1f}{format_float(vega, 8):>16}"
            f"{format_float(products[0, col], 8):>16}{format_float(products[1, col], 8):>16}"
            f"{format_float(products[2, col], 8):>16}"
        )

    print(f"\nVolga for quote:3 {products[0, 3]:.8f}")
    print(f"Cross-tenor for quote:3 and quote:2 {products[0, 2]:.8f}")
    print(f"Cross-strike for quote:3 and quote:1 {products[0, 1]:.8f}")
    print(f"Mixed directional derivative {risk.directional_derivatives[2]:.8f}")


if __name__ == "__main__":
    main()
