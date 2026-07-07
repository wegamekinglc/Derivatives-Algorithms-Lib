//
// Created by dal-implementer on 2026/7/8.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/pde/pde.hpp>
#include <dal/utilities/exceptions.hpp>

#include <memory>

using namespace Dal;
using namespace Dal::PDE;

namespace {
    void AssertNearRelative(double actual, double expected, double tol) {
        const double scale = std::max(1.0, std::abs(expected));
        ASSERT_NEAR(actual, expected, tol * scale);
    }

    void AssertMapDerivative(const CoordinateMap_& map, double y) {
        double dxDy = 0.0;
        double d2xDy2 = 0.0;
        const double x = map(y, &dxDy, &d2xDy2);
        const double h = 1e-5;
        const double xm = map(y - h, nullptr, nullptr);
        const double xp = map(y + h, nullptr, nullptr);
        const double dxDyFd = (xp - xm) / (2.0 * h);
        const double d2xDy2Fd = (xp - 2.0 * x + xm) / (h * h);

        AssertNearRelative(dxDy, dxDyFd, 1e-8);
        AssertNearRelative(d2xDy2, d2xDy2Fd, 1e-5);
    }
} // namespace

TEST(PdeTest, TestConstCoefficientFactories) {
    std::unique_ptr<ScalarCoeff_> scalar(NewConstCoeff(0.07));
    double scalarValue = 0.0;
    scalar->Value(Vector_<>{1.0, 2.0}, &scalarValue);
    ASSERT_EQ(scalarValue, 0.07);
    ASSERT_FALSE(scalar->XDependence().any());

    const Vector_<> vectorInput{1.0, 2.0, 3.0};
    std::unique_ptr<VectorCoeff_> vector(NewConstCoeff(vectorInput));
    Vector_<> vectorValue;
    vector->Value(Vector_<>{4.0}, &vectorValue);
    ASSERT_EQ(vectorValue, vectorInput);
    ASSERT_EQ(vector->XDependence().size(), vectorInput.size());
    for (const auto& dep : vector->XDependence())
        ASSERT_FALSE(dep.any());

    Matrix_<> matrixInput(2, 2);
    matrixInput(0, 0) = 1.0;
    matrixInput(0, 1) = 0.2;
    matrixInput(1, 0) = 0.2;
    matrixInput(1, 1) = 4.0;
    std::unique_ptr<MatrixCoeff_> matrix(NewConstCoeff(matrixInput));
    SquareMatrix_<> matrixValue;
    matrix->Value(Vector_<>{9.0}, &matrixValue);
    ASSERT_EQ(matrixValue.Rows(), 2);
    ASSERT_EQ(matrixValue.Cols(), 2);
    ASSERT_EQ(matrixValue(0, 1), 0.2);
    ASSERT_EQ(matrix->XDependence().Rows(), 2);
    ASSERT_EQ(matrix->XDependence().Cols(), 2);
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            ASSERT_FALSE(matrix->XDependence()(i, j).any());

    Matrix_<> nonSquare(2, 3, 1.0);
    ASSERT_THROW(std::unique_ptr<MatrixCoeff_> bad(NewConstCoeff(nonSquare)), Exception_);
}

