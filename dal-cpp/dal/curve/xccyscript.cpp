#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/curve/xccyscript_internal.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/time/dateutils.hpp>

namespace Dal::XccyScriptInternal {
    using namespace CalibrationInternal;
    namespace {
        String_ Fx(const XccyCashflowPlan_& plan, const Date_& date) {
            return "FIX(" + FxIndexName(plan.config_.pair_) + "," + Date::ToString(date) + ")";
        }
        String_ Notional(const XccyCashflowPlan_& plan, size_t period) {
            if (period == 0 || plan.config_.notionalMode_ == XccyNotionalMode_::Value_::FIXED)
                return "1";
            return "(" + NumberText(plan.config_.foreignNotional_ / plan.config_.domesticNotional_) + "*" +
                   Fx(plan, plan.resets_[period - 1].fxFixingTime_.Date()) + ")";
        }
        Handle_<Index_> ParsedRate(const String_& name, const CalibrationDiagnostic_& d) {
            try {
                return Script::ParseSettingIndex(name, "xccy_rate");
            } catch (const std::exception& error) {
                Fail("UnsupportedCalibrationFixingTime", "/import/fixing/identity", "supported canonical rate identity", String_(error.what()), d);
            }
        }
        void CanonicalLibor(const Index::Libor_* libor, const XccyCouponPeriod_& period, const Ccy_& currency, const CalibrationDiagnostic_& d) {
            Check(libor && libor->ccy_ == currency && Cell::IsEmpty(libor->start_) &&
                      (libor->tenor_ == TradedRate_::Value_::LIBOR_3M_LCH || libor->tenor_ == TradedRate_::Value_::LIBOR_6M_LCH) &&
                      Bytes(libor->Name()) == Bytes(period.rateIndexName_),
                  "UnsupportedCalibrationFixingTime", "/import/fixing/identity", "exact canonical 3M/6M LCH identity without start override",
                  period.rateIndexName_, d);
        }
        void CanonicalGeometry(const Index::Libor_& libor,
                               const XccyCouponPeriod_& period,
                               const RateIndexConvention_& convention,
                               const Ccy_& currency,
                               const CalibrationDiagnostic_& d) {
            const auto basis = Ccy::Conventions::LiborDayBasis()(currency);
            Check(convention.useProjectionCurve_ && convention.forecastTenor_ == libor.tenor_.Period() &&
                      convention.collateral_ == CollateralType_::Value_::OIS &&
                      libor.StartDate(period.rateFixingTime_) == period.schedule_.accrualStart_ &&
                      Date::NominalMaturity(period.schedule_.accrualStart_, libor.tenor_.Period(), currency) == period.schedule_.accrualEnd_ &&
                      basis(period.schedule_.accrualStart_, period.schedule_.accrualEnd_, nullptr) == period.accrual_.dcf_,
                  "UnsupportedCalibrationProduct", "/import/rate", "native projection dates/accrual/collateral exactly matching canonical rate",
                  period.rateIndexName_, d);
        }
        String_ Rate(const XccyCouponPeriod_& period, const RateIndexConvention_& convention, const Ccy_& currency, const CalibrationDiagnostic_& d) {
            Check(period.rateFixingTime_.IsValid() && period.rateFixingTime_.Frac() == 0., "UnsupportedCalibrationFixingTime", "/import/fixing",
                  "exact midnight canonical rate identity", period.rateIndexName_, d);
            const auto parsed = ParsedRate(period.rateIndexName_, d);
            const auto* libor = dynamic_cast<const Index::Libor_*>(parsed.get());
            CanonicalLibor(libor, period, currency, d);
            CanonicalGeometry(*libor, period, convention, currency, d);
            return "FIX(IR[" + String_(currency.String()) + "," + String_(libor->tenor_.String()) + "]," +
                   Date::ToString(period.rateFixingTime_.Date()) + ")";
        }
        void Pay(Vector_<ScriptSourceRow_>* rows, const Date_& payment, const String_& amount) {
            rows->push_back({ScriptSourceRowKind_::Value_::Event, Date::ToString(payment), "PAY PAYS N*(" + amount + ")"});
        }
        String_ CouponExpression(const String_& notional, const String_& rate, double alpha, bool spread, bool foreign, int orientation) {
            return NumberText(spread ? -orientation : orientation) + "*" + notional + "*" + NumberText(alpha) + "*(" + rate +
                   (spread ? "+K)" : "+0)") + "*" + (foreign ? String_("SPOT()") : String_("1"));
        }
        void Coupons(Vector_<ScriptSourceRow_>* rows, const XccyCashflowPlan_& plan, bool foreign, int orientation, const CalibrationDiagnostic_& d) {
            const auto& config = plan.config_;
            const auto& periods = foreign ? plan.foreignPeriods_ : plan.domesticPeriods_;
            const auto& index = foreign ? config.convention_.foreignIndex_ : config.convention_.domesticIndex_;
            const bool spread = foreign == config.convention_.spreadOnForeignLeg_;
            for (size_t i = 0; i < periods.size(); ++i) {
                const auto& period = periods[i];
                const auto rate = Rate(period, index, foreign ? config.pair_.foreign_ : config.pair_.domestic_, d);
                const auto n = foreign ? NumberText(config.foreignNotional_ / config.domesticNotional_) : Notional(plan, i);
                Pay(rows, period.schedule_.paymentDate_, CouponExpression(n, rate, period.accrual_.dcf_, spread, foreign, orientation));
            }
        }
        void Principal(Vector_<ScriptSourceRow_>* rows, const XccyCashflowPlan_& plan, const Date_& date, bool final, int orientation) {
            const int domesticSign = (plan.config_.convention_.spreadOnForeignLeg_ ? orientation : -orientation) * (final ? 1 : -1);
            const size_t period = final ? plan.domesticPeriods_.size() - 1 : 0;
            Pay(rows, date, NumberText(domesticSign) + "*" + Notional(plan, period) + "*1");
            Pay(rows, date,
                NumberText(-domesticSign) + "*" + NumberText(plan.config_.foreignNotional_ / plan.config_.domesticNotional_) + "*" + "SPOT()");
        }
    } // namespace
    Vector_<ScriptSourceRow_> ExportRows(const XccyCashflowPlan_& plan, double coupon, bool receive, const CalibrationDiagnostic_& d) {
        Check(!plan.domesticPeriods_.empty() && !plan.foreignPeriods_.empty(), "UnsupportedCalibrationProduct", "/import/periods", "both legs", "",
              d);
        Vector_<ScriptSourceRow_> rows{{ScriptSourceRowKind_::Value_::Definition, "N", NumberText(plan.config_.domesticNotional_)},
                                       {ScriptSourceRowKind_::Value_::Definition, "K", NumberText(coupon)}};
        const int orientation = receive ? 1 : -1;
        Coupons(&rows, plan, false, orientation, d);
        Coupons(&rows, plan, true, orientation, d);
        if (plan.config_.convention_.initialNotionalExchange_)
            Principal(&rows, plan, plan.start_, false, orientation);
        if (plan.config_.convention_.finalNotionalExchange_)
            Principal(&rows, plan, plan.maturity_, true, orientation);
        if (plan.config_.notionalMode_ == XccyNotionalMode_::Value_::MARK_TO_MARKET) {
            const int sign = plan.config_.convention_.spreadOnForeignLeg_ ? orientation : -orientation;
            for (size_t i = 0; i < plan.resets_.size(); ++i)
                Pay(&rows, plan.resets_[i].effectiveDate_, NumberText(sign) + "*(" + Notional(plan, i + 1) + "-" + Notional(plan, i) + ")*1");
        }
        return rows;
    }
} // namespace Dal::XccyScriptInternal

