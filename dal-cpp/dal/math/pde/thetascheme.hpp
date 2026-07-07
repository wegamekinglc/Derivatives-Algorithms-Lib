//
// Created by dal-implementer on 2026/7/8.
//

#pragma once

#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/decompositions.hpp>
#include <dal/math/pde/pde.hpp>

#include <memory>

namespace Dal::PDE {
    class ThetaScheme_ : public Rollback_ {
    public:
        explicit ThetaScheme_(double theta);

        void Prepare(double dt,
                     const Vector_<CoordinateVector_>& xPoints,
                     const ScalarCoeff_& discounting,
                     const VectorCoeff_& advection,
                     const MatrixCoeff_& diffusion);

        void operator()(double dt,
                        const Vector_<CoordinateVector_>& xPoints,
                        const Vector_<std::shared_ptr<Cube_<>>>& oldVals,
                        const ScalarCoeff_& discounting,
                        const VectorCoeff_& advection,
                        const MatrixCoeff_& diffusion,
                        Vector_<std::shared_ptr<Cube_<>>>* newVals) const override;

        [[nodiscard]] double Theta() const { return theta_; }
        [[nodiscard]] int Decompositions() const { return decompositions_; }

    private:
        double theta_;
        int decompositions_ = 0;
        double preparedDt_ = 0.0;
        CoordinateVector_ points_{0.0, 0.0, 0, Handle_<CoordinateMap_>()};
        Vector_<> x_;
        const ScalarCoeff_* discounting_ = nullptr;
        const VectorCoeff_* advection_ = nullptr;
        const MatrixCoeff_* diffusion_ = nullptr;
        Vector_<> probeSamples_;
        std::unique_ptr<Sparse::TriDiagonal_> explicitOp_;
        std::unique_ptr<SquareMatrixDecomposition_> implicitSolve_;
    };
} // namespace Dal::PDE
