#pragma once

#include <dal/curve/curveinstrumentcalibration.hpp>
#include <dal/curve/curvescript_internal.hpp>

namespace Dal::CurveScriptInternal {
    template <class T_> T_ Observe(const CalibrationCurveObservation_& observation, const CandidateCurveProvider_<T_>& provider) {
        try {
            if (observation.known_)
                return T_(*observation.known_);
            return observation.projection_ ? provider.Forward(observation.route_, observation.start_, observation.end_, observation.basis_)
                                           : provider.Discount(observation.route_, observation.start_, observation.end_);
        } catch (const CalibrationError_& error) {
            auto d = error.Failure().diagnostic_;
            d.stage_ = CalibrationStage_::Value_::Evaluate;
            d.sourceRow_ = observation.source_.sourceRow_;
            d.eventDate_ = observation.source_.eventDate_;
            d.statement_ = observation.source_.statement_;
            CalibrationInternal::Fail(d.code_, d.field_.value_or(Field(d)), d.expected_.value_or("resolvable explicit curve observation"),
                                      observation.index_ + "; fixing=" + Date::ToString(observation.fixing_) + "; " + d.actual_.value_or(""), d);
        }
    }
    template <class T_> T_ Measure(const PreparedCalibrationInstrument_& plan, const CandidateCurveProvider_<T_>& provider, const Date_& today) {
        const auto& descriptor = *std::get<CalibrationCurveFamily_>(plan.Descriptor());
        Affine_<T_> total;
        for (const auto& flow : descriptor.payments_) {
            const auto ab = Evaluate<T_>(flow, [&](size_t index) { return Observe(descriptor.observations_[index], provider); });
            const auto discount = provider.Discount(plan.Instrument()->Definition()->Spec().header_.discountRoute_, today, flow.payment_);
            total.a_ += ab.a_ * discount;
            total.b_ += ab.b_ * discount;
        }
        if (plan.Measure().kind_ == CalibrationMeasureKind_::Value_::Price)
            return total.b_ + descriptor.coupon_ * total.a_;
        const auto& measure = plan.Measure();
        CalibrationInternal::Check(measure.annuitySign_ && measure.minAbsAnnuity_ && std::isfinite(static_cast<double>(total.a_)) &&
                                       static_cast<double>(total.a_) * *measure.annuitySign_ >= *measure.minAbsAnnuity_,
                                   "InvalidCalibrationAnnuity", "/annuity", "actual finite annuity with declared sign and minimum magnitude");
        return -total.b_ / total.a_;
    }
} // namespace Dal::CurveScriptInternal
