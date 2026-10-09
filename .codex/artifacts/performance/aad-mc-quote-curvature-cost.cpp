#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>

#include <dal-public/test-support/dupirecurvaturefixtures.hpp>
#include <dal/platform/initall.hpp>

#if DAL_CURVATURE_COST_MODE != 2
#include <dal-public/src/dupirecurvature.hpp>
#include <dal/math/aad/detail/gradientbumps.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/storage/globals.hpp>
#endif

namespace Fixture = Dal::Script::TestSupport::DupireCurvature;

namespace {
    template <class Range_> double Sum(const Range_& values) { return std::accumulate(values.begin(), values.end(), 0.0); }

#if DAL_CURVATURE_COST_MODE == 1
    struct Gradient_ {
        Dal::DupireScriptRiskResult_ result_;
        Dal::Vector_<> gradient_;
        const Dal::Vector_<>& Gradient() const { return gradient_; }
    };

    struct OwnedReference_ {
        Dal::DupireScriptRiskResult_ base_;
        Dal::Vector_<> point_;
        Dal::Vector_<> gradient_;
        Dal::AAD::BumpOverAADRequest_ bumps_;
        Dal::Matrix_<> products_;
        Dal::DupireScriptCurvatureExecution_ execution_;
    };

    auto Manual(const Dal::DupireScriptCurvaturePlan_& source, const Dal::AAD::BumpOverAADRequest_& bumps) {
        Dal::AAD::RequireRecordingModeChangeAllowed();
        Dal::XGLOBAL::ValuationMutationGuard_ guard;
        const auto plan = source;
        auto* tape = Dal::AAD::Tape();
        const Dal::AAD::NumResultsResetterForAAD_ mode(tape, tape->multi_, tape->numAdj_);
        tape->multi_ = false;
        tape->numAdj_ = 1;
        auto evaluated = Dal::AAD::GradientBumpsDetail::Evaluate(
            [&](const Dal::Vector_<>& point, const Dal::String_&) {
                Dal::Matrix_<> quotes(3, 2);
                std::copy(point.begin(), point.end(), quotes.Data());
                auto result = Dal::ValueByMonteCarloWithDupireRisk(Dal::RecalibrateDupireScriptRisk(plan.BasePlan(), quotes));
                const auto& total = result.QuoteRisk().QuoteRisk().TotalAdjoints();
                Dal::Vector_<> gradient(total.begin(), total.end());
                return Gradient_{std::move(result), std::move(gradient)};
            },
            plan.Point(), bumps);
        Dal::DupireScriptCurvatureExecution_ execution;
        execution.quoteGradientEvaluations_ = 3;
        execution.pathsPerEvaluation_ = 17;
        execution.numericPayloadBytes_ = plan.NumericPayloadBytes();
        return OwnedReference_{std::move(evaluated.base_.result_), plan.Point(),        std::move(evaluated.base_.gradient_), bumps,
                               std::move(evaluated.products_),     std::move(execution)};
    }
#endif

    double Request(const Dal::Handle_<Dal::ScriptProductData_>& product,
                   const Dal::Handle_<Dal::ModelData_>& model,
                   const Dal::DupireCalibrationSnapshot_& calibration) {
#if DAL_CURVATURE_COST_MODE == 2
        const auto result =
            Dal::ValueByMonteCarloWithDupireRisk(Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", Fixture::RiskRequest()));
        return result.Valuation().Values()[0] + Sum(result.QuoteRisk().QuoteRisk().TotalAdjoints());
#else
        Dal::DupireScriptCurvatureRequest_ request;
        request.risk_ = Fixture::RiskRequest();
        request.bumps_.directions_ = Dal::Matrix_<>(1, 6);
        const Dal::Vector_<> direction{0.3, -0.2, 1.0, 0.4, -0.1, 0.2};
        std::copy(direction.begin(), direction.end(), request.bumps_.directions_.Data());
        request.bumps_.steps_ = {2e-4};
        const auto plan = Dal::PlanDupireScriptCurvature(product, model, calibration, "Z_LOCAL", request);
#if DAL_CURVATURE_COST_MODE == 0
        const auto result = Dal::ValueByMonteCarloWithDupireCurvature(plan);
        REQUIRE(result.Execution().quoteGradientEvaluations_ == 3 && result.Execution().pathsPerEvaluation_ == 17, "unexpected curvature workload");
        return result.Base().Valuation().Values()[0] + Sum(result.Gradient()) + Sum(result.HessianProducts());
#else
        const auto result = Manual(plan, request.bumps_);
        return result.base_.Valuation().Values()[0] + Sum(result.gradient_) + Sum(result.products_);
#endif
#endif
    }
} // namespace

int main(int argc, char** argv) {
    try {
        Dal::RegisterAll_::Init();
        const Fixture::SingleWorker_ worker;
        const bool nonflat = argc > 1 && std::string(argv[1]) == "merton";
        const auto flat = Fixture::Calibration();
        const auto calibration = nonflat ? Dal::CalibrateDupireWithRisk(Dal::AAD::MertonIVS_(100.0, 0.2, 0.08, -0.1, 0.15), flat.Inputs()) : flat;
        const auto model = Fixture::Model(calibration);
        const auto product = Fixture::MixedProduct(0.001);
        static_cast<void>(Request(product, model, calibration));
        double checksum = 0.0;
        constexpr int ITERATIONS = 5;
        const auto start = std::chrono::steady_clock::now();
        for (int iteration = 0; iteration < ITERATIONS; ++iteration)
            checksum += Request(product, model, calibration);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() / ITERATIONS;
        REQUIRE(std::isfinite(checksum), "nonfinite benchmark checksum");
        std::cout << std::setprecision(17) << "seconds " << seconds << " checksum " << checksum << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
