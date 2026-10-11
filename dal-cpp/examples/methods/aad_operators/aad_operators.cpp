//
// Created by Codex on 2026/10/11.
//

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/math/aad/bumpoveraad.hpp>
#include <dal/math/aad/forwardoverreverse.hpp>
#include <dal/math/aad/implicitroot.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/structuraljacobiannative.hpp>
#include <dal/platform/initall.hpp>

#include "../../aadreport.hpp"

using namespace Dal;
using Examples::Compare;

namespace {
    void RecordedSolve() {
        AAD::RecordingScope_ recording;
        AAD::Number_ input;
        recording.RegisterInput(input, 3.0);
        recording.StartRecording();
        const SquareMatrix_<> matrix(1, 2.0);
        Matrix_<AAD::Number_> rhs(1, 1);
        rhs(0, 0) = input;
        const auto solved = AAD::LinearSolveWithAccuracy(&recording, matrix, rhs, LinearSolveAccuracyPolicy_{1e-12, 1e-12});
        AAD::Number_ objective = solved.solution_(0, 0) * solved.solution_(0, 0);
        recording.FinishRecording();
        recording.ClearAdjoints();
        AAD::NativeOperations_::SetSeed(objective, 1.0);
        const auto reports = AAD::ReverseWithSolveAccuracy(&recording);
        Compare("Solve: d(input/2)^2 / dinput", AAD::NativeOperations_::ReadAdjoint(input), 1.5);
        recording.Close();
        Compare("Solve: transpose backward error", reports.Report(solved.event_).transposeBackwardErrors_(0, 0), 0.0);
    }

    class SquareRootEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& parameters, const Vector_<>& inputs) const override {
            return {{parameters[0] * parameters[0] - inputs[0]}, SquareMatrix_<>(1, 2.0 * parameters[0]), Matrix_<>(1, 1, -1.0)};
        }
    };

    void ImplicitRoot() {
        AAD::RecordingScope_ recording;
        AAD::Number_ input;
        recording.RegisterInput(input, 4.0);
        recording.StartRecording();
        // The caller supplies a converged positive root; the operator differentiates the equation.
        const auto root = AAD::ImplicitRootWithAccuracy(&recording, SquareRootEquation_(), {2.0}, {input}, {{1e-12}, 1e-12});
        AAD::Number_ objective = 3.0 * root.parameters_[0] + input;
        recording.FinishRecording();
        recording.ClearAdjoints();
        AAD::NativeOperations_::SetSeed(objective, 1.0);
        recording.Reverse();
        Compare("Root: d(3*sqrt(x)+x) / dx", AAD::NativeOperations_::ReadAdjoint(input), 1.75);
        recording.Close();
        Compare("Root: retained equation residual", root.diagnostics_.residuals_[0], 0.0);
    }

    void StructuralJacobian() {
        const auto plan = AAD::PlanStructuralJacobian(4, {{0, 1}, {2}, {1, 3}}, {160});
        const auto mode = AAD::SetNumResultsForAAD(true, 2);
        AAD::RecordingScope_ recording;
        Vector_<AAD::Number_> inputs(4);
        for (size_t column = 0; column < inputs.size(); ++column)
            recording.RegisterInput(inputs[column], static_cast<double>(column + 1));
        recording.StartRecording();
        const auto bindings = AAD::BindStructuralJacobianInputs(&recording, inputs);
        const Vector_<AAD::Number_> outputs{2.0 * inputs[0] + 3.0 * inputs[1], 4.0 * inputs[2], 5.0 * inputs[1] + 6.0 * inputs[3]};
        recording.FinishRecording();
        const auto jacobian = AAD::ExecuteStructuralJacobian(&recording, bindings, plan, inputs, outputs);
        recording.Close();
        const double expected[3][4] = {{2, 3, 0, 0}, {0, 0, 4, 0}, {0, 5, 0, 6}};
        double error = 0.0;
        for (int row = 0; row < jacobian.Rows(); ++row)
            for (int column = 0; column < jacobian.Cols(); ++column)
                error = std::max(error, std::abs(jacobian(row, column) - expected[row][column]));
        Compare("Sparse: full Jacobian max error", error, 0.0);
        Compare("Sparse: compressed directions", double(plan.ColorCount()), 2.0);
    }

    template <class N_> N_ Quadratic(const Vector_<N_>& x) { return 3.0 * x[0] * x[0] + 2.0 * x[0] * x[1] + 5.0 * x[1] * x[1]; }

    void DirectionalCurvature() {
        Matrix_<> directions(1, 2);
        directions(0, 0) = 1.0;
        directions(0, 1) = -0.5;
        AAD::BumpOverAADRequest_ bumps;
        bumps.directions_ = directions;
        bumps.steps_ = {1e-3};
        const auto finiteStep =
            AAD::EvaluateBumpOverAAD([](AAD::RecordingScope_*, const Vector_<AAD::Number_>& x) { return Quadratic(x); }, {2.0, -1.0}, bumps);
        AAD::ForwardOverReverseRequest_ smooth;
        smooth.directions_ = directions;
        const auto mixed = AAD::EvaluateForwardOverReverse(
            [](AAD::RecordingScope_*, const Vector_<AAD::ForwardOverReverseNumber_>& x) { return Quadratic(x); }, {2.0, -1.0}, smooth);
        Compare("Finite-step HVP: x coordinate", finiteStep.HessianProducts()(0, 0), 5.0);
        Compare("Finite-step HVP: y coordinate", finiteStep.HessianProducts()(0, 1), -3.0);
        Compare("Smooth prototype HVP: x", mixed.HessianProducts()(0, 0), 5.0);
        Compare("Smooth prototype HVP: y", mixed.HessianProducts()(0, 1), -3.0);
    }
} // namespace

int main() {
    RegisterAll_::Init();
    AAD::Clear(*AAD::Tape());
    const auto mode = AAD::SetNumResultsForAAD(false, 1);
    Examples::PrintReferenceHeader("Native AAD operators: independent analytic references (raw units)");
    RecordedSolve();
    ImplicitRoot();
    StructuralJacobian();
    DirectionalCurvature();
    std::cout << "All references passed. Forward-over-reverse is an opt-in smooth prototype.\n";
    AAD::Clear(*AAD::Tape());
    return 0;
}
