//
// Created by dal-implementer on 2026/6/28.
//
// Curve-calibration Jacobian micro-benchmark.
//
// The production Jacobian (AnalyticJacobian in YieldCurveCalibrationFunc_) is a private member
// reachable only via a full calibration solve. To isolate the row-by-row AAD sweep that builds it,
// this benchmark records N parameters on the tape, computes M residuals from them, then builds the
// dense M x N Jacobian via, for each residual row: ZeroAdjoints -> seed -> PropagateToStart ->
// harvest N adjoints.
//
// The Jacobian is lower-triangular by maturity: each residual touches only a leading window of
// parameters, so columns at or beyond that window are structural zeros. The "dense harvest" case
// reads all N adjoints per row; the "row-width harvest" case reads only the leading RowWidth(j)
// adjoints and leaves the trailing zeros at their 0.0 fill. This remains a synthetic comparison:
// curve calibration uses full-width harvesting because payment lags and date adjustments make a
// nominal-maturity prefix unsafe.
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

    // A non-trivial residual function: weighted sum of params over a small contiguous window.
    // Each residual touches a leading window of parameters so the Jacobian has the lower-triangular
    // structure real curve-instrument exposures do.
    Number_ Residual(const std::vector<Number_>& params, int rowIdx) {
        const int start = std::max(0, rowIdx - 1);
        const int end = std::min(kN, rowIdx + 3);
        Number_ acc(0.0);
        const double weight = 0.01 + 0.001 * static_cast<double>(rowIdx);
        for (int i = start; i < end; ++i)
            acc += Number_(weight) * params[static_cast<size_t>(i)];
        return acc;
    }

    // Row width = the last parameter this synthetic residual touches + 1. The benchmark can prove
    // this prefix from Residual() itself; production curve residuals do not make that assumption.
    int RowWidth(int rowIdx) { return std::min(kN, rowIdx + 3); }
} // namespace

int main() {
    constexpr int kRepeats = 100;
    // Each harvest body runs all 24 rows (~7-9us): time ~100 bodies per rep so one timed rep
    // clears the ~600us stability floor -- single-iteration timing left these cases in the
    // transient-dominated regime and tripped the paired gate as min-statistic noise.
    constexpr int kHarvestInnerLoops = 100;
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

    // Dense harvest: read all N adjoints per row (the pre-optimization pattern).
    Matrix_<double> jacobianDense(kM, kN, 0.0);
    {
        auto r = Bench::Run("AnalyticJacobian dense harvest (24 x 23)", [&]() {
            for (int j = 0; j < kM; ++j) {
                ZeroAdjoints(*Tape());
                Adjoint(residuals[static_cast<size_t>(j)]) = 1.0;
                PropagateToStart(*Tape());
                for (int i = 0; i < kN; ++i)
                    jacobianDense(static_cast<size_t>(j), i) = Adjoint(params[static_cast<size_t>(i)]);
            }
        }, 3, kRepeats, kHarvestInnerLoops);
        Bench::Print(r);
    }

    // Row-width harvest: read only the provably active leading prefix for this synthetic graph.
    Matrix_<double> jacobianSparse(kM, kN, 0.0);
    {
        auto r = Bench::Run("AnalyticJacobian row-width harvest (24 x 23)", [&]() {
            for (int j = 0; j < kM; ++j) {
                const int width = RowWidth(j);
                ZeroAdjoints(*Tape());
                Adjoint(residuals[static_cast<size_t>(j)]) = 1.0;
                PropagateToStart(*Tape());
                for (int i = 0; i < width; ++i)
                    jacobianSparse(static_cast<size_t>(j), i) = Adjoint(params[static_cast<size_t>(i)]);
            }
        }, 3, kRepeats, kHarvestInnerLoops);
        Bench::Print(r);
    }

    double sink = jacobianDense(0, 0) + jacobianSparse(0, 0);
    Bench::DoNotOptimize(&sink);

    return 0;
}
