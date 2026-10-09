//
// Created by Codex on 2026/10/09.
//

#include <algorithm>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include <dal/benchmarks/aad.hpp>
#include <dal/benchmarks/bench.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/script/montecarlocurvature.hpp>

#include "montecarlocurvatureperf.hpp"

using namespace Dal;
namespace AAD = Dal::AAD;
namespace Script = Dal::Script;

namespace {
    struct Case_ {
        const char* name_;
        int steps_;
        int directions_;
        size_t threads_;
    };

    Case_ ParseCase(int argc, char** argv) {
        REQUIRE(argc == 3 && std::string(argv[1]) == "--case", "MC curvature benchmark expects --case spot|mixed");
        const std::string name = argv[2];
        if (name == "spot")
            return {"spot", 1, 1, 1};
        REQUIRE(name == "mixed", "MC curvature benchmark case must be spot or mixed");
        return {"mixed", 16, 3, 2};
    }

    Script::BlackScholesSegmentedPath_ Kernel(const Case_& shape) {
        Vector_<Cell_> dates{Cell_("SCALE")};
        Vector_<String_> events{"2"};
        const Date_ today(2026, 10, 1);
        for (int step = 1; step <= shape.steps_; ++step) {
            dates.emplace_back(today.AddDays(step));
            events.emplace_back("s = FIX(EQ[DAL196_TEST])");
        }
        events.back() += " pay PAYS SCALE * s * s";
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = today;
        return Script::BlackScholesSegmentedPath_(std::make_shared<const Script::BlackScholesSegmentedPreparation_>(
            Script::PrepareBlackScholesSegmentedScript(Script::ScriptProductData_("", dates, events), valuation)));
    }

    AAD::BumpOverAADRequest_ Request(const Case_& shape) {
        AAD::BumpOverAADRequest_ request;
        request.directions_ = Matrix_<>(shape.directions_, 5, 0.0);
        request.directions_(0, 0) = 1.0;
        if (shape.directions_ > 1) {
            request.directions_(1, 1) = 1.0;
            request.directions_(2, 0) = -2.0;
            request.directions_(2, 2) = 0.3;
            request.directions_(2, 4) = 0.5;
        }
        request.steps_ = Vector_<>(shape.directions_, 1e-4);
        return request;
    }

    Script::MonteCarloCurvatureResult_ Reference(const Script::BlackScholesSegmentedPath_& kernel,
                                                 const Vector_<>& point,
                                                 const AAD::BumpOverAADRequest_& request,
                                                 const Script::SegmentedMonteCarloSettings_& settings) {
        const auto fixedKernel = kernel;
        auto fixedPoint = point;
        auto fixedRequest = request;
        const auto fixedSettings = settings;
        AAD::RequireRecordingModeChangeAllowed();
        auto* tape = AAD::Tape();
        const AAD::NumResultsResetterForAAD_ mode(tape, tape->multi_, tape->numAdj_);
        tape->multi_ = false;
        tape->numAdj_ = 1;
        Script::MonteCarloCurvatureExecution_ execution;
        execution.method_ = "SegmentedGradientSecantReference";
        execution.gradientEvaluations_ = 1 + 2 * fixedRequest.steps_.size();
        execution.numericPayloadBytes_ = AAD::BumpOverAADPayloadBytes(fixedPoint.size(), fixedRequest.steps_.size());
        const auto evaluate = [&](const Vector_<>& parameters) {
            auto evaluated = Script::EvaluateBlackScholesSegmentedMonteCarlo(fixedKernel, parameters, 65, fixedSettings);
            const auto& usage = evaluated.Execution();
            execution.maxPathTapeBytes_ = std::max(execution.maxPathTapeBytes_, usage.maxPathTapeBytes_);
            execution.maxPathCheckpointBytes_ = std::max(execution.maxPathCheckpointBytes_, usage.maxPathCheckpointBytes_);
            execution.maxPathCleanupReserveBytes_ = std::max(execution.maxPathCleanupReserveBytes_, usage.maxPathCleanupReserveBytes_);
            return evaluated;
        };
        auto base = evaluate(fixedPoint);
        Matrix_<> products(fixedRequest.directions_.Rows(), fixedRequest.directions_.Cols());
        for (int row = 0; row < products.Rows(); ++row) {
            Vector_<> plus = fixedPoint, minus = fixedPoint;
            for (int column = 0; column < products.Cols(); ++column) {
                plus[column] = std::fma(fixedRequest.steps_[row], fixedRequest.directions_(row, column), fixedPoint[column]);
                minus[column] = std::fma(-fixedRequest.steps_[row], fixedRequest.directions_(row, column), fixedPoint[column]);
            }
            const auto upper = evaluate(plus), lower = evaluate(minus);
            for (int column = 0; column < products.Cols(); ++column)
                products(row, column) = (upper.MeanGradient()[column] - lower.MeanGradient()[column]) / (2.0 * fixedRequest.steps_[row]);
        }
        return {std::move(base),     std::move(fixedPoint), std::move(fixedRequest), fixedSettings, fixedKernel.PreparedHandle(),
                std::move(products), std::move(execution)};
    }

