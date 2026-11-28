#pragma once

#include <memory>
#include <variant>

#include <dal/currency/currency.hpp>
#include <dal/math/aad/sharedsecondorder.hpp>
#include <dal/model/dupirerisk.hpp>

namespace Dal {
    struct DupireEuropeanCall_ {
        double strike_;
        double maturity_;
        String_ underlyingId_;
        Ccy_ currency_;
    };

    struct DupireEuropeanCallPdeGrid_ {
        double logSpotHalfWidth_;
        int spaceNodes_;
        int timeSteps_;
    };

    struct DupireQuoteCoordinate_ {
        String_ id_;
        double strike_;
        double maturity_;
    };

    namespace DupireSecondOrderDetail {
        struct PlanAccess_;
    } // namespace DupireSecondOrderDetail

    class DupireQuoteSecondOrderPlan_ {
        struct Data_;
        std::shared_ptr<const Data_> data_;

        explicit DupireQuoteSecondOrderPlan_(std::shared_ptr<const Data_> data);
        friend struct DupireSecondOrderDetail::PlanAccess_;
        friend DupireQuoteSecondOrderPlan_
        PrepareDupireEuropeanCallQuoteRisk(const DupireCalibrationSnapshot_&, const DupireEuropeanCall_&, const DupireEuropeanCallPdeGrid_&);

    public:
        [[nodiscard]] const Vector_<DupireQuoteCoordinate_>& InputAxis() const;
    };

    class DupireQuoteSecondOrderRisk_ {
        std::variant<AAD::SecondOrderResult_, AAD::SharedSecondOrderResult_> risk_;
        explicit DupireQuoteSecondOrderRisk_(AAD::SecondOrderResult_&& risk) : risk_(std::move(risk)) {}
        explicit DupireQuoteSecondOrderRisk_(AAD::SharedSecondOrderResult_&& risk) : risk_(std::move(risk)) {}
        friend struct DupireSecondOrderDetail::PlanAccess_;

    public:
        [[nodiscard]] double Value() const;
        [[nodiscard]] const Vector_<>& Point() const;
        [[nodiscard]] const Vector_<>& Gradient() const;
        [[nodiscard]] const Matrix_<>& Directions() const;
        [[nodiscard]] const Vector_<>& DirectionalDerivatives() const;
        [[nodiscard]] const Matrix_<>& HessianProducts() const;
        // Expose execution counts in their original units.
        [[nodiscard]] const AAD::SecondOrderResult_* Pointwise() const { return std::get_if<AAD::SecondOrderResult_>(&risk_); }
        [[nodiscard]] const AAD::SharedSecondOrderResult_* Shared() const { return std::get_if<AAD::SharedSecondOrderResult_>(&risk_); }
    };

    class DupireQuoteSecondOrderResult_ {
        DupireQuoteSecondOrderPlan_ plan_;
        DupireQuoteSecondOrderRisk_ risk_;
        DupireQuoteSecondOrderResult_(const DupireQuoteSecondOrderPlan_& plan, DupireQuoteSecondOrderRisk_&& risk);
        friend struct DupireSecondOrderDetail::PlanAccess_;

    public:
        [[nodiscard]] const DupireQuoteSecondOrderRisk_& Risk() const { return risk_; }
        [[nodiscard]] const Vector_<DupireQuoteCoordinate_>& InputAxis() const { return plan_.InputAxis(); }
        [[nodiscard]] const DupireEuropeanCall_& Call() const;
        [[nodiscard]] const String_& QuoteUnit() const;
        [[nodiscard]] const String_& CalibrationAlgorithm() const;
    };

    // Prepare and evaluate require FE_TONEAREST on the calling thread.
    // The registered quote point uses +0.0 for either input zero sign.
    [[nodiscard]] DupireQuoteSecondOrderPlan_ PrepareDupireEuropeanCallQuoteRisk(const DupireCalibrationSnapshot_& calibration,
                                                                                 const DupireEuropeanCall_& call,
                                                                                 const DupireEuropeanCallPdeGrid_& grid);

    // Columns use the exact plan axis. Requests with at most one direction use
    // pointwise AAD; the qualified multi-direction kernel uses shared AAD.
    [[nodiscard]] DupireQuoteSecondOrderResult_ EvaluateDupireQuoteSecondOrder(const DupireQuoteSecondOrderPlan_& plan,
                                                                               const Vector_<DupireQuoteCoordinate_>& inputAxis,
                                                                               const Matrix_<>& directions,
                                                                               const AAD::SecondOrderOptions_& options = {});
} // namespace Dal
