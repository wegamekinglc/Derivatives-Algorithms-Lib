//
// Created by dal-implementer on 2026/7/8.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/pde/pdegrid.hpp>
#include <dal/math/pde/pdeoperators.hpp>
#include <dal/utilities/exceptions.hpp>

#include <memory>

using namespace Dal;
using namespace Dal::PDE;

namespace {
    Vector_<> LinearValues(const Vector_<>& x, double a, double b) {
        Vector_<> f(x.size());
        for (int i = 0; i < static_cast<int>(x.size()); ++i)
            f[i] = a + b * x[i];
        return f;
    }

    Vector_<> QuadraticValues(const Vector_<>& x, double a, double b, double c) {
        Vector_<> f(x.size());
        for (int i = 0; i < static_cast<int>(x.size()); ++i)
            f[i] = a + b * x[i] + c * x[i] * x[i];
        return f;
    }

    void AssertOperatorsExactOnGrid(const Vector_<>& x, double dxxTolerance) {
        std::unique_ptr<Sparse::TriDiagonal_> dx(NewDx(x));
        std::unique_ptr<Sparse::TriDiagonal_> dxx(NewDxx(x));

        ASSERT_EQ((*dx)(0, 0), 0.0);
        ASSERT_EQ((*dx)(static_cast<int>(x.size()) - 1, static_cast<int>(x.size()) - 1), 0.0);
        ASSERT_EQ((*dxx)(0, 0), 0.0);
        ASSERT_EQ((*dxx)(static_cast<int>(x.size()) - 1, static_cast<int>(x.size()) - 1), 0.0);

        Vector_<> result;
        const Vector_<> linear = LinearValues(x, 1.25, -0.7);
        dx->MultiplyLeft(linear, &result);
        for (int i = 1; i < static_cast<int>(x.size()) - 1; ++i)
            ASSERT_NEAR(result[i], -0.7, 1e-12);

        const Vector_<> quadratic = QuadraticValues(x, -3.0, 0.5, 2.25);
        dx->MultiplyLeft(quadratic, &result);
        for (int i = 1; i < static_cast<int>(x.size()) - 1; ++i)
            ASSERT_NEAR(result[i], 0.5 + 2.0 * 2.25 * x[i], 1e-12);

        dxx->MultiplyLeft(quadratic, &result);
        for (int i = 1; i < static_cast<int>(x.size()) - 1; ++i)
            ASSERT_NEAR(result[i], 2.0 * 2.25, dxxTolerance);
    }
} // namespace

TEST(PdeOperatorsTest, TestOperatorsExactOnUniformGrid) { AssertOperatorsExactOnGrid(GridLocations(MakeUniformGrid(0.0, 10.0, 41)), 1e-12); }

TEST(PdeOperatorsTest, TestOperatorsExactOnConcentratingGrid) {
    AssertOperatorsExactOnGrid(GridLocations(MakeConcentratingGrid(0.0, 10.0, 41, 4.0, 0.2)), 2e-12);
}

TEST(PdeOperatorsTest, TestOperatorValidation) {
    ASSERT_THROW(std::unique_ptr<Sparse::TriDiagonal_> bad(NewDx(Vector_<>{0.0, 1.0})), Exception_);
    ASSERT_THROW(std::unique_ptr<Sparse::TriDiagonal_> bad(NewDxx(Vector_<>{0.0, 1.0})), Exception_);
    ASSERT_THROW(std::unique_ptr<Sparse::TriDiagonal_> bad(NewDx(Vector_<>{0.0, 1.0, 1.0})), Exception_);
    ASSERT_THROW(std::unique_ptr<Sparse::TriDiagonal_> bad(NewDxx(Vector_<>{0.0, 2.0, 1.0})), Exception_);
}
