#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <set>

#include <dal-public/src/calibrationinstrument.hpp>

namespace Dal {
    namespace {
        using namespace CalibrationInternal;
        Json_ V(const String_& value) { return value.empty() ? Json_() : Str(value); }
        Json_ V(double value) { return Num(value); }
        Json_ V(int value) { return Num(value); }
        Json_ V(bool value) { return Bool(value); }
        Json_ V(const Date_& value) { return Str(Date::ToString(value)); }
        template <class T_> Json_ V(const std::optional<T_>& value) { return value ? V(*value) : Json_(); }
        Json_ Numbers(const Vector_<>& values) {
            return Array(values, [](double value) { return Num(value); });
        }
        Json_ Matrix(const Matrix_<>& value) {
            Json_ rows = Json_::Array();
            for (int i = 0; i < value.Rows(); ++i) {
                Json_ row = Json_::Array();
                for (int j = 0; j < value.Cols(); ++j)
                    row.array_.push_back(Num(value(i, j)));
                rows.array_.push_back(std::move(row));
            }
            return rows;
        }
        Json_ Matrix(const std::optional<Matrix_<>>& value) { return value ? Matrix(*value) : Json_(); }
        Json_ SummaryJson(const CalibrationReportSummary_& value) {
            return Obj({{"domain", V(value.domain_)},
                        {"status", V(value.status_)},
                        {"mode", V(value.mode_)},
                        {"converged", V(value.converged_)},
                        {"fit_within_tolerance", V(value.fitWithinTolerance_)},
                        {"numerical_validation_passed", V(value.numericalValidationPassed_)},
                        {"termination_reason", V(value.terminationReason_)},
                        {"raw_l2", V(value.rawL2_)},
                        {"scaled_l2", V(value.scaledL2_)},
                        {"scaled_infinity", V(value.scaledInfinity_)},
                        {"fit_weight", V(value.fitWeight_)},
                        {"fit_norm_tolerance", V(value.fitNormTolerance_)},
                        {"objective", V(value.objective_)},
                        {"data_objective", V(value.dataObjective_)},
                        {"penalty_objective", V(value.penaltyObjective_)},
                        {"iterations", V(value.iterations_)},
                        {"evaluations", V(value.evaluations_)},
                        {"jacobian_evaluations", V(value.jacobianEvaluations_)},
                        {"jacobian_rank", V(value.jacobianRank_)},
                        {"jacobian_condition_estimate", V(value.jacobianConditionEstimate_)},
                        {"context_identity", V(value.contextIdentity_)},
                        {"compiler_identity", V(value.compilerIdentity_)}});
        }
        Json_ ParameterJson(const CalibrationReportParameter_& value) {
            return Obj({{"id", V(value.id_)},
                        {"unit", V(value.unit_)},
                        {"initial", V(value.initial_)},
                        {"value", V(value.value_)},
                        {"scale", V(value.scale_)},
                        {"lower", V(value.lower_)},
                        {"upper", V(value.upper_)},
                        {"active_bound", V(value.activeBound_)}});
        }
        Json_ PlanJson(const CalibrationReportPlan_& value) {
            return Obj({{"observation_id", V(value.observationId_)},
                        {"measure_id", V(value.measureId_)},
                        {"rule_id", V(value.ruleId_)},
                        {"admitted", V(value.admitted_)},
                        {"reason_code", V(value.reasonCode_)},
                        {"reason", V(value.reason_)},
                        {"backend", V(value.backend_)},
                        {"derivative_method", V(value.derivativeMethod_)},
                        {"source_identity", V(value.sourceIdentity_)},
                        {"definition_identity", V(value.definitionIdentity_)},
                        {"binding_identity", V(value.bindingIdentity_)},
                        {"compiler_identity", V(value.compilerIdentity_)},
                        {"context_identity", V(value.contextIdentity_)},
                        {"plan_identity", V(value.planIdentity_)}});
        }
        Json_ ErrorJson(const CalibrationDiagnostic_& value) {
            return Obj({{"code", V(value.code_)},
                        {"message", V(value.message_)},
                        {"field", V(value.field_)},
                        {"observation_id", V(value.observationId_)},
                        {"instrument_id", V(value.instrumentId_)},
                        {"quote_id", V(value.quoteId_)},
                        {"measure_id", V(value.measureId_)},
                        {"parameter_key", V(value.parameterKey_)},
                        {"source_row", V(value.sourceRow_)},
                        {"event_date", V(value.eventDate_)},
                        {"statement", V(value.statement_)},
                        {"actual", V(value.actual_)},
                        {"expected", V(value.expected_)}});
        }
        Json_ SamplingJson(const CalibrationReportSampling_& value) {
            return Obj({{"mode", V(value.mode_)},
                        {"stream_version", V(value.streamVersion_)},
                        {"mapping_identity", V(value.mappingIdentity_)},
                        {"generator", V(value.generator_)},
                        {"normal_precision", V(value.normalPrecision_)},
                        {"seed", V(value.seed_)},
                        {"batches", V(value.batches_)},
                        {"paths_per_batch", V(value.pathsPerBatch_)}});
        }
        Json_ SimulationJson(const CalibrationReportSimulation_& value) {
            return Obj({{"use_bb", V(value.useBb_)},
                        {"compiled", V(value.compiled_)},
                        {"smooth", V(value.smooth_)},
                        {"normal_precision", V(value.normalPrecision_)}});
        }
        Json_ ValidationJson(const CalibrationReportValidation_& value) {
            return Obj({{"observation_id", V(value.observationId_)},
                        {"edge", V(value.edge_)},
                        {"quantity", V(value.quantity_)},
                        {"value", V(value.value_)},
                        {"difference", V(value.difference_)},
                        {"standard_error", V(value.standardError_)},
                        {"absolute_tolerance", V(value.absoluteTolerance_)},
                        {"sigma_multiplier", V(value.sigmaMultiplier_)},
                        {"accepted", V(value.accepted_)},
                        {"reason", V(value.reason_)}});
        }

