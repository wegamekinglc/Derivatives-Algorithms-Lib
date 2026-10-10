//
// Created by Codex on 2026/10/10.
//

#include <cmath>
#include <limits>
#include <new>
#include <string>

#include <dal/math/aad/forwardoverreverse.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/reverseevent.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/platform/platform.hpp>

namespace Dal::AAD {
    namespace ForwardOverReverseDetail {
        struct NumberAccess_ {
            static const Number_& Primal(const ForwardOverReverseNumber_& value) { return value.primal_; }
            static const Number_& Tangent(const ForwardOverReverseNumber_& value) { return value.tangent_; }
            static ForwardOverReverseNumber_ Make(Number_ primal, Number_ tangent) {
                REQUIRE(std::isfinite(Value(primal)) && std::isfinite(Value(tangent)),
                        "ForwardOverReverse: primitive value and directional derivative must be finite");
                return {std::move(primal), std::move(tangent)};
            }
        };
    } // namespace ForwardOverReverseDetail

    namespace {
        using Access = ForwardOverReverseDetail::NumberAccess_;
        using Dual = ForwardOverReverseNumber_;

        struct OPSmoothPower_ {
            static double Eval(double value, double exponent) { return std::pow(value, exponent); }
            // Avoid value/base: smooth integer powers must also differentiate at zero.
            static double Derivative(double value, double, double exponent) { return exponent * std::pow(value, exponent - 1.0); }
        };

        auto SmoothPower(const Number_& base, double exponent) { return UnaryExpression_<Number_, OPSmoothPower_>(base, exponent); }

        double FiniteConstant(double value) {
            REQUIRE(std::isfinite(value), "ForwardOverReverse: constant must be finite");
            return value;
        }

        size_t Sum(size_t lhs, size_t rhs) {
            REQUIRE(rhs <= std::numeric_limits<size_t>::max() - lhs, "ForwardOverReverse: numeric payload sum overflows size_t");
            return lhs + rhs;
        }

        size_t Product(size_t lhs, size_t rhs) {
            REQUIRE(rhs == 0 || lhs <= std::numeric_limits<size_t>::max() / rhs, "ForwardOverReverse: numeric payload product overflows size_t");
            return lhs * rhs;
        }
    } // namespace

    ForwardOverReverseNumber_::ForwardOverReverseNumber_(double value) : primal_(FiniteConstant(value)), tangent_(0.0) {}

    Dual operator+(const Dual& value) { return value; }
    Dual operator-(const Dual& value) { return Access::Make(-Access::Primal(value), -Access::Tangent(value)); }
    Dual operator+(const Dual& lhs, const Dual& rhs) {
        return Access::Make(Access::Primal(lhs) + Access::Primal(rhs), Access::Tangent(lhs) + Access::Tangent(rhs));
    }
    Dual operator-(const Dual& lhs, const Dual& rhs) {
        return Access::Make(Access::Primal(lhs) - Access::Primal(rhs), Access::Tangent(lhs) - Access::Tangent(rhs));
    }
    Dual operator*(const Dual& lhs, const Dual& rhs) {
        return Access::Make(Access::Primal(lhs) * Access::Primal(rhs),
                            Access::Tangent(lhs) * Access::Primal(rhs) + Access::Primal(lhs) * Access::Tangent(rhs));
    }
    Dual operator/(const Dual& lhs, const Dual& rhs) {
        REQUIRE(rhs.Value() != 0.0, "ForwardOverReverse: division requires a nonzero denominator");
        const Number_ primal = Access::Primal(lhs) / Access::Primal(rhs);
        return Access::Make(primal, (Access::Tangent(lhs) - primal * Access::Tangent(rhs)) / Access::Primal(rhs));
    }
    Dual operator+(const Dual& lhs, double rhs) { return Access::Make(Access::Primal(lhs) + FiniteConstant(rhs), Access::Tangent(lhs)); }
    Dual operator-(const Dual& lhs, double rhs) { return Access::Make(Access::Primal(lhs) - FiniteConstant(rhs), Access::Tangent(lhs)); }
    Dual operator*(const Dual& lhs, double rhs) { return Access::Make(Access::Primal(lhs) * FiniteConstant(rhs), Access::Tangent(lhs) * rhs); }
    Dual operator/(const Dual& lhs, double rhs) {
        REQUIRE(FiniteConstant(rhs) != 0.0, "ForwardOverReverse: division requires a nonzero denominator");
        return Access::Make(Access::Primal(lhs) / rhs, Access::Tangent(lhs) / rhs);
    }
    Dual operator+(double lhs, const Dual& rhs) { return rhs + lhs; }
    Dual operator-(double lhs, const Dual& rhs) { return Access::Make(FiniteConstant(lhs) - Access::Primal(rhs), -Access::Tangent(rhs)); }
    Dual operator*(double lhs, const Dual& rhs) { return rhs * lhs; }
    Dual operator/(double lhs, const Dual& rhs) {
        REQUIRE(rhs.Value() != 0.0, "ForwardOverReverse: division requires a nonzero denominator");
        const Number_ primal = FiniteConstant(lhs) / Access::Primal(rhs);
        return Access::Make(primal, -primal * Access::Tangent(rhs) / Access::Primal(rhs));
    }

