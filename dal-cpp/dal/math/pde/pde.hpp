//
// Created by wegam on 2023/2/24.
//

#pragma once

#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/math/ndarray.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>

#include <bitset>
#include <functional>
#include <memory>

namespace Dal::PDE {
    class CoordinateMap_ : noncopyable {
    public:
        virtual ~CoordinateMap_() = default;
        virtual double operator()(double y, double* dxDy, double* d2xDy2) const = 0;
        [[nodiscard]] virtual double Y(double x) const = 0;
    };

    CoordinateMap_* NewSinhMap(double xWidth, double dxdyRange);
    inline CoordinateMap_* NewIdentityMap() { return NewSinhMap(1.0, 1.0); }
    CoordinateMap_* NewConcentratingMap(double xLow, double xHigh, double cPoint, double density);

    struct CoordinateVector_ {
        double yLow_;
        double yHigh_;
        int n_;
        Handle_<CoordinateMap_> yToX_;
    };

    static constexpr size_t MAX_DIMENSIONS = 3;

    class Coeff_ {
    public:
        virtual ~Coeff_() = default;
        using x_dep_t = std::bitset<MAX_DIMENSIONS>;
    };

    class MatrixCoeff_ : public Coeff_ {
    public:
        virtual void Value(const Vector_<>& x, SquareMatrix_<>* value) const = 0;
        [[nodiscard]] virtual Matrix_<x_dep_t> XDependence() const = 0;
    };
    MatrixCoeff_* NewConstCoeff(const Matrix_<>& val);
    MatrixCoeff_* NewMatrixCoeff(std::function<void(const Vector_<>&, SquareMatrix_<>*)> f, const Matrix_<Coeff_::x_dep_t>& dep);
    MatrixCoeff_* NewMatrixCoeff(std::function<double(double)> f);

    class VectorCoeff_ : public Coeff_ {
    public:
        virtual void Value(const Vector_<>& x, Vector_<>* value) const = 0;
        [[nodiscard]] virtual Vector_<x_dep_t> XDependence() const = 0;
    };
    VectorCoeff_* NewConstCoeff(const Vector_<>& val);
    VectorCoeff_* NewVectorCoeff(std::function<void(const Vector_<>&, Vector_<>*)> f, const Vector_<Coeff_::x_dep_t>& dep);
    VectorCoeff_* NewVectorCoeff(std::function<double(double)> f);

    class ScalarCoeff_ : public Coeff_ {
    public:
        virtual void Value(const Vector_<>& x, double* value) const = 0;
        [[nodiscard]] virtual x_dep_t XDependence() const = 0;
    };
    ScalarCoeff_* NewConstCoeff(double val);
    ScalarCoeff_* NewScalarCoeff(std::function<double(const Vector_<>&)> f, Coeff_::x_dep_t dep);
    ScalarCoeff_* NewScalarCoeff(std::function<double(double)> f);

    class Rollback_ {
    public:
        virtual ~Rollback_() = default;
        virtual void operator()(double dt,
                                const Vector_<CoordinateVector_>& x_points,
                                const Vector_<std::shared_ptr<Cube_<>>>& old_vals,
                                const ScalarCoeff_& discounting,
                                const VectorCoeff_& advection,
                                const MatrixCoeff_& diffusion,
                                Vector_<std::shared_ptr<Cube_<>>>* new_vals) const = 0;
    };
} // namespace Dal::PDE
