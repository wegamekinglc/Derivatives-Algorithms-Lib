#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/currency/currencydata.hpp>
#include <dal/model/gsr.hpp>
#include <dal/model/gsrrateoption_internal.hpp>

namespace Dal::GaussianRateOptionInternal {
    using namespace CalibrationInternal;
    namespace {
        bool ValidDates(const Date_& fixing, const Date_& start, const Date_& payment) {
            return fixing.IsValid() && start.IsValid() && payment.IsValid() && fixing <= start;
        }
    } // namespace
    FloatingCoupon_ Coupon(const Date_& fixing,
                           const Date_& start,
                           const Date_& payment,
                           double accrual,
                           const String_& tenor,
                           const Ccy_& currency,
                           const CalibrationDiagnostic_& d) {
        const String_ code = d.stage_ == CalibrationStage_::Value_::Prepare ? "UnsupportedFastEvaluator" : "UnsupportedCalibrationProduct";
        Check(ValidDates(fixing, start, payment), code, "/option/coupon", "valid fixing <= index start", "", d);
        Check(tenor == "3M" || tenor == "6M", code, "/option/tenor", "canonical 3M or 6M Libor", tenor, d);
        const Date_ end = Date::NominalMaturity(start, PeriodLength_(tenor), currency);
        const double indexAccrual = Ccy::Conventions::LiborDayBasis()(currency)(start, end, nullptr);
        Check(start < end && end <= payment && std::isfinite(accrual) && accrual > 0., code, "/option/coupon",
              "positive accrual and index start < end <= payment", "", d);
        return {fixing, start, end, payment, indexAccrual, accrual, tenor};
    }
    GSRPriceResult_ NativePrice(const CalibrationRateOptionDescriptor_& descriptor,
                                const CalibrationPreparationContext_& context,
                                const GSRPricingSettings_& settings) {
        return std::visit(
            [&](const auto& option) {
                return typeid(*context.model_) == typeid(MultiFactorGSRModelData_)
                           ? PriceGSREuropeanOption(static_cast<const MultiFactorGSRModelData_&>(*context.model_), option, settings)
                           : PriceGSREuropeanOption(static_cast<const GSRModelData_&>(*context.model_), option, settings);
            },
            descriptor.option_);
    }
    namespace {
        double Exposure(const AAD::GSR_<double>& model, const Date_& valuation, const Date_& expiry, const Date_& start, const Date_& end) {
            const auto difference = model.GaussianBondLoading((start - valuation) / 365., (end - valuation) / 365.);
            const auto covariance = model.GaussianVariance((expiry - valuation) / 365.);
            double result = 0.;
            for (int i = 0; i < covariance.Rows(); ++i)
                for (int j = 0; j < covariance.Cols(); ++j)
                    result += difference[i] * covariance(i, j) * difference[j];
            return result;
        }
        double Exposure(const AAD::GSR_<double>& model, const Date_& valuation, const Caplet_& option) {
            return Exposure(model, valuation, option.expiry_, option.start_, option.end_);
        }
        double Exposure(const AAD::GSR_<double>& model, const Date_& valuation, const Swaption_& option) {
            double result = 0.;
            for (const auto& coupon : option.fixed_)
                result += Exposure(model, valuation, option.expiry_, option.expiry_, coupon.payment_);
            for (const auto& coupon : option.floating_)
                result += Exposure(model, valuation, option.expiry_, coupon.start_, coupon.end_);
            return result;
        }
        template <class O_> void Dates(const O_& option, const CalibrationPreparationContext_& context, const CalibrationDiagnostic_& d) {
            Check(*context.valuation_.evaluationDate_ < option.expiry_, "UnsupportedFastEvaluator", "/option/expiry",
                  "exercise strictly after valuation", "", d);
        }
        void Coverage(const Caplet_& option, const CalibrationPreparationContext_& context, const CalibrationDiagnostic_& d) {
            Dates(option, context, d);
            Check(option.payment_ <= context.curve_->nodeDates_.back(), "UnsupportedFastEvaluator", "/option/payment", "payment within frozen curve",
                  "", d);
        }
        void Coverage(const Swaption_& option, const CalibrationPreparationContext_& context, const CalibrationDiagnostic_& d) {
            Dates(option, context, d);
            for (const auto& coupon : option.fixed_)
                Check(coupon.payment_ <= context.curve_->nodeDates_.back(), "UnsupportedFastEvaluator", "/option/fixed",
                      "fixed payments within frozen curve", "", d);
            for (const auto& coupon : option.floating_)
                Check(coupon.payment_ <= context.curve_->nodeDates_.back(), "UnsupportedFastEvaluator", "/option/floating",
                      "floating payments within frozen curve", "", d);
        }
        double Exposure(const CalibrationRateOptionDescriptor_& descriptor, const CalibrationPreparationContext_& context) {
            const auto evaluate = [&](const auto& data) {
                const AAD::GSR_<double> model(data);
                return std::visit([&](const auto& option) { return Exposure(model, *context.valuation_.evaluationDate_, option); },
                                  descriptor.option_);
            };
            return typeid(*context.model_) == typeid(MultiFactorGSRModelData_)
                       ? evaluate(static_cast<const MultiFactorGSRModelData_&>(*context.model_))
                       : evaluate(static_cast<const GSRModelData_&>(*context.model_));
        }
    } // namespace

    void RequireDomain(const CalibrationRateOptionDescriptor_& descriptor,
                       const CalibrationPreparationContext_& context,
                       bool aad,
                       const CalibrationDiagnostic_& d) {
        Check(context.curve_->projectionTenors_.empty(), "UnsupportedFastEvaluator", "/context/curve/projection", "single discount/forecast curve",
              "", d);
        std::visit([&](const auto& option) { Coverage(option, context, d); }, descriptor.option_);
        try {
            // Reuse the native effective-rank admission rather than maintaining a second numerical rank algorithm.
            const auto price = NativePrice(descriptor, context);
            Check(std::isfinite(price.price_) && std::isfinite(price.numericalError_), "CalibrationNumericalFailure", "/price",
                  "finite native rate option value", "", d);
            if (aad) {
                const double variance = Exposure(descriptor, context);
                Check(std::isfinite(variance) && variance > 0. && price.price_ > 0., "DerivativeUnavailable", "/context/model/variance",
                      "nondegenerate rate exposure and positive option expectation for hard-payoff AAD", "", d);
            }
        } catch (const CalibrationError_&) {
            throw;
        } catch (const std::exception& error) {
            const String_ cause(error.what());
            const bool rank = Bytes(cause).find("at most three effective Gaussian directions") != std::string::npos;
            Fail(rank ? "UnsupportedFastEvaluator" : "CalibrationNumericalFailure", "/context/model",
                 "resolvable Gaussian option with at most three effective directions", cause, d);
        }
    }
} // namespace Dal::GaussianRateOptionInternal
