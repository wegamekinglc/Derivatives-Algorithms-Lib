//
// Created by dal-implementer on 2026-7-4.
//
// Production Dal::Matrix kernel micro-benchmark.
// The previous incarnation of this target benchmarked a hand-rolled local Matrix_
// class for self-comparison of five matmul variants -- it did NOT exercise any
// Dal::Matrix::* kernel the calibration solver uses (G8 phantom). This version
// times Dal::Matrix::Multiply, AddJSquaredToUpper, and WeightedInnerProduct at
// 200x200 and 500x500 (matching cholesky_perf / krylov_perf sizes) -- the kernels
// consumed by Underdetermined::Find and the curve-Jacobian assembly.
//
// Sparse::SymmetricDecomposition_::QForm (also named in G8) is not exercised here:
// the dense Cholesky path's XSolve_af does not pre-size its output vector, so the
// QForm wrapper's empty Vector_<> wij triggers an out-of-bounds write. That is a
// pre-existing latent bug in dal-cpp/dal/math/matrix/sparse.cpp, outside the
// benchmark-only scope of this change. Production calibration hits the banded
// QForm override (which pre-sizes), not this path.

#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/vectors.hpp>
#include <dal/benchmarks/bench.hpp>

using namespace Dal;

namespace {
    // Build a dense Matrix_<> filled with quasi-normal deviates (deterministic across runs).
    Matrix_<> RandomMatrix(int rows, int cols, int seed) {
        std::unique_ptr<Random_> rsg(NewSobol(cols, seed, /*precise=*/false));
        Matrix_<> m(rows, cols, 0.0);
        Vector_<> row(cols);
        for (int i = 0; i < rows; ++i) {
            rsg->FillNormal(&row);
            for (int j = 0; j < cols; ++j)
                m(i, j) = row[j];
        }
        return m;
    }
} // namespace

int main() {
    constexpr int kRepeats = 10;
    Bench::PrintHeader();

    for (const int n : {200, 500}) {
        Matrix_<> a = RandomMatrix(n, n, 1000);
        Matrix_<> b = RandomMatrix(n, n, 2000);
        Matrix_<> c(n, n, 0.0);
        Vector_<> v(n);
        for (int i = 0; i < n; ++i)
            v[i] = 0.1 * static_cast<double>(i);

        {
            double sink = 0.0;
            auto r = Bench::Run("Dal::Matrix::Multiply (" + std::to_string(n) + "x" + std::to_string(n) + ")",
                                [&]() { Dal::Matrix::Multiply(a, b, &c); }, 1, kRepeats);
            sink += c(0, 0);
            Bench::Print(r);
            Bench::DoNotOptimize(&sink);
        }

        {
            double sink = 0.0;
            Matrix_<> h(n, n, 0.0);
            auto r = Bench::Run("Dal::Matrix::AddJSquaredToUpper (" + std::to_string(n) + "x" + std::to_string(n) + ")",
                                [&]() {
                                    for (int i = 0; i < n; ++i)
                                        for (int j = 0; j < n; ++j)
                                            h(i, j) = 0.0;
                                    Dal::Matrix::AddJSquaredToUpper(a, &h);
                                },
                                1, kRepeats);
            sink += h(0, 0);
            Bench::Print(r);
            Bench::DoNotOptimize(&sink);
        }

        {
            double sink = 0.0;
            auto r = Bench::Run("Dal::Matrix::WeightedInnerProduct (" + std::to_string(n) + ")",
                                [&]() { sink += Dal::Matrix::WeightedInnerProduct(v, a, v); }, 2, kRepeats);
            Bench::Print(r);
            Bench::DoNotOptimize(&sink);
        }
    }

    return 0;
}
