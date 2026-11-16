#include <dal/platform/platform.hpp>

#include <dal-public/src/calibrationinstrument.hpp>

namespace Dal {
    namespace {
        template <class... T_> Vector_<Cell_> Row(const T_&... values) { return {Cell_(values)...}; }
        Cell_ Optional(const std::optional<double>& value) { return value ? Cell_(*value) : Cell_(); }
        Matrix_<Cell_> Table(const Vector_<String_>& columns, const Vector_<Vector_<Cell_>>& rows) {
            Matrix_<Cell_> result(static_cast<int>(rows.size()) + 1, static_cast<int>(columns.size()));
            int col = 0;
            for (const auto& column : columns)
                result(0, col++) = column;
            for (size_t i = 0; i < rows.size(); ++i)
                for (int j = 0; j < result.Cols(); ++j)
                    result(static_cast<int>(i) + 1, j) = rows[i][j];
            return result;
        }
        Vector_<String_> ScheduleColumns(bool evaluated) {
            Vector_<String_> result{"rule",
                                    "mode",
                                    "coupon_id",
                                    "row",
                                    "coupon_row",
                                    "source_key",
                                    "start",
                                    "end",
                                    "accrual",
                                    "rate_start",
                                    "rate_end",
                                    "rate_accrual",
                                    "weight_start",
                                    "weight_end",
                                    "compounding_accrual",
                                    "fixing_index",
                                    "fixing_time",
                                    "historical_required",
                                    "included",
                                    "settled",
                                    "reused_from_row",
                                    "observation_schedule_identity"};
            if (evaluated)
                for (const auto* column : {"source", "rate", "source_row", "statement"})
                    result.push_back(column);
            return result;
        }
        Vector_<Cell_> ScheduleRow(const CompoundedOisResolvedObservation_& row, const String_& rule, const String_& mode, const String_& identity) {
            return Row(rule, mode, row.couponId_, double(row.row_), double(row.couponRow_), row.sourceKey_, row.accrualStart_, row.accrualEnd_,
                       row.accrual_, row.rateStart_, row.rateEnd_, row.rateAccrual_, row.weightStart_, row.weightEnd_, row.compoundingAccrual_,
                       row.fixingIndex_, row.fixingTime_, row.historicalRequired_, row.included_, row.settled_,
                       row.reusedFromRow_ ? Cell_(double(*row.reusedFromRow_)) : Cell_(), identity);
        }
        Matrix_<Cell_> ObservationScheduleTable(const CurveScriptEvaluationResult_& result) {
            Vector_<Vector_<Cell_>> rows;
            for (const auto& r : result.observationSchedule_) {
                auto row = ScheduleRow(r, result.observationRule_, result.observationMode_, result.observationScheduleIdentity_);
                row.push_back(Cell_(r.source_));
                row.push_back(Optional(r.rate_));
                row.push_back(Cell_(double(r.sourceRow_)));
                row.push_back(Cell_(double(r.statement_)));
                rows.push_back(std::move(row));
            }
            return Table(ScheduleColumns(true), rows);
        }
        Matrix_<Cell_> CashflowTable(const CurveScriptEvaluationResult_& result) {
            Vector_<Vector_<Cell_>> rows;
            for (const auto& r : result.cashflows_)
                rows.push_back(Row(r.table_, r.couponId_, double(r.row_), double(r.sourceRow_), double(r.statement_), r.payment_, r.included_,
                                   r.settled_, Optional(r.amount_), Optional(r.discount_), r.pv_));
            return Table({"table", "coupon_id", "row", "source_row", "statement", "payment", "included", "settled", "amount", "discount", "pv"},
                         rows);
        }
        Matrix_<Cell_> ObservationTable(const CurveScriptEvaluationResult_& result) {
            Vector_<Vector_<Cell_>> rows;
            for (const auto& r : result.observations_)
                rows.push_back(Row(double(r.row_), double(r.sourceRow_), double(r.statement_), r.couponId_, r.fixingIndex_, r.projectionRoute_,
                                   r.source_, r.fixingTime_, r.start_, r.end_, r.rateStart_, r.rateEnd_, r.accrual_, r.rateAccrual_,
                                   Optional(r.rate_)));
            return Table({"row", "source_row", "statement", "coupon_id", "fixing_index", "projection_route", "source", "fixing_time", "start", "end",
                          "rate_start", "rate_end", "accrual", "rate_accrual", "rate"},
                         rows);
        }
        Matrix_<Cell_> MeasureTable(const CurveScriptEvaluationResult_& result) {
            Vector_<Vector_<Cell_>> rows;
            for (size_t i = 0; i < result.measureIds_.size(); ++i)
                rows.push_back(Row(result.measureIds_[i], result.measureUnits_[i], CurveScriptEvaluationValue(result, result.measureIds_[i])));
            return Table({"measure_id", "unit", "value"}, rows);
        }
        Matrix_<Cell_> ParameterTable(const CurveScriptEvaluationResult_& result) {
            Vector_<Vector_<Cell_>> rows;
            for (size_t i = 0; i < result.freeParameters_.size(); ++i)
                rows.push_back(Row(result.freeParameters_[i].date_, String_(result.freeParameters_[i].component_.String()), result.parameters_[i]));
            return Table({"date", "component", "value"}, rows);
        }
    } // namespace

    double CurveScriptEvaluationValue(const CurveScriptEvaluationResult_& result, const String_& measureId) {
        for (size_t i = 0; i < result.measureIds_.size(); ++i)
            if (std::string(result.measureIds_[i].data(), result.measureIds_[i].size()) == std::string(measureId.data(), measureId.size())) {
                REQUIRE(i < result.values_.size(), "CurveScriptEvaluation: measure unavailable");
                return result.values_[i];
            }
        THROW("CurveScriptEvaluation: measure not requested");
    }
    Matrix_<> CurveScriptEvaluationPartials(const CurveScriptEvaluationResult_& result) {
        REQUIRE(result.derivativeMethod_ != CalibrationDerivative_::Value_::None && result.nativePartials_.Rows() > 0,
                "CurveScriptEvaluation: native partials unavailable");
        return result.nativePartials_;
    }
    Matrix_<Cell_> CurveScriptEvaluationTable(const CurveScriptEvaluationResult_& result, const String_& table) {
        if (table == "summary")
            return Table({"field", "value"}, {Row("instrument_id", result.instrument_->Id()), Row("context_identity", result.contextIdentity_),
                                              Row("candidate_identity", result.candidateIdentity_),
                                              Row("derivative_method", String_(result.derivativeMethod_.String()))});
        if (table == "cashflows")
            return CashflowTable(result);
        if (table == "observations")
            return ObservationTable(result);
        if (table == "observation_schedule")
            return ObservationScheduleTable(result);
        if (table == "measures")
            return MeasureTable(result);
        if (table == "parameters")
            return ParameterTable(result);
        THROW("CurveScriptEvaluation: unknown table");
    }
    Matrix_<Cell_> CompoundedOisObservationScheduleTable(const Handle_<PreparedCompoundedOis_>& prepared) {
        REQUIRE(prepared, "CompoundedOisObservationScheduleTable: expected prepared OIS tables");
        Vector_<Vector_<Cell_>> rows;
        const String_ mode = prepared->Spec().observationConvention_ ? prepared->Spec().observationConvention_->mode_.String() : "Legacy";
        for (const auto& row : prepared->ObservationSchedule())
            rows.push_back(ScheduleRow(row, prepared->RuleId(), mode, prepared->ObservationScheduleIdentity()));
        return Table(ScheduleColumns(false), rows);
    }
} // namespace Dal
