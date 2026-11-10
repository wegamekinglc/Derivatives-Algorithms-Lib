#pragma once

#include <dal/curve/curvescriptevaluation_internal.hpp>

namespace Dal::CurveInstrumentInternal {
    inline CalibrationDiagnostic_ Diagnostic(const CalibrationInstrument_& instrument, const String_& measureId) {
        CalibrationDiagnostic_ result;
        result.stage_ = CalibrationStage_::Value_::Prepare;
        result.instrumentId_ = instrument.Id();
        result.measureId_ = measureId;
        return result;
    }
    struct PreparedCurvePrice_ {
        PreparedCalibrationInstrument_ plan_;
        std::shared_ptr<const CandidateCurveGeometry_> geometry_;
    };
    struct PreparedCurvePriceContext_ {
        struct Data_;
        std::shared_ptr<const Data_> data_;
    };
    PreparedCurvePriceContext_ PrepareCurvePriceContext(const PreparedCurveInstruments_&);
    PreparedCurvePrice_ PrepareCurvePrice(const CurveInstrumentCalibrationSpec_& spec,
                                          const Handle_<CalibrationInstrument_>& instrument,
                                          const String_& measureId,
                                          const CurveInstrumentCalibrationOptions_& options = {});
    PreparedCurvePrice_
    PrepareCurvePrice(const PreparedCurvePriceContext_&, const Handle_<CalibrationInstrument_>& instrument, const String_& measureId);

    template <class T_>
    T_ CashflowMeasure(const PreparedCalibrationInstrument_& plan,
                       const CandidateCurveProvider_<T_>& provider,
                       const Date_& today,
                       CurveScriptInternal::EvaluationScratch_<T_>* scratch) {
        if (std::holds_alternative<CalibrationCurveFamily_>(plan.Descriptor()))
            return CurveScriptInternal::Measure(plan, provider, today, scratch);
        T_ a(0.), b(0.), price(0.);
        const auto& route = provider.Route(plan.Instrument()->Definition()->Spec().header_.discountRoute_, false);
        const auto curve = provider.Bind(route);
        for (const auto& flow : plan.Cashflows()) {
            const T_ discount = provider.Discount(curve, route, today, flow.eventDate_);
            a += flow.a_ * discount;
            b += flow.b_ * discount;
            price += *flow.amount_ * discount;
        }
        if (plan.Measure().kind_ == CalibrationMeasureKind_::Value_::Price)
            return price;
        const auto& measure = plan.Measure();
        CalibrationInternal::Check(std::isfinite(static_cast<double>(a)) && std::isfinite(static_cast<double>(b)), "CalibrationNumericalFailure",
                                   "/coefficients", "finite affine coefficients");
        CalibrationInternal::Check(measure.annuitySign_ && measure.minAbsAnnuity_, "UnsupportedCalibrationMeasure", "/measure",
                                   "affine measure controls");
        CalibrationInternal::Check(static_cast<double>(a) * *measure.annuitySign_ >= *measure.minAbsAnnuity_, "InvalidCalibrationAnnuity", "/annuity",
                                   "finite annuity with declared sign and minimum magnitude");
        return -b / a;
    }
} // namespace Dal::CurveInstrumentInternal
