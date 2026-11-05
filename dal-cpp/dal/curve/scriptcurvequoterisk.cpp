#include <dal/platform/platform.hpp>

#include <set>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/scriptcurvequoterisk.hpp>
#include <dal/storage/json.hpp>

namespace Dal {
    using namespace CalibrationInternal;

    struct ScriptCurveQuoteRiskSnapshot_::Data_ {
        bool structureValid_ = false;
        ScriptCurveQuoteRiskAccuracy_ accuracy_;
        std::shared_ptr<const ScriptCurveQuoteRiskAxes_> axes_;
        std::shared_ptr<const CurveInstrumentCalibrationSpec_> spec_;
        Vector_<> parameters_;
        Vector_<CalibrationDiagnostic_> diagnostics_;
        Json_ context_;
        String_ contextIdentity_, identity_;
    };

    namespace {
        constexpr const char* INPUT_ERROR = "InvalidScriptCurveQuoteRiskInput";
        constexpr const char* MISMATCH = "ScriptCurveQuoteRiskIdentityMismatch";

        Json_ CurveJson(const Handle_<DiscountCurve_>& curve) { return curve ? Parse(JSON::WriteString(*curve)) : Json_(); }
        Handle_<DiscountCurve_> CopyCurve(const Handle_<DiscountCurve_>& curve) {
            if (!curve)
                return {};
            const auto copy = handle_cast<DiscountCurve_>(JSON::ReadString(JSON::WriteString(*curve), false));
            Check(bool(copy), INPUT_ERROR, "/context/market", "owned discount curve snapshot");
            return copy;
        }
        CurveInstrumentCalibrationSpec_ CopySpec(CurveInstrumentCalibrationSpec_ spec) {
            spec.geometry_.baseCurve_ = CopyCurve(spec.geometry_.baseCurve_);
            for (auto& [key, curve] : spec.market_.discountCurves_)
                curve = CopyCurve(curve);
            for (auto& [key, curve] : spec.market_.forwardCurves_)
                curve = CopyCurve(curve);
            return spec;
        }
        Json_ Dates(const Vector_<Date_>& dates) {
            return Array(dates, [](const auto& date) { return Str(Date::ToString(date)); });
        }
        Json_ Numbers(const Vector_<>& values) {
            return Array(values, [](double value) { return Num(value); });
        }
        Json_ GeometryJson(const CurveInstrumentGeometry_& g) {
            Check(g.today_.IsValid(), INPUT_ERROR, "/context/geometry/today", "valid valuation date");
            return Obj({{"today", Str(Date::ToString(g.today_))},
                        {"currency", Str(g.ccy_)},
                        {"curve_name", Str(g.curveName_)},
                        {"knot_dates", Dates(g.knotDates_)},
                        {"knot_policy", Str(Token(g.knotPolicy_, INPUT_ERROR, "/context/geometry/knot_policy"))},
                        {"parameterization", Str(Token(g.parameterization_, INPUT_ERROR, "/context/geometry/parameterization"))},
                        {"log_df_scheme", Str(Token(g.logDfScheme_, INPUT_ERROR, "/context/geometry/log_df_scheme"))},
                        {"base_curve", CurveJson(g.baseCurve_)},
                        {"discount_target", Bool(g.calibrateDiscountCurve_)},
                        {"target_collateral", Str(Token(g.targetCollateral_, INPUT_ERROR, "/context/geometry/target_collateral"))},
                        {"target_tenor", g.targetTenor_.String() ? Str(g.targetTenor_.String()) : Json_()},
                        {"libor_basis", Str(Token(g.liborBasis_, INPUT_ERROR, "/context/geometry/libor_basis"))}});
        }
        Json_ MarketJson(const CurveInstrumentMarket_& market) {
            auto discounts = Json_::Array(), forwards = Json_::Array();
            for (const auto& [key, curve] : market.discountCurves_)
                discounts.array_.push_back(
                    Obj({{"collateral", Str(Token(key, INPUT_ERROR, "/context/market/discounts"))}, {"curve", CurveJson(curve)}}));
            for (const auto& [key, curve] : market.forwardCurves_)
                forwards.array_.push_back(Obj({{"tenor", Str(Token(key, INPUT_ERROR, "/context/market/forwards"))}, {"curve", CurveJson(curve)}}));
            return Obj({{"discounts", discounts},
                        {"forwards", forwards},
                        {"routes", Array(market.routes_, [](const auto& route) {
                             return Obj({{"id", Str(route.id_)},
                                         {"projection", Bool(route.projection_)},
                                         {"collateral", Str(Token(route.collateral_, INPUT_ERROR, "/context/market/routes"))},
                                         {"tenor", route.tenor_.String() ? Str(route.tenor_.String()) : Json_()}});
                         })}});
        }
        Json_ ValuationJson(const Script::ScriptValuationSettings_& valuation) {
            Check(valuation.evaluationDate_ && valuation.evaluationDate_->IsValid(), INPUT_ERROR, "/context/valuation/evaluation_date",
                  "explicit valid valuation date");
            Check(bool(valuation.fixings_), INPUT_ERROR, "/context/valuation/fixings", "explicit frozen fixing snapshot");
            auto fixings = Json_::Array();
            for (const auto& [index, history] : valuation.fixings_->Values())
                for (const auto& [time, value] : history)
                    fixings.array_.push_back(Obj(
                        {{"index", Str(index)}, {"date", Str(Date::ToString(time.Date()))}, {"fraction", Num(time.Frac())}, {"value", Num(value)}}));
            return Obj({{"evaluation_date", Str(Date::ToString(*valuation.evaluationDate_))},
                        {"today_fixing_policy", Str(Token(valuation.todayFixingPolicy_, INPUT_ERROR, "/context/valuation/today_fixing_policy"))},
                        {"fixings", fixings}});
        }
        Json_ ObservationJson(const Handle_<CalibrationObservation_>& row) {
            Check(bool(row), INPUT_ERROR, "/context/observations", "nonnull typed observation");
            const auto& instrument = *row->Instrument();
            return Obj({{"id", Str(row->Id())},
                        {"instrument_id", Str(instrument.Id())},
                        {"source_identity", Str(instrument.Definition()->SourceIdentity())},
                        {"definition_identity", Str(instrument.Definition()->DefinitionIdentity())},
                        {"binding_identity", Str(instrument.BindingIdentity())},
                        {"instrument_identity", Str(instrument.InstrumentIdentity())},
                        {"measure_id", Str(row->MeasureId())},
                        {"quote", QuoteJson(*row->Quote())},
                        {"error_scale", Num(row->ErrorScale())}});
        }
        Json_ ConfigNumber(double value, const String_& field) {
            Check(std::isfinite(value), INPUT_ERROR, field, "finite configuration number", NumberText(value));
            return Num(value);
        }
        Json_ ConfigNumbers(const Vector_<>& values, const String_& field) {
            auto result = Json_::Array();
            for (size_t i = 0; i < values.size(); ++i)
                result.array_.push_back(ConfigNumber(values[i], field + "/" + OrdinalText(i)));
            return result;
        }
        Json_ SolverJson(const CurveInstrumentSolver_& solver) {
            return Obj({{"mode", Str(Token(solver.solveMode_, INPUT_ERROR, "/context/solver/mode"))},
                        {"initial_guess", ConfigNumber(solver.initialGuess_, "/context/solver/initial_guess")},
                        {"initial_guess_per_node", ConfigNumbers(solver.initialGuessPerNode_, "/context/solver/initial_guess_per_node")},
                        {"parameter_scales", ConfigNumbers(solver.parameterScales_, "/context/solver/parameter_scales")},
                        {"smoothing_weight", ConfigNumber(solver.smoothingWeight_, "/context/solver/smoothing_weight")},
                        {"max_evaluations", Num(solver.maxEvaluations_)},
                        {"max_restarts", Num(solver.maxRestarts_)},
                        {"fit_weight", Optional(solver.fitWeight_, [](double value) { return ConfigNumber(value, "/context/solver/fit_weight"); })},
                        {"fit_norm_tolerance", Optional(solver.fitNormTolerance_,
                                                        [](double value) { return ConfigNumber(value, "/context/solver/fit_norm_tolerance"); })}});
        }
        Json_ ContextJson(const CurveInstrumentCalibrationSpec_& spec) {
            return Obj({{"geometry", GeometryJson(spec.geometry_)},
                        {"market", MarketJson(spec.market_)},
                        {"valuation", ValuationJson(spec.valuation_)},
                        {"observations", Array(spec.observations_, ObservationJson)},
                        {"solver", SolverJson(spec.solver_)}});
        }
        Json_ DefinitionJson(const CurveDefinition_& d) {
            return Obj({{"name", Str(d.name_)},
                        {"currency", Str(d.ccy_)},
                        {"parameterization", Str(Token(d.parameterization_, INPUT_ERROR, "/fit/definition/parameterization"))},
                        {"scheme", Str(Token(d.logDfScheme_, INPUT_ERROR, "/fit/definition/scheme"))},
                        {"nodes", Dates(d.nodeDates_)},
                        {"basis", Str(Token(d.dayCount_, INPUT_ERROR, "/fit/definition/basis"))},
                        {"anchor", Str(Date::ToString(d.anchorDate_))}});
        }
        String_ ParameterId(const CurveFreeParameter_& parameter) {
            return Token(parameter.component_, INPUT_ERROR, "/fit/free_parameters/component") + ":" + Date::ToString(parameter.date_);
        }
        ScriptCurveQuoteRiskAxes_ Axes(const CurveInstrumentCalibrationSpec_& spec, const CurveInstrumentCalibrationResult_& fit) {
            ScriptCurveQuoteRiskAxes_ result;
            for (size_t i = 0; i < spec.observations_.size(); ++i) {
                const auto& row = *spec.observations_[i];
                const auto& instrument = *row.Instrument();
                const auto& quote = *row.Quote();
                result.quotes_.push_back({static_cast<int>(i), quote.Id(), quote.Value(), quote.Unit(), quote.Convention()});
                result.observations_.push_back({static_cast<int>(i), static_cast<int>(i), row.Id(), instrument.Id(), row.MeasureId(),
                                                instrument.Definition()->SourceIdentity(), instrument.Definition()->DefinitionIdentity(),
                                                instrument.BindingIdentity(), row.ErrorScale()});
            }
            for (size_t j = 0; j < fit.freeParameters_.size(); ++j) {
                std::optional<double> value;
                if (j < fit.parameters_.size() && std::isfinite(fit.parameters_[j]))
                    value = fit.parameters_[j];
                const auto unit =
                    fit.freeParameters_[j].component_ == CurveFreeParameterComponent_::Value_::LOG_DISCOUNT_FACTOR ? "Dimensionless" : "Rate";
                result.parameters_.push_back({static_cast<int>(j), ParameterId(fit.freeParameters_[j]), unit, fit.freeParameters_[j], value});
            }
            return result;
        }
        void ValidateAccuracy(const ScriptCurveQuoteRiskAccuracy_& accuracy, size_t rows) {
            Check(accuracy.residualAbsoluteLimits_.size() == rows, INPUT_ERROR, "/accuracy/residual_absolute_limits",
                  "one raw residual limit per observation");
            for (size_t i = 0; i < rows; ++i)
                Check(std::isfinite(accuracy.residualAbsoluteLimits_[i]) && accuracy.residualAbsoluteLimits_[i] >= 0., INPUT_ERROR,
                      "/accuracy/residual_absolute_limits/" + OrdinalText(i), "finite nonnegative raw residual limit",
                      NumberText(accuracy.residualAbsoluteLimits_[i]));
            Check(std::isfinite(accuracy.minReciprocalCondition_) && accuracy.minReciprocalCondition_ > 0. && accuracy.minReciprocalCondition_ <= 1.,
                  INPUT_ERROR, "/accuracy/min_reciprocal_condition", "finite value in (0,1]");
            Check(std::isfinite(accuracy.responseBackwardErrorLimit_) && accuracy.responseBackwardErrorLimit_ > 0., INPUT_ERROR,
                  "/accuracy/response_backward_error_limit", "explicit finite positive backward error limit");
            Check(std::isfinite(accuracy.relativePivotTolerance_) && accuracy.relativePivotTolerance_ > 0. && accuracy.relativePivotTolerance_ < 1.,
                  INPUT_ERROR, "/accuracy/relative_pivot_tolerance", "finite value in (0,1)");
        }
        CalibrationDiagnostic_ Diagnostic(const String_& code, const String_& field, const String_& expected, const String_& actual = "") {
            CalibrationDiagnostic_ d;
            d.code_ = code;
            d.field_ = field;
            d.stage_ = CalibrationStage_::Value_::Validate;
            d.expected_ = expected;
            d.actual_ = actual;
            d.message_ = code + ": " + field + "; expected " + expected + "; actual " + actual;
            return d;
        }
        CalibrationDiagnostic_ RowDiagnostic(const CalibrationObservation_& row, CalibrationDiagnostic_ d = {}) {
            d.stage_ = CalibrationStage_::Value_::Validate;
            d.observationId_ = row.Id();
            d.instrumentId_ = row.Instrument()->Id();
            d.quoteId_ = row.Quote()->Id();
            d.measureId_ = row.MeasureId();
            return d;
        }
        void ValidateQuotes(const CurveInstrumentCalibrationSpec_& spec) {
            std::set<std::string> ids;
            for (size_t i = 0; i < spec.observations_.size(); ++i) {
                const auto& row = *spec.observations_[i];
                const auto d = RowDiagnostic(row);
                const auto field = "/context/observations/" + OrdinalText(i) + "/quote";
                Check(ids.insert(Bytes(row.Quote()->Id())).second, "ScriptCurveQuoteRiskDuplicateQuoteId", field + "/id",
                      "unique quote ID for each observation", row.Quote()->Id(), d);
                const auto kind = row.Quote()->Unit().kind_;
                Check(kind == CalibrationUnitKind_::Value_::Rate || kind == CalibrationUnitKind_::Value_::BasisSpread,
                      "ScriptCurveQuoteRiskUnsupportedUnit", field + "/unit", "decimal Rate or BasisSpread", row.Quote()->Unit().Token(), d);
            }
        }
        void ValidateFitAxes(const CalibrationObservationAxes_& actual, const CalibrationObservationAxes_& expected) {
            const auto rows = expected.observations_.size(), quotes = expected.quotes_.size();
            Check(actual.observations_.size() == rows && actual.quotes_.size() == quotes && actual.quoteIndex_.size() == rows, MISMATCH, "/fit/axes",
                  "resolved observation and quote axis dimensions");
            Check(actual.quoteMap_.Rows() == static_cast<int>(rows) && actual.quoteMap_.Cols() == static_cast<int>(quotes), MISMATCH,
                  "/fit/axes/quote_map", "observation by quote dimensions");
            for (size_t i = 0; i < rows; ++i) {
                const auto d = RowDiagnostic(*expected.observations_[i]);
                const auto field = "/fit/axes/observations/" + OrdinalText(i);
                Check(Jcs(ObservationJson(actual.observations_[i])) == Jcs(ObservationJson(expected.observations_[i])), MISMATCH, field,
                      "observation matching resolved contract", "", d);
                Check(actual.quoteIndex_[i] == expected.quoteIndex_[i], MISMATCH, "/fit/axes/quote_index/" + OrdinalText(i), "resolved quote ordinal",
                      String_(std::to_string(actual.quoteIndex_[i])), d);
                for (size_t j = 0; j < quotes; ++j)
                    Check(actual.quoteMap_(static_cast<int>(i), static_cast<int>(j)) == expected.quoteMap_(static_cast<int>(i), static_cast<int>(j)),
                          MISMATCH, "/fit/axes/quote_map/" + OrdinalText(i) + "/" + OrdinalText(j), "resolved quote map", "", d);
            }
            for (size_t j = 0; j < quotes; ++j)
                Check(actual.quotes_[j] && Jcs(QuoteJson(*actual.quotes_[j])) == Jcs(QuoteJson(*expected.quotes_[j])), MISMATCH,
                      "/fit/axes/quotes/" + OrdinalText(j), "quote matching resolved contract");
        }
        std::optional<CalibrationDiagnostic_> Difference(const Json_& expected, const Json_& actual, const String_& field);
        std::optional<CalibrationDiagnostic_> MemberDifference(const Json_& expected, const Json_& actual, const String_& field) {
            if (expected.kind_ == JsonKind_::OBJECT && expected.object_.size() == actual.object_.size()) {
                for (const auto& [key, value] : expected.object_) {
                    const auto found = actual.object_.find(key);
                    if (found == actual.object_.end())
                        break;
                    if (const auto d = Difference(value, found->second, field + "/" + String_(key)))
                        return d;
                }
            }
            return std::nullopt;
        }
        std::optional<CalibrationDiagnostic_> ElementDifference(const Json_& expected, const Json_& actual, const String_& field) {
            if (expected.kind_ == JsonKind_::ARRAY && expected.array_.size() == actual.array_.size())
                for (size_t i = 0; i < expected.array_.size(); ++i)
                    if (const auto d = Difference(expected.array_[i], actual.array_[i], field + "/" + OrdinalText(i)))
                        return d;
            return std::nullopt;
        }
        std::optional<CalibrationDiagnostic_> Difference(const Json_& expected, const Json_& actual, const String_& field) {
            if (Jcs(expected) == Jcs(actual))
                return std::nullopt;
            if (expected.kind_ == actual.kind_) {
                if (const auto d = MemberDifference(expected, actual, field))
                    return d;
                if (const auto d = ElementDifference(expected, actual, field))
                    return d;
            }
            return Diagnostic(MISMATCH, field, String_(Jcs(expected)), String_(Jcs(actual)));
        }
        CalibrationDiagnostic_ ContextDiagnostic(const CurveInstrumentCalibrationSpec_& spec, CalibrationDiagnostic_ d) {
            for (size_t i = 0; i < spec.observations_.size(); ++i) {
                const auto prefix = Bytes("/context/observations/" + OrdinalText(i) + "/");
                if (Bytes(d.field_.value_or("")).find(prefix) == 0)
                    return RowDiagnostic(*spec.observations_[i], std::move(d));
            }
            return d;
        }
        bool SameText(const std::optional<String_>& actual, const String_& expected) { return actual && Bytes(*actual) == Bytes(expected); }
        void ValidateReportPlans(const CalibrationReportSnapshot_& report, const PreparedCurveInstruments_& prepared) {
            Check(report.plans_.size() == prepared.Plans().size(), MISMATCH, "/fit/report/plans", "one prepared contract per observation");
            for (size_t i = 0; i < report.plans_.size(); ++i) {
                const auto& plan = prepared.Plans()[i];
                const auto& row = report.plans_[i];
                const auto& instrument = *plan.Instrument();
                const bool sameContract = SameText(row.sourceIdentity_, instrument.Definition()->SourceIdentity()) &&
                                          SameText(row.definitionIdentity_, instrument.Definition()->DefinitionIdentity()) &&
                                          SameText(row.bindingIdentity_, instrument.BindingIdentity());
                Check(sameContract && SameText(row.contextIdentity_, plan.ContextIdentity()) &&
                          SameText(row.compilerIdentity_, plan.CompilerIdentity()),
                      MISMATCH, "/fit/report/plans/" + OrdinalText(i), "original prepared source, binding and context", "",
                      RowDiagnostic(*prepared.Axes().observations_[i]));
            }
        }
        void ValidateReport(const CurveInstrumentCalibrationResult_& fit, const PreparedCurveInstruments_& prepared) {
            Check(fit.report_ && fit.report_->Stage() == CalibrationStage_::Value_::Solve && fit.report_->Errors().empty(), MISMATCH, "/fit/report",
                  "successful Solve report");
            const auto& report = fit.report_->Data();
            Check(Bytes(report.summary_.status_) == "Succeeded" && report.summary_.fitWithinTolerance_ == true &&
                      SameText(report.summary_.mode_, "EXACT"),
                  MISMATCH, "/fit/report", "accepted EXACT calibration report");
            ValidateReportPlans(report, prepared);
        }
        void ValidateFitValues(const Vector_<>& values, const CurveInstrumentCalibrationSpec_& spec, const String_& field) {
            Check(values.size() == spec.observations_.size(), MISMATCH, field, "one value per resolved observation");
            for (size_t i = 0; i < values.size(); ++i) {
                Check(std::isfinite(values[i]), "ScriptCurveQuoteRiskNonFinite", field + "/" + OrdinalText(i), "finite raw fit data",
                      NumberText(values[i]), RowDiagnostic(*spec.observations_[i]));
            }
        }
        void ValidateFit(const CurveInstrumentCalibrationResult_& fit, const PreparedCurveInstruments_& prepared) {
            const auto layout = prepared.Describe();
            const auto& spec = prepared.Spec();
            Check(fit.solveMode_ == CurveSolveMode_::Value_::EXACT && spec.solver_.solveMode_ == CurveSolveMode_::Value_::EXACT,
                  "ScriptCurveQuoteRiskUnsupportedMode", "/fit/solve_mode", "EXACT");
            Check(spec.observations_.size() == layout.freeParameters_.size(), "ScriptCurveQuoteRiskNonSquare", "/fit/dimensions",
                  "equal nonzero observation, quote and native parameter counts");
            ValidateFitAxes(fit.axes_, prepared.Axes());
            Check(fit.fitWithinTolerance_, "ScriptCurveQuoteRiskFitNotAccepted", "/fit/fit_within_tolerance", "accepted calibration");
            ValidateReport(fit, prepared);
            Check(Bytes(fit.planIdentity_) == Bytes(layout.planIdentity_), MISMATCH, "/fit/plan_identity",
                  "plan identity rebuilt from actual resolved spec and options", fit.planIdentity_);
            ValidateFitValues(fit.modelMeasures_, spec, "/fit/model_measures");
            ValidateFitValues(fit.rawResiduals_, spec, "/fit/raw_residuals");
            ValidateFitValues(fit.scaledResiduals_, spec, "/fit/scaled_residuals");
            Check(fit.parameters_.size() == layout.freeParameters_.size(), MISMATCH, "/fit/parameters", "native parameter count");
            Check(Jcs(DefinitionJson(fit.definition_)) == Jcs(DefinitionJson(layout.definition_)), MISMATCH, "/fit/definition",
                  "resolved native definition");
            Check(fit.freeParameters_.size() == layout.freeParameters_.size(), MISMATCH, "/fit/free_parameters", "resolved native axis");
            for (size_t j = 0; j < fit.parameters_.size(); ++j) {
                auto d = Diagnostic(MISMATCH, "/fit/free_parameters/" + OrdinalText(j), ParameterId(layout.freeParameters_[j]));
                d.parameterKey_ = ParameterId(layout.freeParameters_[j]);
                Check(Bytes(ParameterId(fit.freeParameters_[j])) == Bytes(ParameterId(layout.freeParameters_[j])), MISMATCH, *d.field_, *d.expected_,
                      ParameterId(fit.freeParameters_[j]), d);
                Check(std::isfinite(fit.parameters_[j]), "ScriptCurveQuoteRiskNonFinite", "/fit/parameters/" + OrdinalText(j),
                      "finite native parameter", NumberText(fit.parameters_[j]), d);
            }
            const auto candidate = Fingerprint(Obj({{"plan", Str(fit.planIdentity_)}, {"parameters", Numbers(fit.parameters_)}}));
            Check(!fit.planIdentity_.empty() && Bytes(candidate) == Bytes(fit.candidateIdentity_), MISMATCH, "/fit/candidate_identity",
                  "candidate identity bound to native parameters", fit.candidateIdentity_);
            const auto expected = BuildDiscountCurveT<double>(layout.definition_, fit.parameters_, spec.geometry_.baseCurve_);
            Check(bool(fit.curve_) && Jcs(CurveJson(fit.curve_)) == Jcs(CurveJson(Handle_<DiscountCurve_>(expected))), MISMATCH, "/fit/curve",
                  "native curve matching parameters and frozen base");
        }
    } // namespace

