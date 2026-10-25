#include <dal/platform/platform.hpp>

#include <dal-public/src/xccymontecarlobasket.hpp>
#include <dal/calibration/validation_internal.hpp>

namespace Dal {
    namespace {
        using namespace CalibrationInternal;
        Json_ Ordinal(size_t value) { return Str(OrdinalText(value)); }
        Json_ Integer(int value) { return Str(String_(std::to_string(value))); }
        Json_ Strings(const Vector_<String_>& values) {
            return Array(values, [](const auto& x) { return Str(x); });
        }
        Json_ Numbers(const Vector_<>& values) {
            return Array(values, [](double x) { return Num(x); });
        }
        Json_ Matrix(const Matrix_<>& values) {
            auto rows = Json_::Array();
            for (int i = 0; i < values.Rows(); ++i) {
                auto row = Json_::Array();
                for (int j = 0; j < values.Cols(); ++j)
                    row.array_.push_back(Num(values(i, j)));
                rows.array_.push_back(std::move(row));
            }
            return rows;
        }
        Json_ Range(const XccyMcRange_& r) {
            return Obj({{"first_path", Ordinal(r.firstPath_)},
                        {"path_end", Ordinal(r.pathEnd_)},
                        {"first_coordinate", Ordinal(r.firstCoordinate_)},
                        {"coordinate_end", Ordinal(r.coordinateEnd_)}});
        }
        Json_ Diagnostic(const CalibrationDiagnostic_& d) {
            return Obj({{"code", Str(d.code_)},
                        {"message", Str(d.message_)},
                        {"stage", Str(d.stage_.String())},
                        {"field", Optional(d.field_, Str)},
                        {"observation_id", Optional(d.observationId_, Str)},
                        {"instrument_id", Optional(d.instrumentId_, Str)},
                        {"measure_id", Optional(d.measureId_, Str)},
                        {"quote_id", Optional(d.quoteId_, Str)},
                        {"parameter_key", Optional(d.parameterKey_, Str)},
                        {"source_row", Optional(d.sourceRow_, Integer)},
                        {"statement", Optional(d.statement_, Integer)},
                        {"event_date", Optional(d.eventDate_, [](const auto& x) { return Str(Date::ToString(x)); })},
                        {"actual", Optional(d.actual_, Str)},
                        {"expected", Optional(d.expected_, Str)}});
        }
        Json_ Mapping(const CalibrationMcMapping_& m) {
            return Obj({{"identity", Str(m.identity_)},
                        {"dimension", Ordinal(m.dimension_)},
                        {"timeline", Numbers(m.timeline_)},
                        {"normal_coordinates", Strings(m.normalCoordinates_)},
                        {"stream_version", Str(m.streamVersion_)},
                        {"seed", Integer(m.seed_)},
                        {"generator", Str(m.generator_)},
                        {"normal_precision", Str(m.normalPrecision_)},
                        {"numeraire", Str(m.numeraire_)},
                        {"brownian_bridge", Bool(m.brownianBridge_)},
                        {"transition_layout", Str("dal.xccy-gaussian-exact/1")},
                        {"economic_factors", Ordinal(3)}});
        }
        Json_ Source(const Script::SourceLocation_& s) {
            return Obj({{"offset", Ordinal(s.offset_)},
                        {"line", Ordinal(s.line_)},
                        {"column", Ordinal(s.column_)},
                        {"row", Ordinal(s.row_)},
                        {"event_date", Optional(s.eventDate_, [](const auto& x) { return Str(Date::ToString(x)); })}});
        }
        Json_ ObservationUse(const Script::ObservationUse_& u) {
            return Obj({{"source", Source(u.source_)},
                        {"event_id", Ordinal(u.eventId_)},
                        {"statement_id", Ordinal(u.statementId_)},
                        {"node_id", Ordinal(u.nodeId_)},
                        {"index_original", Str(u.indexOriginal_)},
                        {"legacy_spot", Bool(u.legacySpot_)},
                        {"fixing_date", Optional(u.fixingDate_, [](const auto& x) { return Str(Date::ToString(x)); })}});
        }
        Json_ Observation(const Script::ObservationRequest_& o) {
            return Obj({{"canonical_index", Str(o.key_.canonicalIndex_)},
                        {"fixing_time", Str(DateTime::ToString(o.key_.fixingTime_))},
                        {"uses", Array(o.uses_, ObservationUse)},
                        {"historical", Bool(o.historical_)},
                        {"history_value_id", Optional(o.historyValueId_, Ordinal)},
                        {"model_slot", Optional(o.modelSlot_, [](const auto& x) {
                             return Obj({{"sample_id", Ordinal(x.sampleId_)}, {"output_id", Ordinal(x.outputId_)}});
                         })}});
        }
        Json_ Fixings(const Handle_<MarketFixingSnapshot_>& snapshot) {
            if (!snapshot)
                return {};
            auto values = Json_::Array();
            for (const auto& [index, history] : snapshot->Values())
                for (const auto& [time, value] : history)
                    values.array_.push_back(Obj({{"index", Str(index)}, {"time", Str(DateTime::ToString(time))}, {"value", Num(value)}}));
            return values;
        }
        Json_ Context(const CalibrationPreparationContext_& c) {
            const auto& x = *c.xccy_;
            return Obj({{"valuation_time", Str(DateTime::ToString(x.valuationTime_))},
                        {"pair", Str(FxIndexName(x.pair_))},
                        {"collateral_currency", Str(x.collateralCurrency_.String())},
                        {"discount_route", Str(x.discountRoute_)},
                        {"fixings_supplied", Bool(bool(x.fixings_))},
                        {"fixings", Fixings(x.fixings_)},
                        {"today_fixing_policy", Str("NativeXccyTimestamp")},
                        {"convention_state", Str(c.conventionState_)}});
        }
        Json_ MemberAllocation(const XccyMcBasketMemberAllocation_& m) {
            return Obj({{"member_id", Str(m.memberId_)},
                        {"instrument_id", Str(m.instrumentId_)},
                        {"instrument_identity", Str(m.instrumentIdentity_)},
                        {"source_identity", Str(m.sourceIdentity_)},
                        {"definition_identity", Str(m.definitionIdentity_)},
                        {"binding_identity", Str(m.bindingIdentity_)},
                        {"measure_id", Str(m.measureId_)},
                        {"measure_unit", Str(m.measureUnit_)},
                        {"preparation_identity", Str(m.preparationIdentity_)},
                        {"context_identity", Str(m.contextIdentity_)},
                        {"compiler_identity", Str(m.compilerIdentity_)},
                        {"low_binding_identity", Str(m.lowBindingIdentity_)},
                        {"high_binding_identity", Str(m.highBindingIdentity_)},
                        {"native_map_identity", Str(m.nativeMapIdentity_)},
                        {"context", Context(m.frozenContext_)},
                        {"mapping", Mapping(m.mapping_)},
                        {"grid", Array(m.grid_, [](const auto& x) { return Str(Date::ToString(x)); })},
                        {"observations", Array(m.observations_.Requests(), Observation)},
                        {"known_values", Numbers(m.observations_.KnownValues())},
                        {"observation_requests", Array(m.requests_,
                                                       [](const auto& r) {
                                                           return Obj({{"kind", Str(r.kind_.String())},
                                                                       {"currency", Str(r.currency_)},
                                                                       {"tenor", Str(r.tenor_)},
                                                                       {"route", Str(r.route_)},
                                                                       {"start", Str(Date::ToString(r.start_))},
                                                                       {"end", Str(Date::ToString(r.end_))},
                                                                       {"accrual", Num(r.accrual_)}});
                                                       })},
                        {"first_coordinate", Ordinal(m.firstCoordinate_)},
                        {"coordinate_end", Ordinal(m.coordinateEnd_)},
                        {"phase_ranges", Array(m.phaseRanges_, Range)},
                        {"ranges", Array(m.ranges_, Range)},
                        {"deterministic", Bool(m.deterministic_)}});
        }
        Json_ Allocation(const XccyMcBasketAllocation_& a) {
            return Obj({{"version", Str(a.version_)},
                        {"identity", Str(a.identity_)},
                        {"execution_identity", Str(a.executionIdentity_)},
                        {"stream_version", Str(a.streamVersion_)},
                        {"normal_precision", Str(a.normalPrecision_)},
                        {"seek_convention", Str(a.seekConvention_)},
                        {"phase", Str(a.phase_.String())},
                        {"seed", Integer(a.seed_)},
                        {"batches", Integer(a.batches_)},
                        {"paths_per_batch", Integer(a.pathsPerBatch_)},
                        {"reserved_paths", Ordinal(a.reservedPaths_)},
                        {"coordinate_end", Ordinal(a.coordinateEnd_)},
                        {"expected_consumption", Ordinal(a.expectedConsumption_)},
                        {"members", Array(a.members_, MemberAllocation)}});
        }
        Json_ Header(const XccyMcBasketExecutionHeader_& h) {
            const auto& s = h.settings_;
            return Obj({{"version", Str(h.version_)},
                        {"rule_id", Str(h.ruleId_)},
                        {"plan_identity", Str(h.planIdentity_)},
                        {"prepared_model_identity", Str(h.preparedModelIdentity_)},
                        {"attempted_model_identity", Optional(h.attemptedModelIdentity_, Str)},
                        {"native_map_identity", Optional(h.nativeMapIdentity_, Str)},
                        {"candidate_geometry_validated", Bool(h.candidateGeometryValidated_)},
                        {"derivative_method", Str(h.derivativeMethod_)},
                        {"parameter_labels", Strings(s.parameterLabels_)},
                        {"parameter_units", Strings(h.parameterUnits_)},
                        {"phase", Str(s.phase_.String())},
                        {"seed", Integer(s.seed_)},
                        {"batches", Integer(s.batches_)},
                        {"paths_per_batch", Integer(s.pathsPerBatch_)},
                        {"stream_version", Str(s.streamVersion_)},
                        {"compiled", Bool(s.compiled_)},
                        {"retain_paths", Bool(s.retainPaths_)}});
        }
        Json_ Coefficients(const CalibrationAffineCoefficients_& c, bool aad) {
            return Obj({{"b", Num(c.b_)}, {"a", Num(c.a_)}, {"db", aad ? Numbers(c.db_) : Json_()}, {"da", aad ? Numbers(c.da_) : Json_()}});
        }
        Json_ Estimate(const XccyMcBasketMemberEstimate_& e) {
            const auto& v = e.estimate_;
            const bool aad = !v.parameterLabels_.empty();
            return Obj({{"mean", Num(v.mean_)},
                        {"standard_error", Num(v.standardError_)},
                        {"validation", Bool(v.validation_)},
                        {"held_out", Bool(v.heldOut_)},
                        {"instrument_identity", Str(v.instrumentIdentity_)},
                        {"context_identity", Str(v.contextIdentity_)},
                        {"mapping_identity", Str(v.mappingIdentity_)},
                        {"measure_id", Str(v.measureId_)},
                        {"plan_identity", Str(v.planIdentity_)},
                        {"parameter_labels", Strings(v.parameterLabels_)},
                        {"gradient", aad ? Numbers(v.gradient_) : Json_()},
                        {"gradient_standard_error", aad ? Numbers(v.gradientStandardError_) : Json_()},
                        {"coefficients", Coefficients(e.coefficients_, aad)},
                        {"coefficient_covariance", Matrix(e.coefficientCovariance_)},
                        {"annuity_standard_error", Optional(v.affine_, [](const auto& x) { return Num(x.annuityStandardError_); })},
                        {"numerical_validation_passed", Json_()},
                        {"quote_risk_available", Bool(false)}});
        }
        Json_ Member(const XccyMcBasketMemberProgress_& m, bool aad) {
            return Obj({{"member_id", Str(m.memberId_)},
                        {"state", Str(m.state_)},
                        {"batches", Array(m.batches_,
                                          [](const auto& b) {
                                              return Obj({{"ordinal", Ordinal(b.ordinal_)},
                                                          {"paths", Ordinal(b.paths_)},
                                                          {"range", Range(b.range_)},
                                                          {"first_normals", Numbers(b.firstNormals_)},
                                                          {"row", Numbers(b.row_)}});
                                          })},
                        {"actual_ranges", Array(m.actualRanges_, Range)},
                        {"consumed_coordinates", Ordinal(m.consumedCoordinates_)},
                        {"completed_paths", Ordinal(m.completedPaths_)},
                        {"completed_batches", Ordinal(m.completedBatches_)},
                        {"completed_estimate", Optional(m.completedEstimate_, Estimate)},
                        {"failure", Optional(m.failure_, Diagnostic)},
                        {"retained_paths", Array(m.retainedPaths_, [aad](const auto& p) {
                             return Obj({{"path", Ordinal(p.path_)},
                                         {"first_coordinate", Ordinal(p.firstCoordinate_)},
                                         {"coefficients", Coefficients(p.coefficients_, aad)}});
                         })}});
        }
        Json_ Axes(const Vector_<XccyMcBasketAxis_>& axes) {
            return Array(axes,
                         [](const auto& x) { return Obj({{"member_id", Str(x.memberId_)}, {"label", Str(x.label_)}, {"unit", Str(x.unit_)}}); });
        }
        XccyMcSettings_ Settings(const XccyMonteCarloBasketRequest_& r) {
            const auto& s = r.simulation_;
            Check(Bytes(r.sampling_.mode_) == "PseudoRandom", "InvalidCalibrationPlan", "/sampling/mode", "PseudoRandom", r.sampling_.mode_);
            Check(!s.useBb_ && s.smooth_ == 0. && Bytes(s.normalPrecision_) == "Precise", "InvalidCalibrationPlan", "/simulation",
                  "PrepareXccyBasketMonteCarlo; zero smoothing, Precise normals and no Brownian bridge");
            Check(r.derivativeMethod_ == CalibrationDerivative_::Value_::None || r.derivativeMethod_ == CalibrationDerivative_::Value_::AAD,
                  "DerivativeUnavailable", "/derivative_method", "explicit None or AAD with ordered native labels");
            XccyMcSettings_ result;
            result.phase_ = r.phase_;
            result.batches_ = r.sampling_.batches_;
            result.pathsPerBatch_ = r.sampling_.pathsPerBatch_;
            result.seed_ = r.sampling_.seed_;
            result.streamVersion_ = r.sampling_.streamVersion_;
            result.compiled_ = s.compiled_.value_or(Script::DEFAULT_COMPILED);
            result.aad_ = r.derivativeMethod_ == CalibrationDerivative_::Value_::AAD;
            result.parameterLabels_ = r.parameterLabels_;
            return result;
        }
        CalibrationPreparationContext_ Context(const Handle_<XccyGaussianModelData_>& model, const Handle_<MarketFixingSnapshot_>& fixings) {
            Check(bool(model), "InvalidCalibrationPlan", "/model", "PrepareXccyBasketMonteCarlo; explicit nonnull XCCY Gaussian model");
            Check(bool(fixings), "InvalidCalibrationPlan", "/context/fixings", "PrepareXccyBasketMonteCarlo; explicit nonnull fixing snapshot");
            const auto& m = *model->market_;
            XccyScriptContext_ x;
            x.valuationTime_ = m.valuationTime_;
            x.pair_ = m.pair_;
            x.collateralCurrency_ = m.collateralCurrency_;
            x.discountRoute_ = m.discountRoute_;
            x.fixings_ = fixings;
            CalibrationPreparationContext_ context;
            context.xccy_ = std::make_shared<const XccyScriptContext_>(x);
            return context;
        }
        Handle_<CalibrationReport_>
        PreparedReport(const PreparedXccyMcBasket_& core, const Handle_<XccyGaussianModelData_>& model, const XccyMonteCarloBasketRequest_& request) {
            CalibrationReportSnapshot_ data;
            data.summary_.domain_ = "XCCYGaussianMonteCarloBasket";
            data.summary_.status_ = "Prepared";
            data.summary_.contextIdentity_ = core.PlanIdentity();
            data.execution_.modelIdentity_ = model->Identity();
            for (const auto& m : core.Allocation().members_) {
                CalibrationReportPlan_ p;
                p.observationId_ = m.memberId_;
                p.measureId_ = m.measureId_;
                p.ruleId_ = "dal.xccy-gaussian-script-mc-basket/1";
                p.backend_ = "ScriptMC";
                p.derivativeMethod_ = request.derivativeMethod_.String();
                p.sourceIdentity_ = m.sourceIdentity_;
                p.definitionIdentity_ = m.definitionIdentity_;
                p.bindingIdentity_ = m.bindingIdentity_;
                p.compilerIdentity_ = m.compilerIdentity_;
                p.contextIdentity_ = m.contextIdentity_;
                p.planIdentity_ = m.preparationIdentity_;
                data.plans_.push_back(std::move(p));
            }
            return Handle_<CalibrationReport_>(new CalibrationReport_(CalibrationStage_::Value_::Prepare, {}, {}, data));
        }
    } // namespace

