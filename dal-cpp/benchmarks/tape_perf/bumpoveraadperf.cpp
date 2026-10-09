//
// Created by Codex on 2026/10/09.
//

#include <array>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

#include <dal/benchmarks/aad.hpp>
#include <dal/benchmarks/bench.hpp>
#include <dal/math/aad/bumpoveraad.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/platform/platform.hpp>

#include "bumpoveraadperf.hpp"

using namespace Dal;
using namespace Dal::AAD;

namespace {
    struct Case_ {
        const char* name_;
        int inputs_;
        int directions_;
    };

    Case_ ParseCase(int argc, char** argv) {
        REQUIRE(argc == 1 || (argc == 3 && std::string(argv[1]) == "--case"), "bump-over-AAD benchmark expects --case small|wide");
        const std::string name = argc == 3 ? argv[2] : "small";
        constexpr std::array<Case_, 2> CASES{{{"small", 4, 1}, {"wide", 32, 3}}};
        for (const auto& item : CASES)
            if (name == item.name_)
                return item;
        THROW("bump-over-AAD benchmark case must be small or wide");
    }

    template <class T_> T_ Polynomial(const Vector_<T_>& x) {
        T_ result(0.0);
        for (size_t i = 0; i < x.size(); ++i) {
            result = result + (1.0 + 0.01 * i) * x[i] * x[i];
            if (i > 0)
                result = result + 0.25 * x[i - 1] * x[i];
        }
        return result;
    }

    Number_ Kernel(RecordingScope_*, const Vector_<Number_>& x) { return Polynomial(x); }

    double DirectionCoordinate(int row, int column, int inputs) {
        if (row == 0)
            return column == 0 ? 1.0 : 0.0;
        return row == 1 ? (column % 2 == 0 ? 1.0 : -0.5) : (column + 1.0) / inputs;
    }

    BumpOverAADRequest_ Request(const Case_& shape) {
        BumpOverAADRequest_ result;
        result.directions_ = Matrix_<>(shape.directions_, shape.inputs_);
        result.steps_ = Vector_<>(shape.directions_, 1e-3);
        for (int row = 0; row < shape.directions_; ++row)
            for (int column = 0; column < shape.inputs_; ++column)
                result.directions_(row, column) = DirectionCoordinate(row, column, shape.inputs_);
        result.numericPayloadBudgetBytes_ = sizeof(double) * (1 + 2 * shape.inputs_ + 2 * shape.inputs_ * shape.directions_ + shape.directions_);
        return result;
    }

#ifdef DAL_BUMP_OVER_AAD_BASELINE
    struct Gradient_ {
        double value_;
        Vector_<> gradient_;
    };

    Gradient_ ReferenceGradient(const NativeScalarFunction_& function, const Vector_<>& point) {
        RecordingScope_ recording;
        Vector_<Number_> inputs(point.size());
        for (size_t column = 0; column < point.size(); ++column)
            recording.RegisterInput(inputs[column], point[column]);
        Number_ zero;
        recording.RegisterInput(zero, 0.0);
        recording.StartRecording();
        const auto output = function(&recording, inputs);
        auto root = NativeOperations_::ActiveRoot(output, zero);
        recording.FinishRecording();
        recording.ClearAdjoints();
        NativeOperations_::SetSeed(root, 1.0);
        recording.Reverse();
        Gradient_ result{Value(output), Vector_<>(point.size())};
        for (size_t column = 0; column < point.size(); ++column)
            result.gradient_[column] = NativeOperations_::ReadAdjoint(inputs[column]);
        recording.Close();
        return result;
    }

