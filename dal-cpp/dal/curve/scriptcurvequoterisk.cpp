#include <dal/platform/platform.hpp>

#include <set>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/curveinstrumentcontext_internal.hpp>
#include <dal/curve/curveinstrumentevaluation_internal.hpp>
#include <dal/curve/curvejacobian.hpp>
#include <dal/curve/scriptcurvequoterisk.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/optimization/implicitroot.hpp>
#include <dal/storage/json.hpp>

namespace Dal {
    using namespace CalibrationInternal;
    using namespace CurveInstrumentInternal;

    struct ScriptCurveQuoteRiskSnapshot_::Data_ {
        bool structureValid_ = false;
        ScriptCurveQuoteRiskAccuracy_ accuracy_;
        std::shared_ptr<const ScriptCurveQuoteRiskAxes_> axes_;
        std::shared_ptr<const CurveInstrumentCalibrationSpec_> spec_;
        Vector_<> parameters_;
        Vector_<CalibrationDiagnostic_> diagnostics_;
        Json_ context_;
        String_ contextIdentity_, identity_;
        ScriptCurveQuoteRiskNumerics_ numerics_;
        Matrix_<> quoteMap_;
        std::shared_ptr<const ImplicitRootLinearization_> linearization_;
    };

    namespace {
        constexpr const char* INPUT_ERROR = "InvalidScriptCurveQuoteRiskInput";
        constexpr const char* MISMATCH = "ScriptCurveQuoteRiskIdentityMismatch";

