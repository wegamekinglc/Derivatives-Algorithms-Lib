#include <cmath>
#include <limits>

#include <dal/math/aad/detail/gradientbumps.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/profiling.hpp>
#include <dal/math/aad/reverseevent.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/platform/platform.hpp>

namespace Dal::AAD {
    namespace {
        using GradientBumpsDetail::Product;
        using GradientBumpsDetail::Sum;

        struct Gradient_ {
            double value_;
            Vector_<> gradient_;
            [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        };

        Gradient_ EvaluateGradient(const NativeScalarFunction_& function, const Vector_<>& point, const String_& context) {
            try {
                using Phase = AADProfilingPhase_::Value_;
                ProfilingSpan_ setup(Phase::PREPARE);
                RecordingScope_ recording;
                Vector_<Number_> inputs(point.size());
                for (size_t column = 0; column < point.size(); ++column)
                    recording.RegisterInput(inputs[column], point[column]);
                Number_ zero;
                recording.RegisterInput(zero, 0.0);
                recording.StartRecording();
                setup.Finish();
                ProfilingSpan_ price(Phase::PATH_FORWARD);
                const auto output = function(&recording, inputs);
                price.Finish();
                ProfilingSpan_ seed(Phase::PREPARE);
                NativeRecordedOperation_::RequireUnsegmented(&recording);
                REQUIRE(std::isfinite(Value(output)), "BumpOverAAD: scalar value must be finite");
                auto root = NativeOperations_::ActiveRoot(output, zero);
                recording.FinishRecording();
                recording.ClearAdjoints();
                NativeOperations_::SetSeed(root, 1.0);
                seed.Finish();
#if defined(DAL_ENABLE_AAD_PROFILING)
                CaptureReverseGraphForProfiling(1);
#endif
                ProfilingSpan_ reverse(Phase::REVERSE);
                recording.Reverse();
                reverse.Finish();
                ProfilingSpan_ pack(Phase::REDUCE);
                Gradient_ result{Value(output), Vector_<>(point.size())};
                for (size_t column = 0; column < point.size(); ++column) {
                    result.gradient_[column] = NativeOperations_::ReadAdjoint(inputs[column]);
                    REQUIRE(std::isfinite(result.gradient_[column]), "BumpOverAAD: gradient components must be finite");
                }
                pack.Finish();
                const ProfilingSpan_ cleanup(Phase::PREPARE);
                recording.Close();
                return result;
            } catch (const Exception_& error) {
                THROW("BumpOverAAD: " + context + "; " + error.what());
            }
        }

    } // namespace

    size_t BumpOverAADPayloadBytes(size_t inputs, size_t directions) {
        const size_t coordinates = Sum(Product(2, inputs), Product(2, Product(inputs, directions)));
        return Product(Sum(Sum(1, coordinates), directions), sizeof(double));
    }

    BumpOverAADResult_ EvaluateBumpOverAAD(const NativeScalarFunction_& function, const Vector_<>& point, const BumpOverAADRequest_& request) {
        using Phase = AADProfilingPhase_::Value_;
        ProfilingSpan_ setup(Phase::PREPARE);
        RequireRecordingModeChangeAllowed();
        const NativeScalarFunction_ fixedFunction = function;
        Vector_<> fixedPoint = point;
        BumpOverAADRequest_ fixedRequest = request;
        BumpOverAADExecution_ execution;
        REQUIRE(static_cast<bool>(fixedFunction), "BumpOverAAD: scalar function must be available");
        execution.numericPayloadBytes_ = GradientBumpsDetail::Validate(fixedPoint, fixedRequest);
        execution.gradientEvaluations_ = Sum(1, Product(2, fixedRequest.steps_.size()));
        execution.reverseSweeps_ = execution.gradientEvaluations_;
        RequireRecordingModeChangeAllowed();
        auto* tape = Tape();
        const NumResultsResetterForAAD_ mode(tape, tape->multi_, tape->numAdj_);
        tape->multi_ = false;
        tape->numAdj_ = 1;
        TapeCapacityBudget_ budget(fixedRequest.recordingCapacityBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
        TapeCapacityScope_ capacity(&budget, true);
        setup.Finish();
        auto result = GradientBumpsDetail::Evaluate(
            [&](const Vector_<>& inputs, const String_& context) { return EvaluateGradient(fixedFunction, inputs, context); }, fixedPoint,
            fixedRequest);
        ProfilingSpan_ cleanup(Phase::PREPARE);
        execution.peakTapeBytes_ = budget.PeakCapacityBytes();
        execution.cleanupReserveBytes_ = TapeCleanupCapacityBytes();
        capacity.Close();
        cleanup.Finish();
        const ProfilingSpan_ pack(Phase::REDUCE);
        return {result.base_.value_,     std::move(result.base_.gradient_), std::move(fixedPoint),
                std::move(fixedRequest), std::move(result.products_),       std::move(execution)};
    }
} // namespace Dal::AAD
