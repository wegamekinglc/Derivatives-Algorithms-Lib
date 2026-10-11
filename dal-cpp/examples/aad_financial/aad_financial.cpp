//
// Created by Codex on 2026/10/11.
//

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <dal/concurrency/threadpool.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/initall.hpp>
#include <dal/script/lsmccurvature.hpp>
#include <dal/script/montecarlocurvature.hpp>
#include <dal/script/portfoliobatch.hpp>
#include <dal/script/simulation.hpp>

#include "../aadreport.hpp"

using namespace Dal;
using Examples::Compare;
namespace Native = Dal::Script::Detail;

namespace {
    Script::ScriptValuationSettings_ Valuation() {
        Script::ScriptValuationSettings_ settings;
        settings.evaluationDate_ = Date_(2026, 10, 10);
        return settings;
    }

    Script::MonteCarloSettings_ Simulation() {
        Script::MonteCarloSettings_ settings;
        settings.enableAad_ = true;
        settings.compiled_ = true;
        return settings;
    }

    Native::PortfolioBatchOutput_ Output(const Native::PreparedPortfolio_& portfolio, size_t trade, size_t slot, double weight) {
        auto coordinate = Script::ScriptRiskOutputAxis(portfolio.Trades()[trade].Product())[slot];
        coordinate.id_ = "trade:" + String_(std::to_string(trade)) + ":" + coordinate.id_;
        return {trade, std::move(coordinate), weight};
    }

