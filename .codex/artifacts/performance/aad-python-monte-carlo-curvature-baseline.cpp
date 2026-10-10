//
// Created by Codex on 2026/10/10.
//

#include <limits>
#include <memory>

#include <dal-public/src/global.hpp>
#include <dal/platform/initall.hpp>
#include <dal/script/montecarlocurvature.hpp>

namespace {
    struct Context_ {
        std::shared_ptr<const Dal::Script::BlackScholesSegmentedPreparation_> preparation_;
        Dal::Script::BlackScholesSegmentedPath_ kernel_;
        Dal::Script::SegmentedMonteCarloSettings_ settings_;
        Dal::AAD::BumpOverAADRequest_ bumps_;

        static auto Prepare() {
            Dal::Script::ScriptValuationSettings_ valuation;
            valuation.evaluationDate_ = Dal::Date_(2026, 1, 2);
            return std::make_shared<const Dal::Script::BlackScholesSegmentedPreparation_>(Dal::Script::PrepareBlackScholesSegmentedScript(
                Dal::Script::ScriptProductData_("cost", {Dal::Cell_("SCALE"), Dal::Cell_(Dal::Date_(2027, 1, 2))},
                                                {"2", "pay PAYS SCALE * FIX(EQ[SEGMENTED_PYTHON]) ^ 2"}),
                valuation));
        }

        Context_() : preparation_(Prepare()), kernel_(preparation_) {
            settings_.firstPath_ = 7;
            bumps_.directions_ = Dal::Matrix_<>(3, 5, 0.0);
            const double rows[3][5] = {{1, 0.3, -0.1, 0.2, 0.5}, {-2, -0.6, 0.2, -0.4, -1}, {0, 1, 0, 0, 0}};
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 5; ++column)
                    bumps_.directions_(row, column) = rows[row][column];
            bumps_.steps_ = {2e-4, 1e-4, 2e-4};
        }
    };

    void Copy(const Dal::Script::SegmentedMonteCarloResult_& mean, double* output) {
        output[0] = mean.MeanValue();
        std::copy(mean.MeanGradient().begin(), mean.MeanGradient().end(), output + 1);
    }
} // namespace

extern "C" {
void* PrepareSegmentedCost() {
    Dal::RegisterAll_::Init();
    Dal::InitGlobalData(1);
    return new Context_();
}

void DestroySegmentedCost(void* context) { delete static_cast<Context_*>(context); }

int EvaluateSegmentedCost(void* context, const double* parameters, int directions, double* output) {
    try {
        const auto& prepared = *static_cast<Context_*>(context);
        const Dal::Vector_<> point(parameters, parameters + 5);
        if (directions == 0)
            Copy(Dal::Script::EvaluateBlackScholesSegmentedMonteCarlo(prepared.kernel_, point, 35, prepared.settings_), output);
        else {
            const auto result =
                Dal::Script::EvaluateBlackScholesMonteCarloCurvature(prepared.kernel_, point, 35, prepared.bumps_, prepared.settings_);
            Copy(result.Base(), output);
            std::copy(result.HessianProducts().begin(), result.HessianProducts().end(), output + 6);
        }
        return 0;
    } catch (const std::exception&) {
        output[0] = std::numeric_limits<double>::quiet_NaN();
        return 1;
    }
}
}
