//
// Created by Codex on 2026/10/10.
//

#pragma once

#include <memory>
#include <utility>

#include <dal/script/montecarlocurvature.hpp>
#include <dal/script/riskresults.hpp>

namespace Dal {
    class BlackScholesMonteCarloResult_;
    class BlackScholesMonteCarloCurvatureResult_;

    class BlackScholesMonteCarloPlan_ {
        Handle_<Script::ScriptProductData_> contract_;
        std::shared_ptr<const Script::BlackScholesSegmentedPath_> kernel_;
        std::shared_ptr<const Vector_<Script::RiskObservationSnapshot_>> observations_;

        BlackScholesMonteCarloPlan_(Handle_<Script::ScriptProductData_> contract,
                                    std::shared_ptr<const Script::BlackScholesSegmentedPath_> kernel,
                                    Vector_<Script::RiskObservationSnapshot_> observations)
            : contract_(std::move(contract)), kernel_(std::move(kernel)),
              observations_(std::make_shared<const Vector_<Script::RiskObservationSnapshot_>>(std::move(observations))) {}
        friend BlackScholesMonteCarloPlan_
        PlanBlackScholesMonteCarlo(const Handle_<Script::ScriptProductData_>&, const Script::ScriptValuationSettings_&, double);
        friend BlackScholesMonteCarloResult_ ValueByBlackScholesSegmentedMonteCarlo(const BlackScholesMonteCarloPlan_&,
                                                                                    const Vector_<>&,
                                                                                    size_t,
                                                                                    const Script::SegmentedMonteCarloSettings_&);
        friend BlackScholesMonteCarloCurvatureResult_ ValueByBlackScholesMonteCarloWithCurvature(const BlackScholesMonteCarloPlan_&,
                                                                                                 const Vector_<>&,
                                                                                                 size_t,
                                                                                                 const AAD::BumpOverAADRequest_&,
                                                                                                 const Script::SegmentedMonteCarloSettings_&);

    public:
        [[nodiscard]] const Script::ScriptProductData_& Contract() const { return *contract_; }
        [[nodiscard]] const Script::PreparedScript_& Prepared() const { return *kernel_->PreparedHandle(); }
        [[nodiscard]] const Script::ScriptValuationSettings_& Valuation() const { return Prepared().Settings(); }
        [[nodiscard]] const Vector_<String_>& ParameterLabels() const { return kernel_->ParameterLabels(); }
        [[nodiscard]] const Vector_<>& ScriptConstants() const { return Prepared().Product().ConstVarValues(); }
        [[nodiscard]] const Vector_<Script::RiskObservationSnapshot_>& Observations() const { return *observations_; }
    };

    [[nodiscard]] BlackScholesMonteCarloPlan_ PlanBlackScholesMonteCarlo(const Handle_<Script::ScriptProductData_>& product,
                                                                         const Script::ScriptValuationSettings_& valuation = {},
                                                                         double smoothing = Script::DEFAULT_SMOOTH);

    class BlackScholesMonteCarloResult_ {
        BlackScholesMonteCarloPlan_ plan_;
        Vector_<> point_;
        Script::SegmentedMonteCarloSettings_ settings_;
        Script::SegmentedMonteCarloResult_ mean_;

        BlackScholesMonteCarloResult_(BlackScholesMonteCarloPlan_ plan,
                                      Vector_<> point,
                                      Script::SegmentedMonteCarloSettings_ settings,
                                      Script::SegmentedMonteCarloResult_ mean)
            : plan_(std::move(plan)), point_(std::move(point)), settings_(std::move(settings)), mean_(std::move(mean)) {}
        friend BlackScholesMonteCarloResult_ ValueByBlackScholesSegmentedMonteCarlo(const BlackScholesMonteCarloPlan_&,
                                                                                    const Vector_<>&,
                                                                                    size_t,
                                                                                    const Script::SegmentedMonteCarloSettings_&);

    public:
        [[nodiscard]] const BlackScholesMonteCarloPlan_& Plan() const { return plan_; }
        [[nodiscard]] const Vector_<>& Point() const { return point_; }
        [[nodiscard]] const Script::SegmentedMonteCarloSettings_& Settings() const { return settings_; }
        [[nodiscard]] const Script::SegmentedMonteCarloResult_& Mean() const { return mean_; }
    };

    [[nodiscard]] BlackScholesMonteCarloResult_ ValueByBlackScholesSegmentedMonteCarlo(const BlackScholesMonteCarloPlan_& plan,
                                                                                       const Vector_<>& point,
                                                                                       size_t paths,
                                                                                       const Script::SegmentedMonteCarloSettings_& settings = {});

    class BlackScholesMonteCarloCurvatureResult_ {
        BlackScholesMonteCarloPlan_ plan_;
        Script::MonteCarloCurvatureResult_ curvature_;

        BlackScholesMonteCarloCurvatureResult_(BlackScholesMonteCarloPlan_ plan, Script::MonteCarloCurvatureResult_ curvature)
            : plan_(std::move(plan)), curvature_(std::move(curvature)) {}
        friend BlackScholesMonteCarloCurvatureResult_ ValueByBlackScholesMonteCarloWithCurvature(const BlackScholesMonteCarloPlan_&,
                                                                                                 const Vector_<>&,
                                                                                                 size_t,
                                                                                                 const AAD::BumpOverAADRequest_&,
                                                                                                 const Script::SegmentedMonteCarloSettings_&);

    public:
        [[nodiscard]] const BlackScholesMonteCarloPlan_& Plan() const { return plan_; }
        [[nodiscard]] const Script::MonteCarloCurvatureResult_& Curvature() const { return curvature_; }
    };

    [[nodiscard]] BlackScholesMonteCarloCurvatureResult_
    ValueByBlackScholesMonteCarloWithCurvature(const BlackScholesMonteCarloPlan_& plan,
                                               const Vector_<>& point,
                                               size_t paths,
                                               const AAD::BumpOverAADRequest_& bumps,
                                               const Script::SegmentedMonteCarloSettings_& settings = {});
} // namespace Dal