        Handle_<DiscountCurve_> CopyCurve(const Handle_<DiscountCurve_>& curve) {
            if (!curve)
                return {};
            const auto copy = handle_cast<DiscountCurve_>(JSON::ReadString(JSON::WriteString(*curve), false));
            Check(bool(copy), INPUT_ERROR, "/context/market", "owned discount curve snapshot");
            return copy;
        }
        CurveInstrumentCalibrationSpec_ CopySpec(CurveInstrumentCalibrationSpec_ spec) {
            for (auto& row : spec.observations_) {
                const Handle_<CalibrationMarketQuote_> quote(new CalibrationMarketQuote_(*row->Quote()));
                row = NewCalibrationObservation(row->Id(), row->Instrument(), row->MeasureId(), quote, row->ErrorScale());
            }
            spec.geometry_.baseCurve_ = CopyCurve(spec.geometry_.baseCurve_);
            for (auto& [key, curve] : spec.market_.discountCurves_)
                curve = CopyCurve(curve);
            for (auto& [key, curve] : spec.market_.forwardCurves_)
                curve = CopyCurve(curve);
            return spec;
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
        void ValidateResponseLimit(double limit) {
            Check(std::isfinite(limit) && limit > 0. && limit <= 1., INPUT_ERROR, "/accuracy/response_backward_error_limit",
                  "explicit finite backward error limit in (0,1]");
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
            ValidateResponseLimit(accuracy.responseBackwardErrorLimit_);
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
        void ValidateEvidence(const CurveInstrumentCalibrationResult_& fit, const CurveInstrumentCalibrationResult_& layout) {
            Check(bool(fit.evidence_), MISMATCH, "/fit/evidence", "original preparation and solve evidence");
            Check(Bytes(fit.evidence_->ConfigurationIdentity()) == Bytes(layout.evidence_->ConfigurationIdentity()), MISMATCH,
                  "/fit/configuration_identity", "configuration bound at original preparation");
            Check(Bytes(fit.evidence_->PlanIdentity()) == Bytes(fit.planIdentity_) &&
                      Bytes(fit.evidence_->CandidateIdentity()) == Bytes(fit.candidateIdentity_) &&
                      Bytes(fit.evidence_->ObjectiveIdentity()) == Bytes(fit.objectiveIdentity_) && !fit.objectiveIdentity_.empty(),
                  MISMATCH, "/fit/evidence", "plan, candidate and objective bound at original solve");
        }
        void ValidateMetric(const Matrix_<>& actual, const String_& identity, const String_& field) {
            Check(!identity.empty() && Bytes(MetricIdentity(actual)) == Bytes(identity), MISMATCH, field, "metric captured before original solve");
        }
        void ValidateReportParameters(const CurveInstrumentCalibrationResult_& fit) {
            const auto& report = fit.report_->Data();
            Check(report.parameters_.size() == fit.parameters_.size() && report.objective_.nativeLayout_.size() == fit.parameters_.size(), MISMATCH,
                  "/fit/report/objective", "original native objective layout");
            for (size_t i = 0; i < fit.parameters_.size(); ++i) {
                const auto& p = report.parameters_[i];
                Check(Bytes(p.id_) == Bytes(ParameterId(fit.freeParameters_[i])) && Bytes(report.objective_.nativeLayout_[i]) == Bytes(p.id_),
                      MISMATCH, "/fit/report/objective", "original native parameter ID");
                Check(p.initial_ == fit.initialParameters_[i] && p.scale_ == fit.parameterScales_[i] && p.value_ == fit.parameters_[i], MISMATCH,
                      "/fit/report/objective", "original initial value, scale and fitted value");
            }
        }
        void ValidateObjective(const CurveInstrumentCalibrationResult_& fit, const CurveInstrumentSolver_& solver) {
            Check(Bytes(ObjectiveIdentity(fit, solver)) == Bytes(fit.evidence_->ObjectiveIdentity()), MISMATCH, "/fit/objective_identity",
                  "objective bound at original solve");
            const auto& objective = fit.report_->Data().objective_;
            Check(objective.theta0_ == fit.initialParameters_ && objective.parameterScales_ == fit.parameterScales_, MISMATCH,
                  "/fit/report/objective", "original initial parameters and coordinate scales");
            Check(objective.lambda_ == fit.fitWeight_ && objective.eta_ == fit.fitNormTolerance_, MISMATCH, "/fit/report/objective",
                  "original objective settings");
            Check(bool(objective.weightMatrix_), MISMATCH, "/fit/report/objective", "original native smoothing metric");
            ValidateMetric(fit.nativeWeights_, fit.evidence_->MetricIdentity(), "/fit/native_weights");
            ValidateMetric(*objective.weightMatrix_, fit.evidence_->MetricIdentity(), "/fit/report/objective");
            ValidateReportParameters(fit);
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
            ValidateEvidence(fit, layout);
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
            ValidateObjective(fit, spec.solver_);
        }

        SquareMatrix_<> SquareJacobian(const Matrix_<>& matrix) {
            SquareMatrix_<> result(matrix.Rows());
            for (int i = 0; i < matrix.Rows(); ++i)
                for (int j = 0; j < matrix.Cols(); ++j)
                    result(i, j) = matrix(i, j);
            return result;
        }
        class QuoteEquation_ final : public ImplicitRootEquation_ {
            ImplicitRootEvaluation_ evaluation_;

        public:
            QuoteEquation_(const ScriptCurveQuoteRiskNumerics_& numerics, const Matrix_<>& quoteMap)
                : evaluation_{numerics.rawResiduals_, SquareJacobian(numerics.nativeJacobian_), quoteMap} {
                for (int i = 0; i < quoteMap.Rows(); ++i)
                    for (int j = 0; j < quoteMap.Cols(); ++j)
                        evaluation_.inputJacobian_(i, j) = -quoteMap(i, j);
            }
            [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>&, const Vector_<>&) const override { return evaluation_; }
        };
        Matrix_<> IdentityMatrix(int n) {
            Matrix_<> result(n, n, 0.);
            for (int i = 0; i < n; ++i)
                result(i, i) = 1.;
            return result;
        }
        void ValidateRoot(const ScriptCurveQuoteRiskSnapshot_::Data_& data) {
            for (size_t i = 0; i < data.numerics_.rawResiduals_.size(); ++i) {
                const auto residual = data.numerics_.rawResiduals_[i];
                Check(std::isfinite(residual), "ScriptCurveQuoteRiskNonFinite", "/mapping/residuals/" + OrdinalText(i), "finite raw root residual",
                      NumberText(residual), RowDiagnostic(*data.spec_->observations_[i]));
                Check(std::abs(residual) <= data.accuracy_.residualAbsoluteLimits_[i], "ScriptCurveQuoteRiskRootResidual",
                      "/mapping/residuals/" + OrdinalText(i), NumberText(data.accuracy_.residualAbsoluteLimits_[i]), NumberText(residual),
                      RowDiagnostic(*data.spec_->observations_[i]));
            }
        }
        void ValidateResponseErrors(const Vector_<>& errors, const ScriptCurveQuoteRiskSnapshot_::Data_& data, bool transpose) {
            for (size_t i = 0; i < errors.size(); ++i) {
                auto d = Diagnostic("ScriptCurveQuoteRiskResponseAccuracy", transpose ? "/mapping/transpose_error/" : "/mapping/response_error/",
                                    NumberText(data.accuracy_.responseBackwardErrorLimit_), NumberText(errors[i]));
                *d.field_ += OrdinalText(i);
                if (transpose)
                    d.parameterKey_ = data.axes_->parameters_[i].id_;
                else
                    d.quoteId_ = data.axes_->quotes_[i].id_;
                Check(std::isfinite(errors[i]) && errors[i] <= data.accuracy_.responseBackwardErrorLimit_, d.code_, *d.field_, *d.expected_,
                      *d.actual_, d);
            }
        }
        Matrix_<> ParameterResponse(const ScriptCurveQuoteRiskSnapshot_::Data_& data, ScriptCurveQuoteRiskNumerics_* numerics) try {
            const auto reverse = data.linearization_->Reverse(IdentityMatrix(static_cast<int>(data.parameters_.size())));
            Matrix_<> response(reverse.inputs_.Cols(), reverse.inputs_.Rows());
            for (int i = 0; i < response.Rows(); ++i)
                for (int j = 0; j < response.Cols(); ++j)
                    response(i, j) = reverse.inputs_(j, i);
            numerics->transposeBackwardErrors_ = reverse.transposeBackwardErrors_;
            numerics->responseBackwardErrors_ = LinearSolveBackwardErrors(SquareJacobian(data.numerics_.nativeJacobian_), data.quoteMap_, response);
            ValidateResponseErrors(numerics->transposeBackwardErrors_, data, true);
            ValidateResponseErrors(numerics->responseBackwardErrors_, data, false);
            return response;
        } catch (const CalibrationError_&) {
            throw;
        } catch (const std::exception& error) {
            Fail("ScriptCurveQuoteRiskResponseAccuracy", "/mapping/response", "checked finite response", error.what());
        }
        std::shared_ptr<const ImplicitRootLinearization_> CaptureRoot(const ScriptCurveQuoteRiskSnapshot_::Data_& data) {
            Vector_<> quotes;
            for (const auto& quote : data.axes_->quotes_)
                quotes.push_back(quote.value_);
            try {
                // Retain measured errors before applying the caller's stricter response gate.
                return std::make_shared<const ImplicitRootLinearization_>(QuoteEquation_(data.numerics_, data.quoteMap_), data.parameters_, quotes,
                                                                          ImplicitRootAccuracyPolicy_{data.accuracy_.residualAbsoluteLimits_, 1.},
                                                                          data.accuracy_.relativePivotTolerance_);
            } catch (const std::exception& error) {
                Fail("ScriptCurveQuoteRiskJacobianRejected", "/mapping/jacobian", "finite full-rank Jacobian with accepted pivots", error.what());
            }
        }
        void CaptureNumerics(ScriptCurveQuoteRiskSnapshot_::Data_* data, CurveInstrumentCalibrationOptions_ options) {
            options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
            options.preparationCache_.reset();
            const PreparedCurveInstruments_ prepared(*data->spec_, options);
            const auto evaluation = prepared.Evaluate(data->parameters_, false);
            data->numerics_.modelMeasures_ = evaluation.modelMeasures_;
            data->numerics_.rawResiduals_ = evaluation.rawResiduals_;
            const auto jacobian = prepared.Jacobian(data->parameters_);
            Check(bool(jacobian), "ScriptCurveQuoteRiskDerivativeUnavailable", "/mapping/jacobian", "native AAD Jacobian");
            data->numerics_.nativeJacobian_ = static_cast<const XCurveJacobian_&>(*jacobian).j_;
            data->quoteMap_ = prepared.Axes().quoteMap_;
            ValidateRoot(*data);
            data->linearization_ = CaptureRoot(*data);
            data->numerics_.factorizationCount_ = 1;
            const double rcond = data->linearization_->ReciprocalJacobianConditionInfinity();
            data->numerics_.reciprocalCondition_ = rcond;
            Check(std::isfinite(rcond) && rcond >= data->accuracy_.minReciprocalCondition_, "ScriptCurveQuoteRiskConditionRejected",
                  "/mapping/reciprocal_condition", NumberText(data->accuracy_.minReciprocalCondition_), NumberText(rcond));
            static_cast<void>(ParameterResponse(*data, &data->numerics_));
        }
    } // namespace

    ScriptCurveQuoteRiskValidation_::ScriptCurveQuoteRiskValidation_(bool passed,
                                                                     std::shared_ptr<const ScriptCurveQuoteRiskAxes_> axes,
                                                                     Vector_<CalibrationDiagnostic_> diagnostics)
        : passed_(passed), axes_(std::move(axes)), diagnostics_(std::move(diagnostics)) {}
    ScriptCurveQuoteRiskSnapshot_::ScriptCurveQuoteRiskSnapshot_(std::shared_ptr<const Data_> data) : data_(std::move(data)) {}
    double ScriptCurveQuoteRiskResult_::PV() const {
        REQUIRE(Available(), "Script curve PV quote risk is unavailable; inspect diagnostics");
        return *pv_;
    }
    const Vector_<>& ScriptCurveQuoteRiskResult_::NativeGradient() const {
        REQUIRE(Available(), "Script curve PV quote risk is unavailable; inspect diagnostics");
        return nativeGradient_;
    }
    const Vector_<>& ScriptCurveQuoteRiskResult_::RawSensitivity() const {
        REQUIRE(Available(), "Script curve PV quote risk is unavailable; inspect diagnostics");
        return raw_;
    }
    const Vector_<>& ScriptCurveQuoteRiskResult_::DV01() const {
        REQUIRE(Available(), "Script curve PV quote risk is unavailable; inspect diagnostics");
        return dv01_;
    }
    namespace {
        Vector_<int> SelectedQuotes(const ScriptCurveQuoteRiskRequest_& request, const ScriptCurveQuoteRiskAxes_& axes) {
            Vector_<int> result;
            if (request.quoteIds_.empty()) {
                for (size_t i = 0; i < axes.quotes_.size(); ++i)
                    result.push_back(static_cast<int>(i));
                return result;
            }
            std::set<std::string> seen;
            for (size_t i = 0; i < request.quoteIds_.size(); ++i) {
                const auto& id = request.quoteIds_[i];
                const auto field = "/request/quote_ids/" + OrdinalText(i);
                Check(seen.insert(Bytes(id)).second, "ScriptCurveQuoteRiskDuplicateSelection", field, "unique quote ID", id);
                const auto quote = std::find_if(axes.quotes_.begin(), axes.quotes_.end(), [&](const auto& q) { return Bytes(q.id_) == Bytes(id); });
                Check(quote != axes.quotes_.end(), "ScriptCurveQuoteRiskUnknownSelection", field, "snapshot quote ID (case sensitive)", id);
                result.push_back(static_cast<int>(quote - axes.quotes_.begin()));
            }
            return result;
        }
        struct NativePrice_ {
            double value_;
            Vector_<> gradient_;
        };
        NativePrice_ NativePrice(const PreparedCurvePrice_& prepared, const Vector_<>& theta, const Date_& today) {
            try {
                AAD::RequireRecordingModeChangeAllowed();
            } catch (const std::exception& error) {
                Fail("ScriptCurveQuoteRiskNestedRecording", "/pv/recording", "no active independent recording", error.what());
            }
            const auto mode = AAD::SetNumResultsForAAD(false, 1);
            AAD::RecordingScope_ recording;
            Vector_<AAD::Number_> active(theta.size());
            for (size_t i = 0; i < theta.size(); ++i)
                recording.RegisterInput(active[i], theta[i]);
            recording.StartRecording();
            const CandidateCurveProvider_<AAD::Number_> provider(prepared.geometry_, active);
            CurveScriptInternal::EvaluationScratch_<AAD::Number_> scratch;
            auto price = CashflowMeasure(prepared.plan_, provider, today, &scratch);
            Check(std::isfinite(static_cast<double>(price)), "ScriptCurveQuoteRiskNonFinite", "/pv", "finite Price");
            recording.FinishRecording();
            recording.ClearAdjoints();
            AAD::Adjoint(price) = 1.;
            recording.Reverse();
            NativePrice_ result{static_cast<double>(price), Vector_<>(theta.size())};
            for (size_t i = 0; i < theta.size(); ++i) {
                result.gradient_[i] = AAD::Adjoint(active[i]);
                Check(std::isfinite(result.gradient_[i]), "ScriptCurveQuoteRiskNonFinite", "/pv/native_gradient/" + OrdinalText(i),
                      "finite native gradient", NumberText(result.gradient_[i]));
            }
            recording.Close();
            return result;
        }
        ImplicitRootAdjoints_ PriceResponse(const ScriptCurveQuoteRiskSnapshot_::Data_& data, const Vector_<>& gradient) {
            Matrix_<> seed(static_cast<int>(gradient.size()), 1);
            for (int i = 0; i < seed.Rows(); ++i)
                seed(i, 0) = gradient[i];
            const auto reverse = data.linearization_->Reverse(seed);
            const double error = reverse.transposeBackwardErrors_[0];
            Check(std::isfinite(error) && error <= data.accuracy_.responseBackwardErrorLimit_, "ScriptCurveQuoteRiskResponseAccuracy",
                  "/pv/transpose_error", NumberText(data.accuracy_.responseBackwardErrorLimit_), NumberText(error));
            return reverse;
        }
    } // namespace

    ScriptCurveQuoteRiskResult_ ValueCalibrationInstrumentWithQuoteRisk(const Handle_<CalibrationInstrument_>& instrument,
                                                                        const ScriptCurveQuoteRiskSnapshot_& snapshot,
                                                                        const CurveInstrumentCalibrationSpec_& context,
                                                                        const ScriptCurveQuoteRiskRequest_& request) {
        Check(bool(instrument), INPUT_ERROR, "/instrument", "bound calibration instrument");
        ScriptCurveQuoteRiskResult_ result;
        const auto& data = *snapshot.data_;
        result.axes_ = *data.axes_;
        result.numerics_ = data.numerics_;
        result.snapshotIdentity_ = data.identity_;
        result.contextIdentity_ = data.contextIdentity_;
        result.sourceIdentity_ = instrument->Definition()->SourceIdentity();
        result.definitionIdentity_ = instrument->Definition()->DefinitionIdentity();
        result.bindingIdentity_ = instrument->BindingIdentity();
        result.currency_ = instrument->Definition()->Spec().header_.resultCurrency_;
        const auto validation = snapshot.ValidateContext(context);
        if (!validation.Passed()) {
            result.diagnostics_ = validation.Diagnostics();
            return result;
        }
        try {
            if (!snapshot.MappingAvailable()) {
                result.diagnostics_ = data.diagnostics_;
                return result;
            }
            Check(!request.liveQuotePayoff_, "ScriptCurveQuoteRiskLiveQuoteUnsupported", "/request/live_quote_payoff", "fixed contract; V_q=0",
                  "true");
            Check(Bytes(request.measureId_) == "Price", "ScriptCurveQuoteRiskUnsupportedMeasure", "/request/measure_id", "Price", request.measureId_);
            const auto selected = SelectedQuotes(request, *data.axes_);
            const auto prepared = PrepareCurvePrice(*data.spec_, instrument, request.measureId_);
            Check(prepared.plan_.Measure().kind_ == CalibrationMeasureKind_::Value_::Price, "ScriptCurveQuoteRiskUnsupportedMeasure",
                  "/request/measure_id", "Price kind", request.measureId_);
            const auto price = NativePrice(prepared, data.parameters_, data.spec_->geometry_.today_);
            const auto reverse = PriceResponse(data, price.gradient_);
            result.numerics_.transposeBackwardErrors_ = reverse.transposeBackwardErrors_;
            result.axes_.quotes_.clear();
            for (const int i : selected) {
                const double raw = reverse.inputs_(i, 0), dv01 = raw * 1e-4;
                Check(std::isfinite(raw) && std::isfinite(dv01), "ScriptCurveQuoteRiskNonFinite", "/pv/raw", "finite quote risk");
                result.axes_.quotes_.push_back(data.axes_->quotes_[i]);
                result.raw_.push_back(raw);
                result.dv01_.push_back(dv01);
            }
            result.nativeGradient_ = price.gradient_;
            result.pv_ = price.value_;
        } catch (const CalibrationError_& error) {
            auto d = error.Failure().diagnostic_;
            d.instrumentId_ = instrument->Id();
            d.measureId_ = request.measureId_;
            result.diagnostics_.push_back(std::move(d));
        } catch (const std::exception& error) {
            auto d = Diagnostic("ScriptCurveQuoteRiskEvaluationUnavailable", "/pv", "independent finite deterministic Price and checked response",
                                error.what());
            d.instrumentId_ = instrument->Id();
            d.measureId_ = request.measureId_;
            result.diagnostics_.push_back(std::move(d));
        }
        return result;
    }
    bool ScriptCurveQuoteRiskSnapshot_::StructureValid() const { return data_->structureValid_; }
    bool ScriptCurveQuoteRiskSnapshot_::MappingAvailable() const { return data_->structureValid_ && data_->diagnostics_.empty(); }
    const ScriptCurveQuoteRiskNumerics_& ScriptCurveQuoteRiskSnapshot_::Numerics() const { return data_->numerics_; }
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
                                               {"configuration", Str(fit.evidence_->ConfigurationIdentity())},
                                               {"parameters", Numbers(data->parameters_)},
                                               {"accuracy", Obj({{"residual_limits", Numbers(accuracy.residualAbsoluteLimits_)},
                                                                 {"min_rcond", Num(accuracy.minReciprocalCondition_)},
                                                                 {"response_error", Num(accuracy.responseBackwardErrorLimit_)},
                                                                 {"pivot", Num(accuracy.relativePivotTolerance_)}})}}));
            data->structureValid_ = true;
            CaptureNumerics(data.get(), *fit.resolvedOptions_);
        } catch (const CalibrationError_& error) {
            data->diagnostics_.push_back(error.Failure().diagnostic_);
        } catch (const std::exception& error) {
            data->diagnostics_.push_back(Diagnostic("ScriptCurveQuoteRiskUnavailable", "/fit", "supported deterministic structure", error.what()));
        }
        return ScriptCurveQuoteRiskSnapshot_(std::move(data));
    }

    ScriptCurveQuoteMapping_::ScriptCurveQuoteMapping_(std::shared_ptr<const ScriptCurveQuoteRiskAxes_> axes,
                                                       ScriptCurveQuoteRiskNumerics_ numerics,
                                                       Vector_<CalibrationDiagnostic_> diagnostics,
                                                       std::optional<Matrix_<>> response)
        : axes_(std::move(axes)), numerics_(std::move(numerics)), diagnostics_(std::move(diagnostics)), response_(std::move(response)) {}
    const Matrix_<>& ScriptCurveQuoteMapping_::ParameterResponse() const {
        REQUIRE(Available(), "Script curve parameter quote mapping is unavailable; inspect diagnostics");
        return *response_;
    }
    ScriptCurveQuoteMapping_ ScriptCurveQuoteRiskSnapshot_::ParameterQuoteMapping(const CurveInstrumentCalibrationSpec_& context) const {
        const auto validation = ValidateContext(context);
        if (!validation.Passed())
            return {data_->axes_, data_->numerics_, validation.Diagnostics(), std::nullopt};
        if (!MappingAvailable())
            return {data_->axes_, data_->numerics_, data_->diagnostics_, std::nullopt};
        auto numerics = data_->numerics_;
        try {
            auto response = ParameterResponse(*data_, &numerics);
            return {data_->axes_, std::move(numerics), {}, std::move(response)};
        } catch (const CalibrationError_& error) {
            return {data_->axes_, std::move(numerics), {error.Failure().diagnostic_}, std::nullopt};
        } catch (const std::exception& error) {
            return {data_->axes_,
                    std::move(numerics),
                    {Diagnostic("ScriptCurveQuoteRiskResponseAccuracy", "/mapping/response", "checked finite response", error.what())},
                    std::nullopt};
        }
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
