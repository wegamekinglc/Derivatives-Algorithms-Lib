//
// Created by Codex on 2026/10/09.
//

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include <dal/math/aad/bumpoveraad.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/reverseevent.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/platform/platform.hpp>

namespace Dal::AAD {
    namespace {
        size_t Sum(size_t lhs, size_t rhs) {
            REQUIRE(rhs <= std::numeric_limits<size_t>::max() - lhs, "BumpOverAAD: numeric payload sum overflows size_t");
            return lhs + rhs;
        }

        size_t Product(size_t lhs, size_t rhs) {
            REQUIRE(rhs == 0 || lhs <= std::numeric_limits<size_t>::max() / rhs, "BumpOverAAD: numeric payload product overflows size_t");
            return lhs * rhs;
        }

        double BumpedCoordinate(double value, double direction, double step) { return std::fma(step, direction, value); }

        void ValidateDirection(const Vector_<>& point, const BumpOverAADRequest_& request, int row) {
            const double step = request.steps_[row];
            const String_ context = "BumpOverAAD: direction=" + String_(std::to_string(row));
            REQUIRE(std::isfinite(step) && step > 0.0, context + "; step must be finite and positive");
            bool nonzero = false;
            for (int column = 0; column < request.directions_.Cols(); ++column) {
                const double direction = request.directions_(row, column);
                REQUIRE(std::isfinite(direction), context + "; direction components must be finite");
                if (direction == 0.0)
                    continue;
                nonzero = true;
                for (double sign : {-1.0, 1.0}) {
                    const double bumped = BumpedCoordinate(point[column], direction, sign * step);
                    REQUIRE(std::isfinite(bumped) && bumped != point[column], context + "; bump must be finite and change every nonzero component");
                }
            }
            REQUIRE(nonzero, context + "; direction must be nonzero");
        }

        size_t Validate(const NativeScalarFunction_& function, const Vector_<>& point, const BumpOverAADRequest_& request) {
            REQUIRE(static_cast<bool>(function), "BumpOverAAD: scalar function must be available");
            REQUIRE(point.size() <= static_cast<size_t>(std::numeric_limits<int>::max()), "BumpOverAAD: point exceeds matrix integer range");
            REQUIRE(request.directions_.Rows() >= 0 && request.directions_.Cols() == static_cast<int>(point.size()),
                    "BumpOverAAD: directions must have nonnegative rows and one column per input");
            REQUIRE(request.steps_.size() == static_cast<size_t>(request.directions_.Rows()), "BumpOverAAD: exactly one step per direction required");
            const size_t bytes = BumpOverAADPayloadBytes(point.size(), request.steps_.size());
            REQUIRE(!request.numericPayloadBudgetBytes_ || bytes <= *request.numericPayloadBudgetBytes_,
                    "BumpOverAAD: numeric payload budget exceeded");
            for (double value : point)
                REQUIRE(std::isfinite(value), "BumpOverAAD: point components must be finite");
            for (int row = 0; row < request.directions_.Rows(); ++row)
                ValidateDirection(point, request, row);
            return bytes;
        }

        struct Gradient_ {
            double value_;
            Vector_<> gradient_;
        };

        Gradient_ EvaluateGradient(const NativeScalarFunction_& function, const Vector_<>& point, const String_& context) {
            try {
                RecordingScope_ recording;
                Vector_<Number_> inputs(point.size());
                for (size_t column = 0; column < point.size(); ++column)
                    recording.RegisterInput(inputs[column], point[column]);
                Number_ zero;
                recording.RegisterInput(zero, 0.0);
                recording.StartRecording();
                const auto output = function(&recording, inputs);
                NativeRecordedOperation_::RequireUnsegmented(&recording);
                REQUIRE(std::isfinite(Value(output)), "BumpOverAAD: scalar value must be finite");
                auto root = NativeOperations_::ActiveRoot(output, zero);
                recording.FinishRecording();
                recording.ClearAdjoints();
                NativeOperations_::SetSeed(root, 1.0);
                recording.Reverse();
                Gradient_ result{Value(output), Vector_<>(point.size())};
                for (size_t column = 0; column < point.size(); ++column) {
                    result.gradient_[column] = NativeOperations_::ReadAdjoint(inputs[column]);
                    REQUIRE(std::isfinite(result.gradient_[column]), "BumpOverAAD: gradient components must be finite");
                }
                recording.Close();
                return result;
            } catch (const Exception_& error) {
                THROW("BumpOverAAD: " + context + "; " + error.what());
            }
        }

