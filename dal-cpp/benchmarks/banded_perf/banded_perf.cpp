//
// Created by dal-implementer on 2026-6-28.
//
// Banded tri-diagonal matrix-vector multiply micro-benchmark.
// Builds a TriDiagonal_ of size 10K and measures the tri mat-vec path that the
// PDE solver exercises (TriDecomp_::XMultiplyLeft_af, reached via the public
// SquareMatrixDecomposition_::MultiplyLeft on the decomposition).

#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/vectors.hpp>
#include <dal/benchmarks/bench.hpp>

using namespace Dal;
using Dal::Sparse::TriDiagonal_;

namespace {
    constexpr int kSize = 10000;
} // namespace

int main() {
    constexpr int kMulRepeats = 1000;
    // Each multiply is ~8us; time ~100 per rep so one timed rep clears the ~600us stability
    // floor instead of being dominated by scheduler transients.
    constexpr int kMulInnerLoops = 100;
    constexpr int kDecomposeRepeats = 100;
    Bench::PrintHeader();

    TriDiagonal_ A(kSize);
    for (int i = 0; i < kSize; ++i) {
        A.Set(i, i, 2.0);
        if (i > 0)
            A.Set(i, i - 1, -1.0);
        if (i + 1 < kSize)
            A.Set(i, i + 1, -1.0);
    }
    Vector_<> x(kSize);
    Vector_<> b(kSize, 0.0);
    for (int i = 0; i < kSize; ++i)
        x[i] = 1.0 / static_cast<double>(i + 1);

    // Decompose once; measure the tri mat-vec through the decomposition.
    std::unique_ptr<SquareMatrixDecomposition_> decomp(A.Decompose());

    {
        double sink = 0.0;
        auto r = Bench::Run("TriDecomp MultiplyLeft (10K)", [&]() {
            decomp->MultiplyLeft(x, &b);
            sink += b[0];
        }, 3, kMulRepeats, kMulInnerLoops);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    // Raw tri mat-vec on the matrix itself (no decomposition).
    {
        double sink = 0.0;
        auto r = Bench::Run("TriDiagonal MultiplyLeft (10K)", [&]() {
            A.MultiplyLeft(x, &b);
            sink += b[0];
        }, 3, kMulRepeats, kMulInnerLoops);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    // Tri-diagonal LU decomposition cost (the PDE per-time-step setup).
    {
        auto r = Bench::Run("TriDiagonal Decompose (10K)", [&]() {
            std::unique_ptr<SquareMatrixDecomposition_> d(A.Decompose());
            Bench::DoNotOptimize(d.get());
        }, 3, kDecomposeRepeats);
        Bench::Print(r);
    }

    return 0;
}
