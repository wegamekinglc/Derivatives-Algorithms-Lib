//
// Created by dal-implementer on 2026-6-28.
//
// Dense Cholesky decomposition micro-benchmark.
// Builds a 200x200 symmetric positive-definite dense matrix and measures
// CholeskyDecomposition plus one forward/back Solve.

#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/math/matrix/cholesky.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/math/vectors.hpp>
#include <dal/benchmarks/bench.hpp>

using namespace Dal;

namespace {
    constexpr int kSize = 200;
} // namespace

int main() {
    constexpr int kRepeats = 20;
    Bench::PrintHeader();

    // Build an SPD matrix: A = I + u u^T (rank-one perturbation of identity).
    SquareMatrix_<double> A(kSize, 0.0);
    Vector_<> u(kSize);
    for (int i = 0; i < kSize; ++i)
        u[i] = 0.1 * std::cos(static_cast<double>(i));
    for (int i = 0; i < kSize; ++i) {
        A(i, i) += 1.0;
        for (int j = 0; j < kSize; ++j)
            A(i, j) += u[i] * u[j];
    }
    Vector_<> b(kSize);
    Vector_<> x(kSize, 0.0);
    for (int i = 0; i < kSize; ++i)
        b[i] = 1.0 + 0.01 * static_cast<double>(i);

    {
        double sink = 0.0;
        auto r = Bench::Run("CholeskyDecompose (200x200)", [&]() {
            std::unique_ptr<Sparse::SymmetricDecomposition_> decomp(CholeskyDecomposition(A));
            decomp->Solve(b, &x);
            sink += x[0];
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    {
        double sink = 0.0;
        auto r = Bench::Run("CholeskyDecompose+Multiply (200x200)", [&]() {
            std::unique_ptr<Sparse::SymmetricDecomposition_> decomp(CholeskyDecomposition(A));
            decomp->Multiply(b, &x);
            sink += x[0];
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}
