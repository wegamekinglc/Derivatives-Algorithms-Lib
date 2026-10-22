#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/curve/xccyscript_internal.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/time/dateutils.hpp>

namespace Dal::XccyScriptInternal {
    using namespace CalibrationInternal;
    namespace {
        Json_ Conventions(const Ccy_& currency) {
            return Obj({{"basis", Str(Ccy::Conventions::LiborDayBasis()(currency).String())},
                        {"spot_lag", Num(Ccy::Conventions::LiborFixDays()(currency))},
                        {"fixing_holidays", Str(Ccy::Conventions::LiborFixHolidays()(currency).String())}});
        }
    } // namespace
    String_ ContextIdentity(const CalibrationPreparationContext_& c, const CalibrationDiagnostic_& d) {
        Check(!c.model_ && !c.curve_ && !c.valuation_.evaluationDate_ && !c.valuation_.fixings_ && c.smoothing_ == 0. &&
                  c.valuation_.todayFixingPolicy_ == TodayFixingPolicy_::Value_::MODEL,
              "InvalidCalibrationFixingPolicy", "/context", "dedicated NativeXccyTimestamp context without generic valuation/model", "", d);
        return ContextIdentity(*c.xccy_, nullptr, d);
    }
    String_ ContextIdentity(const XccyScriptContext_& context, const MarketFixingSnapshot_* captured, const CalibrationDiagnostic_& d) {
        Check(context.valuationTime_.IsValid() && context.valuationTime_.Frac() == 0., "UnsupportedCalibrationFixingTime", "/context/valuation_time",
              "exact midnight valuation for NativeXccyTimestamp",
              context.valuationTime_.IsValid() ? NumberText(context.valuationTime_.Frac()) : String_("invalid"), d);
        Check(!context.todayFixingOverride_, "InvalidCalibrationFixingPolicy", "/context/today_fixing_policy",
              "NativeXccyTimestamp without generic override", "", d);
        Check(context.collateralCurrency_ == context.pair_.domestic_ && !context.discountRoute_.empty(), "CalibrationContextMismatch", "/context",
              "explicit domestic collateral and OIS discount route", "", d);
        const auto* fixings = context.fixings_ ? context.fixings_.get() : captured;
        auto values = Json_::Array();
        if (fixings)
            for (const auto& index : fixings->Values())
                for (const auto& value : index.second)
                    values.array_.push_back(Obj({{"index", Str(index.first)},
                                                 {"date", Str(Date::ToString(value.first.Date()))},
                                                 {"time", Num(value.first.Frac())},
                                                 {"value", Num(value.second)}}));
        return Fingerprint(Obj({{"policy", Str("NativeXccyTimestamp/1")},
                                {"valuation", Str(Date::ToString(context.valuationTime_.Date()))},
                                {"domestic", Str(Token(context.pair_.domestic_, "CalibrationContextMismatch", "/context/pair", d))},
                                {"foreign", Str(Token(context.pair_.foreign_, "CalibrationContextMismatch", "/context/pair", d))},
                                {"route", Str(context.discountRoute_)},
                                {"supplied", Bool(static_cast<bool>(context.fixings_))},
                                {"fixings", values},
                                {"domestic_conventions", Conventions(context.pair_.domestic_)},
                                {"foreign_conventions", Conventions(context.pair_.foreign_)}}));
    }
    CalibrationPreparationCapability_ Admit(const CalibrationEvaluationOptions_& options, const CalibrationDiagnostic_& d) {
        Token(options.evaluation_, "UnsupportedFastEvaluator", "/options/evaluation", d);
        Token(options.derivativeMethod_, "DerivativeUnavailable", "/options/derivative_method", d);
        Check(options.evaluation_ != CalibrationEvaluation_::Value_::ScriptMC, "MCCrossCurrencyUnsupported", "/options/evaluation",
              "matching stochastic FX/basis/collateral model; deterministic pricing is separate", "", d);
        Check(options.evaluation_ != CalibrationEvaluation_::Value_::GenericDeterministic, "UnsupportedFastEvaluator", "/options/evaluation",
              "complete XCCY fast template; generic script executor is not admitted", "", d);
        const auto derivative = options.derivativeMethod_ == CalibrationDerivative_::Value_::Auto
                                    ? CalibrationDerivative_(CalibrationDerivative_::Value_::AAD)
                                    : options.derivativeMethod_;
        Check(derivative == CalibrationDerivative_::Value_::AAD || derivative == CalibrationDerivative_::Value_::FiniteDifference ||
                  derivative == CalibrationDerivative_::Value_::None,
              "DerivativeUnavailable", "/options/derivative_method", "native templated AAD, caller FD or None", "", d);
        return {"dal.xccy-cashflows/1", CalibrationBackend_::Value_::FastDeterministic, derivative, true, true};
    }
    namespace {
        bool NeedsProjection(const Vector_<XccyCouponPeriod_>& periods, const CalibrationXccyDescriptor_& descriptor) {
            for (const auto& period : periods) {
                if (period.schedule_.paymentDate_ < descriptor.valuationTime_.Date() || period.rateFixingTime_ < descriptor.valuationTime_)
                    continue;
                if (period.rateFixingTime_ != descriptor.valuationTime_ || !descriptor.fixings_->Find(period.rateIndexName_, period.rateFixingTime_))
                    return true;
            }
            return false;
        }
        template <class T_>
        void Routes(const Tape::JointCurveBlock_<T_>* block,
                    const Ccy_& currency,
                    const RateIndexConvention_& index,
                    bool live,
                    const CalibrationDiagnostic_& d) {
            Check(block != nullptr, "CalibrationContextMismatch", "/market/routes", "both explicit currency blocks", "", d);
            const auto discount = block->discountCurves.find(index.collateral_);
            const auto forward = block->forwardCurves.find(index.forecastTenor_);
            Check(discount != block->discountCurves.end() && discount->second && discount->second->ccy_ == currency, "CalibrationContextMismatch",
                  "/market/routes", "explicit OIS discount of matching currency", "", d);
            if (live)
                Check(forward != block->forwardCurves.end() && forward->second && forward->second->ccy_ == currency, "CalibrationContextMismatch",
                      "/market/routes", "explicit live projection of matching tenor/currency; no fallback", "", d);
        }
        template <class T_>
        const CalibrationXccyDescriptor_&
        Descriptor(const PreparedCalibrationInstrument_& prepared, const XccyMarketView_<T_>& market, const CalibrationDiagnostic_& d) {
            const auto* matched = std::get_if<CalibrationXccy_>(&prepared.Descriptor());
            Check(matched && *matched, "InvalidCalibrationPlan", "/plan", "shared prepared XCCY plan", "", d);
            const auto& descriptor = **matched;
            const auto& plan = descriptor.plan_;
            Check(market.valuationTime_ == descriptor.valuationTime_ && market.pair_ == plan.config_.pair_ &&
                      market.collateralCurrency_ == plan.config_.pair_.domestic_,
                  "CalibrationContextMismatch", "/market", "frozen timestamp/pair/domestic collateral", "", d);
            Routes(market.domestic_, plan.config_.pair_.domestic_, plan.config_.convention_.domesticIndex_,
                   NeedsProjection(plan.domesticPeriods_, descriptor), d);
            Routes(market.foreign_, plan.config_.pair_.foreign_, plan.config_.convention_.foreignIndex_,
                   NeedsProjection(plan.foreignPeriods_, descriptor), d);
            return descriptor;
        }
        template <class T_>
        T_ Value(const PreparedCalibrationInstrument_& prepared,
                 const CalibrationXccyDescriptor_& descriptor,
                 const XccyContractValues_<T_>& values,
                 const CalibrationDiagnostic_& d) {
            const bool foreign = descriptor.plan_.config_.convention_.spreadOnForeignLeg_;
            const double sign = descriptor.receiveNonSpreadPaySpread_ ? 1. : -1.;
            const T_ a = T_(-sign) * (foreign ? values.foreignSpreadAnnuity_ : values.domesticSpreadAnnuity_);
            const T_ b = T_(sign) * (foreign ? values.domesticPv_ - values.foreignPv_ : values.foreignPv_ - values.domesticPv_);
            if (prepared.Measure().kind_ == CalibrationMeasureKind_::Value_::Price) {
                const T_ result = b + T_(descriptor.coupon_) * a;
                Check(std::isfinite(static_cast<double>(result)), "CalibrationNumericalFailure", "/measure", "finite contract Price", "", d);
                return result;
            }
            const auto& measure = prepared.Measure();
            Check(measure.annuitySign_ && measure.minAbsAnnuity_ && static_cast<double>(a) * *measure.annuitySign_ >= *measure.minAbsAnnuity_,
                  "InvalidCalibrationAnnuity", "/annuity", "declared signed annuity and floor", "", d);
            const T_ result = -b / a;
            Check(std::isfinite(static_cast<double>(result)), "CalibrationNumericalFailure", "/measure", "finite raw spread", "", d);
            return result;
        }
    } // namespace
} // namespace Dal::XccyScriptInternal

namespace Dal {
    using namespace CalibrationInternal;
    template <class T_> T_ EvaluateXccyScript(const PreparedCalibrationInstrument_& prepared, const XccyMarketView_<T_>& market) {
        CalibrationDiagnostic_ d;
        d.stage_ = CalibrationStage_::Value_::Evaluate;
        d.instrumentId_ = prepared.Instrument()->Id();
        d.measureId_ = prepared.Measure().id_;
        const auto& descriptor = XccyScriptInternal::Descriptor(prepared, market, d);
        try {
            return XccyScriptInternal::Value(prepared, descriptor, XccyContractValues(descriptor.plan_, market, *descriptor.fixings_), d);
        } catch (const CalibrationError_&) {
            throw;
        } catch (const std::exception& error) {
            Fail("CalibrationNumericalFailure", "/market", "valid XCCY pricing inputs", String_(error.what()), d);
        }
    }
    template double EvaluateXccyScript(const PreparedCalibrationInstrument_&, const XccyMarketView_<double>&);
    template AAD::Number_ EvaluateXccyScript(const PreparedCalibrationInstrument_&, const XccyMarketView_<AAD::Number_>&);
} // namespace Dal