namespace Dal {
    using namespace CalibrationInternal;
    Handle_<CalibrationInstrument_> NewXccyScriptInstrument(const CrossCurrencySwap_& native, const XccyScriptImportSettings_& settings) {
        CalibrationDiagnostic_ d;
        d.instrumentId_ = settings.instrumentId_;
        const auto& config = native.Config();
        Check(!settings.discountRoute_.empty(), "CalibrationContextMismatch", "/import/discount_route", "explicit domestic OIS route", "", d);
        if (config.notionalMode_ != XccyNotionalMode_::Value_::FIXED)
            Check(config.fxReset_.fixingHour_ == 0 && config.fxReset_.fixingMinute_ == 0, "UnsupportedCalibrationFixingTime", "/import/fx_reset",
                  "exact midnight FX reset", "", d);
        const auto [start, end] = native.TimeSpan();
        const auto plan = BuildXccyCashflowPlan(start, end, config);
        ScriptInstrumentDefinitionSpec_ spec;
        spec.header_.payCurrency_ = spec.header_.resultCurrency_ = spec.header_.collateralCurrency_ = config.pair_.domestic_;
        spec.header_.discountRoute_ = settings.discountRoute_;
        spec.source_.rows_ = XccyScriptInternal::ExportRows(plan, native.MarketRate(), settings.receiveNonSpreadPaySpread_, d);
        const auto price = CalibrationValueUnit_::Parse("Price:" + String_(config.pair_.domestic_.String()));
        const auto spread = CalibrationValueUnit_::Parse("BasisSpread");
        spec.parameters_ = {{"N", ScriptParameterType_::Value_::Real, price, ScriptParameterRole_::Value_::Notional, true, 0., {}, false, true},
                            {"K", ScriptParameterType_::Value_::Real, spread, ScriptParameterRole_::Value_::Coupon, true}};
        spec.measures_ = {{"Price", CalibrationMeasureKind_::Value_::Price, "PAY", {}, price},
                          {"BasisSpread", CalibrationMeasureKind_::Value_::ParAffineSpread, "PAY", String_("K"), spread,
                           settings.receiveNonSpreadPaySpread_ ? -1 : 1, settings.minAbsAnnuity_}};
        return NewScriptCalibrationInstrument({settings.instrumentId_, NewScriptInstrumentDefinition(spec), {}, end});
    }
} // namespace Dal
