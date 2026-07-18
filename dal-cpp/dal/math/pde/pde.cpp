//
// Created by wegam on 2023/2/24.
//

#include <dal/math/pde/pde.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal::PDE {
    namespace {
        struct IdentityMap_ : CoordinateMap_ {
            double operator()(double y, double* xp, double* xpp) const override {
                ASSIGN(xp, 1.0);
                ASSIGN(xpp, 0.0);
                return y;
            }
            [[nodiscard]] double Y(double x) const override { return x; }
        };

        struct SinhMap_ : CoordinateMap_ {
            // x = \lambda sinh(y / \lambda)
            double lambda_;
            explicit SinhMap_(double lambda) : lambda_(lambda) {}
            double operator()(double y, double* xp, double* xpp) const override {
                ASSIGN(xp, std::cosh(y / lambda_));
                ASSIGN(xpp, std::sinh(y / lambda_) / lambda_);
                return lambda_ * std::sinh(y / lambda_);
            }
            [[nodiscard]] double Y(double x) const override { return lambda_ * std::asinh(x / lambda_); }
        };

        struct ConcentratingMap_ : CoordinateMap_ {
            double xLow_;
            double xHigh_;
            double cPoint_;
            double rho_;
            double c1_;
            double c2_;

            ConcentratingMap_(double xLow, double xHigh, double cPoint, double density)
                : xLow_(xLow), xHigh_(xHigh), cPoint_(cPoint), rho_(density * (xHigh - xLow)), c1_(std::asinh((xLow - cPoint) / rho_)),
                  c2_(std::asinh((xHigh - cPoint) / rho_)) {}

            double operator()(double y, double* dxDy, double* d2xDy2) const override {
                const double span = c2_ - c1_;
                const double arg = c1_ * (1.0 - y) + c2_ * y;
                ASSIGN(dxDy, rho_ * span * std::cosh(arg));
                ASSIGN(d2xDy2, rho_ * span * span * std::sinh(arg));
                if (y == 0.0)
                    return xLow_;
                if (y == 1.0)
                    return xHigh_;
                return cPoint_ + rho_ * std::sinh(arg);
            }

            [[nodiscard]] double Y(double x) const override {
                if (x == xLow_)
                    return 0.0;
                if (x == xHigh_)
                    return 1.0;
                return (std::asinh((x - cPoint_) / rho_) - c1_) / (c2_ - c1_);
            }
        };

        struct ConstScalarCoeff_ : ScalarCoeff_ {
            double val_;
            explicit ConstScalarCoeff_(double val) : val_(val) {}

            void Value(const Vector_<>&, double* value) const override { ASSIGN(value, val_); }
            [[nodiscard]] x_dep_t XDependence() const override { return x_dep_t(); }
        };

        struct ConstVectorCoeff_ : VectorCoeff_ {
            Vector_<> val_;
            explicit ConstVectorCoeff_(const Vector_<>& val) : val_(val) {}

            void Value(const Vector_<>&, Vector_<>* value) const override {
                REQUIRE(value != nullptr, "coefficient value output must be non-null");
                *value = val_;
            }

            [[nodiscard]] Vector_<x_dep_t> XDependence() const override { return Vector_<x_dep_t>(val_.size(), x_dep_t()); }
        };

        struct ConstMatrixCoeff_ : MatrixCoeff_ {
            Matrix_<> val_;
            explicit ConstMatrixCoeff_(const Matrix_<>& val) : val_(val) {}

            void Value(const Vector_<>&, SquareMatrix_<>* value) const override {
                REQUIRE(value != nullptr, "coefficient value output must be non-null");
                value->Resize(val_.Rows());
                for (int i = 0; i < val_.Rows(); ++i)
                    for (int j = 0; j < val_.Cols(); ++j)
                        (*value)(i, j) = val_(i, j);
            }

            [[nodiscard]] Matrix_<x_dep_t> XDependence() const override { return Matrix_<x_dep_t>(val_.Rows(), val_.Cols(), x_dep_t()); }
        };

        struct CallableScalarCoeff_ : ScalarCoeff_ {
            std::function<double(const Vector_<>&)> f_;
            x_dep_t dep_;

            CallableScalarCoeff_(std::function<double(const Vector_<>&)> f, x_dep_t dep) : f_(std::move(f)), dep_(dep) {}

            void Value(const Vector_<>& x, double* value) const override {
                REQUIRE(value != nullptr, "coefficient value output must be non-null");
                *value = f_(x);
            }

            [[nodiscard]] x_dep_t XDependence() const override { return dep_; }
        };

        struct CallableVectorCoeff_ : VectorCoeff_ {
            std::function<void(const Vector_<>&, Vector_<>*)> f_;
            Vector_<x_dep_t> dep_;

            CallableVectorCoeff_(std::function<void(const Vector_<>&, Vector_<>*)> f, const Vector_<x_dep_t>& dep) : f_(std::move(f)), dep_(dep) {}

            void Value(const Vector_<>& x, Vector_<>* value) const override {
                REQUIRE(value != nullptr, "coefficient value output must be non-null");
                value->Resize(dep_.size());
                f_(x, value);
            }

            [[nodiscard]] Vector_<x_dep_t> XDependence() const override { return dep_; }
        };

        struct CallableMatrixCoeff_ : MatrixCoeff_ {
            std::function<void(const Vector_<>&, SquareMatrix_<>*)> f_;
            Matrix_<x_dep_t> dep_;

            CallableMatrixCoeff_(std::function<void(const Vector_<>&, SquareMatrix_<>*)> f, const Matrix_<x_dep_t>& dep)
                : f_(std::move(f)), dep_(dep) {}

            void Value(const Vector_<>& x, SquareMatrix_<>* value) const override {
                REQUIRE(value != nullptr, "coefficient value output must be non-null");
                value->Resize(dep_.Rows());
                f_(x, value);
            }

            [[nodiscard]] Matrix_<x_dep_t> XDependence() const override { return dep_; }
        };
    } // namespace

    std::unique_ptr<CoordinateMap_> NewSinhMap(double xWidth, double dxdyRange) {
        REQUIRE(IsPositive(xWidth) && dxdyRange >= 1.0, "xWidth should be positive and dxdyRange should be greater than 1");
        double sinhMaxY = std::sqrt(Square(dxdyRange) - 1.0);
        return IsZero(Square(sinhMaxY)) ? std::unique_ptr<CoordinateMap_>(new IdentityMap_) : std::make_unique<SinhMap_>(xWidth / sinhMaxY);
    }

    std::unique_ptr<CoordinateMap_> NewConcentratingMap(double xLow, double xHigh, double cPoint, double density) {
        REQUIRE(xHigh > xLow, "concentrating map requires xHigh > xLow");
        REQUIRE(cPoint >= xLow && cPoint <= xHigh, "concentrating map requires cPoint in [xLow, xHigh]");
        REQUIRE(density > 0.0, "concentrating map requires density > 0");
        return std::make_unique<ConcentratingMap_>(xLow, xHigh, cPoint, density);
    }

    std::unique_ptr<ScalarCoeff_> NewConstCoeff(double val) { return std::make_unique<ConstScalarCoeff_>(val); }

    std::unique_ptr<VectorCoeff_> NewConstCoeff(const Vector_<>& val) { return std::make_unique<ConstVectorCoeff_>(val); }

    std::unique_ptr<MatrixCoeff_> NewConstCoeff(const Matrix_<>& val) {
        REQUIRE(val.Rows() == val.Cols(), "constant matrix coefficient must be square");
        return std::make_unique<ConstMatrixCoeff_>(val);
    }

    std::unique_ptr<ScalarCoeff_> NewScalarCoeff(std::function<double(const Vector_<>&)> f, Coeff_::x_dep_t dep) {
        REQUIRE(static_cast<bool>(f), "coefficient callable must be non-empty");
        return std::make_unique<CallableScalarCoeff_>(std::move(f), dep);
    }

    std::unique_ptr<VectorCoeff_> NewVectorCoeff(std::function<void(const Vector_<>&, Vector_<>*)> f, const Vector_<Coeff_::x_dep_t>& dep) {
        REQUIRE(static_cast<bool>(f), "coefficient callable must be non-empty");
        return std::make_unique<CallableVectorCoeff_>(std::move(f), dep);
    }

    std::unique_ptr<MatrixCoeff_> NewMatrixCoeff(std::function<void(const Vector_<>&, SquareMatrix_<>*)> f, const Matrix_<Coeff_::x_dep_t>& dep) {
        REQUIRE(static_cast<bool>(f), "coefficient callable must be non-empty");
        REQUIRE(dep.Rows() == dep.Cols(), "matrix coefficient dependence must be square");
        return std::make_unique<CallableMatrixCoeff_>(std::move(f), dep);
    }

    std::unique_ptr<ScalarCoeff_> NewScalarCoeff(std::function<double(double)> f) {
        REQUIRE(static_cast<bool>(f), "coefficient callable must be non-empty");
        Coeff_::x_dep_t dep;
        dep.set(0);
        return NewScalarCoeff([f = std::move(f)](const Vector_<>& x) { return f(x[0]); }, dep);
    }

    std::unique_ptr<VectorCoeff_> NewVectorCoeff(std::function<double(double)> f) {
        REQUIRE(static_cast<bool>(f), "coefficient callable must be non-empty");
        Coeff_::x_dep_t dep;
        dep.set(0);
        Vector_<Coeff_::x_dep_t> deps(1, dep);
        return NewVectorCoeff([f = std::move(f)](const Vector_<>& x, Vector_<>* out) { (*out)[0] = f(x[0]); }, deps);
    }

    std::unique_ptr<MatrixCoeff_> NewMatrixCoeff(std::function<double(double)> f) {
        REQUIRE(static_cast<bool>(f), "coefficient callable must be non-empty");
        Coeff_::x_dep_t dep;
        dep.set(0);
        Matrix_<Coeff_::x_dep_t> deps(1, 1);
        deps(0, 0) = dep;
        return NewMatrixCoeff([f = std::move(f)](const Vector_<>& x, SquareMatrix_<>* out) { (*out)(0, 0) = f(x[0]); }, deps);
    }
} // namespace Dal::PDE
