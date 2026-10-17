//
// Created by Codex on 2026/10/09.
//

#pragma once

#include <memory>

#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/math/aad/structuraljacobian.hpp>

namespace Dal {
    struct RateCurveParameterCoordinate_ {
        String_ componentKey_;
        size_t parameterOrdinal_ = 0;
    };

    // Owning immutable metadata; moved-from descriptors must be reassigned before use.
    class RateStructuralJacobianDescriptor_ {
    public:
        struct Data_;

    private:
        std::shared_ptr<const Data_> data_;
        explicit RateStructuralJacobianDescriptor_(std::shared_ptr<const Data_> data);
        const Data_& Data() const;
        friend RateStructuralJacobianDescriptor_
        CaptureRateStructuralJacobian(const Vector_<RateTradeDefinition_>&, const RatePricingMarket_&, const Vector_<RateCurveParameterCoordinate_>&);
        friend bool SameRateStructuralJacobianStructure(const RateStructuralJacobianDescriptor_&, const RateStructuralJacobianDescriptor_&);

    public:
        [[nodiscard]] bool Available() const;
        [[nodiscard]] const String_& Reason() const;
        [[nodiscard]] size_t Inputs() const;
        [[nodiscard]] size_t Outputs() const;
        [[nodiscard]] const Vector_<RateCurveParameterCoordinate_>& InputAxis() const;
        [[nodiscard]] const Vector_<String_>& OutputAxis() const;
        [[nodiscard]] const Vector_<size_t>& RowSupport(size_t row) const;
    };

    class RateStructuralJacobianPlan_ {
        RateStructuralJacobianDescriptor_ descriptor_;
        AAD::StructuralJacobianPlan_ numeric_;

        RateStructuralJacobianPlan_(RateStructuralJacobianDescriptor_ descriptor, AAD::StructuralJacobianPlan_ numeric);
        friend RateStructuralJacobianPlan_ PlanRateStructuralJacobian(const RateStructuralJacobianDescriptor_&,
                                                                      const AAD::StructuralJacobianSettings_&);

    public:
        [[nodiscard]] const RateStructuralJacobianDescriptor_& Descriptor() const { return descriptor_; }
        [[nodiscard]] const AAD::StructuralJacobianPlan_& NumericPlan() const { return numeric_; }
    };

    [[nodiscard]] RateStructuralJacobianDescriptor_ CaptureRateStructuralJacobian(const Vector_<RateTradeDefinition_>& trades,
                                                                                  const RatePricingMarket_& market,
                                                                                  const Vector_<RateCurveParameterCoordinate_>& inputAxis);
    [[nodiscard]] RateStructuralJacobianPlan_ PlanRateStructuralJacobian(const RateStructuralJacobianDescriptor_& descriptor,
                                                                         const AAD::StructuralJacobianSettings_& settings = {});
    [[nodiscard]] bool SameRateStructuralJacobianStructure(const RateStructuralJacobianDescriptor_& stored,
                                                           const RateStructuralJacobianDescriptor_& current);
} // namespace Dal
