#include <dal/platform/platform.hpp>

#include <string>

#include <dal/calibration/validation_internal.hpp>
#include <dal/model/gsrrateoption_internal.hpp>

namespace Dal {
    using namespace CalibrationInternal;
    namespace {
        String_ AccrualLiteral(double accrual) {
            const auto text = Bytes(NumberText(accrual));
            const auto exponent = text.find_first_of("eE");
            if (exponent == std::string::npos)
                return String_(text);
            auto digits = text.substr(0, exponent);
            const auto point = digits.find('.');
            const int position = static_cast<int>(point == std::string::npos ? digits.size() : point) + std::stoi(text.substr(exponent + 1));
            if (point != std::string::npos)
                digits.erase(point, 1);
            if (position <= 0)
                return String_("0." + std::string(static_cast<size_t>(-position), '0') + digits);
            if (static_cast<size_t>(position) >= digits.size())
                digits.append(static_cast<size_t>(position) - digits.size(), '0');
            else
                digits.insert(static_cast<size_t>(position), ".");
            return String_(digits);
        }
        String_ Libor(const String_& currency, const String_& tenor, const Date_& start, const Date_& expiry) {
            const String_ index = tenor == "3M" ? "LIBOR_3M_LCH" : "LIBOR_6M_LCH";
            return "FIX(IR[" + currency + "," + index + "," + Date::ToString(start) + "]," + Date::ToString(expiry) + ")";
        }
        String_ Discount(const String_& currency, const Date_& payment, const Date_& expiry) {
            return "FIX(IR[" + currency + ",DF," + Date::ToString(payment) + "]," + Date::ToString(expiry) + ")";
        }
        String_ Difference(const String_& asset, const String_& strike, const OptionType_& type, const CalibrationDiagnostic_& d) {
            Check(type == OptionType_::Value_::CALL || type == OptionType_::Value_::PUT, "UnsupportedCalibrationProduct", "/option/type",
                  "CALL or PUT", "", d);
            return type == OptionType_::Value_::CALL ? asset + "-" + strike : strike + "-" + asset;
        }
        Handle_<CalibrationInstrument_>
        Import(const Date_& event, double strike, const String_& amount, const RateOptionScriptImportSettings_& settings) {
            const auto price = CalibrationValueUnit_::Parse("Price:" + String_(settings.currency_.String()));
            ScriptInstrumentDefinitionSpec_ spec;
            spec.header_.payCurrency_ = spec.header_.resultCurrency_ = spec.header_.collateralCurrency_ = settings.currency_;
            spec.header_.discountRoute_ = settings.discountRoute_;
            spec.source_.rows_ = {{ScriptSourceRowKind_::Value_::Definition, "N", NumberText(settings.notional_)},
                                  {ScriptSourceRowKind_::Value_::Definition, "K", NumberText(strike)},
                                  {ScriptSourceRowKind_::Value_::Event, Date::ToString(event), "PAY PAYS N*(" + amount + ")"}};
            spec.parameters_ = {
                {"N", ScriptParameterType_::Value_::Real, price, ScriptParameterRole_::Value_::Notional, true, 0., {}, false, true},
                {"K", ScriptParameterType_::Value_::Real, CalibrationValueUnit_::Parse("Rate"), ScriptParameterRole_::Value_::RateStrike, true}};
            spec.measures_ = {{"Price", CalibrationMeasureKind_::Value_::Price, "PAY", {}, price}};
            return NewScriptCalibrationInstrument({settings.instrumentId_, NewScriptInstrumentDefinition(spec)});
        }
        void ValidateCoupon(const FloatingCoupon_& input, const Ccy_& currency, const CalibrationDiagnostic_& d) {
            const auto derived =
                GaussianRateOptionInternal::Coupon(input.fixing_, input.start_, input.payment_, input.couponAccrual_, input.tenor_, currency, d);
            Check(input.end_ == derived.end_, "UnsupportedCalibrationProduct", "/option/coupon/end", Date::ToString(derived.end_),
                  Date::ToString(input.end_), d);
            Check(input.indexAccrual_ == derived.indexAccrual_, "UnsupportedCalibrationProduct", "/option/coupon/index_accrual",
                  NumberText(derived.indexAccrual_), NumberText(input.indexAccrual_), d);
        }
        String_ FixedSource(const Swaption_& option, const String_& currency, const CalibrationDiagnostic_& d) {
            String_ result;
            for (const auto& coupon : option.fixed_) {
                Check(coupon.payment_.IsValid() && coupon.payment_ > option.expiry_ && std::isfinite(coupon.accrual_) && coupon.accrual_ > 0.,
                      "UnsupportedCalibrationProduct", "/option/fixed", "positive fixed accrual paid after exercise", "", d);
                result += (result.empty() ? "" : "+") + AccrualLiteral(coupon.accrual_) + "*" + Discount(currency, coupon.payment_, option.expiry_);
            }
            return result;
        }
        String_ FloatingSource(const Swaption_& option, const Ccy_& currency, const CalibrationDiagnostic_& d) {
            String_ result;
            for (const auto& coupon : option.floating_) {
                ValidateCoupon(coupon, currency, d);
                Check(coupon.fixing_ >= option.expiry_ && coupon.payment_ == coupon.end_, "UnsupportedCalibrationProduct", "/option/floating",
                      "future fixing and floating payment at index end; no payment lag", "", d);
                // Payment=end makes native future-fixing convexity zero: exercise-date forward PV is exact.
                result += (result.empty() ? "" : "+") + AccrualLiteral(coupon.couponAccrual_) + "*" +
                          Libor(currency.String(), coupon.tenor_, coupon.start_, option.expiry_) + "*" +
                          Discount(currency.String(), coupon.end_, option.expiry_);
            }
            return result;
        }
    } // namespace

