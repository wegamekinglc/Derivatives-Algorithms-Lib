#pragma once

#include <dal/model/marketivlocalvol.hpp>
#include <dal/script/preparation.hpp>

#include <dal-public/src/marketivsurface.hpp>

namespace Dal {
    Handle_<MarketIvLocalVolModelData_>
    NewMarketIvLocalVolModelData(const String_& name, const MarketIvDupireResult_& grid, const MarketIvLocalVolModelSettings_& settings);
    MarketIvLocalVolExecution_ InspectMarketIvLocalVolExecution(const Script::ScriptProductData_& product,
                                                                const MarketIvLocalVolModelData_& model,
                                                                const Script::ScriptValuationSettings_& valuation = {},
                                                                const Script::MonteCarloSettings_& simulation = {});
} // namespace Dal

namespace Dal::MarketIvLocalVolFields {
    using Settings_ = MarketIvLocalVolModelSettings_;
    inline const Vector_<MarketIvSettings::Field_<Settings_, String_>>& TextFields() {
        static const Vector_<MarketIvSettings::Field_<Settings_, String_>> fields{
            {"index", &Settings_::index_}, {"currency", &Settings_::currency_}, {"factor", &Settings_::factor_}};
        return fields;
    }
    inline const Vector_<MarketIvSettings::Field_<Settings_, double>>& RealFields() {
        static const Vector_<MarketIvSettings::Field_<Settings_, double>> fields{{"horizon", &Settings_::horizon_},
                                                                                 {"max_step", &Settings_::maxStep_}};
        return fields;
    }
    inline const Vector_<MarketIvSettings::Field_<Settings_, size_t>>& CountFields() {
        static const Vector_<MarketIvSettings::Field_<Settings_, size_t>> fields{
            {"max_internal_steps", &Settings_::maxInternalSteps_},
            {"max_simulation_dimension", &Settings_::maxSimulationDimension_},
            {"numeric_payload_budget_bytes", &Settings_::numericPayloadBudgetBytes_},
            {"workspace_budget_bytes", &Settings_::workspaceBudgetBytes_}};
        return fields;
    }
    template <class F_> void Visit(const Settings_& x, F_ f) {
        f("schema", String_("dal.market-iv-local-vol-settings/1"));
        for (const auto& field : TextFields())
            f(field.name_, x.*field.member_);
        for (const auto& field : RealFields())
            f(field.name_, x.*field.member_);
        for (const auto& field : CountFields())
            f(field.name_, x.*field.member_);
    }
    template <class F_> void Visit(const MarketIvLocalVolModelSummary_& x, F_ f) {
        f("schema", x.schema_);
        f("source_identity", x.sourceIdentity_);
        f("model_identity", x.modelIdentity_);
        f("evaluation_date", x.evaluationDate_);
        f("spot", x.spot_);
        f("carry_mode", x.carryMode_);
        f("index", x.index_);
        f("currency", x.currency_);
        f("factor", x.factor_);
        f("horizon", x.horizon_);
        f("max_step", x.maxStep_);
        f("grid_rows", x.gridRows_);
        f("grid_columns", x.gridColumns_);
        f("grid_first_time", x.gridFirstTime_);
        f("grid_last_time", x.gridLastTime_);
        f("fit_status", x.fitStatus_);
        f("underidentified", x.underidentified_);
        f("near_tie", x.nearTie_);
        f("time_basis", x.timeBasis_);
        f("step_scheme", x.stepScheme_);
        f("local_vol_scheme", x.localVolScheme_);
        f("carry_scheme", x.carryScheme_);
        f("timeline_scheme", x.timelineScheme_);
    }
    template <class F_> void Visit(const MarketIvLocalVolCarryTerm_& x, F_ f) {
        f("ordinal", x.ordinal_);
        f("date", x.date_);
        f("time", x.time_);
        f("forward", x.forward_);
        f("discount", x.discount_);
        f("log_forward_over_spot", x.logForwardOverSpot_);
        f("log_discount", x.logDiscount_);
        f("origin", x.origin_);
    }
    template <class F_> void Visit(const MarketIvLocalVolCapabilities_& x, F_ f) {
        f("schema", x.schema_);
        f("passive_price", x.passivePrice_);
        f("discount_factors", x.discountFactors_);
        f("brownian_bridge", x.brownianBridge_);
        f("compiled", x.compiled_);
        f("tree", x.tree_);
        f("aad", x.aad_);
        f("native_risk", x.nativeRisk_);
        f("quote_risk", x.quoteRisk_);
        f("curvature", x.curvature_);
        f("segmented", x.segmented_);
        f("exercise", x.exercise_);
        f("archive", x.archive_);
    }
    template <class F_> void Visit(const MarketIvLocalVolWarning_& x, F_ f) {
        f("code", x.code_);
        f("field", x.field_);
        f("message", x.message_);
    }
    template <class F_> void Visit(const MarketIvLocalVolConstructionExecution_& x, F_ f) {
        f("schema", x.schema_);
        f("planned_numeric_payload_bytes", x.plannedNumericPayloadBytes_);
        f("numeric_payload_bytes", x.numericPayloadBytes_);
        f("planned_workspace_bytes", x.plannedWorkspaceBytes_);
        f("peak_workspace_bytes", x.peakWorkspaceBytes_);
    }
    template <class F_> void Visit(const MarketIvLocalVolExecution_& x, F_ f) {
        f("schema", x.schema_);
        f("source_identity", x.sourceIdentity_);
        f("model_identity", x.modelIdentity_);
        f("request_identity", x.requestIdentity_);
        f("evaluation_date", x.evaluationDate_);
        f("requested_max_step", x.requestedMaxStep_);
        f("resolved_max_step", x.resolvedMaxStep_);
        f("required_time", x.requiredTime_);
        f("carry_knots", x.carryKnots_);
        f("grid_time_knots", x.gridTimeKnots_);
        f("anchor_count", x.anchorCount_);
        f("internal_steps", x.internalSteps_);
        f("simulation_dimension", x.simulationDimension_);
        f("workers", x.workers_);
        f("shared_numeric_bytes", x.sharedNumericBytes_);
        f("per_worker_numeric_bytes", x.perWorkerNumericBytes_);
        f("planned_numeric_payload_bytes", x.plannedNumericPayloadBytes_);
        f("numeric_payload_bytes", x.numericPayloadBytes_);
        f("planned_workspace_bytes", x.plannedWorkspaceBytes_);
        f("peak_workspace_bytes", x.peakWorkspaceBytes_);
        f("prepared_columns", x.preparedColumns_);
        f("prepared_market_queries", x.preparedMarketQueries_);
        f("per_path_market_queries", x.perPathMarketQueries_);
    }
} // namespace Dal::MarketIvLocalVolFields
