//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>

#include "../pde/sampledthetastepreference.hpp"
#include "sampledthetastepfixture.hpp"

using namespace Dal;
using namespace Dal::AAD;
using namespace DalTest::SampledPDEReference;

namespace {
    template <class F_> Matrix_<> ReadMatrix(const Matrix_<Number_>& values, const F_& valueOf) {
        Matrix_<> result(values.Rows(), values.Cols());
        for (int row = 0; row < result.Rows(); ++row)
            for (int column = 0; column < result.Cols(); ++column)
                result(row, column) = valueOf(values(row, column));
        return result;
    }

    Vector_<> ReadRisks(const Vector_<Number_>& values) {
        Vector_<> result(values.size());
        for (size_t index = 0; index < result.size(); ++index)
            result[index] = NativeOperations_::ReadAdjoint(values[index]);
        return result;
    }

    void CheckNativeReference(const StepReference_& reference) {
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(false, 1);
        RecordingScope_ scope;
        const auto numeric = ReferenceInputs(reference);
        const auto active = DalTest::NativePDE::RegisterBindings(&scope, numeric);
        scope.StartRecording();
        auto step = SampledThetaStepWithAccuracy(&scope, numeric, active, LinearSolveAccuracyPolicy_{1e-14, 1e-14});
        scope.FinishRecording();
        scope.ClearAdjoints();
        const auto seeds = ReferenceMatrix(step.solution_.Rows(), step.solution_.Cols(), reference.seeds_);
        for (int row = 0; row < seeds.Rows(); ++row)
            for (int layer = 0; layer < seeds.Cols(); ++layer)
                NativeOperations_::SetSeed(step.solution_(row, layer), seeds(row, layer));
        const auto reports = ReverseWithSolveAccuracy(&scope);
        const auto readRisk = [](const Number_& value) { return NativeOperations_::ReadAdjoint(value); };
        ASSERT_NO_FATAL_FAILURE(
            CheckReferenceValues(reference.solution_, ReadMatrix(step.solution_, [](const Number_& value) { return Value(value); })));
        ASSERT_NO_FATAL_FAILURE(CheckReferenceValues(reference.oldRisk_, ReadMatrix(active.oldValues_, readRisk)));
        ASSERT_NO_FATAL_FAILURE(CheckReferenceValues(reference.boundaryRisk_, ReadMatrix(active.externalValues_, readRisk)));
        ASSERT_NO_FATAL_FAILURE(CheckReferenceVector(reference.rateRisk_, ReadRisks(active.rates_)));
        ASSERT_NO_FATAL_FAILURE(CheckReferenceVector(reference.driftRisk_, ReadRisks(active.drifts_)));
        ASSERT_NO_FATAL_FAILURE(CheckReferenceVector(reference.varianceRisk_, ReadRisks(active.variances_)));
        ASSERT_NEAR(NativeOperations_::ReadAdjoint(*active.dt_), reference.dtRisk_, 1e-10);
        ASSERT_NEAR(NativeOperations_::ReadAdjoint(*active.theta_), reference.thetaRisk_, 1e-10);
        const auto& errors = reports.Report(step.event_).transposeBackwardErrors_;
        ASSERT_EQ(errors.Rows(), reference.layers_);
        ASSERT_EQ(errors.Cols(), 1);
        for (double error : errors)
            ASSERT_LE(error, 1e-14);
        scope.Close();
        Clear(*Tape());
    }
} // namespace

TEST(AADSampledThetaStepTest, TestIndependentNonuniformFrozenStepReferences) {
    for (int index = 0; index < 6; ++index) {
        SCOPED_TRACE(index);
        ASSERT_NO_FATAL_FAILURE(CheckNativeReference(REFERENCES[index]));
    }
}

TEST(AADSampledThetaStepTest, TestIndependentConsecutivePivotFrozenStepReference) { ASSERT_NO_FATAL_FAILURE(CheckNativeReference(REFERENCES[6])); }