    ScriptCurveQuoteRiskValidation_::ScriptCurveQuoteRiskValidation_(bool passed,
                                                                     std::shared_ptr<const ScriptCurveQuoteRiskAxes_> axes,
                                                                     Vector_<CalibrationDiagnostic_> diagnostics)
        : passed_(passed), axes_(std::move(axes)), diagnostics_(std::move(diagnostics)) {}
    ScriptCurveQuoteRiskSnapshot_::ScriptCurveQuoteRiskSnapshot_(std::shared_ptr<const Data_> data) : data_(std::move(data)) {}
    bool ScriptCurveQuoteRiskSnapshot_::StructureValid() const { return data_->structureValid_; }
    const Vector_<CalibrationDiagnostic_>& ScriptCurveQuoteRiskSnapshot_::Diagnostics() const { return data_->diagnostics_; }
    const ScriptCurveQuoteRiskAccuracy_& ScriptCurveQuoteRiskSnapshot_::Accuracy() const { return data_->accuracy_; }
    const Vector_<ScriptCurveQuoteRiskQuote_>& ScriptCurveQuoteRiskSnapshot_::QuoteAxis() const { return data_->axes_->quotes_; }
    const Vector_<ScriptCurveQuoteRiskObservation_>& ScriptCurveQuoteRiskSnapshot_::ObservationAxis() const { return data_->axes_->observations_; }
    const Vector_<ScriptCurveQuoteRiskParameter_>& ScriptCurveQuoteRiskSnapshot_::ParameterAxis() const { return data_->axes_->parameters_; }
    const std::shared_ptr<const CurveInstrumentCalibrationSpec_>& ScriptCurveQuoteRiskSnapshot_::ResolvedSpec() const { return data_->spec_; }
    const Vector_<>& ScriptCurveQuoteRiskSnapshot_::Parameters() const { return data_->parameters_; }
    const String_& ScriptCurveQuoteRiskSnapshot_::ContextIdentity() const { return data_->contextIdentity_; }
    const String_& ScriptCurveQuoteRiskSnapshot_::Identity() const { return data_->identity_; }

