//
// Created by dal-implementer on 2026/7/12.
//

#include <dal/platform/strict.hpp>

#include <dal/curve/aadjacobian.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace {
        int RowWidth(const Vector_<int>& rowWidths, int row, int columns) {
            if (rowWidths.empty())
                return columns;
            const int result = rowWidths[row];
            REQUIRE(result >= 0 && result <= columns, "HarvestCurveJacobian: row width is outside the Jacobian column range");
            return result;
        }

        void PrepareAdjoints(AAD::Tape_& tape) {
#if defined(DAL_USE_XAD_AAD) || defined(DAL_USE_CODIPACK_AAD) || defined(DAL_USE_ADEPT_AAD)
            AAD::ZeroAdjoints(tape);
#else
            (void)tape;
#endif
        }

        void ClearIndependentAdjoints(Vector_<AAD::Number_>& independents) {
#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
            for (auto& independent : independents)
                AAD::Adjoint(independent) = 0.0;
#else
            (void)independents;
#endif
        }

        void HarvestRow(AAD::Tape_& tape, Vector_<AAD::Number_>& independents, AAD::Number_& residual, int row, int width, Matrix_<>& result) {
            PrepareAdjoints(tape);
            AAD::Adjoint(residual) = 1.0;
            AAD::PropagateToStart(tape);
            for (int column = 0; column < width; ++column)
                result(row, column) = AAD::AdjointValue(independents[column]);
            ClearIndependentAdjoints(independents);
        }
    } // namespace

    Matrix_<>
    HarvestCurveJacobian(AAD::Tape_& tape, Vector_<AAD::Number_>& independents, Vector_<AAD::Number_>& residuals, const Vector_<int>& rowWidths) {
        const int rows = static_cast<int>(residuals.size());
        const int columns = static_cast<int>(independents.size());
        REQUIRE(rowWidths.empty() || static_cast<int>(rowWidths.size()) == rows,
                "HarvestCurveJacobian: row-width count must equal the residual count");

        Matrix_<> result(rows, columns, 0.0);
        for (int row = 0; row < rows; ++row) {
            const int width = RowWidth(rowWidths, row, columns);
            HarvestRow(tape, independents, residuals[row], row, width, result);
        }
        return result;
    }
} // namespace Dal
