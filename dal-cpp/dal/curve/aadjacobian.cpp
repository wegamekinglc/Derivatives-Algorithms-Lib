#include <dal/platform/strict.hpp>

#include <algorithm>

#include <dal/curve/aadjacobian.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/jointrate.hpp>
#include <dal/math/aad/native.hpp>
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

        void ClearIndependentAdjoints(Vector_<AAD::Number_>& independents) {
            for (auto& independent : independents)
                AAD::Adjoint(independent) = 0.0;
        }

        void HarvestRow(AAD::Tape_& tape, Vector_<AAD::Number_>& independents, AAD::Number_& residual, int row, int width, Matrix_<>& result) {
            AAD::Adjoint(residual) = 1.0;
            AAD::PropagateToStart(tape);
            for (int column = 0; column < width; ++column)
                result(row, column) = AAD::AdjointValue(independents[column]);
            ClearIndependentAdjoints(independents);
        }
        void ValidateVectorAxes(const Vector_<AAD::Number_>& inputs, const Vector_<AAD::Number_>& outputs, const Vector_<int>& widths) {
            for (int row = 0; row < static_cast<int>(outputs.size()); ++row) {
                RowWidth(widths, row, static_cast<int>(inputs.size()));
                static_cast<void>(AAD::NativeOperations_::ReadAdjoint(outputs[row]));
            }
            for (const auto& input : inputs)
                static_cast<void>(AAD::NativeOperations_::ReadAdjoint(input));
        }

        void CopyVectorRows(const Vector_<AAD::Number_>& inputs, const Vector_<int>& widths, size_t first, size_t count, Matrix_<>* result) {
            for (size_t lane = 0; lane < count; ++lane) {
                const int row = static_cast<int>(first + lane);
                const int columns = widths.empty() ? result->Cols() : widths[row];
                for (int column = 0; column < columns; ++column)
                    (*result)(row, column) = AAD::NativeOperations_::ReadAdjoint(inputs[column], lane);
            }
        }

        Matrix_<> HarvestVectorRows(AAD::Tape_& tape, Vector_<AAD::Number_>& inputs, Vector_<AAD::Number_>& outputs, const Vector_<int>& widths) {
            REQUIRE(&tape == AAD::Tape(), "HarvestCurveJacobian: vector adjoints require the current tape");
            ValidateVectorAxes(inputs, outputs, widths);
            Matrix_<> result(static_cast<int>(outputs.size()), static_cast<int>(inputs.size()), 0.);
            for (size_t first = 0; first < outputs.size(); first += tape.numAdj_) {
                const size_t count = std::min(tape.numAdj_, outputs.size() - first);
                for (size_t lane = 0; lane < count; ++lane)
                    AAD::NativeOperations_::SetSeed(outputs[first + lane], 1., lane);
                AAD::PropagateToStart(tape);
                CopyVectorRows(inputs, widths, first, count, &result);
                // An output may alias a leaf, and several columns may name the same input.
                for (size_t lane = 0; lane < count; ++lane)
                    AAD::NativeOperations_::ClearSeeds(outputs[first + lane]);
                for (auto& input : inputs)
                    AAD::NativeOperations_::ClearSeeds(input);
            }
            return result;
        }
    } // namespace

    Matrix_<> RecordStagedResidualJacobian(const CurveDefinition_& definition,
                                           const CurveCalibrationStageRecord_& stage,
                                           const Vector_<Handle_<YCInstrument_>>& ordered,
                                           const CurveCalibrationStageRecord_* discount) {
        Vector_<> parameters;
        if (discount)
            parameters = discount->solution_;
        const size_t baseWidth = parameters.size();
        parameters.Append(stage.solution_);
        const auto jacobian = TapeResidualJacobian(parameters, [&](const Vector_<AAD::Number_>& active) {
            using ActiveCurve_ = Tape::DiscountCurve_<AAD::Number_>;
            Handle_<ActiveCurve_> base;
            if (discount) {
                const auto& spec = discount->spec_;
                const auto baseDefinition = MakeCurveDefinition(spec.curveName_, spec.ccy_, spec.parameterization_, spec.logDfScheme_,
                                                                spec.knotDates_, spec.today_, spec.liborBasis_);
                base = Handle_<ActiveCurve_>(BuildDiscountCurveUniqueT<AAD::Number_>(baseDefinition, {active.begin(), active.begin() + baseWidth}));
            }
            const auto curve = BuildDiscountCurveUniqueT<AAD::Number_, ActiveCurve_>(definition, {active.begin() + baseWidth, active.end()}, base);
            Tape::JointCurveBlock_<AAD::Number_> block;
            block.discountCurves[stage.spec_.targetCollateral_] = discount ? base.get() : curve.get();
            if (discount)
                block.forwardCurves[stage.spec_.targetTenor_] = curve.get();
            Vector_<AAD::Number_> residuals;
            residuals.reserve(ordered.size());
            for (const auto& instrument : ordered) {
                const auto rate = Tape::ProjectionRateAt<AAD::Number_>(*instrument);
                residuals.push_back((*rate)(block)-instrument->MarketRate());
            }
            return residuals;
        });
        return std::move(jacobian->j_);
    }

    Matrix_<>
    HarvestCurveJacobian(AAD::Tape_& tape, Vector_<AAD::Number_>& independents, Vector_<AAD::Number_>& residuals, const Vector_<int>& rowWidths) {
        const int rows = static_cast<int>(residuals.size());
        const int columns = static_cast<int>(independents.size());
        REQUIRE(rowWidths.empty() || static_cast<int>(rowWidths.size()) == rows,
                "HarvestCurveJacobian: row-width count must equal the residual count");

        if (tape.multi_)
            return HarvestVectorRows(tape, independents, residuals, rowWidths);
        Matrix_<> result(rows, columns, 0.0);
        for (int row = 0; row < rows; ++row) {
            const int width = RowWidth(rowWidths, row, columns);
            HarvestRow(tape, independents, residuals[row], row, width, result);
        }
        return result;
    }
} // namespace Dal