    Script::MonteCarloCurvatureResult_ Execute(const Script::BlackScholesSegmentedPath_& kernel,
                                               const Vector_<>& point,
                                               const AAD::BumpOverAADRequest_& request,
                                               const Script::SegmentedMonteCarloSettings_& settings) {
#ifdef DAL_MONTE_CARLO_CURVATURE_BASELINE
        return Reference(kernel, point, request, settings);
#else
        return Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, request, settings);
#endif
    }

    void Verify(const Script::MonteCarloCurvatureResult_& result, const Script::MonteCarloCurvatureResult_& reference) {
        Bench::VerifyAadResult(result.Base().MeanValue(), reference.Base().MeanValue());
        for (size_t i = 0; i < result.Base().MeanGradient().size(); ++i)
            Bench::VerifyAadResult(result.Base().MeanGradient()[i], reference.Base().MeanGradient()[i]);
        for (int row = 0; row < result.HessianProducts().Rows(); ++row)
            for (int column = 0; column < result.HessianProducts().Cols(); ++column)
                Bench::VerifyAadResult(result.HessianProducts()(row, column), reference.HessianProducts()(row, column));
    }

    void Run(const Case_& shape) {
        ThreadPool_::GetInstance()->Start(shape.threads_, true);
        const auto kernel = Kernel(shape);
        const Vector_<> point{100.0, 0.2, 0.03, 0.01, 2.0};
        const auto request = Request(shape);
        Script::SegmentedMonteCarloSettings_ settings;
        settings.firstPath_ = 7;
        settings.path_.segmentSteps_ = 4;
        const auto reference = Reference(kernel, point, request, settings);
        std::optional<Script::MonteCarloCurvatureResult_> result = Execute(kernel, point, request, settings);
        Verify(*result, reference);
        const auto timing = Bench::Run(
            "segmented MC directional curvature",
            [&]() {
                result.emplace(Execute(kernel, point, request, settings));
                Bench::DoNotOptimize(&*result);
            },
            1, 3);
        Verify(*result, reference);
        const auto& execution = result->Execution();
        std::cout << std::setprecision(17) << "{\"case\":\"" << shape.name_ << "\",\"method\":\"" << execution.method_
                  << "\",\"min_ns\":" << timing.minNs << ",\"value\":" << result->Base().MeanValue()
                  << ",\"spot_gamma\":" << result->HessianProducts()(0, 0) << ",\"gradient_evaluations\":" << execution.gradientEvaluations_
                  << ",\"payload_bytes\":" << execution.numericPayloadBytes_ << ",\"peak_path_tape_bytes\":" << execution.maxPathTapeBytes_
                  << ",\"peak_path_checkpoint_bytes\":" << execution.maxPathCheckpointBytes_
                  << ",\"peak_path_cleanup_reserve_bytes\":" << execution.maxPathCleanupReserveBytes_ << "}\n";
    }
} // namespace

int RunMonteCarloCurvatureBenchmarks(int argc, char** argv) {
    try {
        Dal::RegisterAll_::Init();
        Run(ParseCase(argc, argv));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
