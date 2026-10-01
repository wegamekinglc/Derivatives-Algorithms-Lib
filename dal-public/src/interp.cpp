//
// Created by wegam on 2022/5/9.
//

#include <limits>

#include <dal-public/src/interp.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/math/smooth.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
    Handle_<Interp1_> Interp1NewLinear(const String_& name,
                                       const Vector_<>& x,
                                       const Vector_<>& y) {
        return Handle_<Interp1_>(Interp::NewLinear(name, x, y));
    }

    Handle_<Interp1_>
    Interp1NewLinearSmoothed(const String_& name, const Vector_<>& x, const Vector_<>& y, double smoothing, const Vector_<>& fitWeights) {
        REQUIRE(smoothing >= 0.0, "smoothing must be non-negative");
        REQUIRE(fitWeights.empty() || fitWeights.size() == x.size(), "fitWeights must be empty or have one weight per x value");
        return Interp1NewLinear(name, x, SmoothedVals(x, y, fitWeights, smoothing));
    }

    Handle_<Interp1_>
    Interp1NewCubic(const String_& name, const Vector_<>& x, const Vector_<>& y, const Vector_<int>& boundaryOrder, const Vector_<>& boundaryValue) {
        REQUIRE(boundaryOrder.size() <= 2 && boundaryValue.size() <= 2, "Can only specify two boundary conditions");
        REQUIRE(boundaryValue.empty() || !boundaryOrder.empty(), "Can't specify boundary value without specifying order");
        for (int order : boundaryOrder)
            REQUIRE(order > 0 && order <= 3, "Boundary order must be in the range (0, 3]");
        Interp::Boundary_ left(3, 0.0), right(3, 0.0);
        if (!boundaryOrder.empty()) {
            left.order_ = boundaryOrder.front();
            right.order_ = boundaryOrder.back();
        }
        if (!boundaryValue.empty()) {
            left.value_ = boundaryValue.front();
            right.value_ = boundaryValue.back();
        }
        return Handle_<Interp1_>(Interp::NewCubic(name, x, y, left, right));
    }

    Vector_<> Interp1Get(const Handle_<Interp1_>& interpolator, const Vector_<>& x) {
        Vector_<> values;
        Interp1Get(interpolator, x, &values);
        return values;
    }

    void Interp1Get(const Handle_<Interp1_>& interpolator, const Vector_<>& x, Vector_<>* values) {
        REQUIRE(interpolator, "interpolator must be a non-null Interp1 handle");
        REQUIRE(values, "values must be a non-null output vector");
        values->Resize(x.size());
        for (size_t index = 0; index < x.size(); ++index) {
            REQUIRE(interpolator->IsInBounds(x[index]), "X (= " + std::to_string(x[index]) + ") is outside interpolation domain");
            (*values)[index] = (*interpolator)(x[index]);
        }
    }

    Handle_<Interp2_> Interp2NewLinear(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<>& values) {
        return Handle_<Interp2_>(Interp::NewLinear2(name, x, y, values));
    }

    Matrix_<> Interp2Get(const Handle_<Interp2_>& interpolator, const Vector_<>& x, const Vector_<>& y) {
        Matrix_<> values;
        Interp2Get(interpolator, x, y, &values);
        return values;
    }

    void Interp2Get(const Handle_<Interp2_>& interpolator, const Vector_<>& x, const Vector_<>& y, Matrix_<>* values) {
        REQUIRE(interpolator, "interpolator must be a non-null Interp2 handle");
        REQUIRE(values, "values must be a non-null output matrix");
        const size_t maxDimension = static_cast<size_t>(std::numeric_limits<int>::max());
        REQUIRE(x.size() <= maxDimension && y.size() <= maxDimension, "interpolation query dimensions must fit the native matrix range");
        values->Resize(static_cast<int>(x.size()), static_cast<int>(y.size()));
        for (int row = 0; row < values->Rows(); ++row) {
            for (int col = 0; col < values->Cols(); ++col) {
                REQUIRE(interpolator->IsInBounds(x[row], y[col]),
                        "X (= " + std::to_string(x[row]) + ") Y (= " + std::to_string(y[col]) + ") is outside interpolation domain");
                (*values)(row, col) = (*interpolator)(x[row], y[col]);
            }
        }
    }
} // namespace Dal
