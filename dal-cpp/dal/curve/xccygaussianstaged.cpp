#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/xccygaussianmoments.hpp>
#include <dal/curve/xccygaussianstaged.hpp>
#include <dal/curve/xccyscript_internal.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/math/operators.hpp>

namespace Dal {
    namespace {
        Handle_<DiscountCurve_> AnchoredBasis(const XccyInstrumentCalibrationSpec_& spec, const Handle_<DiscountCurve_>& basis) {
            if (spec.basisParameterization_ == CurveParameterization_::Value_::LOG_DISCOUNT)
                return basis;
            const auto* pwc = dynamic_cast<const Tape::DiscountPWC_<double>*>(basis.get());
            CalibrationInternal::Check(pwc && !pwc->Base(), "InvalidCalibrationPlan", "/model/market/basis",
                                       "native staged forward curve without base");
            auto dates = Vector_<Date_>{spec.context_.valuationTime_.Date()};
            dates.Append(pwc->KnotDates());
            auto values = Vector_<>{pwc->FRight().front()};
            values.Append(pwc->FRight());
            return Handle_<DiscountCurve_>(new Tape::DiscountPWC_<double>(basis->Name(), basis->ccy_.String(), dates, values));
        }
        Handle_<XccyGaussianCurveData_>
        NativeBasis(const XccyInstrumentCalibrationSpec_& spec, const DiscountCurve_& original, const XccyGaussianCurveData_& anchored) {
            if (spec.basisParameterization_ == CurveParameterization_::Value_::LOG_DISCOUNT)
                return SnapshotXccyGaussianCurve(original, spec.context_.valuationTime_.Date(), spec.context_.pair_.domestic_.String(),
                                                 anchored.route_);
            const auto& pwc = dynamic_cast<const Tape::DiscountPWC_<double>&>(original);
            Vector_<String_> labels;
            for (const auto& date : pwc.KnotDates())
                labels.push_back("forward:" + anchored.currency_ + ":" + anchored.route_ + ":" + Date::ToString(date));
            Matrix_<> jacobian(anchored.snapshotJacobian_.Rows(), static_cast<int>(labels.size()), 0.);
            for (int i = 0; i < jacobian.Rows(); ++i)
                for (int j = 0; j < jacobian.Cols(); ++j)
                    jacobian(i, j) = anchored.snapshotJacobian_(i, j + 1) + (j == 0 ? anchored.snapshotJacobian_(i, 0) : 0.);
            return Handle_<XccyGaussianCurveData_>(new XccyGaussianCurveData_(original.Name(), anchored.currency_, anchored.route_,
                                                                              anchored.nodeDates_, anchored.logDF_, labels, pwc.FRight(), jacobian));
        }
    } // namespace
    Handle_<XccyGaussianModelData_> XccyGaussianBasisCandidate(const XccyInstrumentCalibrationSpec_& spec, const Handle_<DiscountCurve_>& basis) {
        CalibrationInternal::Check(static_cast<bool>(spec.gaussianDynamics_), "InvalidCalibrationPlan", "/model/dynamics", "explicit fixed dynamics");
        XccyGaussianMarketSpec_ market;
        market.valuationTime_ = spec.context_.valuationTime_;
        market.pair_ = spec.context_.pair_;
        market.collateralCurrency_ = spec.context_.collateralCurrency_;
        market.discountRoute_ = spec.context_.discountRoute_;
        market.fxSpot_ = spec.fxSpot_;
        market.domesticBlock_ = spec.domesticCurveBlock_;
        market.foreignBlock_ = spec.foreignCurveBlock_;
        market.basis_ = AnchoredBasis(spec, basis);
        const XccyGaussianMarket_ captured("market", market);
        const Handle_<XccyGaussianMarket_> native(new XccyGaussianMarket_(
            "market", spec.context_.valuationTime_.Date(), spec.context_.pair_.domestic_.String(), spec.context_.pair_.foreign_.String(),
            spec.context_.collateralCurrency_.String(), spec.fxSpot_, spec.context_.discountRoute_, captured.domestic_, captured.foreign_,
            NativeBasis(spec, *basis, *captured.basis_), captured.projections_));
        return Handle_<XccyGaussianModelData_>(new XccyGaussianModelData_("basis-candidate", native, spec.gaussianDynamics_));
    }
    template <class T_>
    Vector_<T_> EvaluateXccyGaussianStaged(const XccyInstrumentCalibrationSpec_& spec,
                                           const CurveDefinition_& definition,
                                           const Vector_<PreparedCalibrationInstrument_>& plans,
                                           const Vector_<T_>& parameters) {
        Vector_<> passive;
        for (const auto& value : parameters)
            passive.push_back(Value(value));
        const Handle_<DiscountCurve_> basis(BuildDiscountCurveT<double>(definition, passive));
        const auto model = XccyGaussianBasisCandidate(spec, basis);
        const auto inputs = XccyGaussianMomentInputs(*model);
        Vector_<T_> numeric;
        for (double value : inputs.values_)
            numeric.push_back(T_(value));
        const auto activeBasis = BuildDiscountCurveT<T_>(definition, parameters);
        for (size_t i = 1; i < model->market_->basis_->nodeDates_.size(); ++i)
            numeric[inputs.curveOffsets_[2] + i - 1] =
                Dal::log((*activeBasis)(spec.context_.valuationTime_.Date(), model->market_->basis_->nodeDates_[i]));
        Vector_<T_> result;
        for (size_t i = 0; i < plans.size(); ++i) {
            try {
                const auto& plan = plans[i];
                const auto& descriptor = *std::get<CalibrationXccy_>(plan.Descriptor());
                CalibrationInternal::Check(XccyScriptInternal::ContextIdentity(spec.context_, descriptor.fixings_.get(), {}) ==
                                               plan.ContextIdentity(),
                                           "CalibrationContextMismatch", "/context", "unchanged frozen conventions and history");
                result.push_back(EvaluateXccyGaussianMoments<T_>(plan, *model, numeric).Value(plan));
            } catch (const CalibrationError_& error) {
                auto d = error.Failure().diagnostic_;
                d.observationId_ = spec.observations_[i]->Id();
                d.quoteId_ = spec.observations_[i]->Quote()->Id();
                CalibrationInternal::Fail(d.code_, d.field_.value_or("/model"), d.expected_.value_or("valid Gaussian measure"),
                                          d.actual_.value_or(""), d);
            }
        }
        return result;
    }
    template Vector_<double> EvaluateXccyGaussianStaged(const XccyInstrumentCalibrationSpec_&,
                                                        const CurveDefinition_&,
                                                        const Vector_<PreparedCalibrationInstrument_>&,
                                                        const Vector_<double>&);
    template Vector_<AAD::Number_> EvaluateXccyGaussianStaged(const XccyInstrumentCalibrationSpec_&,
                                                              const CurveDefinition_&,
                                                              const Vector_<PreparedCalibrationInstrument_>&,
                                                              const Vector_<AAD::Number_>&);
} // namespace Dal
