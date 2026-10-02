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

        void ValidateSnapshotAxes(const GSRCurveData_& model, const GSRCurveData_& risk) {
            REQUIRE(model.evaluationDate_ == risk.evaluationDate_ && model.currency_ == risk.currency_ && model.nodeDates_ == risk.nodeDates_ &&
                        model.projectionTenors_ == risk.projectionTenors_,
                    "InvalidGSRSLVQuoteRisk: curve risk snapshot mismatch");
        }

        void ValidateSnapshot(const GSRCurveData_& model, const GSRCurveData_& risk) {
            ValidateSnapshotAxes(model, risk);
            REQUIRE(model.discountLogDF_ == risk.discountLogDF_ && model.projectionLogDF_.Rows() == risk.projectionLogDF_.Rows() &&
                        model.projectionLogDF_.Cols() == risk.projectionLogDF_.Cols(),
                    "InvalidGSRSLVQuoteRisk: curve risk snapshot mismatch");
            for (int row = 0; row < model.projectionLogDF_.Rows(); ++row)
                for (int col = 0; col < model.projectionLogDF_.Cols(); ++col)
                    REQUIRE(model.projectionLogDF_(row, col) == risk.projectionLogDF_(row, col),
                            "InvalidGSRSLVQuoteRisk: projection risk snapshot mismatch");
        }

        GSRSLVQuoteRiskResult_ BaseRisk(const GSRSLVModelData_& initial,
                                        const Vector_<GSRCalibrationQuote_>& quotes,
                                        const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                        const Vector_<GSREuropeanOption_>& targets,
                                        const GSRSLVCalibrationSettings_& settings,
                                        const GSRCurveQuoteRisk_* curveRisk) {
            GSRSLVQuoteRiskResult_ result;
            if (curveRisk)
                ValidateSnapshot(*initial.gaussian_->curve_, curveRisk->Snapshot());
            result.calibration_ = CalibrateGSRSLV(initial, quotes, parameters, settings);
            REQUIRE(result.calibration_.converged_, "InvalidGSRSLVQuoteRisk: base calibration did not converge");
            REQUIRE(result.calibration_.jacobianRank_ == static_cast<int>(parameters.size()) || settings.solver_.priorWeight_ > 0.0,
                    "InvalidGSRSLVQuoteRisk: rank-deficient fit requires a positive prior weight");
            for (const auto& price : PriceGSRSLVEuropeanOptions(*result.calibration_.model_, targets, settings.pricing_))
                result.prices_.push_back(price.price_);
            const size_t columns = quotes.size() + (curveRisk ? curveRisk->QuoteNames().size() : 0);
            result.curveRiskIncluded_ = curveRisk != nullptr;
            result.sensitivities_ = result.refinementErrors_ = Matrix_<>(targets.size(), columns, 0.0);
            return result;
        }

        struct RiskCoordinate_ {
            String_ name_, unit_;
            double step_;
        };

        RiskCoordinate_ Coordinate(size_t col,
                                   const Vector_<GSRCalibrationQuote_>& quotes,
                                   const GSRCurveQuoteRisk_* curveRisk,
                                   const GSRSLVQuoteRiskSettings_& settings) {
            if (col < quotes.size())
                return {quotes[col].name_, "PRICE_PER_NOTIONAL", std::max(settings.absoluteBump_, settings.relativeBump_ * quotes[col].priceScale_)};
            const size_t curveQuote = col - quotes.size();
            return {curveRisk->QuoteNames()[curveQuote], curveRisk->QuoteUnits()[curveQuote], settings.absoluteBump_};
        }

        struct BumpedInputs_ {
            Vector_<GSRCalibrationQuote_> up_, down_;
            Handle_<GSRSLVModelData_> highInitial_, lowInitial_;
            double denominator_;
        };

        struct BumpedRisk_ {
            Vector_<> sensitivities_;
            bool activeStable_;
        };

        class RiskProblem_ {
            const GSRSLVModelData_& initial_;
            const Vector_<GSRCalibrationQuote_>& quotes_;
            const Vector_<GSRSLVCalibrationParameter_>& parameters_;
            const Vector_<GSREuropeanOption_>& targets_;
            const GSRSLVCalibrationSettings_& settings_;
            const GSRCurveQuoteRisk_* curveRisk_;

            BumpedInputs_ Inputs(size_t col, double bump) const {
                BumpedInputs_ inputs{quotes_, quotes_, {}, {}, 2.0 * bump};
                if (col < quotes_.size()) {
                    inputs.up_[col].price_ += bump;
                    inputs.down_[col].price_ = std::max(0.0, inputs.down_[col].price_ - bump);
                    REQUIRE(std::isfinite(inputs.up_[col].price_) && inputs.up_[col].price_ > inputs.down_[col].price_,
                            "InvalidGSRSLVQuoteRisk: quote bump is unresolved or overflows");
                    inputs.denominator_ = inputs.up_[col].price_ - inputs.down_[col].price_;
                } else {
                    const size_t quote = col - quotes_.size();
                    inputs.highInitial_ = WithCurve(initial_, curveRisk_->Shifted(quote, bump));
                    inputs.lowInitial_ = WithCurve(initial_, curveRisk_->Shifted(quote, -bump));
                }
                REQUIRE(std::isfinite(inputs.denominator_) && inputs.denominator_ > 0.0,
                        "InvalidGSRSLVQuoteRisk: bump denominator is unresolved or nonfinite");
                return inputs;
            }

            GSRSLVCalibrationResult_ Fit(const Handle_<GSRSLVModelData_>& overrideInitial, const Vector_<GSRCalibrationQuote_>& quotes) const {
                auto fit = GSRSLVCalibrationInternal::FitModel(overrideInitial ? *overrideInitial : initial_, quotes, parameters_, settings_);
                REQUIRE(fit.converged_, "InvalidGSRSLVQuoteRisk: bumped calibration did not converge");
                return fit;
            }

        public:
            RiskProblem_(const GSRSLVModelData_& initial,
                         const Vector_<GSRCalibrationQuote_>& quotes,
                         const Vector_<GSRSLVCalibrationParameter_>& parameters,
                         const Vector_<GSREuropeanOption_>& targets,
                         const GSRSLVCalibrationSettings_& settings,
                         const GSRCurveQuoteRisk_* curveRisk)
                : initial_(initial), quotes_(quotes), parameters_(parameters), targets_(targets), settings_(settings), curveRisk_(curveRisk) {}

            BumpedRisk_ Bump(size_t col, double step, const Vector_<bool>& baseBounds) const {
                const auto inputs = Inputs(col, step);
                const auto high = Fit(inputs.highInitial_, inputs.up_), low = Fit(inputs.lowInitial_, inputs.down_);
                const auto highPrices = PriceGSRSLVEuropeanOptions(*high.model_, targets_, settings_.pricing_);
                const auto lowPrices = PriceGSRSLVEuropeanOptions(*low.model_, targets_, settings_.pricing_);
                BumpedRisk_ result{{}, high.activeBounds_ == baseBounds && low.activeBounds_ == baseBounds};
                for (size_t row = 0; row < targets_.size(); ++row) {
                    const double sensitivity = (highPrices[row].price_ - lowPrices[row].price_) / inputs.denominator_;
                    REQUIRE(std::isfinite(sensitivity), "InvalidGSRSLVQuoteRisk: nonfinite sensitivity");
                    result.sensitivities_.push_back(sensitivity);
                }
                return result;
            }
        };

        void StoreColumn(size_t col, const BumpedRisk_& coarse, const BumpedRisk_& fine, double tolerance, GSRSLVQuoteRiskResult_* result) {
            const bool activeStable = coarse.activeStable_ && fine.activeStable_;
            bool stable = true;
            for (size_t row = 0; row < fine.sensitivities_.size(); ++row) {
                const double sensitivity = fine.sensitivities_[row];
                result->sensitivities_(row, col) = sensitivity;
                result->refinementErrors_(row, col) = std::abs(sensitivity - coarse.sensitivities_[row]);
                stable = stable && result->refinementErrors_(row, col) <= tolerance * std::max(1.0, std::abs(sensitivity));
            }
            result->activeSetStable_.push_back(activeStable);
            result->stable_.push_back(stable && activeStable);
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
        auto result = BaseRisk(initial, quotes, parameters, targets, calibrationSettings, curveRisk);
        const RiskProblem_ problem(initial, quotes, parameters, targets, calibrationSettings, curveRisk);
        for (int col = 0; col < result.sensitivities_.Cols(); ++col) {
            const auto coordinate = Coordinate(col, quotes, curveRisk, riskSettings);
            REQUIRE(std::find(result.quoteNames_.begin(), result.quoteNames_.end(), coordinate.name_) == result.quoteNames_.end(),
                    "InvalidGSRSLVQuoteRisk: duplicate quote coordinate name");
            result.quoteNames_.push_back(coordinate.name_);
            result.quoteUnits_.push_back(coordinate.unit_);
            const auto coarse = problem.Bump(col, coordinate.step_, result.calibration_.activeBounds_);
            const auto fine = problem.Bump(col, coordinate.step_ / 2.0, result.calibration_.activeBounds_);
            StoreColumn(col, coarse, fine, riskSettings.stabilityTolerance_, &result);
        }
        return result;
    }
} // namespace Dal
