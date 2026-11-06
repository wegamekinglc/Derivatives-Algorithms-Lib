#include <dal/platform/platform.hpp>

#include <dal-public/src/scriptcurvequoterisk.hpp>

namespace Dal {
    namespace {
        using Row_ = Vector_<Cell_>;
        template <class T_> Cell_ Cell(const T_& value) {
            Cell_ result;
            result = value;
            return result;
        }
        template <class... T_> Row_ Row(const T_&... values) { return {Cell(values)...}; }
        Matrix_<Cell_> Cells(const Vector_<Row_>& rows) {
            Matrix_<Cell_> result(static_cast<int>(rows.size()), static_cast<int>(rows.front().size()));
            for (int i = 0; i < result.Rows(); ++i)
                for (int j = 0; j < result.Cols(); ++j)
                    result(i, j) = rows[i][j];
            return result;
        }
        template <class T_> Cell_ Optional(const std::optional<T_>& value) { return value ? Cell(*value) : Cell_(); }
        Matrix_<Cell_> Quotes(const Vector_<ScriptCurveQuoteRiskQuote_>& quotes) {
            Vector_<Row_> rows{Row("ordinal", "id", "value", "unit", "convention", "convention_version")};
            for (const auto& q : quotes)
                rows.push_back(Row(q.ordinal_, q.id_, q.value_, q.unit_.Token(), q.convention_.id_.String(), q.convention_.version_));
            return Cells(rows);
        }
        Matrix_<Cell_> Diagnostics(const Vector_<CalibrationDiagnostic_>& diagnostics) {
            Vector_<Row_> rows{Row("code", "message", "stage", "field", "expected", "actual", "observation_id", "instrument_id", "quote_id",
                                   "measure_id", "parameter_key", "source_row", "event_date", "statement")};
            for (const auto& d : diagnostics)
                rows.push_back(Row(d.code_, d.message_, d.stage_.String(), Optional(d.field_), Optional(d.expected_), Optional(d.actual_),
                                   Optional(d.observationId_), Optional(d.instrumentId_), Optional(d.quoteId_), Optional(d.measureId_),
                                   Optional(d.parameterKey_), Optional(d.sourceRow_), Optional(d.eventDate_), Optional(d.statement_)));
            return Cells(rows);
        }
        Matrix_<Cell_> Observations(const Vector_<ScriptCurveQuoteRiskObservation_>& observations) {
            Vector_<Row_> rows{Row("ordinal", "id", "quote_ordinal", "instrument_id", "measure_id", "error_scale", "source_identity",
                                   "definition_identity", "binding_identity")};
            for (const auto& o : observations)
                rows.push_back(Row(o.ordinal_, o.id_, o.quoteOrdinal_, o.instrumentId_, o.measureId_, o.errorScale_, o.sourceIdentity_,
                                   o.definitionIdentity_, o.bindingIdentity_));
            return Cells(rows);
        }
        Matrix_<Cell_> Parameters(const Vector_<ScriptCurveQuoteRiskParameter_>& parameters) {
            Vector_<Row_> rows{Row("ordinal", "id", "unit", "value", "date", "component")};
            for (const auto& p : parameters)
                rows.push_back(Row(p.ordinal_, p.id_, p.unit_, Optional(p.value_), p.coordinate_.date_, p.coordinate_.component_.String()));
            return Cells(rows);
        }
        Matrix_<Cell_> Sensitivities(const ScriptCurveQuoteRiskResult_& result) {
            Vector_<Row_> rows{Row("ordinal", "id", "unit", "currency", "raw_sensitivity", "dv01")};
            for (size_t i = 0; i < result.Axes().quotes_.size(); ++i) {
                const auto& q = result.Axes().quotes_[i];
                rows.push_back(Row(q.ordinal_, q.id_, q.unit_.Token(), result.Currency().String(),
                                   result.Available() ? Cell_(result.RawSensitivity()[i]) : Cell_(),
                                   result.Available() ? Cell_(result.DV01()[i]) : Cell_()));
            }
            return Cells(rows);
        }
        String_ Status(bool available) { return available ? "Available" : "Unavailable"; }
    } // namespace

    Matrix_<Cell_> ScriptCurveQuoteRiskTable(const ScriptCurveQuoteRiskSnapshot_& snapshot, const String_& table) {
        if (table == "QuoteAxis")
            return Quotes(snapshot.QuoteAxis());
        if (table == "Diagnostics")
            return Diagnostics(snapshot.Diagnostics());
        if (table == "ObservationAxis")
            return Observations(snapshot.ObservationAxis());
        if (table == "ParameterAxis")
            return Parameters(snapshot.ParameterAxis());
        REQUIRE(table == "Summary", "ScriptCurveQuoteRiskTable: expected Summary, QuoteAxis, ObservationAxis, ParameterAxis or Diagnostics");
        return Cells({Row("field", "value"), Row("status", Status(snapshot.RiskAvailable())), Row("structure_valid", snapshot.StructureValid()),
                      Row("mapping_available", snapshot.MappingAvailable()), Row("snapshot_identity", snapshot.Identity()),
                      Row("context_identity", snapshot.ContextIdentity()),
                      Row("reciprocal_condition", Optional(snapshot.Numerics().reciprocalCondition_)),
                      Row("factorization_count", snapshot.Numerics().factorizationCount_)});
    }
    Matrix_<Cell_> ScriptCurveQuoteRiskTable(const ScriptCurveQuoteRiskResult_& result, const String_& table) {
        if (table == "QuoteAxis")
            return Quotes(result.Axes().quotes_);
        if (table == "Diagnostics")
            return Diagnostics(result.Diagnostics());
        if (table == "ObservationAxis")
            return Observations(result.Axes().observations_);
        if (table == "ParameterAxis")
            return Parameters(result.Axes().parameters_);
        if (table == "Sensitivity")
            return Sensitivities(result);
        REQUIRE(table == "Summary",
                "ScriptCurveQuoteRiskTable: expected Summary, QuoteAxis, ObservationAxis, ParameterAxis, Diagnostics or Sensitivity");
        return Cells({Row("field", "value"), Row("status", Status(result.Available())), Row("currency", result.Currency().String()),
                      Row("pv", result.Available() ? Cell_(result.PV()) : Cell_()), Row("method", result.Method()),
                      Row("snapshot_identity", result.SnapshotIdentity()), Row("context_identity", result.ContextIdentity()),
                      Row("source_identity", result.SourceIdentity()), Row("definition_identity", result.DefinitionIdentity()),
                      Row("binding_identity", result.BindingIdentity()),
                      Row("reciprocal_condition", Optional(result.Numerics().reciprocalCondition_))});
    }
} // namespace Dal