    BumpOverAADResult_ Execute(const NativeScalarFunction_& function, const Vector_<>& point, const BumpOverAADRequest_& request) {
        const auto fixedFunction = function;
        Vector_<> fixedPoint = point;
        BumpOverAADRequest_ fixedRequest = request;
        const auto mode = SetNumResultsForAAD(false, 1);
        TapeCapacityBudget_ budget(std::numeric_limits<size_t>::max());
        TapeCapacityScope_ capacity(&budget, true);
        auto base = ReferenceGradient(fixedFunction, fixedPoint);
        Matrix_<> products(fixedRequest.directions_.Rows(), fixedRequest.directions_.Cols());
        for (int row = 0; row < products.Rows(); ++row) {
            Vector_<> plus = fixedPoint, minus = fixedPoint;
            for (int column = 0; column < products.Cols(); ++column) {
                plus[column] = std::fma(fixedRequest.steps_[row], fixedRequest.directions_(row, column), fixedPoint[column]);
                minus[column] = std::fma(-fixedRequest.steps_[row], fixedRequest.directions_(row, column), fixedPoint[column]);
            }
            const auto upper = ReferenceGradient(fixedFunction, plus), lower = ReferenceGradient(fixedFunction, minus);
            for (int column = 0; column < products.Cols(); ++column)
                products(row, column) = (upper.gradient_[column] - lower.gradient_[column]) / (2.0 * fixedRequest.steps_[row]);
        }
        BumpOverAADExecution_ execution;
        execution.method_ = "NativeGradientSecantReference";
        execution.gradientEvaluations_ = 1 + 2 * fixedRequest.steps_.size();
        execution.reverseSweeps_ = execution.gradientEvaluations_;
        execution.numericPayloadBytes_ = *fixedRequest.numericPayloadBudgetBytes_;
        execution.peakTapeBytes_ = budget.PeakCapacityBytes();
        execution.cleanupReserveBytes_ = TapeCleanupCapacityBytes();
        capacity.Close();
        return {base.value_, std::move(base.gradient_), std::move(fixedPoint), std::move(fixedRequest), std::move(products), std::move(execution)};
    }
#else
    BumpOverAADResult_ Execute(const NativeScalarFunction_& function, const Vector_<>& point, const BumpOverAADRequest_& request) {
        return EvaluateBumpOverAAD(function, point, request);
    }
#endif

    void Verify(const BumpOverAADResult_& result) {
        const auto& x = result.Point();
        Bench::VerifyAadResult(result.Value(), Polynomial(x));
        for (int column = 0; column < result.HessianProducts().Cols(); ++column) {
            const double left = column == 0 ? 0.0 : x[column - 1];
            const double right = column + 1 == static_cast<int>(x.size()) ? 0.0 : x[column + 1];
            Bench::VerifyAadResult(result.Gradient()[column], 2.0 * (1.0 + 0.01 * column) * x[column] + 0.25 * (left + right));
            for (int row = 0; row < result.HessianProducts().Rows(); ++row) {
                const auto& directions = result.Directions();
                const double before = column == 0 ? 0.0 : directions(row, column - 1);
                const double after = column + 1 == directions.Cols() ? 0.0 : directions(row, column + 1);
                Bench::VerifyAadResult(result.HessianProducts()(row, column),
                                       2.0 * (1.0 + 0.01 * column) * directions(row, column) + 0.25 * (before + after));
            }
        }
    }

    template <class V_> void PrintValues(const V_& values) {
        const char* separator = "";
        std::cout << '[';
        for (double value : values) {
            std::cout << separator << value;
            separator = ",";
        }
        std::cout << ']';
    }

    void Run(const Case_& shape) {
        Vector_<> point(shape.inputs_);
        for (int i = 0; i < shape.inputs_; ++i)
            point[i] = 0.5 + 0.01 * i;
        const auto request = Request(shape);
        const NativeScalarFunction_ function = Kernel;
        std::optional<BumpOverAADResult_> result = Execute(function, point, request);
        Verify(*result);
        const auto timing = Bench::Run(
            "native directional curvature",
            [&]() {
                result.emplace(Execute(function, point, request));
                Bench::DoNotOptimize(&*result);
            },
            1, 3, 100);
        Verify(*result);
        const auto& execution = result->Execution();
        std::cout << std::setprecision(17) << "{\"case\":\"" << shape.name_ << "\",\"method\":\"" << execution.method_
                  << "\",\"min_ns\":" << timing.minNs << ",\"value\":" << result->Value()
                  << ",\"gradient_evaluations\":" << execution.gradientEvaluations_ << ",\"payload_bytes\":" << execution.numericPayloadBytes_
                  << ",\"peak_tape_bytes\":" << execution.peakTapeBytes_ << ",\"cleanup_reserve_bytes\":" << execution.cleanupReserveBytes_
                  << ",\"gradient\":";
        PrintValues(result->Gradient());
        std::cout << ",\"products\":";
        PrintValues(result->HessianProducts());
        std::cout << "}\n";
    }
} // namespace

int RunBumpOverAADBenchmarks(int argc, char** argv) {
    try {
        Dal::RegisterAll_::Init();
        Run(ParseCase(argc, argv));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