    Dual& Dual::operator+=(const Dual& rhs) { return *this = *this + rhs; }
    Dual& Dual::operator-=(const Dual& rhs) { return *this = *this - rhs; }
    Dual& Dual::operator*=(const Dual& rhs) { return *this = *this * rhs; }
    Dual& Dual::operator/=(const Dual& rhs) { return *this = *this / rhs; }
    Dual& Dual::operator+=(double rhs) { return *this = *this + rhs; }
    Dual& Dual::operator-=(double rhs) { return *this = *this - rhs; }
    Dual& Dual::operator*=(double rhs) { return *this = *this * rhs; }
    Dual& Dual::operator/=(double rhs) { return *this = *this / rhs; }

    Dual exp(const Dual& value) {
        const Number_ primal = AAD::exp(Access::Primal(value));
        return Access::Make(primal, primal * Access::Tangent(value));
    }
    Dual log(const Dual& value) {
        REQUIRE(value.Value() > 0.0, "ForwardOverReverse: log requires a positive argument");
        return Access::Make(AAD::log(Access::Primal(value)), Access::Tangent(value) / Access::Primal(value));
    }
    Dual sqrt(const Dual& value) {
        REQUIRE(value.Value() > 0.0, "ForwardOverReverse: sqrt requires a positive argument");
        const Number_ primal = AAD::sqrt(Access::Primal(value));
        return Access::Make(primal, Access::Tangent(value) / (2.0 * primal));
    }
    Dual pow(const Dual& base, double exponent) {
        FiniteConstant(exponent);
        if (exponent == 0.0)
            return Dual(1.0);
        if (exponent == 1.0)
            return base;
        REQUIRE(base.Value() > 0.0 || (std::trunc(exponent) == exponent && (base.Value() != 0.0 || exponent >= 2.0)),
                "ForwardOverReverse: power requires a smooth finite real domain");
        return Access::Make(SmoothPower(Access::Primal(base), exponent),
                            exponent * SmoothPower(Access::Primal(base), exponent - 1.0) * Access::Tangent(base));
    }
    Dual pow(const Dual& base, const Dual& exponent) {
        REQUIRE(base.Value() > 0.0, "ForwardOverReverse: active power requires a positive base");
        const Number_ primal = AAD::pow(Access::Primal(base), Access::Primal(exponent));
        return Access::Make(primal, primal * (Access::Tangent(exponent) * AAD::log(Access::Primal(base)) +
                                              Access::Primal(exponent) * Access::Tangent(base) / Access::Primal(base)));
    }
    Dual pow(double base, const Dual& exponent) {
        REQUIRE(FiniteConstant(base) > 0.0, "ForwardOverReverse: active power requires a positive base");
        const Number_ primal = AAD::pow(base, Access::Primal(exponent));
        return Access::Make(primal, primal * std::log(base) * Access::Tangent(exponent));
    }
    Dual erfc(const Dual& value) {
        const auto& x = Access::Primal(value);
        return Access::Make(AAD::erfc(x), -1.1283791670955125739 * AAD::exp(-x * x) * Access::Tangent(value));
    }
    Dual NCDF(const Dual& value) { return Access::Make(AAD::NCDF(Access::Primal(value)), AAD::NPDF(Access::Primal(value)) * Access::Tangent(value)); }
    Dual NPDF(const Dual& value) {
        const Number_ primal = AAD::NPDF(Access::Primal(value));
        return Access::Make(primal, -Access::Primal(value) * primal * Access::Tangent(value));
    }

