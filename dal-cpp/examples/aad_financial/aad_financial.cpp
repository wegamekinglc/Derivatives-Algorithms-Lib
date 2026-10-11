//
// Created by Codex on 2026/10/11.
//

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal-public/src/lsmccurvature.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/montecarlocurvature.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/platform/initall.hpp>

#include "../aadreport.hpp"

using namespace Dal;
using Examples::Compare;

namespace {
    ScriptValuationSettings_ Valuation() {
        ScriptValuationSettings_ settings;
        settings.evaluationDate_ = Date_(2026, 10, 10);
        return settings;
    }

    void WeightedAndBlockedRisk() {
        const auto product =
            NewScriptProduct("components", {Cell_("X"), Cell_("Y"), Cell_(Date_(2027, 10, 10))}, {"2", "3", "a = X * Y b = X + Y pay PAYS 5"});
        const auto model = NewBSModelData("bs", 100.0, 0.2, 0.0, 0.0);
        Script::JacobianRiskRequest_ rows;
        rows.selection_.outputs_ = Vector_<String_>{"output:0", "output:1", "payoff"};
        rows.selection_.inputs_ = Vector_<String_>{"constant:0", "constant:1"};
        rows.maxBlockWidth_ = 2;
        const auto jacobian = ValueByMonteCarloWithJacobianRisk(product, model, 33, rows, Valuation());
        Script::WeightedRiskRequest_ objective;
        objective.selection_ = rows.selection_;
        objective.weights_ = Vector_<>{2.0, -1.0, 0.5};
        const auto weighted = ValueByMonteCarloWithWeightedRisk(product, model, 33, objective, Valuation());
        Compare("Weighted value: 2XY-X-Y+2.5", weighted.WeightedValue(), 9.5);
        const Vector_<> expected{5.0, 3.0};
        for (int column = 0; column < 2; ++column) {
            double transposeSum = 0.0;
            for (int row = 0; row < 3; ++row)
                transposeSum += weighted.Weights()[row] * jacobian.Jacobian()(row, column);
            Compare(column == 0 ? "Weighted dV/dX" : "Weighted dV/dY", weighted.Jacobian()(0, column), expected[column]);
            Compare(column == 0 ? "J transpose weights: X" : "J transpose weights: Y", transposeSum, expected[column]);
        }
    }

    void SharedOwnerPortfolio() {
        const auto model = NewBSModelData("shared", 100.0, 0.0, 0.0, 0.0);
        const auto first = NewScriptProduct("A", {Cell_(Date_(2027, 10, 10))}, {"pay PAYS 2 * spot() + 5"});
        const auto second = NewScriptProduct("B", {Cell_(Date_(2027, 10, 10))}, {"pay PAYS 3 * spot() + 7"});
        const Handle_<Script::ScriptPortfolioData_> portfolio(
            new Script::ScriptPortfolioData_("portfolio", {{"A", first, model}, {"B", second, model}}));
        PortfolioWeightedRiskRequest_ request;
        request.selection_.outputs_ = Vector_<String_>{"trade:0:payoff", "trade:1:payoff"};
        request.selection_.inputs_ = Vector_<String_>{"model:0:parameter:0"};
        request.weights_ = Vector_<>{2.0, -1.0};
        const auto risk = ValuePortfolioByMonteCarloWithWeightedRisk(portfolio, 33, request, Valuation());
        Compare("Shared-owner portfolio PV", risk.WeightedValue(), 103.0);
        Compare("Shared-owner portfolio Delta", risk.Jacobian()(0, 0), 1.0);
        REQUIRE(risk.Provenance().modelOwners_ == Vector_<int>({0, 0}), "Expected one shared model owner");
    }

    AAD::BumpOverAADRequest_ SpotDirection(size_t coordinates) {
        AAD::BumpOverAADRequest_ request;
        request.directions_ = Matrix_<>(1, static_cast<int>(coordinates), 0.0);
        request.directions_(0, 0) = 1.0;
        request.steps_ = {0.1};
        return request;
    }

