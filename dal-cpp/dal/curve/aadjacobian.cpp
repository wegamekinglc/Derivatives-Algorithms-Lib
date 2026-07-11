//
// Created by dal-implementer on 2026/7/12.
//

#include <dal/platform/strict.hpp>

#include <dal/curve/aadjacobian.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    Matrix_<>
    HarvestCurveJacobian(AAD::Tape_& tape, Vector_<AAD::Number_>& independents, Vector_<AAD::Number_>& residuals, const Vector_<int>& rowWidths) {
        const int rows = static_cast<int>(residuals.size());
        const int columns = static_cast<int>(independents.size());
        REQUIRE(rowWidths.empty() || static_cast<int>(rowWidths.size()) == rows,
                "HarvestCurveJacobian: row-width count must equal the residual count");

        Matrix_<> result(rows, columns, 0.0);
        for (int row = 0; row < rows; ++row) {
            const int width = rowWidths.empty() ? columns : rowWidths[row];
            REQUIRE(width >= 0 && width <= columns, "HarvestCurveJacobian: row width is outside the Jacobian column range");
#if defined(DAL_USE_XAD_AAD) || defined(DAL_USE_CODIPACK_AAD) || defined(DAL_USE_ADEPT_AAD)
            AAD::ZeroAdjoints(tape);
#endif
            AAD::Adjoint(residuals[row]) = 1.0;
            AAD::PropagateToStart(tape);
            for (int column = 0; column < width; ++column)
                result(row, column) = AAD::AdjointValue(independents[column]);
#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
            for (int column = 0; column < columns; ++column)
                AAD::Adjoint(independents[column]) = 0.0;
#endif
        }
        return result;
    }
} // namespace Dal
