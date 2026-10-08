//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include <dal/math/pde/sampledthetastep.hpp>
#include <dal/platform/platform.hpp>

#include "sampledthetastepreference.hpp"

using Dal::Matrix_;
using Dal::Vector_;
using Dal::PDE::SampledThetaStepInputs_;
using Dal::PDE::SampledThetaStepPullback_;
using namespace DalTest::SampledPDEReference;

namespace {
    void CheckStepReference(const StepReference_& reference) {
        const auto inputs = ReferenceInputs(reference);
        const int n = static_cast<int>(inputs.x_.size());
        const SampledThetaStepPullback_ step(inputs, Dal::LinearSolveAccuracyPolicy_{1e-14, 1e-14});
        CheckReferenceValues(reference.solution_, step.Solution());
        const auto risk = step.Reverse(ReferenceMatrix(n, reference.layers_, reference.seeds_));
        CheckReferenceValues(reference.oldRisk_, risk.oldValues_);
        CheckReferenceValues(reference.boundaryRisk_, risk.externalValues_);
        CheckReferenceVector(reference.rateRisk_, risk.rates_);
        CheckReferenceVector(reference.driftRisk_, risk.drifts_);
        CheckReferenceVector(reference.varianceRisk_, risk.variances_);
        ASSERT_NEAR(risk.dt_, reference.dtRisk_, 1e-10);
        ASSERT_NEAR(risk.theta_, reference.thetaRisk_, 1e-10);
        ASSERT_EQ(risk.transposeBackwardErrors_.size(), reference.layers_);
        for (double error : risk.transposeBackwardErrors_)
            ASSERT_LE(error, 1e-14);
    }

} // namespace

TEST(SampledThetaStepTest, TestIndependentNonuniformStepReferences) {
    for (int index = 0; index < 6; ++index) {
        SCOPED_TRACE(index);
        CheckStepReference(REFERENCES[index]);
    }
}

TEST(SampledThetaStepTest, TestIndependentPublicConsecutivePivotReference) { CheckStepReference(REFERENCES[6]); }
