#pragma once

#include <dal/model/surface/marketivsurface.hpp>

namespace Dal::MarketIvSettings {
    template <class S_, class T_> struct Field_ {
        const char* name_;
        T_ S_::* member_;
    };
    inline const Vector_<Field_<MarketIvFitSettings_, double>>& RealFields(const MarketIvFitSettings_&) {
        static const Vector_<Field_<MarketIvFitSettings_, double>> fields{
            {"rho_limit", &MarketIvFitSettings_::rhoLimit_},
            {"constraint_margin", &MarketIvFitSettings_::constraintMargin_},
            {"theta_floor", &MarketIvFitSettings_::thetaFloor_},
            {"max_variance_increment", &MarketIvFitSettings_::maxVarianceIncrement_},
            {"initial_volatility", &MarketIvFitSettings_::initialVolatility_},
            {"projected_gradient_tolerance", &MarketIvFitSettings_::projectedGradientTolerance_},
            {"parameter_step_tolerance", &MarketIvFitSettings_::parameterStepTolerance_},
            {"objective_tie_tolerance", &MarketIvFitSettings_::objectiveTieTolerance_},
            {"normalized_price_tolerance", &MarketIvFitSettings_::normalizedPriceTolerance_},
            {"decimal_iv_tolerance", &MarketIvFitSettings_::decimalIvTolerance_},
            {"normalized_price_scale_floor", &MarketIvFitSettings_::normalizedPriceScaleFloor_},
            {"feasibility_tolerance", &MarketIvFitSettings_::feasibilityTolerance_}};
        return fields;
    }
    inline const Vector_<Field_<MarketIvFitSettings_, size_t>>& CountFields(const MarketIvFitSettings_&) {
        static const Vector_<Field_<MarketIvFitSettings_, size_t>> fields{
            {"max_model_row_evaluations", &MarketIvFitSettings_::maxModelRowEvaluations_},
            {"max_quotes", &MarketIvFitSettings_::maxQuotes_},
            {"max_expiries", &MarketIvFitSettings_::maxExpiries_},
            {"max_market_terms", &MarketIvFitSettings_::maxMarketTerms_},
            {"numeric_payload_budget_bytes", &MarketIvFitSettings_::numericPayloadBudgetBytes_},
            {"workspace_budget_bytes", &MarketIvFitSettings_::workspaceBudgetBytes_}};
        return fields;
    }
    inline const Vector_<Field_<MarketIvFitSettings_, int>>& IntegerFields(const MarketIvFitSettings_&) {
        static const Vector_<Field_<MarketIvFitSettings_, int>> fields{{"max_iterations_per_start", &MarketIvFitSettings_::maxIterationsPerStart_},
                                                                       {"max_line_search_trials", &MarketIvFitSettings_::maxLineSearchTrials_}};
        return fields;
    }
    inline const Vector_<Field_<MarketIvEvaluationSettings_, double>>& RealFields(const MarketIvEvaluationSettings_&) {
        static const Vector_<Field_<MarketIvEvaluationSettings_, double>> fields{
            {"density_factor_floor", &MarketIvEvaluationSettings_::densityFactorFloor_},
            {"min_query_time", &MarketIvEvaluationSettings_::minQueryTime_},
            {"min_total_variance", &MarketIvEvaluationSettings_::minTotalVariance_}};
        return fields;
    }
    inline const Vector_<Field_<MarketIvDupireSettings_, size_t>>& CountFields(const MarketIvDupireSettings_&) {
        static const Vector_<Field_<MarketIvDupireSettings_, size_t>> fields{
            {"max_grid_points", &MarketIvDupireSettings_::maxGridPoints_},
            {"numeric_payload_budget_bytes", &MarketIvDupireSettings_::numericPayloadBudgetBytes_},
            {"workspace_budget_bytes", &MarketIvDupireSettings_::workspaceBudgetBytes_}};
        return fields;
    }
    inline const Vector_<Field_<MarketIvRefitRiskSettings_, size_t>>& CountFields(const MarketIvRefitRiskSettings_&) {
        static const Vector_<Field_<MarketIvRefitRiskSettings_, size_t>> fields{
            {"max_selected_quotes", &MarketIvRefitRiskSettings_::maxSelectedQuotes_},
            {"max_outputs", &MarketIvRefitRiskSettings_::maxOutputs_},
            {"numeric_payload_budget_bytes", &MarketIvRefitRiskSettings_::numericPayloadBudgetBytes_},
            {"workspace_budget_bytes", &MarketIvRefitRiskSettings_::workspaceBudgetBytes_}};
        return fields;
    }
    template <class S_> const Vector_<Field_<S_, double>>& RealFields(const S_&) {
        static const Vector_<Field_<S_, double>> fields;
        return fields;
    }
    template <class S_> const Vector_<Field_<S_, int>>& IntegerFields(const S_&) {
        static const Vector_<Field_<S_, int>> fields;
        return fields;
    }
    template <class S_> const Vector_<Field_<S_, size_t>>& CountFields(const S_&) {
        static const Vector_<Field_<S_, size_t>> fields;
        return fields;
    }
} // namespace Dal::MarketIvSettings
