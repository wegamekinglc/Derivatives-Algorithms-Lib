//
// Created by Codex on 2026/10/10.
//

#include <limits>
#include <memory>

#include <dal-public/src/global.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/initall.hpp>
#include <dal/script/lsmccurvature.hpp>

namespace {
    struct Context_ {
        std::shared_ptr<const Dal::Script::PreparedScript_> prepared_;
        Dal::AAD::BumpOverAADRequest_ bumps_;

        explicit Context_(bool retrained) {
            Dal::Script::ScriptValuationSettings_ valuation;
            valuation.evaluationDate_ = Dal::Date_(2026, 10, 10);
            Dal::Script::MonteCarloSettings_ simulation;
            simulation.enableAad_ = true;
            simulation.compiled_ = true;
            simulation.smooth_ = 2.0;
            simulation.lsmcTrainingPaths_ = 128;
            simulation.lsmcPolicyRiskMode_ = retrained ? "RetrainedBump" : "Frozen";
            Dal::AAD::BlackScholes_<> model(100.0, 0.2, 0.05, 0.0);
            const Dal::Script::ScriptProductData_ product(
                "cost", {Dal::Cell_("K"), Dal::Cell_(Dal::Date_(2027, 4, 10)), Dal::Cell_(Dal::Date_(2027, 10, 10))},
                {"45", "EXERCISE 2 * K - spot()", "EXERCISE 2 * K - spot()"});
            prepared_ = std::make_shared<const Dal::Script::PreparedScript_>(Dal::Script::PrepareScript(product, &model, valuation, simulation));
            bumps_.directions_ = Dal::Matrix_<>(1, 5, 0.0);
            const double row[5] = {1, 0.01, 0.002, -0.003, 0.2};
            for (int j = 0; j < 5; ++j)
                bumps_.directions_(0, j) = row[j];
            bumps_.steps_ = {0.1};
        }
    };
} // namespace

extern "C" {
void* PrepareLsmcCost(int retrained) {
    Dal::RegisterAll_::Init();
    Dal::InitGlobalData(1);
    return new Context_(retrained != 0);
}

void DestroyLsmcCost(void* context) { delete static_cast<Context_*>(context); }

int EvaluateLsmcCost(void* context, const double* parameters, double* output) {
    try {
        const auto& prepared = *static_cast<Context_*>(context);
        const Dal::Vector_<> point(parameters, parameters + 5);
        const auto result = Dal::Script::EvaluateBlackScholesLsmcCurvature(prepared.prepared_, point, 35, prepared.bumps_);
        output[0] = result.Value();
        std::copy(result.Gradient().begin(), result.Gradient().end(), output + 1);
        std::copy(result.HessianProducts().begin(), result.HessianProducts().end(), output + 6);
        return 0;
    } catch (const std::exception&) {
        output[0] = std::numeric_limits<double>::quiet_NaN();
        return 1;
    }
}
}