    void SegmentedCurvature() {
        const auto product = NewScriptProduct("quadratic", {Cell_("SCALE"), Cell_(Date_(2027, 10, 10))}, {"2", "pay PAYS SCALE * spot() ^ 2"});
        const auto plan = PlanBlackScholesMonteCarlo(product, Valuation());
        const Vector_<> point{100.0, 0.2, 0.03, 0.01, plan.ScriptConstants()[0]};
        Script::SegmentedMonteCarloSettings_ settings;
        settings.firstPath_ = 7;
        settings.path_.segmentSteps_ = 1;
        const auto result = ValueByBlackScholesMonteCarloWithCurvature(plan, point, 33, SpotDirection(point.size()), settings);
        const auto& risk = result.Curvature();
        // For every fixed Gaussian path this payoff is proportional to initial spot squared.
        Compare("Common-path spot Delta", risk.Base().MeanGradient()[0], 2.0 * risk.Base().MeanValue() / point[0]);
        Compare("Common-path spot Gamma", risk.HessianProducts()(0, 0), 2.0 * risk.Base().MeanValue() / (point[0] * point[0]));
        REQUIRE(risk.Execution().gradientEvaluations_ == 3, "Expected base/plus/minus complete gradients");
        std::cout << "  Segmented MC: 33 paths, offset 7, one spot direction, outer step 0.1.\n";
    }

    void PolicyCurvature() {
        const auto product = NewScriptProduct("bermudan", {Cell_(Date_(2027, 4, 10)), Cell_(Date_(2027, 10, 10))},
                                              {"EXERCISE MAX(100 - spot(), 0)", "EXERCISE MAX(100 - spot(), 0)"});
        const Vector_<> point{100.0, 0.2, 0.05, 0.0};
        std::cout << "\nBermudan put: finite-path, finite-step estimates (raw spot units)\n"
                  << std::left << std::setw(20) << "Policy estimator" << std::right << std::setw(18) << "PV" << std::setw(18) << "Spot Delta"
                  << std::setw(18) << "Spot HVP" << '\n'
                  << std::string(74, '-') << '\n';
        for (const auto* policy : {"Frozen", "RetrainedBump"}) {
            auto simulation = DefaultRiskMonteCarloSettings();
            simulation.compiled_ = true;
            simulation.lsmcTrainingPaths_ = 256;
            simulation.lsmcPolicyRiskMode_ = policy;
            simulation.lsmcPolicyBumpRelative_ = 0.002;
            const auto plan = PlanBlackScholesLsmc(product, simulation, Valuation());
            const auto result = ValueByBlackScholesLsmcWithCurvature(plan, point, 129, SpotDirection(point.size()));
            const auto& risk = result.Curvature();
            REQUIRE(std::isfinite(risk.Value()) && std::isfinite(risk.Gradient()[0]) && std::isfinite(risk.HessianProducts()(0, 0)),
                    "Expected finite LSMC estimator");
            REQUIRE(risk.BasePolicy().size() == 2 && risk.Execution().gradientEvaluations_ == 3,
                    "Expected two retained policies and three gradients");
            std::cout << std::left << std::setw(20) << policy << std::right << std::setw(18) << risk.Value() << std::setw(18) << risk.Gradient()[0]
                      << std::setw(18) << risk.HessianProducts()(0, 0) << '\n';
        }
        std::cout << "Outer spot step 0.1; RetrainedBump inner relative step 0.002.\n"
                  << "Frozen holds the fitted policy fixed; RetrainedBump differences the retrained gradient estimator.\n"
                  << "129 pricing / 256 training paths illustrate the API; assess step and sampling error for production.\n";
    }
} // namespace

int main() {
    RegisterAll_::Init();
    ThreadPool_::GetInstance()->Start(1, true);
    Examples::PrintReferenceHeader("Native financial AAD: raw-coordinate references");
    WeightedAndBlockedRisk();
    SharedOwnerPortfolio();
    SegmentedCurvature();
    PolicyCurvature();
    return 0;
}
