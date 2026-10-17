//
// Created by Codex on 2026/10/09.
//

#pragma once

#include <functional>
#include <optional>
#include <utility>

#include <dal/math/aad/recording.hpp>
#include <dal/math/matrix/matrixs.hpp>

namespace Dal::AAD {
    using NativeScalarFunction_ = std::function<Number_(RecordingScope_*, const Vector_<Number_>&)>;

    struct BumpOverAADRequest_ {
        Matrix_<> directions_;
        Vector_<> steps_;
        std::optional<size_t> numericPayloadBudgetBytes_;
        std::optional<size_t> recordingCapacityBudgetBytes_;
    };

    struct BumpOverAADExecution_ {
        String_ method_ = "BumpOverNativeAAD";
        size_t gradientEvaluations_ = 0;
        size_t reverseSweeps_ = 0;
        size_t numericPayloadBytes_ = 0;
        size_t peakTapeBytes_ = 0;
        size_t cleanupReserveBytes_ = 0;
    };

    class BumpOverAADResult_ {
        double value_;
        Vector_<> gradient_;
        Vector_<> point_;
        BumpOverAADRequest_ request_;
        Matrix_<> products_;
        BumpOverAADExecution_ execution_;

    public:
        BumpOverAADResult_(
            double value, Vector_<> gradient, Vector_<> point, BumpOverAADRequest_ request, Matrix_<> products, BumpOverAADExecution_ execution)
            : value_(value), gradient_(std::move(gradient)), point_(std::move(point)), request_(std::move(request)), products_(std::move(products)),
              execution_(std::move(execution)) {}
        [[nodiscard]] double Value() const { return value_; }
        [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        [[nodiscard]] const Matrix_<>& HessianProducts() const { return products_; }
        [[nodiscard]] const Vector_<>& Point() const { return point_; }
        [[nodiscard]] const Matrix_<>& Directions() const { return request_.directions_; }
        [[nodiscard]] const Vector_<>& Steps() const { return request_.steps_; }
        [[nodiscard]] const BumpOverAADExecution_& Execution() const { return execution_; }
    };

    [[nodiscard]] size_t BumpOverAADPayloadBytes(size_t inputs, size_t directions);
    [[nodiscard]] BumpOverAADResult_
    EvaluateBumpOverAAD(const NativeScalarFunction_& function, const Vector_<>& point, const BumpOverAADRequest_& request);
} // namespace Dal::AAD
