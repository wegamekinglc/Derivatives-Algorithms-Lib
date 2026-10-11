//
// Created by Codex on 2026/10/10.
//

#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/globals.hpp>

#include <dal-public/src/lsmccurvature.hpp>

namespace Dal {
    BlackScholesLsmcPlan_ PlanBlackScholesLsmc(const Handle_<Script::ScriptProductData_>& product,
                                               const Script::MonteCarloSettings_& simulation,
                                               const Script::ScriptValuationSettings_& valuation) {
        XGLOBAL::ValuationMutationGuard_ guard;
        REQUIRE2(product, "BlackScholesLsmcPlan: product must be present", ScriptError_);
        const auto fixedSimulation = simulation;
        const auto fixedValuation = valuation;
        REQUIRE2(fixedSimulation.enableAad_, "BlackScholesLsmcPlan: native AAD preparation required", ScriptError_);
        const Handle_<Script::ScriptProductData_> contract(
            new Script::ScriptProductData_(product->Name(), product->Dates(), product->EventTexts(), product->Settings()));
        // Preparation binds only model type/observations; evaluation supplies every numeric coordinate.
        AAD::BlackScholes_<> model(100.0, 0.2, 0.03, 0.0);
        auto prepared = std::make_shared<const Script::PreparedScript_>(Script::PrepareScript(*contract, &model, fixedValuation, fixedSimulation));
        REQUIRE2(!prepared->AllExpired() && prepared->Product().ContainsExercise(), "BlackScholesLsmcPlan: live EXERCISE preparation required",
                 ScriptError_);
        Vector_<Script::RiskObservationSnapshot_> observations;
        for (const auto& request : prepared->Plan().Requests())
            observations.push_back(
                {request.key_.canonicalIndex_, request.key_.fixingTime_, request.historical_,
                 request.historyValueId_ ? std::optional<double>(prepared->Plan().KnownValue(*request.historyValueId_)) : std::nullopt});
        return {contract, prepared, Vector::Join(model.ParameterLabels(), prepared->ConstVarNames()), std::move(observations)};
    }

    BlackScholesLsmcCurvatureResult_ ValueByBlackScholesLsmcWithCurvature(const BlackScholesLsmcPlan_& plan,
                                                                          const Vector_<>& point,
                                                                          size_t paths,
                                                                          const AAD::BumpOverAADRequest_& bumps) {
        const auto fixedPlan = plan;
        auto curvature = Script::EvaluateBlackScholesLsmcCurvature(fixedPlan.prepared_, point, paths, bumps);
        return {fixedPlan, std::move(curvature)};
    }
} // namespace Dal
