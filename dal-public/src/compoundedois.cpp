#include <dal/platform/platform.hpp>

#include <dal-public/src/calibrationinstrument.hpp>

namespace Dal {
    namespace {
        template <class... T_> Vector_<Cell_> Row(const T_&... values) { return {Cell_(values)...}; }
        Cell_ Optional(const std::optional<double>& value) { return value ? Cell_(*value) : Cell_(); }
        Matrix_<Cell_> Table(std::initializer_list<const char*> columns, const Vector_<Vector_<Cell_>>& rows) {
            Matrix_<Cell_> result(static_cast<int>(rows.size()) + 1, static_cast<int>(columns.size()));
            int col = 0;
            for (const auto* column : columns)
                result(0, col++) = String_(column);
            for (size_t i = 0; i < rows.size(); ++i)
                for (int j = 0; j < result.Cols(); ++j)
                    result(static_cast<int>(i) + 1, j) = rows[i][j];
            return result;
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
        Vector_<Vector_<Cell_>> rows;
        if (table == "summary")
            return Table({"field", "value"}, {Row("instrument_id", result.instrument_->Id()), Row("context_identity", result.contextIdentity_),
                                              Row("candidate_identity", result.candidateIdentity_),
                                              Row("derivative_method", String_(result.derivativeMethod_.String()))});
        if (table == "cashflows") {
            for (const auto& r : result.cashflows_)
                rows.push_back(Row(r.table_, r.couponId_, double(r.row_), double(r.sourceRow_), double(r.statement_), r.payment_, r.included_,
                                   r.settled_, Optional(r.amount_), Optional(r.discount_), r.pv_));
            return Table({"table", "coupon_id", "row", "source_row", "statement", "payment", "included", "settled", "amount", "discount", "pv"},
                         rows);
        }
        if (table == "observations") {
            for (const auto& r : result.observations_)
                rows.push_back(Row(double(r.row_), double(r.sourceRow_), double(r.statement_), r.couponId_, r.fixingIndex_, r.projectionRoute_,
                                   r.source_, r.fixingTime_, r.start_, r.end_, r.rateStart_, r.rateEnd_, r.accrual_, r.rateAccrual_,
                                   Optional(r.rate_)));
            return Table({"row", "source_row", "statement", "coupon_id", "fixing_index", "projection_route", "source", "fixing_time", "start", "end",
                          "rate_start", "rate_end", "accrual", "rate_accrual", "rate"},
                         rows);
        }
        if (table == "measures") {
            for (size_t i = 0; i < result.measureIds_.size(); ++i)
                rows.push_back(Row(result.measureIds_[i], result.measureUnits_[i], CurveScriptEvaluationValue(result, result.measureIds_[i])));
            return Table({"measure_id", "unit", "value"}, rows);
        }
        if (table == "parameters") {
            for (size_t i = 0; i < result.freeParameters_.size(); ++i)
                rows.push_back(Row(result.freeParameters_[i].date_, String_(result.freeParameters_[i].component_.String()), result.parameters_[i]));
            return Table({"date", "component", "value"}, rows);
        }
        THROW("CurveScriptEvaluation: unknown table");
    }
} // namespace Dal