    void WeightedAndBlockedRisk() {
        const Handle_<Script::ScriptProductData_> product(new Script::ScriptProductData_(
            "components", {Cell_("X"), Cell_("Y"), Cell_(Date_(2027, 10, 10))}, {"2", "3", "a = X * Y b = X + Y pay PAYS 5"}));
        const Handle_<ModelData_> model(new BSModelData_("bs", 100.0, 0.2, 0.0, 0.0));
        const Handle_<Script::ScriptPortfolioData_> data(new Script::ScriptPortfolioData_("components", {{"A", product, model}}));
        const auto portfolio = Native::PrepareScriptPortfolio(data, 33, Valuation(), Simulation());
        const Vector_<Native::PortfolioBatchOutput_> outputs{Output(portfolio, 0, 0, 2.0), Output(portfolio, 0, 1, -1.0),
                                                             Output(portfolio, 0, portfolio.Trades()[0].PayOffIdx(), 0.5)};
        const auto weighted = Native::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 33}, outputs);
        Matrix_<> jacobian(3, 2, 0.0);
        // Each block has at most two output lanes; the final block has one.
        for (size_t first = 0; first < outputs.size(); first += 2) {
            const size_t width = std::min(size_t(2), outputs.size() - first);
            const Vector_<Native::PortfolioBatchOutput_> block(outputs.begin() + first, outputs.begin() + first + width);
            const auto result = Native::EvaluatePortfolioJacobianBatch(portfolio, 0, {0, 33}, block, width);
            for (size_t row = 0; row < width; ++row)
                for (int column = 0; column < 2; ++column)
                    jacobian(static_cast<int>(first + row), column) = result.constantGradientSums_[0](static_cast<int>(row), column) / 33.0;
        }
        Compare("Weighted value: 2XY-X-Y+2.5", weighted.weightedSum_ / 33.0, 9.5);
        const Vector_<> expected{5.0, 3.0};
        for (int column = 0; column < 2; ++column) {
            double transposeSum = 0.0;
            for (int row = 0; row < 3; ++row)
                transposeSum += outputs[row].weight_ * jacobian(row, column);
            Compare(column == 0 ? "Weighted dV/dX" : "Weighted dV/dY", weighted.constantGradientSums_[0][column] / 33.0, expected[column]);
            Compare(column == 0 ? "J transpose weights: X" : "J transpose weights: Y", transposeSum, expected[column]);
        }
    }

    void SharedOwnerPortfolio() {
        const Handle_<ModelData_> model(new BSModelData_("shared", 100.0, 0.0, 0.0, 0.0));
        const Handle_<Script::ScriptProductData_> first(
            new Script::ScriptProductData_("A", {Cell_(Date_(2027, 10, 10))}, {"pay PAYS 2 * spot() + 5"}));
        const Handle_<Script::ScriptProductData_> second(
            new Script::ScriptProductData_("B", {Cell_(Date_(2027, 10, 10))}, {"pay PAYS 3 * spot() + 7"}));
        const Handle_<Script::ScriptPortfolioData_> data(new Script::ScriptPortfolioData_("portfolio", {{"A", first, model}, {"B", second, model}}));
        const auto portfolio = Native::PrepareScriptPortfolio(data, 33, Valuation(), Simulation());
        const Vector_<Native::PortfolioBatchOutput_> outputs{Output(portfolio, 0, portfolio.Trades()[0].PayOffIdx(), 2.0),
                                                             Output(portfolio, 1, portfolio.Trades()[1].PayOffIdx(), -1.0)};
        const auto risk = Native::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 33}, outputs);
        Compare("Shared-owner portfolio PV", risk.weightedSum_ / 33.0, 103.0);
        Compare("Shared-owner portfolio Delta", risk.modelGradientSums_[0] / 33.0, 1.0);
        REQUIRE(portfolio.Groups().size() == 1 && risk.generatedScenarios_ == 33 && risk.evaluatorCalls_ == 66,
                "Expected one shared model group and one scenario per path");
    }

    AAD::BumpOverAADRequest_ SpotDirection(size_t coordinates) {
        AAD::BumpOverAADRequest_ request;
        request.directions_ = Matrix_<>(1, static_cast<int>(coordinates), 0.0);
        request.directions_(0, 0) = 1.0;
        request.steps_ = {0.1};
        return request;
    }

    void SegmentedCurvature() {
        const Script::ScriptProductData_ product("quadratic", {Cell_("SCALE"), Cell_(Date_(2027, 4, 10)), Cell_(Date_(2027, 10, 10))},
                                                 {"2", "pay PAYS SCALE * spot() ^ 2", "pay PAYS SCALE * spot() ^ 2"});
        const auto preparation =
            std::make_shared<const Script::BlackScholesSegmentedPreparation_>(Script::PrepareBlackScholesSegmentedScript(product, Valuation()));
        const Script::BlackScholesSegmentedPath_ kernel(preparation);
        const Vector_<> point{100.0, 0.2, 0.03, 0.01, 2.0};
        Script::SegmentedMonteCarloSettings_ settings;
        settings.firstPath_ = 7;
        settings.path_.segmentSteps_ = 1;
        const auto risk = Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 33, SpotDirection(point.size()), settings);
        Compare("Common-path spot Delta", risk.Base().MeanGradient()[0], 2.0 * risk.Base().MeanValue() / point[0]);
        Compare("Common-path spot Gamma", risk.HessianProducts()(0, 0), 2.0 * risk.Base().MeanValue() / (point[0] * point[0]));
        REQUIRE(risk.Execution().gradientEvaluations_ == 3, "Expected base/plus/minus complete gradients");
        std::cout << "  Segmented MC: two payment dates, 33 paths, offset 7, one-step segments, outer step 0.1.\n";
    }

    void PolicyCurvature() {
        const Script::ScriptProductData_ product("bermudan", {Cell_(Date_(2027, 4, 10)), Cell_(Date_(2027, 10, 10))},
                                                 {"EXERCISE MAX(100 - spot(), 0)", "EXERCISE MAX(100 - spot(), 0)"});
        const Vector_<> point{100.0, 0.2, 0.05, 0.0};
        std::cout << "\nBermudan put: finite-path, finite-step estimates (raw spot units)\n"
                  << std::left << std::setw(20) << "Policy estimator" << std::right << std::setw(18) << "PV" << std::setw(18) << "Spot Delta"
                  << std::setw(18) << "Spot HVP" << '\n'
                  << std::string(74, '-') << '\n';
        for (const auto* policy : {"Frozen", "RetrainedBump"}) {
            auto simulation = Simulation();
            simulation.lsmcTrainingPaths_ = 256;
            simulation.lsmcPolicyRiskMode_ = policy;
            simulation.lsmcPolicyBumpRelative_ = 0.002;
            AAD::BlackScholes_<> model(1.0, 0.0);
            const auto prepared = std::make_shared<const Script::PreparedScript_>(Script::PrepareScript(product, &model, Valuation(), simulation));
            const auto risk = Script::EvaluateBlackScholesLsmcCurvature(prepared, point, 129, SpotDirection(point.size()));
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
    Examples::PrintReferenceHeader("Core financial AAD: raw-coordinate references");
    WeightedAndBlockedRisk();
    SharedOwnerPortfolio();
    SegmentedCurvature();
    PolicyCurvature();
    return 0;
}
