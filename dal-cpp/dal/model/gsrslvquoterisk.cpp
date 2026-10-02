//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>

#include <dal/model/gsrslvcalibrationinternal.hpp>

namespace Dal {
    namespace {
        Handle_<GSRSLVModelData_> WithCurve(const GSRSLVModelData_& initial, const Handle_<GSRCurveData_>& curve) {
            const Handle_<MultiFactorGSRModelData_> gaussian(new MultiFactorGSRModelData_(initial.gaussian_->name_, curve, initial.gaussian_->vol_));
            return Handle_<GSRSLVModelData_>(new GSRSLVModelData_(
                initial.name_, gaussian, initial.leverage_, {initial.kappa_, initial.volOfVol_, initial.varianceCorrelations_, initial.maxStep_}));
        }

        void ValidateSnapshot(const GSRCurveData_& model, const GSRCurveData_& risk) {
            REQUIRE(model.evaluationDate_ == risk.evaluationDate_ && model.currency_ == risk.currency_ && model.nodeDates_ == risk.nodeDates_ &&
                        model.projectionTenors_ == risk.projectionTenors_ && model.discountLogDF_ == risk.discountLogDF_ &&
                        model.projectionLogDF_.Rows() == risk.projectionLogDF_.Rows() &&
                        model.projectionLogDF_.Cols() == risk.projectionLogDF_.Cols(),
                    "InvalidGSRSLVQuoteRisk: curve risk snapshot mismatch");
            for (int row = 0; row < model.projectionLogDF_.Rows(); ++row)
                for (int col = 0; col < model.projectionLogDF_.Cols(); ++col)
                    REQUIRE(model.projectionLogDF_(row, col) == risk.projectionLogDF_(row, col),
                            "InvalidGSRSLVQuoteRisk: projection risk snapshot mismatch");
        }
    } // namespace

    GSRSLVQuoteRiskResult_ GSRSLVQuoteRisk(const GSRSLVModelData_& initial,
                                           const Vector_<GSRCalibrationQuote_>& quotes,
                                           const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                           const Vector_<GSREuropeanOption_>& targets,
                                           const GSRSLVCalibrationSettings_& calibrationSettings,
                                           const GSRSLVQuoteRiskSettings_& riskSettings,
                                           const GSRCurveQuoteRisk_* curveRisk) {
        for (double value : {riskSettings.relativeBump_, riskSettings.absoluteBump_, riskSettings.stabilityTolerance_})
            REQUIRE(std::isfinite(value) && value > 0.0, "InvalidGSRSLVQuoteRisk: bumps and stability tolerance must be finite and positive");
        GSRSLVQuoteRiskResult_ result;
        if (curveRisk)
            ValidateSnapshot(*initial.gaussian_->curve_, curveRisk->Snapshot());
        result.calibration_ = CalibrateGSRSLV(initial, quotes, parameters, calibrationSettings);
        REQUIRE(result.calibration_.converged_, "InvalidGSRSLVQuoteRisk: base calibration did not converge");
        REQUIRE(result.calibration_.jacobianRank_ == static_cast<int>(parameters.size()) || calibrationSettings.solver_.priorWeight_ > 0.0,
                "InvalidGSRSLVQuoteRisk: rank-deficient fit requires a positive prior weight");
        const auto prices = PriceGSRSLVEuropeanOptions(*result.calibration_.model_, targets, calibrationSettings.pricing_);
        for (const auto& price : prices)
            result.prices_.push_back(price.price_);
        const size_t columns = quotes.size() + (curveRisk ? curveRisk->QuoteNames().size() : 0);
        result.curveRiskIncluded_ = curveRisk != nullptr;
        result.sensitivities_ = result.refinementErrors_ = Matrix_<>(targets.size(), columns, 0.0);
        for (size_t col = 0; col < columns; ++col) {
            const bool smile = col < quotes.size();
            const size_t curveQuote = smile ? 0 : col - quotes.size();
            const auto& name = smile ? quotes[col].name_ : curveRisk->QuoteNames()[curveQuote];
            REQUIRE(std::find(result.quoteNames_.begin(), result.quoteNames_.end(), name) == result.quoteNames_.end(),
                    "InvalidGSRSLVQuoteRisk: duplicate quote coordinate name");
            result.quoteNames_.push_back(name);
            result.quoteUnits_.push_back(smile ? String_("PRICE_PER_NOTIONAL") : curveRisk->QuoteUnits()[curveQuote]);
            const double step =
                smile ? std::max(riskSettings.absoluteBump_, riskSettings.relativeBump_ * quotes[col].priceScale_) : riskSettings.absoluteBump_;
            Vector_<> coarse;
            bool activeStable = true, stable = true;
            for (int refinement = 0; refinement < 2; ++refinement) {
                auto up = quotes, down = quotes;
                const double bump = step / (refinement + 1);
                double denominator = 2.0 * bump;
                Handle_<GSRSLVModelData_> highInitial, lowInitial;
                if (smile) {
                    up[col].price_ += bump;
                    down[col].price_ = std::max(0.0, down[col].price_ - bump);
                    REQUIRE(std::isfinite(up[col].price_) && up[col].price_ > down[col].price_,
                            "InvalidGSRSLVQuoteRisk: quote bump is unresolved or overflows");
                    denominator = up[col].price_ - down[col].price_;
                } else {
                    highInitial = WithCurve(initial, curveRisk->Shifted(curveQuote, bump));
                    lowInitial = WithCurve(initial, curveRisk->Shifted(curveQuote, -bump));
                }
                REQUIRE(std::isfinite(denominator) && denominator > 0.0, "InvalidGSRSLVQuoteRisk: bump denominator is unresolved or nonfinite");
                const auto high = GSRSLVCalibrationInternal::FitModel(highInitial ? *highInitial : initial, up, parameters, calibrationSettings);
                const auto low = GSRSLVCalibrationInternal::FitModel(lowInitial ? *lowInitial : initial, down, parameters, calibrationSettings);
                REQUIRE(high.converged_ && low.converged_, "InvalidGSRSLVQuoteRisk: bumped calibration did not converge");
                activeStable =
                    activeStable && high.activeBounds_ == result.calibration_.activeBounds_ && low.activeBounds_ == result.calibration_.activeBounds_;
                const auto highPrices = PriceGSRSLVEuropeanOptions(*high.model_, targets, calibrationSettings.pricing_);
                const auto lowPrices = PriceGSRSLVEuropeanOptions(*low.model_, targets, calibrationSettings.pricing_);
                for (size_t row = 0; row < targets.size(); ++row) {
                    const double sensitivity = (highPrices[row].price_ - lowPrices[row].price_) / denominator;
                    REQUIRE(std::isfinite(sensitivity), "InvalidGSRSLVQuoteRisk: nonfinite sensitivity");
                    if (refinement == 0)
                        coarse.push_back(sensitivity);
                    else {
                        result.sensitivities_(row, col) = sensitivity;
                        result.refinementErrors_(row, col) = std::abs(sensitivity - coarse[row]);
                        stable =
                            stable && result.refinementErrors_(row, col) <= riskSettings.stabilityTolerance_ * std::max(1.0, std::abs(sensitivity));
                    }
                }
            }
            result.activeSetStable_.push_back(activeStable);
            result.stable_.push_back(stable && activeStable);
        }
        return result;
    }
} // namespace Dal
