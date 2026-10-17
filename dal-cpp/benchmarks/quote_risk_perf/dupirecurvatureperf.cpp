//
// Created by Codex on 2026/10/10.
//

#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

#include <dal/benchmarks/aad.hpp>
#include <dal/benchmarks/bench.hpp>
#include <dal/math/aad/detail/gradientbumps.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/model/dupire.hpp>
#include <dal/model/dupirecurvature.hpp>

#include "dupirecurvatureperf.hpp"

namespace {
    using namespace Dal;
    namespace Bumps = AAD::GradientBumpsDetail;

    class FlatIVS_ final : public AAD::IVS_ {
    public:
        FlatIVS_() : IVS_(100.0, 0.05, 0.02) {}
        [[nodiscard]] double ImpliedVol(double, double) const override { return 0.2; }
    };

    struct ReferenceResult_ {
        double value_;
        Vector_<> gradient_;
        Vector_<> point_;
        AAD::BumpOverAADRequest_ request_;
        Matrix_<> products_;
        DupireCalibrationSnapshot_ calibration_;
        DupireQuoteCurvatureExecution_ execution_;
        [[nodiscard]] double Value() const { return value_; }
        [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        [[nodiscard]] const Matrix_<>& HessianProducts() const { return products_; }
    };

    Matrix_<> Quotes(const Vector_<>& point) {
        Matrix_<> result(3, 2);
        std::copy(point.begin(), point.end(), result.Data());
        return result;
    }

    AAD::Number_ Objective(AAD::RecordingScope_*, const Vector_<AAD::Number_>& x) { return x[8] * x[8] + 0.5 * x[9] * x[9] + x[8] * x[20]; }

    ReferenceResult_ ReferenceGradient(const DupireCalibrationSnapshot_& original, const Vector_<>& point) {
        auto calibration = RecalibrateDupireWithRisk(original, Quotes(point));
        const auto& surface = calibration.Surface()->vols_;
        Vector_<> parameters(surface.begin(), surface.end());
        parameters.Append(point);
        AAD::BumpOverAADRequest_ firstOrder;
        firstOrder.directions_ = Matrix_<>(0, static_cast<int>(parameters.size()));
        const auto differentiated = AAD::EvaluateBumpOverAAD(Objective, parameters, firstOrder);
        Matrix_<> seeds(surface.Rows(), surface.Cols());
        const size_t nodes = static_cast<size_t>(surface.Rows()) * surface.Cols();
        std::copy_n(differentiated.Gradient().begin(), nodes, seeds.Data());
        Matrix_<> direct(3, 2);
        std::copy(differentiated.Gradient().begin() + static_cast<std::ptrdiff_t>(nodes), differentiated.Gradient().end(), direct.Data());
        AAD::TapeCapacityBudget_ budget(std::numeric_limits<size_t>::max());
        AAD::TapeCapacityScope_ capacity(&budget, true);
        const auto risk = PullbackDupireCalibration(calibration, {calibration, seeds}, DupireDirectQuoteAdjoints_{calibration, direct});
        DupireQuoteCurvatureExecution_ execution;
        execution.peakTapeBytes_ = std::max(differentiated.Execution().peakTapeBytes_, budget.PeakCapacityBytes());
        execution.cleanupReserveBytes_ = AAD::TapeCleanupCapacityBytes();
        capacity.Close();
        return {differentiated.Value(),
                Vector_<>(risk.TotalAdjoints().begin(), risk.TotalAdjoints().end()),
                {},
                {},
                {},
                std::move(calibration),
                execution};
    }

