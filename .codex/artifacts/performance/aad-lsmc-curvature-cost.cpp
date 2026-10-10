//
// Created by Codex on 2026/10/10.
//

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include <dal-public/src/global.hpp>
#include <dal/script/simulation.hpp>

#if DAL_LSMC_CURVATURE_COST_MODE
#include <dal/script/lsmccurvature.hpp>
#endif

int main(int argc, char** argv) {
    using namespace Dal;
    REQUIRE(argc == 3, "expected Frozen/RetrainedBump and repeat count");
    InitGlobalData(1);
    Script::MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.compiled_ = true;
    simulation.smooth_ = 2.0;
    simulation.lsmcTrainingPaths_ = 256;
    simulation.lsmcPolicyRiskMode_ = argv[1];
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 10, 10);
    const Script::ScriptProductData_ product("", {Cell_("K"), Cell_(Date_(2027, 4, 10)), Cell_(Date_(2027, 10, 10))},
                                             {"100", "EXERCISE K - spot()", "EXERCISE K - spot()"});
    AAD::BlackScholes_<> model(100.0, 0.2, 0.05, 0.0);
    const auto prepared = std::make_shared<const Script::PreparedScript_>(Script::PrepareScript(product, &model, valuation, simulation));
#if DAL_LSMC_CURVATURE_COST_MODE
    AAD::BumpOverAADRequest_ bumps;
    bumps.directions_ = Matrix_<>(1, 5, 0.0);
    bumps.directions_(0, 0) = 1.0;
    bumps.steps_ = {0.1};
    const auto evaluate = [&] {
        const auto result = Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0, 100.0}, 128, bumps);
        return result.Value() + result.Gradient()[0] + result.HessianProducts()(0, 0);
    };
#else
    const Handle_<ModelData_> data(new BSModelData_("", 100.0, 0.2, 0.05, 0.0));
    const auto evaluate = [&] {
        const auto result = Script::MCLsmcAadSimulation(*prepared, data, 128);
        return result.aggregated_ / 128.0 + result.risks_[0];
    };
#endif
    (void)evaluate();
    const int repeats = std::stoi(argv[2]);
    REQUIRE(repeats > 0, "positive repeat count required");
    const auto start = std::chrono::steady_clock::now();
    double checksum = 0.0;
    for (int i = 0; i < repeats; ++i)
        checksum += evaluate();
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::cout << std::setprecision(17) << "seconds " << seconds << " checksum " << checksum << '\n';
}