    size_t ForwardOverReversePayloadBytes(size_t inputs, size_t directions) {
        const size_t coordinates = Sum(Product(2, inputs), Product(2, Product(inputs, directions)));
        return Product(Sum(Sum(1, coordinates), directions), sizeof(double));
    }

    namespace {
        size_t Validate(const Vector_<>& point, const ForwardOverReverseRequest_& request) {
            REQUIRE(point.size() <= static_cast<size_t>(std::numeric_limits<int>::max()), "ForwardOverReverse: point exceeds matrix integer range");
            REQUIRE(request.directions_.Rows() >= 0 && request.directions_.Cols() == static_cast<int>(point.size()),
                    "ForwardOverReverse: directions require nonnegative rows and one column per input");
            const size_t bytes = ForwardOverReversePayloadBytes(point.size(), request.directions_.Rows());
            REQUIRE(!request.numericPayloadBudgetBytes_ || bytes <= *request.numericPayloadBudgetBytes_,
                    "ForwardOverReverse: numeric payload budget exceeded");
            for (double value : point)
                REQUIRE(std::isfinite(value), "ForwardOverReverse: point components must be finite");
            for (int row = 0; row < request.directions_.Rows(); ++row)
                for (int column = 0; column < request.directions_.Cols(); ++column)
                    REQUIRE(std::isfinite(request.directions_(row, column)), "ForwardOverReverse: direction components must be finite");
            return bytes;
        }

        Vector_<> ReadGradient(const Vector_<Number_>& inputs) {
            Vector_<> result(inputs.size());
            for (size_t column = 0; column < inputs.size(); ++column) {
                result[column] = NativeOperations_::ReadAdjoint(inputs[column]);
                REQUIRE(std::isfinite(result[column]), "ForwardOverReverse: reverse components must be finite");
            }
            return result;
        }

        struct DirectionResult_ {
            double value_;
            double directionalDerivative_;
            Vector_<> gradient_;
            Vector_<> product_;
        };

        DirectionResult_ RecordDirection(const NativeDirectionalFunction_& function, const Vector_<>& point, const Matrix_<>& directions, int row) {
            RecordingScope_ recording;
            Vector_<Number_> primals(point.size()), tangents(point.size());
            for (size_t column = 0; column < point.size(); ++column) {
                recording.RegisterInput(primals[column], point[column]);
                recording.RegisterInput(tangents[column], directions.Rows() == 0 ? 0.0 : directions(row, static_cast<int>(column)));
            }
            Number_ zero;
            recording.RegisterInput(zero, 0.0);
            recording.StartRecording();
            Vector_<Dual> inputs;
            inputs.reserve(point.size());
            for (size_t column = 0; column < point.size(); ++column)
                inputs.push_back(Access::Make(primals[column], tangents[column]));
            const Dual output = function(&recording, inputs);
            NativeRecordedOperation_::RequireUnsegmented(&recording);
            REQUIRE(Tape()->ReverseEventCount() == 0, "ForwardOverReverse: opaque reverse events are unsupported");
            auto primalRoot = NativeOperations_::ActiveRoot(Access::Primal(output), zero);
            auto tangentRoot = NativeOperations_::ActiveRoot(Access::Tangent(output), zero);
            recording.FinishRecording();
            DirectionResult_ result{output.Value(), output.DirectionalDerivative(), {}, {}};
            if (row == 0) {
                recording.ClearAdjoints();
                NativeOperations_::SetSeed(primalRoot, 1.0);
                recording.Reverse();
                result.gradient_ = ReadGradient(primals);
            }
            if (directions.Rows() != 0) {
                recording.ClearAdjoints();
                NativeOperations_::SetSeed(tangentRoot, 1.0);
                recording.Reverse();
                result.product_ = ReadGradient(primals);
            }
            recording.Close();
            return result;
        }

