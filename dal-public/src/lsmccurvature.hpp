//
// Created by Codex on 2026/10/10.
//

#pragma once

#include <memory>
#include <utility>

#include <dal/script/lsmccurvature.hpp>
#include <dal/script/riskresults.hpp>

#include <dal-public/src/value.hpp>

namespace Dal {
    class BlackScholesLsmcCurvatureResult_;

    class BlackScholesLsmcPlan_ {
        Handle_<Script::ScriptProductData_> contract_;
        std::shared_ptr<const Script::PreparedScript_> prepared_;
        std::shared_ptr<const Vector_<String_>> labels_;
        std::shared_ptr<const Vector_<Script::RiskObservationSnapshot_>> observations_;

        BlackScholesLsmcPlan_(Handle_<Script::ScriptProductData_> contract,
                              std::shared_ptr<const Script::PreparedScript_> prepared,
                              Vector_<String_> labels,
                              Vector_<Script::RiskObservationSnapshot_> observations)
            : contract_(std::move(contract)), prepared_(std::move(prepared)), labels_(std::make_shared<const Vector_<String_>>(std::move(labels))),
              observations_(std::make_shared<const Vector_<Script::RiskObservationSnapshot_>>(std::move(observations))) {}
        friend BlackScholesLsmcPlan_
        PlanBlackScholesLsmc(const Handle_<Script::ScriptProductData_>&, const Script::MonteCarloSettings_&, const Script::ScriptValuationSettings_&);
        friend BlackScholesLsmcCurvatureResult_
        ValueByBlackScholesLsmcWithCurvature(const BlackScholesLsmcPlan_&, const Vector_<>&, size_t, const AAD::BumpOverAADRequest_&);

    public:
        [[nodiscard]] const Script::ScriptProductData_& Contract() const { return *contract_; }
        [[nodiscard]] const Script::PreparedScript_& Prepared() const { return *prepared_; }
        [[nodiscard]] const Script::ScriptValuationSettings_& Valuation() const { return Prepared().Settings(); }
        [[nodiscard]] const Script::MonteCarloSettings_& Simulation() const { return Prepared().Simulation(); }
        [[nodiscard]] const Vector_<String_>& ParameterLabels() const { return *labels_; }
        [[nodiscard]] const Vector_<>& ScriptConstants() const { return Prepared().Product().ConstVarValues(); }
        [[nodiscard]] const Vector_<Script::RiskObservationSnapshot_>& Observations() const { return *observations_; }
    };

    [[nodiscard]] BlackScholesLsmcPlan_ PlanBlackScholesLsmc(const Handle_<Script::ScriptProductData_>& product,
                                                             const Script::MonteCarloSettings_& simulation = DefaultRiskMonteCarloSettings(),
                                                             const Script::ScriptValuationSettings_& valuation = {});

    class BlackScholesLsmcCurvatureResult_ {
        BlackScholesLsmcPlan_ plan_;
        Script::BlackScholesLsmcCurvatureResult_ curvature_;

        BlackScholesLsmcCurvatureResult_(BlackScholesLsmcPlan_ plan, Script::BlackScholesLsmcCurvatureResult_ curvature)
            : plan_(std::move(plan)), curvature_(std::move(curvature)) {}
        friend BlackScholesLsmcCurvatureResult_
        ValueByBlackScholesLsmcWithCurvature(const BlackScholesLsmcPlan_&, const Vector_<>&, size_t, const AAD::BumpOverAADRequest_&);

    public:
        [[nodiscard]] const BlackScholesLsmcPlan_& Plan() const { return plan_; }
        [[nodiscard]] const Script::BlackScholesLsmcCurvatureResult_& Curvature() const { return curvature_; }
    };

    [[nodiscard]] BlackScholesLsmcCurvatureResult_ ValueByBlackScholesLsmcWithCurvature(const BlackScholesLsmcPlan_& plan,
                                                                                        const Vector_<>& point,
                                                                                        size_t paths,
                                                                                        const AAD::BumpOverAADRequest_& bumps);
} // namespace Dal
