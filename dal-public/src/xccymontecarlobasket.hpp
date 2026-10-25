#pragma once

#include <dal-public/src/xccymontecarlo.hpp>
#include <dal/script/xccymcbasket.hpp>

namespace Dal {
    struct XccyMonteCarloBasketRequest_ {
        XccyMonteCarloPhase_ phase_;
        CalibrationMonteCarloSampling_ sampling_;
        Handle_<MarketFixingSnapshot_> fixings_;
        CalibrationSimulationSettings_ simulation_{false, {}, 0., "Precise"};
        CalibrationDerivative_ derivativeMethod_ = CalibrationDerivative_::Value_::None;
        Vector_<String_> parameterLabels_;
    };
    using XccyMonteCarloBasketResult_ = XccyMcBasketResult_;

    class PreparedXccyMonteCarloBasket_ {
        PreparedXccyMcBasket_ core_;
        Handle_<XccyGaussianModelData_> model_;
        Handle_<CalibrationReport_> report_;
        PreparedXccyMonteCarloBasket_(PreparedXccyMcBasket_ core, Handle_<XccyGaussianModelData_> model, Handle_<CalibrationReport_> report);
        friend PreparedXccyMonteCarloBasket_
        PrepareXccyBasketMonteCarlo(const Vector_<XccyMcBasketMember_>&, const Handle_<XccyGaussianModelData_>&, const XccyMonteCarloBasketRequest_&);

    public:
        [[nodiscard]] const XccyMcBasketAllocation_& Allocation() const { return core_.Allocation(); }
        [[nodiscard]] const String_& PlanIdentity() const { return core_.PlanIdentity(); }
        [[nodiscard]] const Handle_<CalibrationReport_>& Report() const { return report_; }
        [[nodiscard]] const Handle_<XccyGaussianModelData_>& Model() const { return model_; }
        [[nodiscard]] XccyMonteCarloBasketResult_ Evaluate(const Handle_<XccyGaussianModelData_>& candidate = {}) const;
    };
    [[nodiscard]] PreparedXccyMonteCarloBasket_ PrepareXccyBasketMonteCarlo(const Vector_<XccyMcBasketMember_>& members,
                                                                            const Handle_<XccyGaussianModelData_>& model,
                                                                            const XccyMonteCarloBasketRequest_& request);
    [[nodiscard]] String_ DumpXccyMonteCarloBasketResultJson(const XccyMonteCarloBasketResult_& result);
    [[nodiscard]] String_ DumpXccyMonteCarloBasketPartialJson(const Handle_<XccyMcBasketPartial_>& partial);
} // namespace Dal