    PreparedXccyMonteCarloBasket_::PreparedXccyMonteCarloBasket_(PreparedXccyMcBasket_ core,
                                                                 Handle_<XccyGaussianModelData_> model,
                                                                 Handle_<CalibrationReport_> report)
        : core_(std::move(core)), model_(std::move(model)), report_(std::move(report)) {}
    PreparedXccyMonteCarloBasket_ PrepareXccyBasketMonteCarlo(const Vector_<XccyMcBasketMember_>& members,
                                                              const Handle_<XccyGaussianModelData_>& model,
                                                              const XccyMonteCarloBasketRequest_& request) {
        const auto settings = Settings(request);
        const auto context = Context(model, request.fixings_);
        PreparedXccyMcBasket_ core(members, context, model, settings);
        const auto report = PreparedReport(core, model, request);
        return PreparedXccyMonteCarloBasket_(std::move(core), model, report);
    }
    XccyMonteCarloBasketResult_ PreparedXccyMonteCarloBasket_::Evaluate(const Handle_<XccyGaussianModelData_>& candidate) const {
        return core_.Evaluate(candidate);
    }
    String_ DumpXccyMonteCarloBasketResultJson(const XccyMonteCarloBasketResult_& result) {
        const auto& s = result.Snapshot();
        return String_(Jcs(Obj({{"schema", Str("dal.xccy-gaussian-public-basket-result/1")},
                                {"core_schema", Str(s.version_)},
                                {"status", Str("Estimate")},
                                {"basket_complete", Bool(s.basketComplete_)},
                                {"header", Header(s.header_)},
                                {"allocation", Allocation(s.allocation_)},
                                {"members", Array(s.members_, [&](const auto& m) { return Member(m, s.header_.settings_.aad_); })},
                                {"coefficient_axes", Axes(s.coefficientAxes_)},
                                {"output_axes", Axes(s.outputAxes_)},
                                {"coefficient_rows", Array(s.coefficientRows_, Numbers)},
                                {"coefficient_means", Numbers(s.coefficientMeans_)},
                                {"coefficient_covariance", Matrix(s.coefficientCovariance_)},
                                {"mean_covariance", Matrix(s.meanCovariance_)},
                                {"output_jacobian", Matrix(s.outputJacobian_)},
                                {"output_covariance", Matrix(s.outputCovariance_)},
                                {"model_identity", Str(s.model_->Identity())},
                                {"consumed_coordinates", Ordinal(s.consumedCoordinates_)},
                                {"report", Parse(DumpCalibrationReportJson(result.Report()))},
                                {"numerical_validation_passed", Json_()},
                                {"quote_risk_available", Bool(false)}})));
    }
    String_ DumpXccyMonteCarloBasketPartialJson(const Handle_<XccyMcBasketPartial_>& partial) {
        if (!partial)
            CalibrationInputFailure("CalibrationCandidateUnavailable", "/outcome/partial", "nonnull owned XCCY basket partial");
        const auto& p = partial->Snapshot();
        const auto location = Obj({{"member", Optional(p.failedMember_, Ordinal)},
                                   {"batch", Optional(p.failedBatch_, Ordinal)},
                                   {"path", Optional(p.failedPath_, Ordinal)},
                                   {"observed_cursor", Optional(p.observedCursor_, Ordinal)}});
        return String_(Jcs(Obj({{"schema", Str(p.version_)},
                                {"status", Str("Failed")},
                                {"basket_complete", Bool(false)},
                                {"header", Header(p.header_)},
                                {"allocation", Allocation(p.allocation_)},
                                {"members", Array(p.members_, [&](const auto& m) { return Member(m, p.header_.settings_.aad_); })},
                                {"completed_members", Ordinal(p.completedMembers_)},
                                {"consumed_coordinates", Ordinal(p.consumedCoordinates_)},
                                {"failure_location", location},
                                {"report", Parse(DumpCalibrationReportJson(partial->Report()))},
                                {"coefficient_rows", Json_()},
                                {"coefficient_means", Json_()},
                                {"coefficient_axes", Json_()},
                                {"output_axes", Json_()},
                                {"coefficient_covariance", Json_()},
                                {"mean_covariance", Json_()},
                                {"output_jacobian", Json_()},
                                {"output_covariance", Json_()},
                                {"mean", Json_()},
                                {"standard_error", Json_()},
                                {"numerical_validation_passed", Json_()},
                                {"quote_risk_available", Bool(false)}})));
    }
} // namespace Dal