        Json_ ObservationJson(const CalibrationReportObservation_& row, int ordinal) {
            const auto& instrument = *row.instrument_;
            return Obj({{"ordinal", Num(ordinal)},
                        {"observation_id", V(row.observationId_)},
                        {"instrument_id", V(instrument.Id())},
                        {"definition_identity", V(instrument.Definition()->DefinitionIdentity())},
                        {"binding_identity", V(instrument.BindingIdentity())},
                        {"measure_id", V(row.measureId_)},
                        {"representation", V(row.representation_)},
                        {"quote_id", V(row.quoteId_)},
                        {"quote_index", V(row.quoteIndex_)},
                        {"unit", V(row.unit_)},
                        {"target", V(row.target_)},
                        {"error_scale", V(row.errorScale_)},
                        {"model_value", V(row.modelValue_)},
                        {"raw_residual", V(row.rawResidual_)},
                        {"scaled_residual", V(row.scaledResidual_)},
                        {"fit_row_passed", V(row.fitRowPassed_)},
                        {"validation_value", V(row.validationValue_)},
                        {"validation_error", V(row.validationError_)},
                        {"standard_error", V(row.standardError_)},
                        {"backend", V(row.backend_)},
                        {"derivative_method", V(row.derivativeMethod_)},
                        {"plan_identity", V(row.planIdentity_)}});
        }
        Json_ Bindings(const Vector_<CalibrationReportObservation_>& observations) {
            Json_ rows = Json_::Array();
            std::set<std::string> seen;
            for (const auto& row : observations) {
                const auto& instrument = *row.instrument_;
                if (!seen.insert(Bytes(instrument.Id())).second)
                    continue;
                for (const auto& binding : instrument.Binding())
                    rows.array_.push_back(Obj({{"instrument_id", V(instrument.Id())},
                                               {"instrument_identity", V(instrument.InstrumentIdentity())},
                                               {"parameter_key", V(binding.key_)},
                                               {"value", Num(binding.value_)},
                                               {"unit", V(binding.unit_.Token())},
                                               {"origin", V(String_(binding.origin_.String()))},
                                               {"quote_id", V(binding.quoteId_)},
                                               {"quote_fingerprint", V(binding.quoteFingerprint_)}}));
            }
            return rows;
        }
        Json_ ObjectiveJson(const CalibrationReportObjective_& o) {
            return Obj({{"theta0", Numbers(o.theta0_)},
                        {"parameter_scales", Numbers(o.parameterScales_)},
                        {"native_layout", Array(o.nativeLayout_, [](const auto& label) { return V(label); })},
                        {"weight_matrix", Matrix(o.weightMatrix_)},
                        {"weight_provenance", V(o.weightProvenance_)},
                        {"lambda", V(o.lambda_)},
                        {"eta", V(o.eta_)},
                        {"legacy_import", Json_()}});
        }
        Json_ ExecutionJson(const CalibrationReportExecution_& e) {
            auto sampling = [](const auto& value) {
                auto json = SamplingJson(value);
                json.object_["first_paths"] = Numbers(value.firstPaths_);
                json.object_["path_counts"] = Numbers(value.pathCounts_);
                return json;
            };
            return Obj({{"valuation_date", V(e.valuationDate_)},
                        {"today_fixing_policy", V(e.todayFixingPolicy_)},
                        {"fixing_snapshot_identity", V(e.fixingSnapshotIdentity_)},
                        {"route_identities", Array(e.routeIdentities_, [](const auto& value) { return V(value); })},
                        {"model_identity", V(e.modelIdentity_)},
                        {"model_time_basis", V(e.modelTimeBasis_)},
                        {"numeraire_convention", V(e.numeraireConvention_)},
                        {"convention_state_identity", V(e.conventionStateIdentity_)},
                        {"simulation", Optional(e.simulation_, SimulationJson)},
                        {"sampling", Optional(e.sampling_, sampling)},
                        {"validation_sampling", Optional(e.validationSampling_, sampling)}});
        }
        Vector_<CalibrationReportObservation_> ReportRows(const CalibrationReport_& report) {
            if (!report.Data().observations_.empty())
                return report.Data().observations_;
            Vector_<CalibrationReportObservation_> rows;
            for (size_t i = 0; i < report.Axes().observations_.size(); ++i) {
                const auto& observation = *report.Axes().observations_[i];
                CalibrationReportObservation_ row;
                row.instrument_ = observation.Instrument();
                row.observationId_ = observation.Id();
                row.measureId_ = observation.MeasureId();
                row.quoteId_ = observation.Quote()->Id();
                row.quoteIndex_ = report.Axes().quoteIndex_[i];
                row.target_ = observation.Quote()->Value();
                row.errorScale_ = observation.ErrorScale();
                row.unit_ = observation.Quote()->Unit().Token();
                rows.push_back(row);
            }
            return rows;
        }
        Json_ ReportJson(const Handle_<CalibrationReport_>& report) {
            Check(bool(report), "InvalidCalibrationPlan", "/report", "nonnull report");
            const auto& data = report->Data();
            auto summary = SummaryJson(data.summary_);
            summary.object_["schema"] = Str(report->Schema());
            summary.object_["stage"] = Str(report->Stage().String());
            summary.object_["quote_risk_available"] = Bool(false);
            Json_ observations = Json_::Array(), parameters = Json_::Array(), quotes = Json_::Array();
            const auto rows = ReportRows(*report);
            for (size_t i = 0; i < rows.size(); ++i)
                observations.array_.push_back(ObservationJson(rows[i], static_cast<int>(i)));
            for (size_t i = 0; i < data.parameters_.size(); ++i) {
                auto parameter = ParameterJson(data.parameters_[i]);
                parameter.object_["ordinal"] = Num(static_cast<double>(i));
                parameters.array_.push_back(std::move(parameter));
            }
            for (size_t i = 0; i < report->Axes().quotes_.size(); ++i) {
                const auto& q = *report->Axes().quotes_[i];
                quotes.array_.push_back(Obj({{"quote_index", Num(static_cast<double>(i))},
                                             {"quote_id", V(q.Id())},
                                             {"value", Num(q.Value())},
                                             {"unit", V(q.Unit().Token())},
                                             {"convention_id", Str(q.Convention().id_.String())},
                                             {"convention_version", Num(q.Convention().version_)}}));
            }
            auto errors = Array(report->Errors(), [](const auto& d) {
                auto json = ErrorJson(d);
                json.object_["stage"] = Str(d.stage_.String());
                return json;
            });
            return Obj({{"summary", summary},
                        {"observations", observations},
                        {"parameters", parameters},
                        {"quotes", quotes},
                        {"plans", Array(data.plans_, PlanJson)},
                        {"errors", errors},
                        {"bindings", Bindings(rows)},
                        {"objective_settings", ObjectiveJson(data.objective_)},
                        {"execution", ExecutionJson(data.execution_)},
                        {"validation", Array(data.validation_, ValidationJson)},
                        {"quote_jacobian", Matrix(data.quoteJacobian_)},
                        {"quote_map", Matrix(report->Axes().quoteMap_)}});
        }
        Cell_ ReportCell(const Json_& json) {
            switch (json.kind_.Switch()) {
            case JsonKind_::STRING:
                return Cell_(String_(json.string_));
            case JsonKind_::NUMBER:
                return Cell_(json.number_);
            case JsonKind_::BOOLEAN:
                return Cell_(json.boolean_);
            default:
                return Cell_();
            }
        }
    } // namespace
    String_ DumpCalibrationReportJson(const Handle_<CalibrationReport_>& report) { return String_(Jcs(ReportJson(report))); }
    Matrix_<Cell_> CalibrationReportTable(const Handle_<CalibrationReport_>& report, const String_& table) {
        static const std::map<std::string, Vector_<String_>> COLUMNS = {
            {"summary",
             {"schema",
              "domain",
              "status",
              "stage",
              "mode",
              "converged",
              "fit_within_tolerance",
              "numerical_validation_passed",
              "termination_reason",
              "raw_l2",
              "scaled_l2",
              "scaled_infinity",
              "fit_weight",
              "fit_norm_tolerance",
              "objective",
              "data_objective",
              "penalty_objective",
              "iterations",
              "evaluations",
              "jacobian_evaluations",
              "jacobian_rank",
              "jacobian_condition_estimate",
              "context_identity",
              "compiler_identity",
              "quote_risk_available"}},
            {"observations",
             {"ordinal",
              "observation_id",
              "instrument_id",
              "definition_identity",
              "binding_identity",
              "measure_id",
              "representation",
              "quote_id",
              "quote_index",
              "unit",
              "target",
              "error_scale",
              "model_value",
              "raw_residual",
              "scaled_residual",
              "fit_row_passed",
              "validation_value",
              "validation_error",
              "standard_error",
              "backend",
              "derivative_method",
              "plan_identity"}},
            {"quotes", {"quote_index", "quote_id", "value", "unit", "convention_id", "convention_version"}},
            {"parameters", {"ordinal", "id", "unit", "initial", "value", "scale", "lower", "upper", "active_bound"}},
            {"plans",
             {"observation_id", "measure_id", "rule_id", "admitted", "reason_code", "reason", "backend", "derivative_method", "source_identity",
              "definition_identity", "binding_identity", "compiler_identity", "context_identity", "plan_identity"}},
            {"errors",
             {"code", "message", "stage", "field", "observation_id", "instrument_id", "quote_id", "measure_id", "parameter_key", "source_row",
              "event_date", "statement", "actual", "expected"}}};
        const auto found = COLUMNS.find(Bytes(table));
        Check(found != COLUMNS.end(), "InvalidCalibrationPlan", "/report/table", "a supported report table", table);
        const auto json = ReportJson(report);
        const auto& group = json.object_.at(found->first);
        const auto& columns = found->second;
        if (found->first == "summary") {
            Matrix_<Cell_> result(static_cast<int>(columns.size()) + 1, 2);
            result(0, 0) = Cell_("key");
            result(0, 1) = Cell_("value");
            for (size_t i = 0; i < columns.size(); ++i) {
                result(static_cast<int>(i) + 1, 0) = Cell_(columns[i]);
                result(static_cast<int>(i) + 1, 1) = ReportCell(group.object_.at(Bytes(columns[i])));
            }
            return result;
        }
        Matrix_<Cell_> result(static_cast<int>(group.array_.size()) + 1, static_cast<int>(columns.size()));
        for (size_t col = 0; col < columns.size(); ++col) {
            result(0, static_cast<int>(col)) = Cell_(columns[col]);
            for (size_t row = 0; row < group.array_.size(); ++row)
                result(static_cast<int>(row) + 1, static_cast<int>(col)) = ReportCell(group.array_[row].object_.at(Bytes(columns[col])));
        }
        return result;
    }
} // namespace Dal
