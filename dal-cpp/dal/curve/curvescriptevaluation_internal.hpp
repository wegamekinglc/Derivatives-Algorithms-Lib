#pragma once

#include <dal/curve/curveinstrumentcalibration.hpp>
#include <dal/curve/curvescript_internal.hpp>

namespace Dal {
    template <class T_>
    T_ CandidateCurveProvider_<T_>::Discount(const CurveBinding_& binding, const CurveScriptRoute_& route, const Date_& from, const Date_& to) const {
        if (!(from.IsValid() && to.IsValid() && from >= geometry_.today_ && to >= from))
            CalibrationInternal::Fail("CalibrationContextMismatch", "/candidate/dates",
                                      "ordered dates on or after valuation; history is resolved before evaluation");
        const T_ value = binding.target_ ? (*binding.target_)(from, to) : T_((*binding.background_)(from, to));
        if (!(std::isfinite(static_cast<double>(value)) && static_cast<double>(value) > 0.))
            CalibrationInternal::Fail("CalibrationNumericalFailure", "/routes/" + route.id_, "finite positive discount factor");
        return value;
    }
    template <class T_>
    T_ CandidateCurveProvider_<T_>::Forward(
        const CurveBinding_& binding, const CurveScriptRoute_& route, const Date_& start, const Date_& end, const DayBasis_& basis) const {
        const double accrual = basis(start, end, nullptr);
        if (!(std::isfinite(accrual) && accrual > 0.))
            CalibrationInternal::Fail("InvalidCalibrationPlan", "/routes/" + route.id_ + "/accrual", "explicit positive accrual");
        const T_ value = (T_(1.) / Discount(binding, route, start, end) - T_(1.)) / accrual;
        if (!std::isfinite(static_cast<double>(value))) {
            CalibrationDiagnostic_ d;
            d.stage_ = CalibrationStage_::Value_::Evaluate;
            CalibrationInternal::Fail("CalibrationNumericalFailure", "/routes/" + route.id_ + "/forward", "finite forward in native rate units",
                                      CalibrationInternal::NumberText(static_cast<double>(value)), d);
        }
        return value;
    }
} // namespace Dal

namespace Dal::CurveScriptInternal {
    template <class T_, class F_> T_ ObserveChecked(const CalibrationCurveObservation_& observation, F_&& evaluate) {
        try {
            return evaluate();
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
    template <class T_> T_ Observe(const CalibrationCurveObservation_& observation, const CandidateCurveProvider_<T_>& provider) {
        return ObserveChecked<T_>(observation, [&]() -> T_ {
            if (observation.known_)
                return T_(*observation.known_);
            const auto& route = provider.Route(observation.route_, observation.projection_);
            const auto curve = provider.Bind(route);
            return observation.projection_ ? provider.Forward(curve, route, observation.start_, observation.end_, observation.basis_)
                                           : provider.Discount(curve, route, observation.start_, observation.end_);
        });
    }
    template <class T_>
    T_ Observe(const CalibrationCurveObservation_& observation,
               const CandidateCurveProvider_<T_>& provider,
               const CurveScriptRoute_& route,
               const typename CandidateCurveProvider_<T_>::CurveBinding_& curve) {
        return ObserveChecked<T_>(observation, [&]() -> T_ {
            if (observation.known_)
                return T_(*observation.known_);
            return observation.projection_ ? provider.Forward(curve, route, observation.start_, observation.end_, observation.basis_)
                                           : provider.Discount(curve, route, observation.start_, observation.end_);
        });
    }
    template <class T_> struct ObservationBinding_ {
        const CurveScriptRoute_* route_ = nullptr;
        typename CandidateCurveProvider_<T_>::CurveBinding_ curve_;
    };
    template <class T_> struct EvaluationScratch_ {
        ProgramValues_<Affine_<T_>> affine_;
        ProgramValues_<T_> independent_;
        SmallProgramValues_<Affine_<T_>> affineSmall_;
        SmallProgramValues_<T_> independentSmall_;
        Vector_<ObservationBinding_<T_>> observations_;
    };
    template <class T_, class F_>
    Affine_<T_> EvaluatePayment(const CalibrationCurvePayment_& flow, const F_& fixing, EvaluationScratch_<T_>* scratch) {
        const bool small = flow.program_.size() <= SmallProgramValues_<Affine_<T_>>::LOCAL_SIZE;
        if (flow.program_.back().dependent_) {
            if (small)
                return Evaluate<T_>(flow, fixing, CalibrationStage_::Value_::Evaluate, &scratch->affineSmall_);
            return Evaluate<T_>(flow, fixing, CalibrationStage_::Value_::Evaluate, &scratch->affine_);
        }
        if (small)
            return {T_(0.), EvaluateIndependent<T_>(flow, fixing, CalibrationStage_::Value_::Evaluate, &scratch->independentSmall_)};
        return {T_(0.), EvaluateIndependent<T_>(flow, fixing, CalibrationStage_::Value_::Evaluate, &scratch->independent_)};
    }
    template <class T_>
    T_ Measure(const PreparedCalibrationInstrument_& plan,
               const CandidateCurveProvider_<T_>& provider,
               const Date_& today,
               EvaluationScratch_<T_>* scratch = nullptr) {
        const auto& descriptor = *std::get<CalibrationCurveFamily_>(plan.Descriptor());
        Affine_<T_> total;
        EvaluationScratch_<T_> local;
        auto& workspace = scratch ? *scratch : local;
        const auto& route = provider.Route(plan.Instrument()->Definition()->Spec().header_.discountRoute_, false);
        const auto curve = provider.Bind(route);
        auto& observations = workspace.observations_;
        observations.clear();
        observations.reserve(descriptor.observations_.size());
        for (const auto& observation : descriptor.observations_) {
            if (observation.known_)
                observations.push_back({});
            else {
                const auto& observationRoute = provider.Route(observation.route_, observation.projection_);
                observations.push_back({&observationRoute, provider.Bind(observationRoute)});
            }
        }
        for (const auto& flow : descriptor.payments_) {
            const auto fixing = [&](size_t index) -> T_ {
                const auto& observation = descriptor.observations_[index];
                if (observation.known_)
                    return T_(*observation.known_);
                const auto& bound = observations[index];
                return Observe(observation, provider, *bound.route_, bound.curve_);
            };
            const auto ab = EvaluatePayment<T_>(flow, fixing, &workspace);
            const auto discount = provider.Discount(curve, route, today, flow.payment_);
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