    ReferenceResult_ Reference(const DupireCalibrationSnapshot_& original, const AAD::BumpOverAADRequest_& request) {
        AAD::RequireRecordingModeChangeAllowed();
        const AAD::NativeScalarFunction_ fixedObjective = Objective;
        const auto calibration = original;
        auto fixedRequest = request;
        const auto& quotes = calibration.Inputs().quoteSpreads_;
        Vector_<> point(quotes.begin(), quotes.end());
        const size_t payload = Bumps::Validate(point, fixedRequest);
        ValidateDupireQuoteRecalibration(calibration, Quotes(point));
        for (int row = 0; row < fixedRequest.directions_.Rows(); ++row)
            for (double sign : {-1.0, 1.0})
                ValidateDupireQuoteRecalibration(calibration, Quotes(Bumps::BumpedPoint(point, fixedRequest, row, sign)));
        const auto* tape = AAD::Tape();
        const AAD::NumResultsResetterForAAD_ mode(AAD::Tape(), tape->multi_, tape->numAdj_);
        AAD::Tape()->multi_ = false;
        AAD::Tape()->numAdj_ = 1;
        auto result = ReferenceGradient(calibration, point);
        result.products_ = Matrix_<>(fixedRequest.directions_.Rows(), 6);
        for (int row = 0; row < result.products_.Rows(); ++row) {
            const auto plus = ReferenceGradient(calibration, Bumps::BumpedPoint(point, fixedRequest, row, 1.0));
            const auto minus = ReferenceGradient(calibration, Bumps::BumpedPoint(point, fixedRequest, row, -1.0));
            for (int column = 0; column < 6; ++column)
                result.products_(row, column) = Bumps::CentralQuotient(plus.Gradient()[column], minus.Gradient()[column], fixedRequest.steps_[row]);
            result.execution_.peakTapeBytes_ =
                std::max({result.execution_.peakTapeBytes_, plus.execution_.peakTapeBytes_, minus.execution_.peakTapeBytes_});
        }
        result.point_ = std::move(point);
        result.request_ = std::move(fixedRequest);
        result.execution_.numericPayloadBytes_ = payload;
        result.execution_.quoteGradientEvaluations_ = 1 + 2 * result.request_.steps_.size();
        result.execution_.calibrations_ = result.execution_.quoteGradientEvaluations_;
        result.execution_.objectiveReverseSweeps_ = result.execution_.quoteGradientEvaluations_;
        result.execution_.calibrationReverseSweeps_ = result.execution_.quoteGradientEvaluations_;
        Bench::DoNotOptimize(&fixedObjective);
        return result;
    }

    auto Execute(const DupireCalibrationSnapshot_& calibration, const AAD::BumpOverAADRequest_& request) {
#ifdef DAL_DUPIRE_CURVATURE_BASELINE
        return Reference(calibration, request);
#else
        return EvaluateDupireQuoteCurvature(Objective, calibration, request);
#endif
    }

    template <class T_> void Verify(const T_& actual, const ReferenceResult_& expected) {
        Bench::VerifyAadResult(actual.Value(), expected.Value());
        for (int quote = 0; quote < 6; ++quote) {
            Bench::VerifyAadResult(actual.Gradient()[quote], expected.Gradient()[quote]);
            for (int row = 0; row < expected.HessianProducts().Rows(); ++row)
                Bench::VerifyAadResult(actual.HessianProducts()(row, quote), expected.HessianProducts()(row, quote));
        }
    }

    void Run(const std::string& name) {
        const FlatIVS_ flat;
        const AAD::MertonIVS_ merton(100.0, 0.2, 0.08, -0.1, 0.15);
        const DupireRiskInputs_ inputs{{75.0, 105.0, 135.0}, {0.4, 1.2}, Matrix_<>(3, 2, 0.001), {60.0, 100.0, 140.0}, 10.0, {0.5, 1.0}, 0.5};
        const auto calibration = CalibrateDupireWithRisk(name == "flat" ? static_cast<const AAD::IVS_&>(flat) : merton, inputs);
        AAD::BumpOverAADRequest_ request;
        request.directions_ = Matrix_<>(name == "mixed" ? 2 : 1, 6, 1.0);
        if (name == "mixed")
            for (int quote = 0; quote < 6; ++quote)
                request.directions_(1, quote) = std::cos(0.7 * quote);
        request.steps_ = Vector_<>(request.directions_.Rows(), 2e-4);
        const auto reference = Reference(calibration, request);
        using Result_ = decltype(Execute(calibration, request));
        std::optional<Result_> result(Execute(calibration, request));
        Verify(*result, reference);
        const auto timing = Bench::Run(
            "Dupire quote curvature",
            [&]() {
                result.emplace(Execute(calibration, request));
                Bench::DoNotOptimize(&*result);
            },
            1, 3);
        Verify(*result, reference);
        std::cout << std::setprecision(17) << "{\"case\":\"" << name << "\",\"min_ns\":" << timing.minNs << ",\"value\":" << result->Value()
                  << ",\"hvp0\":" << result->HessianProducts()(0, 0) << "}\n";
    }
} // namespace

int RunDupireQuoteCurvatureBenchmarks(int argc, char** argv) {
    try {
        if (argc == 1) {
            Run("flat");
            Run("mixed");
            return 0;
        }
        REQUIRE(argc == 3 && std::string(argv[1]) == "--dupire-curvature", "Dupire curvature benchmark expects --dupire-curvature flat|mixed");
        const std::string name = argv[2];
        REQUIRE(name == "flat" || name == "mixed", "Dupire curvature case must be flat or mixed");
        Run(name);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
