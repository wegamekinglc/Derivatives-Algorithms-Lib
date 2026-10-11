"""Owning risk, shared portfolios, common-path curvature and fixed-grid PDE risk."""

import math

import dal
from risk_report import compare, print_reference_header


VALUATION = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 10, 10))
MATURITY = dal.Date_(2027, 10, 10)


def spot_direction(coordinates):
    return dal.BumpOverAADRequest_(
        directions=dal.DoubleMatrix_([[1.0, *([0.0] * (coordinates - 1))]]),
        steps=[0.1],
    )


def weighted_and_blocked_risk():
    product = dal.Product_New(
        ["X", "Y", MATURITY], ["2", "3", "a = X * Y b = X + Y pay PAYS 5"]
    )
    model = dal.BSModelData_New(100.0, 0.2, 0.0, 0.0)
    selection = dict(outputs=["output:0", "output:1", "payoff"], inputs=["constant:0", "constant:1"])
    rows = dal.MonteCarlo_ValueWithJacobianRisk(
        product, model, 33, valuation=VALUATION,
        request=dal.JacobianRiskRequest_(**selection, max_block_width=2),
    )
    weighted = dal.MonteCarlo_ValueWithWeightedRisk(
        product, model, 33, valuation=VALUATION,
        request=dal.WeightedRiskRequest_(**selection, weights=[2.0, -1.0, 0.5]),
    )
    compare("Weighted value: 2XY-X-Y+2.5", weighted.weighted_value, 9.5)
    for column, expected in enumerate([5.0, 3.0]):
        compare(f"Weighted gradient column {column}", weighted.jacobian[0, column], expected)
        transpose_sum = sum(weight * rows.jacobian[row, column] for row, weight in enumerate(weighted.weights))
        compare(f"J transpose weights column {column}", transpose_sum, expected)


def shared_owner_portfolio():
    products = [dal.Product_New([MATURITY], [event]) for event in
                ["pay PAYS 2 * spot() + 5", "pay PAYS 3 * spot() + 7"]]
    model = dal.BSModelData_New(100.0, 0.0, 0.0, 0.0)
    portfolio = dal.ScriptPortfolio_New(["A", "B"], products, [model, model])
    risk = dal.PortfolioMonteCarlo_ValueWithWeightedRisk(
        portfolio, 33, valuation=VALUATION,
        request=dal.PortfolioWeightedRiskRequest_(
            outputs=["trade:0:payoff", "trade:1:payoff"], weights=[2.0, -1.0],
            inputs=["model:0:parameter:0"],
        ),
    )
    compare("Shared-owner portfolio PV", risk.weighted_value, 103.0)
    compare("Shared-owner portfolio Delta", risk.jacobian[0, 0], 1.0)
    if risk.provenance.model_owners != [0, 0]:
        raise RuntimeError("Expected one shared model owner")


def segmented_curvature():
    product = dal.Product_New(["SCALE", MATURITY], ["2", "pay PAYS SCALE * spot() ^ 2"])
    plan = dal.BlackScholesMonteCarloPlan_New(product, valuation=VALUATION)
    point = [100.0, 0.2, 0.03, 0.01, *plan.script_constants]
    risk = dal.BlackScholesMonteCarlo_Get_Curvature(
        plan, point, 33, spot_direction(len(point)),
        settings=dal.SegmentedMonteCarloSettings_(first_path=7, segment_steps=1),
    )
    # For each common path the payoff is proportional to initial spot squared.
    compare("Common-path spot Delta", risk.base.gradient[0], 2 * risk.base.value / point[0])
    compare("Common-path spot Gamma", risk.hessian_products[0, 0], 2 * risk.base.value / point[0] ** 2)
    if risk.execution.gradient_evaluations != 3:
        raise RuntimeError("Expected base/plus/minus complete gradients")


def policy_curvature():
    product = dal.Product_New(
        [dal.Date_(2027, 4, 10), MATURITY], ["EXERCISE MAX(100 - spot(), 0)"] * 2
    )
    point = [100.0, 0.2, 0.05, 0.0]
    print("\nBermudan put: finite-path, finite-step estimates (raw spot units)")
    print(f"{'Policy estimator':<20}{'PV':>18}{'Spot Delta':>18}{'Spot HVP':>18}\n{'-' * 74}")
    for policy in ["Frozen", "RetrainedBump"]:
        plan = dal.BlackScholesLsmcPlan_New(
            product, valuation=VALUATION,
            simulation=dal.MonteCarloSettings_(
                enable_aad=True, compiled=True, lsmc_training_paths=256,
                lsmc_policy_risk_mode=policy, lsmc_policy_bump_relative=0.002,
            ),
        )
        risk = dal.BlackScholesLsmc_Get_Curvature(plan, point, 129, spot_direction(len(point)))
        values = [risk.value, risk.gradient[0], risk.hessian_products[0, 0]]
        if not all(math.isfinite(value) for value in values):
            raise RuntimeError("Expected finite LSMC estimator")
        if len(risk.base_policy) != 2 or risk.execution.gradient_evaluations != 3:
            raise RuntimeError("Expected two retained policies and three gradients")
        print(f"{policy:<20}" + "".join(f"{value:>18.8f}" for value in values))
    print("Outer spot step 0.1; RetrainedBump inner relative step 0.002.")
    print("Frozen holds the fitted policy fixed; RetrainedBump differences the retrained gradient estimator.")
    print("129 pricing / 256 training paths illustrate the API; assess step and sampling error for production.")


def pde_risk():
    risk = dal.EuropeanPdeRiskResult_New(
        0.05, 0.20, 110.0, settings=dal.EuropeanPdeSettings_(grid_points=65, ordinary_steps=64)
    )
    print("\nFixed-grid European PDE: PV and raw derivatives at the reported spot")
    print(f"{'Payoff':<12}{'PV':>16}{'dPV/dRate':>16}{'dPV/dVol':>16}{'dPV/dStrike':>16}\n{'-' * 76}")
    for row, payoff in enumerate(risk.payoff_labels):
        values = [risk.prices[row], *risk.jacobian.to_rows()[row]]
        if not all(math.isfinite(value) for value in values):
            raise RuntimeError("Expected finite fixed-grid PDE result")
        print(f"{payoff:<12}" + "".join(f"{value:>16.8f}" for value in values))
    print(f"Spot {risk.spot}; derivatives hold the mesh fixed. Assess grid convergence separately.")


def main():
    print_reference_header("Native financial AAD: raw-coordinate references")
    weighted_and_blocked_risk()
    shared_owner_portfolio()
    segmented_curvature()
    policy_curvature()
    pde_risk()
    print("Native AAD risk examples passed.")


if __name__ == "__main__":
    main()