        DirectionResult_ EvaluateDirection(const NativeDirectionalFunction_& function, const Vector_<>& point, const Matrix_<>& directions, int row) {
            try {
                return RecordDirection(function, point, directions, row);
            } catch (const std::bad_alloc&) {
                throw;
            } catch (const std::exception& error) {
                THROW("ForwardOverReverse: direction=" + String_(std::to_string(row)) + "; " + error.what());
            }
        }
    } // namespace

    ForwardOverReverseResult_
    EvaluateForwardOverReverse(const NativeDirectionalFunction_& function, const Vector_<>& point, const ForwardOverReverseRequest_& request) {
        RequireRecordingModeChangeAllowed();
        const NativeDirectionalFunction_ fixedFunction = function;
        Vector_<> fixedPoint = point;
        ForwardOverReverseRequest_ fixedRequest = request;
        REQUIRE(static_cast<bool>(fixedFunction), "ForwardOverReverse: directional function must be available");
        ForwardOverReverseExecution_ execution;
        execution.numericPayloadBytes_ = Validate(fixedPoint, fixedRequest);
        execution.callbackEvaluations_ = std::max<size_t>(1, fixedRequest.directions_.Rows());
        execution.recordings_ = execution.callbackEvaluations_;
        execution.reverseSweeps_ = Sum(1, static_cast<size_t>(fixedRequest.directions_.Rows()));
        auto* tape = Tape();
        const NumResultsResetterForAAD_ mode(tape, tape->multi_, tape->numAdj_);
        tape->multi_ = false;
        tape->numAdj_ = 1;
        TapeCapacityBudget_ budget(fixedRequest.recordingCapacityBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
        TapeCapacityScope_ capacity(&budget, true);
        auto first = EvaluateDirection(fixedFunction, fixedPoint, fixedRequest.directions_, 0);
        const double value = first.value_;
        auto gradient = std::move(first.gradient_);
        Vector_<> directionalDerivatives(fixedRequest.directions_.Rows());
        Matrix_<> products(fixedRequest.directions_.Rows(), fixedRequest.directions_.Cols());
        for (int row = 0; row < fixedRequest.directions_.Rows(); ++row) {
            auto direction = row == 0 ? std::move(first) : EvaluateDirection(fixedFunction, fixedPoint, fixedRequest.directions_, row);
            REQUIRE(direction.value_ == value, "ForwardOverReverse: callback value changed between directional recordings");
            directionalDerivatives[row] = direction.directionalDerivative_;
            for (int column = 0; column < products.Cols(); ++column)
                products(row, column) = direction.product_[column];
        }
        execution.peakTapeBytes_ = budget.PeakCapacityBytes();
        execution.cleanupReserveBytes_ = TapeCleanupCapacityBytes();
        capacity.Close();
        return {value,
                std::move(gradient),
                std::move(fixedPoint),
                std::move(fixedRequest.directions_),
                std::move(directionalDerivatives),
                std::move(products),
                std::move(execution)};
    }
} // namespace Dal::AAD
