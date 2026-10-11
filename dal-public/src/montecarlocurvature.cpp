//
// Created by Codex on 2026/10/10.
//

#include <dal/platform/platform.hpp>
#include <dal/storage/globals.hpp>

#include <dal-public/src/montecarlocurvature.hpp>

namespace Dal {
    BlackScholesMonteCarloPlan_ PlanBlackScholesMonteCarlo(const Handle_<Script::ScriptProductData_>& product,
                                                           const Script::ScriptValuationSettings_& valuation,
                                                           double smoothing) {
        XGLOBAL::ValuationMutationGuard_ guard;
        REQUIRE2(product, "BlackScholesMonteCarloPlan: product must be present", ScriptError_);
        const Handle_<Script::ScriptProductData_> contract(
            new Script::ScriptProductData_(product->Name(), product->Dates(), product->EventTexts(), product->Settings()));
        const auto preparation = std::make_shared<const Script::BlackScholesSegmentedPreparation_>(
            Script::PrepareBlackScholesSegmentedScript(*contract, valuation, {}, {}, smoothing));
        auto kernel = std::make_shared<const Script::BlackScholesSegmentedPath_>(preparation);
        const auto& prepared = preparation->Prepared();
        Vector_<Script::RiskObservationSnapshot_> observations;
        for (const auto& request : prepared.Plan().Requests())
            observations.push_back(
                {request.key_.canonicalIndex_, request.key_.fixingTime_, request.historical_,
                 request.historyValueId_ ? std::optional<double>(prepared.Plan().KnownValue(*request.historyValueId_)) : std::nullopt});
        return {contract, std::move(kernel), std::move(observations)};
    }

    BlackScholesMonteCarloResult_ ValueByBlackScholesSegmentedMonteCarlo(const BlackScholesMonteCarloPlan_& plan,
                                                                         const Vector_<>& point,
                                                                         size_t paths,
                                                                         const Script::SegmentedMonteCarloSettings_& settings) {
        const auto fixedPlan = plan;
        auto fixedPoint = point;
        const auto fixedSettings = settings;
        auto result = Script::EvaluateBlackScholesSegmentedMonteCarlo(*fixedPlan.kernel_, fixedPoint, paths, fixedSettings);
        return {fixedPlan, std::move(fixedPoint), fixedSettings, std::move(result)};
    }

    BlackScholesMonteCarloCurvatureResult_ ValueByBlackScholesMonteCarloWithCurvature(const BlackScholesMonteCarloPlan_& plan,
                                                                                      const Vector_<>& point,
                                                                                      size_t paths,
                                                                                      const AAD::BumpOverAADRequest_& bumps,
                                                                                      const Script::SegmentedMonteCarloSettings_& settings) {
        const auto fixedPlan = plan;
        auto result = Script::EvaluateBlackScholesMonteCarloCurvature(*fixedPlan.kernel_, point, paths, bumps, settings);
        return {fixedPlan, std::move(result)};
    }
} // namespace Dal