    ScriptCurveQuoteRiskSnapshot_ NewScriptCurveQuoteRiskSnapshot(const CurveInstrumentCalibrationResult_& fit,
                                                                  const ScriptCurveQuoteRiskAccuracy_& accuracy) {
        if (fit.report_ && Bytes(fit.report_->Data().summary_.domain_) != "Curve") {
            ValidateAccuracy(accuracy, fit.axes_.observations_.size());
            CurveInstrumentCalibrationSpec_ axesSpec;
            axesSpec.observations_ = fit.axes_.observations_;
            auto data = std::make_shared<ScriptCurveQuoteRiskSnapshot_::Data_>();
            data->accuracy_ = accuracy;
            data->axes_ = std::make_shared<const ScriptCurveQuoteRiskAxes_>(Axes(axesSpec, fit));
            data->diagnostics_.push_back(
                Diagnostic("ScriptCurveQuoteRiskUnsupportedDomain", "/fit/report/domain", "Curve", fit.report_->Data().summary_.domain_));
            return ScriptCurveQuoteRiskSnapshot_(std::move(data));
        }
        Check(bool(fit.resolvedSpec_), INPUT_ERROR, "/fit/resolved_spec", "owned resolved calibration spec");
        Check(bool(fit.resolvedOptions_), INPUT_ERROR, "/fit/resolved_options", "owned resolved calibration options");
        Token(fit.solveMode_, INPUT_ERROR, "/fit/solve_mode");
        ValidateAccuracy(accuracy, fit.resolvedSpec_->observations_.size());
        const auto context = ContextJson(*fit.resolvedSpec_);
        auto data = std::make_shared<ScriptCurveQuoteRiskSnapshot_::Data_>();
        data->accuracy_ = accuracy;
        data->axes_ = std::make_shared<const ScriptCurveQuoteRiskAxes_>(Axes(*fit.resolvedSpec_, fit));
        data->context_ = context;
        data->contextIdentity_ = Fingerprint(context);
        try {
            data->spec_ = std::make_shared<const CurveInstrumentCalibrationSpec_>(CopySpec(*fit.resolvedSpec_));
            ValidateQuotes(*data->spec_);
            const PreparedCurveInstruments_ prepared(*data->spec_, *fit.resolvedOptions_);
            ValidateFit(fit, prepared);
            data->parameters_ = fit.parameters_;
            data->identity_ = Fingerprint(Obj({{"context", Str(data->contextIdentity_)},
                                               {"parameters", Numbers(data->parameters_)},
                                               {"accuracy", Obj({{"residual_limits", Numbers(accuracy.residualAbsoluteLimits_)},
                                                                 {"min_rcond", Num(accuracy.minReciprocalCondition_)},
                                                                 {"response_error", Num(accuracy.responseBackwardErrorLimit_)},
                                                                 {"pivot", Num(accuracy.relativePivotTolerance_)}})}}));
            data->structureValid_ = true;
            data->diagnostics_.push_back(Diagnostic("ScriptCurveQuoteRiskNumericsUnavailable", "/risk", "validated implicit mapping and PV gradient",
                                                    "structural validation only"));
        } catch (const CalibrationError_& error) {
            data->diagnostics_.push_back(error.Failure().diagnostic_);
        } catch (const std::exception& error) {
            data->diagnostics_.push_back(Diagnostic("ScriptCurveQuoteRiskUnavailable", "/fit", "supported deterministic structure", error.what()));
        }
        return ScriptCurveQuoteRiskSnapshot_(std::move(data));
    }

    ScriptCurveQuoteRiskValidation_ ScriptCurveQuoteRiskSnapshot_::ValidateContext(const CurveInstrumentCalibrationSpec_& context) const {
        if (!StructureValid())
            return {false, data_->axes_, data_->diagnostics_};
        auto actual = ContextJson(context);
        try {
            if (context.geometry_.knotPolicy_ != CurveKnotPolicy_::Value_::INPUT)
                actual = ContextJson(PreparedCurveInstruments_(context).Spec());
            if (const auto d = Difference(data_->context_, actual, "/context"))
                return {false, data_->axes_, {ContextDiagnostic(*data_->spec_, *d)}};
            return {true, data_->axes_, {}};
        } catch (const CalibrationError_& error) {
            return {false, data_->axes_, {error.Failure().diagnostic_}};
        } catch (const std::exception& error) {
            return {false, data_->axes_, {Diagnostic("ScriptCurveQuoteRiskUnavailable", "/context", "compatible context", error.what())}};
        }
    }
} // namespace Dal
