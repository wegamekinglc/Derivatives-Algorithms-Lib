//
// Created by dal-implementer on 2026-7-4.
//
// Production Dal::Matrix kernel micro-benchmark.

#include <memory>

#include <dal/platform/platform.hpp>

#include <dal/benchmarks/bench.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/numerics.hpp>

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
    constexpr int REPEATS = 10;
    Bench::PrintHeader();

    for (const int n : {5, 10, 16, 200, 500}) {
        const int innerLoops = n <= 16 ? 1000 : 1;
        Matrix_<> a = RandomMatrix(n, n, 1000);
        Matrix_<> b = RandomMatrix(n, n, 2000);
        Matrix_<> c(n, n, 0.0);
        Vector_<> v(n);
        for (int i = 0; i < n; ++i)
            v[i] = 0.1 * static_cast<double>(i);

        {
            double sink = 0.0;
            auto r = Bench::Run(
                "Dal::Matrix::Multiply (" + std::to_string(n) + "x" + std::to_string(n) + ")", [&]() { Dal::Matrix::Multiply(a, b, &c); }, 1, REPEATS,
                innerLoops);
            sink += c(0, 0);
            Bench::Print(r);
            Bench::DoNotOptimize(&sink);
        }

        {
            double sink = 0.0;
            Matrix_<> h(n, n, 0.0);
            auto r = Bench::Run(
                "Dal::Matrix::AddJSquaredToUpper (" + std::to_string(n) + "x" + std::to_string(n) + ")",
                [&]() {
                    for (int i = 0; i < n; ++i)
                        for (int j = 0; j < n; ++j)
                            h(i, j) = 0.0;
                    Dal::Matrix::AddJSquaredToUpper(a, &h);
                },
                1, REPEATS, innerLoops);
            sink += h(0, 0);
            Bench::Print(r);
            Bench::DoNotOptimize(&sink);
        }

        {
            double sink = 0.0;
            auto r = Bench::Run(
                "Dal::Matrix::WeightedInnerProduct (" + std::to_string(n) + ")", [&]() { sink += Dal::Matrix::WeightedInnerProduct(v, a, v); }, 2,
                REPEATS, innerLoops);
            Bench::Print(r);
            Bench::DoNotOptimize(&sink);
        }

        {
            double sink = 0.0;
            Vector_<> mv(n);
            auto r = Bench::Run(
                "Dal::Matrix::Multiply vector x matrix (" + std::to_string(n) + ")", [&]() { Dal::Matrix::Multiply(v, a, &mv); }, 5, REPEATS,
                innerLoops);
            sink += mv[0];
            Bench::Print(r);
            Bench::DoNotOptimize(&sink);
        }

        {
            double sink = 0.0;
            auto r =
                Bench::Run("Dal::InnerProduct vector x vector (" + std::to_string(n) + ")", [&]() { sink += InnerProduct(v, v); }, 2, REPEATS, 1000);
            Bench::Print(r);
            Bench::DoNotOptimize(&sink);
        }

        {
            double sink = 0.0;
            auto r = Bench::Run(
                "Dal::InnerProduct mutable row x vector (" + std::to_string(n) + ")", [&]() { sink += InnerProduct(a.Row(0), v); }, 2, REPEATS, 1000);
            Bench::Print(r);
            Bench::DoNotOptimize(&sink);
        }
    }

    for (const int cols : {17, 33, 200}) {
        Matrix_<> a = RandomMatrix(16, cols, 3000);
        Matrix_<> b = RandomMatrix(cols, 10, 4000);
        Matrix_<> c(16, 10, 0.0);
        auto r =
            Bench::Run("Dal::Matrix::Multiply (16x" + std::to_string(cols) + "x10)", [&]() { Dal::Matrix::Multiply(a, b, &c); }, 1, REPEATS, 100);
        Bench::Print(r);
        Bench::DoNotOptimize(c.Data());
    }

    return 0;
}
