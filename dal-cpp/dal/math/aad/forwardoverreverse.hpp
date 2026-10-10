//
// Created by Codex on 2026/10/10.
//

#pragma once

#include <functional>
#include <optional>
#include <utility>

#include <dal/math/aad/recording.hpp>
#include <dal/math/matrix/matrixs.hpp>

namespace Dal::AAD {
    namespace ForwardOverReverseDetail {
        struct NumberAccess_;
    }

    class ForwardOverReverseNumber_ {
        Number_ primal_;
        Number_ tangent_;
        friend struct ForwardOverReverseDetail::NumberAccess_;
        ForwardOverReverseNumber_(Number_ primal, Number_ tangent) : primal_(std::move(primal)), tangent_(std::move(tangent)) {}

    public:
        ForwardOverReverseNumber_() : ForwardOverReverseNumber_(0.0) {}
        explicit ForwardOverReverseNumber_(double value);
        [[nodiscard]] double Value() const { return AAD::Value(primal_); }
        [[nodiscard]] double DirectionalDerivative() const { return AAD::Value(tangent_); }
        ForwardOverReverseNumber_& operator+=(const ForwardOverReverseNumber_& rhs);
        ForwardOverReverseNumber_& operator-=(const ForwardOverReverseNumber_& rhs);
        ForwardOverReverseNumber_& operator*=(const ForwardOverReverseNumber_& rhs);
        ForwardOverReverseNumber_& operator/=(const ForwardOverReverseNumber_& rhs);
        ForwardOverReverseNumber_& operator+=(double rhs);
        ForwardOverReverseNumber_& operator-=(double rhs);
        ForwardOverReverseNumber_& operator*=(double rhs);
        ForwardOverReverseNumber_& operator/=(double rhs);
    };

    ForwardOverReverseNumber_ operator+(const ForwardOverReverseNumber_& value);
    ForwardOverReverseNumber_ operator-(const ForwardOverReverseNumber_& value);
    ForwardOverReverseNumber_ operator+(const ForwardOverReverseNumber_& lhs, const ForwardOverReverseNumber_& rhs);
    ForwardOverReverseNumber_ operator-(const ForwardOverReverseNumber_& lhs, const ForwardOverReverseNumber_& rhs);
    ForwardOverReverseNumber_ operator*(const ForwardOverReverseNumber_& lhs, const ForwardOverReverseNumber_& rhs);
    ForwardOverReverseNumber_ operator/(const ForwardOverReverseNumber_& lhs, const ForwardOverReverseNumber_& rhs);
    ForwardOverReverseNumber_ operator+(const ForwardOverReverseNumber_& lhs, double rhs);
    ForwardOverReverseNumber_ operator-(const ForwardOverReverseNumber_& lhs, double rhs);
    ForwardOverReverseNumber_ operator*(const ForwardOverReverseNumber_& lhs, double rhs);
    ForwardOverReverseNumber_ operator/(const ForwardOverReverseNumber_& lhs, double rhs);
    ForwardOverReverseNumber_ operator+(double lhs, const ForwardOverReverseNumber_& rhs);
    ForwardOverReverseNumber_ operator-(double lhs, const ForwardOverReverseNumber_& rhs);
    ForwardOverReverseNumber_ operator*(double lhs, const ForwardOverReverseNumber_& rhs);
    ForwardOverReverseNumber_ operator/(double lhs, const ForwardOverReverseNumber_& rhs);
    ForwardOverReverseNumber_ exp(const ForwardOverReverseNumber_& value);
    ForwardOverReverseNumber_ log(const ForwardOverReverseNumber_& value);
    ForwardOverReverseNumber_ sqrt(const ForwardOverReverseNumber_& value);
    ForwardOverReverseNumber_ pow(const ForwardOverReverseNumber_& base, double exponent);
    ForwardOverReverseNumber_ pow(const ForwardOverReverseNumber_& base, const ForwardOverReverseNumber_& exponent);
    ForwardOverReverseNumber_ pow(double base, const ForwardOverReverseNumber_& exponent);
    ForwardOverReverseNumber_ erfc(const ForwardOverReverseNumber_& value);
    ForwardOverReverseNumber_ NCDF(const ForwardOverReverseNumber_& value);
    ForwardOverReverseNumber_ NPDF(const ForwardOverReverseNumber_& value);

    using NativeDirectionalFunction_ = std::function<ForwardOverReverseNumber_(RecordingScope_*, const Vector_<ForwardOverReverseNumber_>&)>;

    struct ForwardOverReverseRequest_ {
        Matrix_<> directions_;
        std::optional<size_t> numericPayloadBudgetBytes_;
        std::optional<size_t> recordingCapacityBudgetBytes_;
    };

    struct ForwardOverReverseExecution_ {
        String_ method_ = "NativeForwardOverReversePrototype";
        size_t callbackEvaluations_ = 0;
        size_t recordings_ = 0;
        size_t reverseSweeps_ = 0;
        size_t numericPayloadBytes_ = 0;
        size_t peakTapeBytes_ = 0;
        size_t cleanupReserveBytes_ = 0;
    };

    struct ForwardOverReverseCapabilities_ {
        bool smoothDirectionalProducts_ = true;
        bool prototype_ = true;
        bool independentNesting_ = false;
        bool reverseEvents_ = false;
        bool nonsmoothOperators_ = false;
    };

    [[nodiscard]] constexpr ForwardOverReverseCapabilities_ ForwardOverReverseCapabilities() { return {}; }

    class ForwardOverReverseResult_ {
        double value_;
        Vector_<> gradient_;
        Vector_<> point_;
        Matrix_<> directions_;
        Vector_<> directionalDerivatives_;
        Matrix_<> products_;
        ForwardOverReverseExecution_ execution_;

    public:
        ForwardOverReverseResult_(double value,
                                  Vector_<> gradient,
                                  Vector_<> point,
                                  Matrix_<> directions,
                                  Vector_<> directionalDerivatives,
                                  Matrix_<> products,
                                  ForwardOverReverseExecution_ execution)
            : value_(value), gradient_(std::move(gradient)), point_(std::move(point)), directions_(std::move(directions)),
              directionalDerivatives_(std::move(directionalDerivatives)), products_(std::move(products)), execution_(std::move(execution)) {}
        [[nodiscard]] double Value() const { return value_; }
        [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        [[nodiscard]] const Vector_<>& Point() const { return point_; }
        [[nodiscard]] const Matrix_<>& Directions() const { return directions_; }
        [[nodiscard]] const Vector_<>& DirectionalDerivatives() const { return directionalDerivatives_; }
        [[nodiscard]] const Matrix_<>& HessianProducts() const { return products_; }
        [[nodiscard]] const ForwardOverReverseExecution_& Execution() const { return execution_; }
    };

    [[nodiscard]] size_t ForwardOverReversePayloadBytes(size_t inputs, size_t directions);
    [[nodiscard]] ForwardOverReverseResult_
    EvaluateForwardOverReverse(const NativeDirectionalFunction_& function, const Vector_<>& point, const ForwardOverReverseRequest_& request);
} // namespace Dal::AAD