TEST(PdeTest, TestCallableCoefficientFactories) {
    Coeff_::x_dep_t scalarDep;
    scalarDep.set(1);
    std::unique_ptr<ScalarCoeff_> scalar(NewScalarCoeff([](const Vector_<>& x) { return x[0] + 2.0 * x[1]; }, scalarDep));
    double scalarValue = 0.0;
    scalar->Value(Vector_<>{3.0, 4.0}, &scalarValue);
    ASSERT_EQ(scalarValue, 11.0);
    ASSERT_EQ(scalar->XDependence(), scalarDep);

    Vector_<Coeff_::x_dep_t> vectorDep(2);
    vectorDep[0].set(0);
    vectorDep[1].set(1);
    std::unique_ptr<VectorCoeff_> vector(NewVectorCoeff(
        [](const Vector_<>& x, Vector_<>* out) {
            (*out)[0] = x[0] * x[0];
            (*out)[1] = x[1] + 1.0;
        },
        vectorDep));
    Vector_<> vectorValue;
    vector->Value(Vector_<>{2.0, 5.0}, &vectorValue);
    ASSERT_EQ(vectorValue.size(), 2);
    ASSERT_EQ(vectorValue[0], 4.0);
    ASSERT_EQ(vectorValue[1], 6.0);
    ASSERT_EQ(vector->XDependence(), vectorDep);

    Matrix_<Coeff_::x_dep_t> matrixDep(2, 2);
    matrixDep(0, 0).set(0);
    matrixDep(1, 1).set(1);
    std::unique_ptr<MatrixCoeff_> matrix(NewMatrixCoeff(
        [](const Vector_<>& x, SquareMatrix_<>* out) {
            (*out)(0, 0) = x[0];
            (*out)(0, 1) = 0.0;
            (*out)(1, 0) = 0.0;
            (*out)(1, 1) = x[1];
        },
        matrixDep));
    SquareMatrix_<> matrixValue;
    matrix->Value(Vector_<>{7.0, 8.0}, &matrixValue);
    ASSERT_EQ(matrixValue.Rows(), 2);
    ASSERT_EQ(matrixValue(0, 0), 7.0);
    ASSERT_EQ(matrixValue(1, 1), 8.0);
    ASSERT_EQ(matrix->XDependence().Rows(), 2);

    std::unique_ptr<ScalarCoeff_> oneDScalar(NewScalarCoeff([](double x) { return 3.0 * x; }));
    oneDScalar->Value(Vector_<>{4.0}, &scalarValue);
    ASSERT_EQ(scalarValue, 12.0);
    ASSERT_TRUE(oneDScalar->XDependence().test(0));

    std::unique_ptr<VectorCoeff_> oneDVector(NewVectorCoeff([](double x) { return x + 1.0; }));
    oneDVector->Value(Vector_<>{4.0}, &vectorValue);
    ASSERT_EQ(vectorValue.size(), 1);
    ASSERT_EQ(vectorValue[0], 5.0);

    std::unique_ptr<MatrixCoeff_> oneDMatrix(NewMatrixCoeff([](double x) { return x * x; }));
    oneDMatrix->Value(Vector_<>{4.0}, &matrixValue);
    ASSERT_EQ(matrixValue.Rows(), 1);
    ASSERT_EQ(matrixValue.Cols(), 1);
    ASSERT_EQ(matrixValue(0, 0), 16.0);

    Matrix_<Coeff_::x_dep_t> nonSquareDep(2, 3);
    ASSERT_THROW(std::unique_ptr<MatrixCoeff_> bad(NewMatrixCoeff([](const Vector_<>&, SquareMatrix_<>*) {}, nonSquareDep)), Exception_);
}

TEST(PdeTest, TestCoordinateMapsRoundTripAndDerivatives) {
    std::unique_ptr<CoordinateMap_> identity(NewIdentityMap());
    for (double y : Vector_<>{-2.0, -0.5, 0.0, 0.5, 2.0}) {
        double dxDy = 0.0;
        double d2xDy2 = 0.0;
        const double x = (*identity)(y, &dxDy, &d2xDy2);
        ASSERT_EQ(x, y);
        ASSERT_EQ(identity->Y(x), y);
        ASSERT_EQ(dxDy, 1.0);
        ASSERT_EQ(d2xDy2, 0.0);
    }

    std::unique_ptr<CoordinateMap_> sinh(NewSinhMap(4.0, 3.0));
    for (double y : Vector_<>{-2.0, -0.25, 0.0, 0.25, 2.0}) {
        const double x = (*sinh)(y, nullptr, nullptr);
        ASSERT_NEAR(sinh->Y(x), y, 1e-10);
        AssertMapDerivative(*sinh, y);
    }

    for (double density : Vector_<>{0.01, 0.1, 1.0, 10.0}) {
        std::unique_ptr<CoordinateMap_> concentrating(NewConcentratingMap(0.0, 5.0, 2.5, density));
        ASSERT_EQ((*concentrating)(0.0, nullptr, nullptr), 0.0);
        ASSERT_EQ((*concentrating)(1.0, nullptr, nullptr), 5.0);
        ASSERT_EQ(concentrating->Y(0.0), 0.0);
        ASSERT_EQ(concentrating->Y(5.0), 1.0);
        for (double y : Vector_<>{0.05, 0.25, 0.5, 0.75, 0.95}) {
            const double x = (*concentrating)(y, nullptr, nullptr);
            ASSERT_NEAR(concentrating->Y(x), y, 1e-10);
            AssertMapDerivative(*concentrating, y);
        }
    }

    ASSERT_THROW(std::unique_ptr<CoordinateMap_> bad(NewConcentratingMap(5.0, 0.0, 2.5, 1.0)), Exception_);
    ASSERT_THROW(std::unique_ptr<CoordinateMap_> bad(NewConcentratingMap(0.0, 5.0, 6.0, 1.0)), Exception_);
    ASSERT_THROW(std::unique_ptr<CoordinateMap_> bad(NewConcentratingMap(0.0, 5.0, 2.5, 0.0)), Exception_);
}
