//
// Created by Codex on 2026/10/10.
//

#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>

#include <dal-public/src/calibrationrisk.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/ratecurvature.hpp>
#include <dal/math/aad/detail/gradientbumps.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/tapecapacity.hpp>

#include "jointquoteriskfixtures.hpp"

namespace {
    namespace Bumps = Dal::AAD::GradientBumpsDetail;

    Dal::CurveCalibrationSpec_ SingleSpec() {
        const auto joint = JointQuoteRiskFixtures::Spec(2, 1);
        Dal::CurveCalibrationSpec_ result;
        result.today_ = joint.today_;
        result.ccy_ = joint.ccy_;
        result.curveName_ = "cost";
        result.instruments_ = joint.curves_[0].instruments_;
        result.knotDates_ = {joint.today_};
        for (const auto& instrument : result.instruments_)
            result.knotDates_.push_back(instrument->TimeSpan().second);
        result.parameterization_ = Dal::CurveParameterization_::Value_::LOG_DISCOUNT;
        result.tolerance_ = joint.tolerance_;
        result.initialGuess_ = joint.initialGuess_;
        return result;
    }

    Dal::AAD::Number_ Objective(Dal::AAD::RecordingScope_*, const Dal::Vector_<Dal::AAD::Number_>& inputs) {
        Dal::AAD::Number_ result = inputs[0] * inputs[0];
        for (size_t index = 1; index < inputs.size(); ++index)
            result += static_cast<double>(index + 1) * inputs[index] * inputs[index];
        return result;
    }

    struct Gradient_ {
        double value_;
        Dal::Vector_<> gradient_;
        Dal::RateCalibrationSnapshot_ calibration_;
        const Dal::Vector_<>& Gradient() const { return gradient_; }
    };

    [[maybe_unused]] Dal::RateQuoteCurvatureResult_ Manual(const Dal::RateCalibrationSnapshot_& original,
                                                           const Dal::AAD::BumpOverAADRequest_& request) {
        auto* tape = Dal::AAD::Tape();
        const Dal::AAD::NumResultsResetterForAAD_ mode(tape, tape->multi_, tape->numAdj_);
        tape->multi_ = false;
        tape->numAdj_ = 1;
        Dal::RateQuoteCurvatureExecution_ execution;
        execution.numericPayloadBytes_ = Bumps::Validate(original.Point(), request);
        execution.quoteGradientEvaluations_ = 1 + 2 * request.steps_.size();
        execution.calibrations_ = execution.quoteGradientEvaluations_;
        execution.objectiveReverseSweeps_ = execution.quoteGradientEvaluations_;
        const auto gradient = [&](const Dal::Vector_<>& point, const Dal::String_&) {
            const auto rebuilt = [&] {
                Dal::AAD::TapeCapacityBudget_ budget(std::numeric_limits<size_t>::max());
                Dal::AAD::TapeCapacityScope_ capacity(&budget, true);
                auto calibrated = Dal::RecalibrateRateWithRisk(original, point);
                execution.peakTapeBytes_ = std::max(execution.peakTapeBytes_, budget.PeakCapacityBytes());
                execution.cleanupReserveBytes_ = std::max(execution.cleanupReserveBytes_, Dal::AAD::TapeCleanupCapacityBytes());
                capacity.Close();
                return calibrated;
            }();
            auto inputs = rebuilt.Parameters();
            inputs.Append(point);
            Dal::AAD::BumpOverAADRequest_ firstOrder;
            firstOrder.directions_ = Dal::Matrix_<>(0, static_cast<int>(inputs.size()));
            const auto differentiated = Dal::AAD::EvaluateBumpOverAAD(Objective, inputs, firstOrder);
            execution.peakTapeBytes_ = std::max(execution.peakTapeBytes_, differentiated.Execution().peakTapeBytes_);
            execution.cleanupReserveBytes_ = std::max(execution.cleanupReserveBytes_, differentiated.Execution().cleanupReserveBytes_);
            const auto pullback = Dal::NewCalibrationPullback(rebuilt.Provenance());
            const int count = static_cast<int>(point.size());
            Dal::Matrix_<> parameters(count, 1), direct(count, 1);
            std::copy_n(differentiated.Gradient().begin(), count, parameters.Data());
            std::copy_n(differentiated.Gradient().begin() + count, count, direct.Data());
            const auto risk = Dal::PullbackCalibration(pullback, Dal::NewCalibrationParameterAdjoints(pullback, parameters),
                                                       Dal::NewCalibrationDirectQuoteAdjoints(pullback, direct));
            return Gradient_{differentiated.Value(), Dal::Vector_<>(risk.TotalAdjoints().begin(), risk.TotalAdjoints().end()), rebuilt};
        };
        auto result = Bumps::Evaluate(gradient, original.Point(), request);
        return {result.base_.value_,         result.base_.gradient_,    original.Point(), request,
                std::move(result.products_), result.base_.calibration_, execution};
    }

