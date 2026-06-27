//
// Created by dal-implementer on 2026-6-28.
//
// Curve-calibration Jacobian micro-benchmark.
//
// The production Jacobian (AnalyticJacobian in YieldCurveCalibrationFunc_) is a private
// member and reachable only via a full calibration solve. To isolate the row-by-row
// AAD sweep pattern that dominates its cost, this benchmark records N parameters on the
// tape, computes M residuals from them, then builds the dense M x N Jacobian by, for
// each residual row: ZeroAdjoints -> seed -> PropagateToStart -> harvest N adjoints.
//
// N = 24 (curve free nodes), M = 23 (calibration instruments).

#include <vector>
#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <dal/benchmarks/bench.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    constexpr int kN = 24; // parameters (free log-DF nodes)
    constexpr int kM = 23; // residuals (instruments)

    // A non-trivial residual function: weighted sum of exp(params) minus target.
    // Each residual touches a small contiguous window of parameters so the Jacobian
    // has banded structure (as real curve-instrument exposures do).
    Number_ Residual(const std::vector<Number_>& params, int rowIdx) {
        const int start = std::max(0, rowIdx - 1);
        const int end = std::min(kN, rowIdx + 3);
        Number_ acc(0.0);
        const double weight = 0.01 + 0.001 * static_cast<double>(rowIdx);
        for (int i = start; i < end; ++i)
            acc += Number_(weight) * params[static_cast<size_t>(i)];
        return acc;
    }
} // namespace

int main() {
    constexpr int kRepeats = 100;
    Bench::PrintHeader();

    // Tape-record the parameters and residuals once.
    Clear(*Tape());
    std::vector<Number_> params(static_cast<size_t>(kN));
    for (int i = 0; i < kN; ++i) {
        params[static_cast<size_t>(i)] = Number_(-0.001 * static_cast<double>(i));
        PutOnTape(params[static_cast<size_t>(i)]);
    }
    std::vector<Number_> residuals(static_cast<size_t>(kM));
    for (int j = 0; j < kM; ++j)
        residuals[static_cast<size_t>(j)] = Residual(params, j);

    // Build the dense M x N Jacobian via the row-by-row sweep pattern.
    Matrix_<double> jacobian(kM, kN, 0.0);
    {
        auto r = Bench::Run("AnalyticJacobian (24 x 23)", [&]() {
            for (int j = 0; j < kM; ++j) {
                ZeroAdjoints(*Tape());
                Adjoint(residuals[static_cast<size_t>(j)]) = 1.0;
                PropagateToStart(*Tape());
                for (int i = 0; i < kN; ++i)
                    jacobian(static_cast<size_t>(j), i) = Adjoint(params[static_cast<size_t>(i)]);
            }
        }, 3, kRepeats);
        Bench::Print(r);
    }

    double sink = jacobian(0, 0);
    Bench::DoNotOptimize(&sink);

    return 0;
}