        Vector_<> BumpedPoint(const Vector_<>& point, const BumpOverAADRequest_& request, int row, double sign) {
            Vector_<> result(point.size());
            for (int column = 0; column < request.directions_.Cols(); ++column)
                result[column] = BumpedCoordinate(point[column], request.directions_(row, column), sign * request.steps_[row]);
            return result;
        }

        double CentralQuotient(double plus, double minus, double step) {
            int gradientExponent = 0, stepExponent = 0;
            (void)std::frexp(std::max(std::abs(plus), std::abs(minus)), &gradientExponent);
            const double stepFraction = std::frexp(step, &stepExponent);
            const double difference = std::scalbn(plus, -gradientExponent) - std::scalbn(minus, -gradientExponent);
            const double result = std::scalbn(difference / stepFraction, gradientExponent - stepExponent - 1);
            REQUIRE(std::isfinite(result) && (difference == 0.0 || result != 0.0),
                    "BumpOverAAD: central quotient outside nonzero finite double range");
            return result;
        }
    } // namespace

    size_t BumpOverAADPayloadBytes(size_t inputs, size_t directions) {
        const size_t coordinates = Sum(Product(2, inputs), Product(2, Product(inputs, directions)));
        return Product(Sum(Sum(1, coordinates), directions), sizeof(double));
    }

    BumpOverAADResult_ EvaluateBumpOverAAD(const NativeScalarFunction_& function, const Vector_<>& point, const BumpOverAADRequest_& request) {
        RequireRecordingModeChangeAllowed();
        const NativeScalarFunction_ fixedFunction = function;
        Vector_<> fixedPoint = point;
        BumpOverAADRequest_ fixedRequest = request;
        BumpOverAADExecution_ execution;
        execution.numericPayloadBytes_ = Validate(fixedFunction, fixedPoint, fixedRequest);
        execution.gradientEvaluations_ = Sum(1, Product(2, fixedRequest.steps_.size()));
        execution.reverseSweeps_ = execution.gradientEvaluations_;
        RequireRecordingModeChangeAllowed();
        auto* tape = Tape();
        const NumResultsResetterForAAD_ mode(tape, tape->multi_, tape->numAdj_);
        tape->multi_ = false;
        tape->numAdj_ = 1;
        TapeCapacityBudget_ budget(fixedRequest.recordingCapacityBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
        TapeCapacityScope_ capacity(&budget, true);
        auto base = EvaluateGradient(fixedFunction, fixedPoint, "base");
        Matrix_<> products(fixedRequest.directions_.Rows(), fixedRequest.directions_.Cols(), 0.0);
        for (int row = 0; row < fixedRequest.directions_.Rows(); ++row) {
            const String_ context = "direction=" + String_(std::to_string(row));
            const auto plus = EvaluateGradient(fixedFunction, BumpedPoint(fixedPoint, fixedRequest, row, 1.0), context + "; plus");
            const auto minus = EvaluateGradient(fixedFunction, BumpedPoint(fixedPoint, fixedRequest, row, -1.0), context + "; minus");
            for (int column = 0; column < products.Cols(); ++column)
                products(row, column) = CentralQuotient(plus.gradient_[column], minus.gradient_[column], fixedRequest.steps_[row]);
        }
        execution.peakTapeBytes_ = budget.PeakCapacityBytes();
        execution.cleanupReserveBytes_ = TapeCleanupCapacityBytes();
        capacity.Close();
        return {base.value_, std::move(base.gradient_), std::move(fixedPoint), std::move(fixedRequest), std::move(products), std::move(execution)};
    }
} // namespace Dal::AAD