    [[maybe_unused]] double FirstOrder() {
        const auto spec = SingleSpec();
        const Dal::CurveCalibrationOptions_ options;
        const auto result = Dal::CalibrateYieldCurve(spec, options);
        Dal::RatePricingMarket_ market;
        market.valuationTime_ = Dal::DateTime_(spec.today_);
        market.resultCurrency_ = Dal::Ccy_(spec.ccy_);
        market.curveComponents_["cost"] =
            Dal::Handle_<Dal::DiscountCurve_>(std::shared_ptr<const Dal::DiscountCurve_>(std::shared_ptr<void>(), result.curve_.get()));
        const auto provenance = Dal::BuildSingleCurveQuoteRiskProvenance(spec, result, options, market, {"cost", {{"cost", "cost"}}, true});
        const auto pullback = Dal::NewCalibrationPullback(provenance);
        const auto risk = Dal::PullbackCalibration(pullback, Dal::NewCalibrationParameterAdjoints(pullback, Dal::Matrix_<>(2, 1, 1.0)));
        return std::accumulate(risk.TotalAdjoints().begin(), risk.TotalAdjoints().end(), 0.0);
    }
} // namespace

int main(int argc, char** argv) {
    Dal::InitGlobalData(1);
    const bool joint = argc > 1 && std::string(argv[1]) == "joint";
#if DAL_CURVATURE_COST_MODE != 2
    const auto calibration =
        joint ? Dal::NewRateCalibration(JointQuoteRiskFixtures::Spec(4, 2, Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true))
              : Dal::NewRateCalibration(SingleSpec());
    Dal::AAD::BumpOverAADRequest_ request;
    request.directions_ = Dal::Matrix_<>(1, static_cast<int>(calibration.Point().size()), 0.6);
    request.directions_(0, 0) = -0.8;
    request.steps_ = {2.0e-4};
#else
    (void)joint;
#endif
    const auto evaluate = [&] {
#if DAL_CURVATURE_COST_MODE == 0
        const auto result = Dal::EvaluateRateQuoteCurvature(Objective, calibration, request);
#elif DAL_CURVATURE_COST_MODE == 1
        const auto result = Manual(calibration, request);
#else
        return FirstOrder();
#endif
#if DAL_CURVATURE_COST_MODE != 2
        return result.Value() + std::accumulate(result.Gradient().begin(), result.Gradient().end(), 0.0) +
               std::accumulate(result.HessianProducts().begin(), result.HessianProducts().end(), 0.0);
#endif
    };
    (void)evaluate();
    const auto start = std::chrono::steady_clock::now();
    double checksum = 0.0;
    for (int repeat = 0; repeat < 5; ++repeat)
        checksum += evaluate();
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::cout << std::setprecision(17) << "seconds " << seconds << " checksum " << checksum << '\n';
    return 0;
}
