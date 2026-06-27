//
// Created by dal-implementer on 2026-6-28.
//
// Conjugate-gradient (CG) Krylov-solver micro-benchmark.
// Builds a 500x500 symmetric positive-definite tri-diagonal matrix (strictly
// diagonally dominant) and runs CGSolve with a 200-iteration budget.

#include <dal/platform/platform.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/bcg.hpp>
#include <dal/math/vectors.hpp>
#include <dal/benchmarks/bench.hpp>

using namespace Dal;
using Dal::Sparse::TriDiagonal_;

namespace {
    constexpr int kSize = 500;
    constexpr int kMaxIters = 200;
} // namespace

int main() {
    constexpr int kRepeats = 20;
    Bench::PrintHeader();

    // Build an SPD tri-diagonal system A x = b with known solution.
    TriDiagonal_ A(kSize);
    for (int i = 0; i < kSize; ++i) {
        A.Set(i, i, 4.0); // dominant diagonal ensures SPD + diagonal dominance
        if (i > 0)
            A.Set(i, i - 1, -1.0);
        if (i + 1 < kSize)
            A.Set(i, i + 1, -1.0);
    }
    Vector_<> xTrue(kSize);
    Vector_<> b(kSize);
    for (int i = 0; i < kSize; ++i) {
        xTrue[i] = 1.0 + 0.01 * static_cast<double>(i);
        b[i] = 0.0;
    }
    A.MultiplyLeft(xTrue, &b);

    {
        double sink = 0.0;
        auto r = Bench::Run("CGSolve (500x500 tridiag)", [&]() {
            Vector_<> x(kSize, 0.0); // zero initial guess
            Sparse::CGSolve(A, b, 1e-10, 1e-12, kMaxIters, &x);
            sink += x[0];
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    {
        double sink = 0.0;
        auto r = Bench::Run("BCGSolve (500x500 tridiag)", [&]() {
            Vector_<> x(kSize, 0.0);
            Sparse::BCGSolve(A, b, 1e-10, 1e-12, kMaxIters, &x);
            sink += x[0];
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}
