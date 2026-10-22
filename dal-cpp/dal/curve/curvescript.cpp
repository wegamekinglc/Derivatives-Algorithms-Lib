#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curvescript.hpp>
#include <dal/curve/curvescript_internal.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/dateutils.hpp>

namespace Dal {
    using namespace CalibrationInternal;
    String_ CurveScriptProjectionRoute(const Ccy_& currency, const PeriodLength_& tenor) {
        return CurveScriptInternal::ProjectionRoute(currency, tenor);
    }
    namespace {
        struct Export_ {
            Vector_<ScriptSourceRow_> rows_;
            Date_ pillar_;
        };
        struct Coupon_ {
            SchedulePeriod_ schedule_;
            AccrualPeriod_ accrual_;
        };
        CalibrationDiagnostic_ ImportDiagnostic(const CurveScriptImportSettings_& settings) {
            CalibrationDiagnostic_ d;
            d.instrumentId_ = settings.instrumentId_;
            Token(settings.currency_, "UnsupportedCalibrationProduct", "/import/currency", d);
            Check(std::isfinite(settings.notional_) && settings.notional_ > 0., "InvalidCalibrationParameterBinding", "/import/notional",
                  "finite positive explicit notional", "", d);
            return d;
        }
        void ValidateIndex(const RateIndexConvention_& index, const CalibrationDiagnostic_& d) {
            Check(index.collateral_ == CollateralType_::Value_::OIS && index.fixingLag_ >= 0, "UnsupportedCalibrationProduct", "/import/convention",
                  "OIS collateral and nonnegative fixing lag", "", d);
            Token(index.dayBasis_, "UnsupportedCalibrationProduct", "/import/day_basis", d);
        }
        double Accrual(const SchedulePeriod_& period, const DayBasis_& basis, const CalibrationDiagnostic_& d) {
            const double result = basis(period.accrualStart_, period.accrualEnd_, period.dayCountContext_.get());
            Check(std::isfinite(result) && result > 0., "UnsupportedCalibrationProduct", "/import/accrual", "positive resolved native accrual", "",
                  d);
            return result;
        }
        String_ Forward(const SchedulePeriod_& period, const RateIndexConvention_& index, const Ccy_& currency, const CalibrationDiagnostic_& d) {
            ValidateIndex(index, d);
            const auto start = Date::ToString(period.accrualStart_), end = Date::ToString(period.accrualEnd_);
            const auto fixing = Date::ToString(period.fixingDate_);
            const double alpha = Accrual(period, index.dayBasis_, d);
            if (!index.useProjectionCurve_)
                return "((1/FIX(IR[" + String_(currency.String()) + ",DF," + end + "," + start + "]," + fixing + ")-1)/" + NumberText(alpha) + ")";
            const auto route = CurveScriptProjectionRoute(currency, index.forecastTenor_);
            const auto basis = Ccy::Conventions::LiborDayBasis()(currency);
            Check(period.accrualEnd_ == Date::NominalMaturity(period.accrualStart_, index.forecastTenor_, currency) &&
                      alpha == basis(period.accrualStart_, period.accrualEnd_, nullptr),
                  "UnsupportedCalibrationProduct", "/import/projection", "native dates/accrual matching canonical nominal Libor observation", "", d);
            const String_ indexToken = route.substr(3);
            return "FIX(IR[" + indexToken + "," + start + "]," + fixing + ")";
        }
        void Payment(Export_* result, const Date_& date, const Date_& dependency, const String_& amount, const CalibrationDiagnostic_& d) {
            Check(date.IsValid() && dependency.IsValid(), "UnsupportedCalibrationProduct", "/import/payment", "concrete valid payment/dependency", "",
                  d);
            result->rows_.push_back({ScriptSourceRowKind_::Value_::Event, Date::ToString(date), "PAY PAYS N*(" + amount + ")"});
            result->pillar_ = result->pillar_.IsValid() ? std::max(result->pillar_, std::max(date, dependency)) : std::max(date, dependency);
        }
        Export_ Floating(const Date_& start,
                         const Date_& end,
                         const RateLegConvention_& leg,
                         const RateIndexConvention_& index,
                         const Ccy_& currency,
                         bool spread,
                         const CalibrationDiagnostic_& d) {
            ValidateIndex(index, d);
            Export_ result;
            for (const auto& period : BuildLegPeriods<Coupon_>(start, end, leg, index.fixingLag_, index.fixingHolidays_)) {
                const auto alpha = NumberText(static_cast<double>(period.accrual_.dcf_));
                const auto forward = Forward(period.schedule_, index, currency, d);
                const String_ amount = spread ? alpha + "*(" + forward + "+K)" : "-" + alpha + "*" + forward;
                Payment(&result, period.schedule_.paymentDate_, period.schedule_.accrualEnd_, amount, d);
            }
            return result;
        }
        Export_ Fixed(const Date_& start, const Date_& end, const RateLegConvention_& leg, const CalibrationDiagnostic_& d) {
            Export_ result;
            for (const auto& period : BuildLegPeriods<Coupon_>(start, end, leg, 0, Holidays::None()))
                Payment(&result, period.schedule_.paymentDate_, period.schedule_.accrualEnd_,
                        NumberText(static_cast<double>(period.accrual_.dcf_)) + "*K", d);
            return result;
        }
        Export_ Combine(Export_ left, const Export_& right) {
            for (const auto& row : right.rows_)
                left.rows_.push_back(row);
            left.pillar_ = std::max(left.pillar_, right.pillar_);
            return left;
        }
        Export_ SwapCashflows(const Swap_& native, const CurveScriptImportSettings_& settings, const CalibrationDiagnostic_& d) {
            const auto [start, end] = native.TimeSpan();
            return Combine(Fixed(start, end, native.FixedLegConvention(), d),
                           Floating(start, end, native.FloatLegConvention(), native.FloatConvention(), settings.currency_, false, d));
        }
        Handle_<CalibrationInstrument_> Contract(const Export_& exported, double coupon, bool spread, const CurveScriptImportSettings_& settings) {
            ScriptInstrumentDefinitionSpec_ spec;
            const auto price = CalibrationValueUnit_::Parse("Price:" + String_(settings.currency_.String()));
            const auto unit = CalibrationValueUnit_::Parse(spread ? "BasisSpread" : "Rate");
            spec.header_.payCurrency_ = spec.header_.resultCurrency_ = spec.header_.collateralCurrency_ = settings.currency_;
            spec.header_.discountRoute_ = settings.discountRoute_;
            spec.source_.rows_ = {{ScriptSourceRowKind_::Value_::Definition, "N", NumberText(settings.notional_)},
                                  {ScriptSourceRowKind_::Value_::Definition, "K", NumberText(coupon)}};
            for (const auto& row : exported.rows_)
                spec.source_.rows_.push_back(row);
            spec.parameters_ = {{"N", ScriptParameterType_::Value_::Real, price, ScriptParameterRole_::Value_::Notional, true, 0., {}, false, true},
                                {"K", ScriptParameterType_::Value_::Real, unit, ScriptParameterRole_::Value_::Coupon, true}};
            spec.measures_ = {{"Price", CalibrationMeasureKind_::Value_::Price, "PAY", {}, price},
                              {spread ? String_("BasisSpread") : String_("ParRate"),
                               spread ? CalibrationMeasureKind_(CalibrationMeasureKind_::Value_::ParAffineSpread)
                                      : CalibrationMeasureKind_(CalibrationMeasureKind_::Value_::ParAffineRate),
                               "PAY", String_("K"), unit, 1, settings.minAbsAnnuity_}};
            return NewScriptCalibrationInstrument({settings.instrumentId_, NewScriptInstrumentDefinition(spec), {}, exported.pillar_});
        }
        template <class F_>
        Handle_<CalibrationInstrument_> Import(const CurveScriptImportSettings_& settings, double coupon, bool spread, const F_& cashflows) {
            const auto d = ImportDiagnostic(settings);
            try {
                Check(std::isfinite(coupon), "InvalidCalibrationParameterBinding", "/import/coupon", "finite fixed native coupon", "", d);
                return Contract(cashflows(d), coupon, spread, settings);
            } catch (const CalibrationError_& error) {
                auto source = error.Failure().diagnostic_;
                source.instrumentId_ = settings.instrumentId_;
                Fail(source.code_, source.field_.value_or("/import"), source.expected_.value_or("valid complete native import"),
                     source.actual_.value_or(""), source);
            } catch (const std::exception& error) {
                Fail("UnsupportedCalibrationProduct", "/import/convention", "resolvable complete native schedule/convention", String_(error.what()),
                     d);
            }
        }
    } // namespace
    Handle_<CalibrationInstrument_> NewFraScriptInstrument(const FRA_& native, const Date_& payment, const CurveScriptImportSettings_& settings) {
        return Import(settings, native.MarketRate(), false, [&](const auto& d) {
            Check(typeid(native) == typeid(FRA_), "UnsupportedCalibrationProduct", "/import/type", "FRA; futures/STIR retain legacy semantics", "",
                  d);
            const auto [start, end] = native.TimeSpan();
            const auto& index = native.FloatConvention();
            ValidateIndex(index, d);
            auto period = BuildSinglePeriodSchedule(start, end, index, SinglePeriodCouponMonths(index, start, end));
            period.fixingDate_ = index.fixingLag_ == 0 ? period.accrualStart_
                                                       : Date::NBusDays(index.fixingLag_, index.fixingHolidays_)->BackFrom(period.accrualStart_);
            const auto forward = Forward(period, index, settings.currency_, d);
            const auto alpha = NumberText(Accrual(period, index.dayBasis_, d));
            Check(payment == period.accrualStart_ || payment >= period.accrualEnd_, "UnsupportedCalibrationProduct", "/import/payment",
                  "explicit advance settlement at start or arrears payment at/after end", "", d);
            String_ amount = alpha + "*(K-" + forward + ")";
            if (payment == period.accrualStart_)
                amount = "(" + amount + ")/(1+" + alpha + "*" + forward + ")";
            Export_ result;
            Payment(&result, payment, period.accrualEnd_, amount, d);
            return result;
        });
    }
    Handle_<CalibrationInstrument_> NewSwapScriptInstrument(const Swap_& native, const CurveScriptImportSettings_& settings) {
        return Import(settings, native.MarketRate(), false, [&](const auto& d) {
            Check(typeid(native) == typeid(Swap_), "UnsupportedCalibrationProduct", "/import/type", "explicit Swap template", "", d);
            return SwapCashflows(native, settings, d);
        });
    }
    Handle_<CalibrationInstrument_> NewPeriodForwardOisScriptInstrument(const OISSwap_& native, const CurveScriptImportSettings_& settings) {
        return Import(settings, native.MarketRate(), false, [&](const auto& d) {
            Check(typeid(native) == typeid(OISSwap_) && !native.FloatConvention().useProjectionCurve_, "UnsupportedCalibrationProduct",
                  "/import/type", "explicit single-curve period-forward OIS; no unrepresented daily observation convention", "", d);
            return SwapCashflows(native, settings, d);
        });
    }
    Handle_<CalibrationInstrument_> NewBasisSwapScriptInstrument(const BasisSwap_& native, const CurveScriptImportSettings_& settings) {
        return Import(settings, native.MarketRate(), true, [&](const auto& d) {
            Check(typeid(native) == typeid(BasisSwap_), "UnsupportedCalibrationProduct", "/import/type", "explicit same-currency BasisSwap", "", d);
            const auto [start, end] = native.TimeSpan();
            auto reference = Floating(start, end, native.ReferenceLegConvention(), native.ReferenceIndexConvention(), settings.currency_, false, d);
            auto spread = Floating(start, end, native.SpreadLegConvention(), native.SpreadIndexConvention(), settings.currency_, true, d);
            return Combine(std::move(spread), reference);
        });
    }
} // namespace Dal
