#pragma once

#include <cmath>
#include <map>

#include <dal/curve/xccygaussianmoments.hpp>
#include <dal/script/xccyobservations.hpp>
#include <dal/storage/_repository.hpp>
#include <dal/storage/globals.hpp>

#include "xccygaussianmomentfixture.hpp"

namespace Dal::TestXccyExecution {
    using State_ = AAD::XccyGaussianState_<double>;
    using Payments_ = Vector_<Script::XccyScriptPayment_<double>>;

    inline CalibrationPreparationContext_ Context(const XccyGaussianModelSpec_& spec, const Handle_<MarketFixingSnapshot_>& fixings = {}) {
        XccyScriptContext_ context;
        context.valuationTime_ = spec.market_.valuationTime_;
        context.pair_ = *spec.market_.pair_;
        context.collateralCurrency_ = spec.market_.collateralCurrency_;
        context.discountRoute_ = spec.market_.discountRoute_;
        context.fixings_ = fixings;
        CalibrationPreparationContext_ result;
        result.xccy_ = std::make_shared<const XccyScriptContext_>(context);
        return result;
    }
    inline Handle_<MarketFixingSnapshot_> Empty() { return Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({})); }
    inline PreparedCalibrationInstrument_ PrepareMoment(const Handle_<CalibrationInstrument_>& instrument,
                                                        const CalibrationPreparationContext_& context,
                                                        const String_& measure = "Price") {
        return PrepareCalibrationInstrument(instrument, measure, context);
    }
    inline Handle_<CalibrationInstrument_> Rebind(const Handle_<CalibrationInstrument_>& instrument, double spread, double notional = 110.) {
        CalibrationParameterBindings_ bindings;
        bindings.values_ = {{"K", spread, CalibrationValueUnit_::Parse("BasisSpread")}, {"N", notional, CalibrationValueUnit_::Parse("Price:USD")}};
        return BindCalibrationInstrumentParameters(instrument, bindings);
    }
    inline double Sum(const Payments_& payments) {
        double result = 0.;
        for (const auto& payment : payments)
            result += payment.discountedAmount_;
        return result;
    }
    inline double ZeroPrice(const Script::PreparedXccyScript_& script, bool compiled) {
        const Script::XccyScriptPathProvider_<> provider(script);
        const auto path = provider.GeneratePath(Vector_<>(provider.Model().SimDim(), 0.));
        return Sum(script.EvaluatePayments(path, compiled));
    }
    class ScopedHistory_ {
        XGLOBAL::ValuationMutationGuard_ guard_;
        String_ index_;
        FixHistory_ original_;

    public:
        ScopedHistory_(String_ index, const FixHistory_& values) : index_(std::move(index)), original_(Global::Fixings_().History(index_)) {
            Set(values);
        }
        void Set(const FixHistory_& values) const { XGLOBAL::StoreFixings(index_, values, false); }
        ~ScopedHistory_() {
            if (!original_.vals_.empty())
                Set(original_);
            else
                for (const auto& object : ObjectAccess_::Find(".*"))
                    if (object->Name() == "##GLOBAL##FixingsFor:" + index_)
                        static_cast<void>(ObjectAccess_::Erase(*object));
        }
    };
    inline std::map<Date_, double> ByDate(const Payments_& payments) {
        std::map<Date_, double> result;
        for (const auto& payment : payments)
            result[payment.date_] += payment.discountedAmount_;
        return result;
    }
    inline Vector_<> Normals(size_t dimension, double shift) {
        Vector_<> result(dimension);
        for (size_t i = 0; i < dimension; ++i)
            result[i] = std::sin(.7 * (i + 1)) + shift * std::cos(.3 * (i + 1));
        return result;
    }
    inline const State_& At(const Vector_<State_>& states, const Date_& date) {
        const auto found = std::lower_bound(states.begin(), states.end(), date, [](const State_& state, const Date_& d) { return state.date_ < d; });
        REQUIRE(found != states.end() && found->date_ == date, "explicit cashflow date on prepared grid");
        return *found;
    }

    // Independent scalar Brownian integration; no production transition or moment kernels.
    class CashflowReference_ {
        const XccyGaussianModelSpec_& spec_;
        const Vector_<State_>& states_;
        Handle_<MarketFixingSnapshot_> history_;
        int panels_;

        double Time(const Date_& date) const { return (date - spec_.market_.valuationTime_.Date()) / 365.; }
        static double H(double a, double t) { return a == 0. ? t : -std::expm1(-a * t) / a; }
        double Sigma(size_t driver, size_t bucket) const {
            const auto& d = spec_.dynamics_;
            return driver == 0 ? d.domesticSigma_[bucket] : (driver == 1 ? d.foreignSigma_[bucket] : d.fxSigma_[bucket]);
        }
        double Interval(size_t driver, double start, double end, double maturity, size_t bucket) const {
            const double a = driver == 0 ? spec_.dynamics_.domesticA_ : spec_.dynamics_.foreignA_;
            const double step = (end - start) / panels_, sigma = Sigma(driver, bucket);
            double result = 0.;
            for (int i = 0; i <= panels_; ++i) {
                const double kernel = driver == 2 ? 1. : H(a, maturity - start - i * step);
                const double weight = i == 0 || i == panels_ ? 1. : (i % 2 ? 4. : 2.);
                result += weight * sigma * sigma * kernel * kernel;
            }
            return result * step / 3.;
        }
        double Variance(size_t driver, const Date_& from, const Date_& to) const {
            const double start = Time(from), finish = Time(to);
            double result = 0., previous = 0.;
            for (size_t bucket = 0; bucket < spec_.dynamics_.bucketEndDates_.size(); ++bucket) {
                const double boundary = Time(spec_.dynamics_.bucketEndDates_[bucket]);
                const double left = std::max(start, previous), right = std::min(finish, boundary);
                if (right > left)
                    result += Interval(driver, left, right, finish, bucket);
                previous = boundary;
            }
            return result;
        }
        double Effective(bool foreign, const Date_& date) const {
            const auto& market = spec_.market_;
            const auto& block = foreign ? market.foreignBlock_ : market.domesticBlock_;
            const double own = block->Discount(CollateralType_("OIS"))(market.valuationTime_.Date(), date);
            return foreign ? own / (*market.basis_)(market.valuationTime_.Date(), date) : own;
        }
        double Phi(bool foreign, const Date_& date) const {
            return -std::log(Effective(foreign, date)) + .5 * Variance(foreign ? 1 : 0, spec_.market_.valuationTime_.Date(), date);
        }
        double ProjectionShift(bool foreign, const Date_& date) const {
            const auto& block = foreign ? spec_.market_.foreignBlock_ : spec_.market_.domesticBlock_;
            const double projection =
                block->Forward(PeriodLength_(foreign ? "6M" : "3M"), CollateralType_("OIS"))(spec_.market_.valuationTime_.Date(), date);
            return std::log(projection / Effective(foreign, date));
        }
        std::optional<double> Known(const String_& name, const Date_& date) const {
            if (history_ && DateTime_(date) <= spec_.market_.valuationTime_)
                return history_->Find(name, DateTime_(date));
            return {};
        }
        double LaterNotional(XccyNotionalMode_ mode, double notional) const {
            return mode == XccyNotionalMode_::Value_::FIXED || spec_.market_.valuationTime_.Date() > Date_(2027, 11, 4)
                       ? notional
                       : notional * (100. / 110.) * Reset(Date_(2027, 8, 4));
        }
        double Coupon(bool foreign, bool spreadLeg, double spread, double notional, const Date_& fixing, const Date_& start, const Date_& end) const {
            return notional * ((end - start) / 360.) * (Rate(foreign, fixing, start, end) + (spreadLeg ? spread : 0.));
        }

    public:
        CashflowReference_(const XccyGaussianModelSpec_& spec,
                           const Vector_<State_>& states,
                           Handle_<MarketFixingSnapshot_> history = {},
                           int panels = 64)
            : spec_(spec), states_(states), history_(std::move(history)), panels_(panels) {}
        double Discount(const Date_& date) const { return std::exp(-Phi(false, date) - At(states_, date).values_[2]); }
        double Spot(const Date_& date) const {
            const auto& x = At(states_, date).values_;
            return spec_.market_.fxSpot_ *
                   std::exp(Phi(false, date) - Phi(true, date) + x[2] - x[3] - .5 * Variance(2, spec_.market_.valuationTime_.Date(), date) + x[4]);
        }
        double Reset(const Date_& date) const {
            const auto known = Known("FX[EUR/USD]", date);
            return known ? *known : Spot(date);
        }
        double Rate(bool foreign, const Date_& fixing, const Date_& start, const Date_& end) const {
            const auto known = Known(foreign ? "IR:EUR,LIBOR_6M_LCH" : "IR:USD,LIBOR_3M_LCH", fixing);
            if (known)
                return *known;
            const double a = foreign ? spec_.dynamics_.foreignA_ : spec_.dynamics_.domesticA_;
            const double x = At(states_, fixing).values_[foreign ? 1 : 0];
            const double load = H(a, Time(end) - Time(fixing)) - H(a, Time(start) - Time(fixing));
            const double exponent = Phi(foreign, end) - Phi(foreign, start) + load * x +
                                    .5 * (Variance(foreign ? 1 : 0, fixing, start) - Variance(foreign ? 1 : 0, fixing, end)) +
                                    ProjectionShift(foreign, start) - ProjectionShift(foreign, end);
            return std::expm1(exponent) / ((end - start) / 360.);
        }
        Payments_ Payments(XccyNotionalMode_ mode,
                           bool foreignSpread,
                           bool receive,
                           double spread = .001,
                           double notional = 110.,
                           bool initial = true,
                           bool final = true) const {
            const Date_ start(2027, 5, 4), reset(2027, 8, 4), end(2027, 11, 4), delayed(2027, 11, 8);
            const double sign = (foreignSpread ? 1. : -1.) * (receive ? 1. : -1.);
            const double foreignN = notional * (100. / 110.);
            const double later = LaterNotional(mode, notional);
            Payments_ result;
            const auto pay = [&](const Date_& date, const auto& amount) {
                if (date >= spec_.market_.valuationTime_.Date())
                    result.push_back({date, amount() * Discount(date)});
            };
            pay(reset, [&] { return sign * Coupon(false, !foreignSpread, spread, notional, Date_(2027, 4, 30), start, reset); });
            pay(end, [&] { return sign * Coupon(false, !foreignSpread, spread, later, Date_(2027, 8, 2), reset, end); });
            pay(delayed, [&] { return -sign * Coupon(true, foreignSpread, spread, foreignN, Date_(2027, 4, 30), start, end) * Spot(delayed); });
            if (initial) {
                pay(start, [&] { return -sign * notional; });
                pay(start, [&] { return sign * foreignN * Spot(start); });
            }
            if (final) {
                pay(end, [&] { return sign * later; });
                pay(end, [&] { return -sign * foreignN * Spot(end); });
            }
            if (mode == XccyNotionalMode_::Value_::MARK_TO_MARKET)
                pay(reset, [&] { return sign * (later - notional); });
            return result;
        }
    };

    inline std::map<Date_, double>
    RealizedTerms(const XccyGaussianMomentProgram_& program, const AAD::XccyGaussian_<>& model, const Vector_<State_>& states, double spread) {
        std::map<Date_, double> result;
        for (const auto& term : program.Terms()) {
            double value = term.weight_ / model.DomesticNumeraire(At(states, term.payment_));
            if (term.foreignPayment_)
                value *= model.Spot(At(states, term.payment_));
            if (term.reset_)
                value *= model.Spot(At(states, *term.reset_));
            if (term.growth_) {
                const auto& growth = *term.growth_;
                value *= 1. + growth.accrual_.dcf_ * model.ForwardRate(term.foreignRate_ ? "EUR" : "USD", term.projectionRoute_,
                                                                       At(states, growth.rateFixingTime_.Date()), growth.schedule_.accrualStart_,
                                                                       growth.schedule_.accrualEnd_, growth.accrual_.dcf_);
            }
            result[term.payment_] += value * (term.annuity_ ? spread : 1.);
        }
        return result;
    }
} // namespace Dal::TestXccyExecution