    Handle_<CalibrationInstrument_> NewCapletScriptInstrument(const Caplet_& option, const RateOptionScriptImportSettings_& settings) {
        CalibrationDiagnostic_ d;
        d.instrumentId_ = settings.instrumentId_;
        Token(settings.currency_, "UnsupportedCalibrationProduct", "/import/currency", d);
        ValidateCoupon({option.expiry_, option.start_, option.end_, option.payment_, option.indexAccrual_, option.couponAccrual_, option.tenor_},
                       settings.currency_, d);
        const auto difference = Difference(Libor(settings.currency_.String(), option.tenor_, option.start_, option.expiry_), "K", option.type_, d);
        return Import(option.payment_, option.strike_, AccrualLiteral(option.couponAccrual_) + "*MAX(" + difference + ",0)", settings);
    }

    Handle_<CalibrationInstrument_> NewSwaptionScriptInstrument(const Swaption_& option, const RateOptionScriptImportSettings_& settings) {
        CalibrationDiagnostic_ d;
        d.instrumentId_ = settings.instrumentId_;
        const auto currency = Token(settings.currency_, "UnsupportedCalibrationProduct", "/import/currency", d);
        Check(option.expiry_.IsValid() && !option.fixed_.empty() && !option.floating_.empty(), "UnsupportedCalibrationProduct", "/option",
              "valid exercise and nonempty physical fixed/floating schedules", "", d);
        const auto fixed = FixedSource(option, currency, d), floating = FloatingSource(option, settings.currency_, d);
        return Import(option.expiry_, option.strike_, "MAX(" + Difference("(" + floating + ")", "K*(" + fixed + ")", option.type_, d) + ",0)",
                      settings);
    }

    GSRPriceResult_ PriceGaussianRateOption(const PreparedCalibrationInstrument_& plan,
                                            const Handle_<CalibrationInstrument_>& instrument,
                                            const CalibrationPreparationContext_& context,
                                            const GSRPricingSettings_& settings) {
        plan.RequireCompatible(instrument, context);
        CalibrationDiagnostic_ d;
        d.stage_ = CalibrationStage_::Value_::Evaluate;
        d.instrumentId_ = instrument->Id();
        d.measureId_ = plan.Measure().id_;
        const auto* descriptor = std::get_if<CalibrationRateOptionDescriptor_>(&plan.Descriptor());
        Check(descriptor && plan.Capability().backend_ == CalibrationBackend_::Value_::FastGaussian &&
                  plan.Measure().kind_ == CalibrationMeasureKind_::Value_::Price,
              "UnsupportedFastEvaluator", "/plan", "admitted hard Gaussian Caplet/Swaption Price", "", d);
        try {
            auto result = GaussianRateOptionInternal::NativePrice(*descriptor, context, settings);
            result.price_ *= descriptor->notional_;
            result.numericalError_ *= descriptor->notional_;
            Check(std::isfinite(result.price_) && std::isfinite(result.numericalError_), "CalibrationNumericalFailure", "/price",
                  "finite contract Price and numerical error", "", d);
            return result;
        } catch (const CalibrationError_&) {
            throw;
        } catch (const std::exception& error) {
            Fail("CalibrationNumericalFailure", "/price", "finite, resolvable Gaussian rate option Price", String_(error.what()), d);
        }
    }
} // namespace Dal
