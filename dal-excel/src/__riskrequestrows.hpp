//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <array>
#include <cmath>
#include <limits>

#include <dal-public/src/value.hpp>

#include "__scriptsettingsrows.hpp"

namespace Dal::Excel {
    inline Vector_<String_> ListValue(const Cell_& cell, const String_& context) {
        if (Cell::IsEmpty(cell))
            return {};
        const auto text = TextValue(cell, context);
        const auto items = String::Split(text, ';', true);
        for (const auto& item : items)
            REQUIRE(!item.empty() && item.find('\0') == String_::npos, context + "expected semicolon-separated nonempty entries without NUL");
        return items;
    }

    inline double RiskListNumber(const String_& item, const String_& context, const char* kind) {
        try {
            return String::ToDouble(item);
        } catch (const std::exception& error) {
            THROW(context + "expected numeric " + kind + "; value=" + item + "; " + String_(error.what()));
        }
    }

    inline Vector_<> FactorList(const Cell_& cell, const String_& context) {
        Vector_<> factors;
        for (const auto& item : ListValue(cell, context)) {
            const double factor = RiskListNumber(item, context, "report factor");
            REQUIRE(std::isfinite(factor) && factor > 0.0, context + "report factor must be finite and positive");
            factors.push_back(factor);
        }
        return factors;
    }

    inline Vector_<> WeightList(const Cell_& cell, const String_& context) {
        Vector_<> weights;
        for (const auto& item : ListValue(cell, context)) {
            const double weight = RiskListNumber(item, context, "weight");
            REQUIRE(std::isfinite(weight), context + "weight must be finite");
            weights.push_back(weight);
        }
        return weights;
    }

    inline size_t PayloadBudget(const Cell_& cell, const String_& context, const char* field = "numeric_payload_budget_bytes") {
        const auto* number = std::get_if<double>(&cell.val_);
        REQUIRE(number && std::isfinite(*number) && std::trunc(*number) == *number && *number >= 0.0 && *number <= 9007199254740991.0 &&
                    static_cast<long double>(*number) <= static_cast<long double>((std::numeric_limits<size_t>::max)()),
                context + field + " must be an exactly representable nonnegative size_t integer at most 2^53-1");
        return static_cast<size_t>(*number);
    }

    inline void ReadRiskSelectionCell(const String_& key, const Cell_& cell, const String_& context, Script::RiskRequest_* value) {
        if (key == "inputs")
            value->inputs_ = ListValue(cell, context);
        else if (key == "outputs")
            value->outputs_ = ListValue(cell, context);
        else if (key == "report_factors")
            value->reportFactors_ = FactorList(cell, context);
        else
            value->numericPayloadBudgetBytes_ = PayloadBudget(cell, context);
    }

    [[maybe_unused]] static Matrix_<Cell_> NumericCells(const Matrix_<>& source) {
        if (source.Cols() == 0)
            return Matrix_<Cell_>(1, 1);
        Matrix_<Cell_> cells(source.Rows(), source.Cols());
        for (int row = 0; row < source.Rows(); ++row)
            for (int column = 0; column < source.Cols(); ++column)
                cells(row, column).val_.emplace<double>(source(row, column));
        return cells;
    }

    inline Cell_ RiskCell(int value) { return Cell_(double(value)); }
    template <class T_> Cell_ RiskCell(const T_& value) { return Cell_(value); }

    template <class T_> Cell_ OptionalCell(const std::optional<T_>& value) { return value ? RiskCell(*value) : Cell_(); }

    template <class T_> std::pair<String_, Cell_> Field(const String_& key, const T_& value) { return {key, RiskCell(value)}; }

    inline Matrix_<Cell_> FieldCells(const Vector_<std::pair<String_, Cell_>>& fields) {
        if (fields.empty())
            return Matrix_<Cell_>(1, 1);
        Matrix_<Cell_> cells(static_cast<int>(fields.size()), 2);
        for (int row = 0; row < cells.Rows(); ++row) {
            cells(row, 0) = fields[row].first;
            cells(row, 1) = fields[row].second;
        }
        return cells;
    }

    inline Matrix_<Cell_> RiskCoordinateCells(const Vector_<Script::RiskCoordinate_>& axis) {
        const Vector_<String_> headers{"id", "label", "family", "ordinal", "value", "native_unit", "physical_unit", "report_scale"};
        Matrix_<Cell_> cells(static_cast<int>(axis.size()) + 1, static_cast<int>(headers.size()));
        for (int column = 0; column < cells.Cols(); ++column)
            cells(0, column) = headers[static_cast<size_t>(column)];
        for (size_t index = 0; index < axis.size(); ++index) {
            const auto& coordinate = axis[index];
            const int row = static_cast<int>(index) + 1;
            cells(row, 0) = coordinate.id_;
            cells(row, 1) = coordinate.label_;
            cells(row, 2) = coordinate.family_;
            cells(row, 3) = double(coordinate.ordinal_);
            cells(row, 4) = coordinate.value_;
            cells(row, 5) = coordinate.nativeUnit_;
            cells(row, 6) = OptionalCell(coordinate.physicalUnit_);
            cells(row, 7) = coordinate.reportScale_;
        }
        return cells;
    }

    inline Matrix_<Cell_> RiskShapeCells(const Matrix_<>& source) {
        Matrix_<Cell_> cells(1, 2);
        cells(0, 0) = double(source.Rows());
        cells(0, 1) = double(source.Cols());
        return cells;
    }

    inline const Script::RiskExecutionSnapshot_& RiskExecution(const Script::RiskResultProvenance_& source) {
        REQUIRE(source.execution_, "InvalidRiskResult: execution snapshot is unavailable on a caller-provided conversion result");
        return *source.execution_;
    }

    inline Matrix_<Cell_> RiskProvenanceCells(const Script::RiskResultProvenance_& source) {
        const auto& execution = RiskExecution(source);
        const auto& simulation = execution.simulation_;
        return FieldCells({Field("method", source.method_),
                           Field("engine", source.engine_),
                           Field("normalization", source.normalization_),
                           Field("calibration", source.calibration_),
                           Field("model_type", source.modelType_),
                           Field("evaluation_date", OptionalCell(source.evaluationDate_)),
                           Field("paths_per_replicate", double(execution.pathsPerReplicate_)),
                           Field("pricing_replicates", double(execution.pricingReplicates_)),
                           Field("all_expired", execution.allExpired_),
                           Field("rsg", simulation.rsg_),
                           Field("use_bb", simulation.useBb_),
                           Field("enable_aad", simulation.enableAad_),
                           Field("smooth", simulation.smooth_),
                           Field("compiled", simulation.compiled_.value_or(false)),
                           Field("lsmc_basis_degree", double(simulation.lsmcBasisDegree_)),
                           Field("lsmc_training_paths", OptionalCell(simulation.lsmcTrainingPaths_)),
                           Field("lsmc_validation_paths", OptionalCell(simulation.lsmcValidationPaths_)),
                           Field("lsmc_rqmc_replicates", OptionalCell(simulation.lsmcRqmcReplicates_)),
                           Field("lsmc_training_seed", OptionalCell(simulation.lsmcTrainingSeed_)),
                           Field("lsmc_pricing_seed", OptionalCell(simulation.lsmcPricingSeed_)),
                           Field("lsmc_policy_risk_mode", simulation.lsmcPolicyRiskMode_),
                           Field("lsmc_policy_bump_relative", simulation.lsmcPolicyBumpRelative_),
                           Field("today_fixing_policy", execution.todayFixingPolicy_),
                           Field("fixing_source", execution.fixingSource_),
                           Field("default_index", execution.productSettings_.defaultIndex_),
                           Field("regression_features", String::Accumulate(execution.productSettings_.regressionFeatures_, ";"))});
    }

    inline Matrix_<Cell_> RiskHistoryCells(const Script::RiskExecutionSnapshot_& execution) {
        const auto& observations = execution.observations_;
        Matrix_<Cell_> cells(static_cast<int>(observations.size()) + 1, 4);
        constexpr std::array<const char*, 4> HEADERS{"index", "fixing_time", "historical", "value"};
        for (int column = 0; column < 4; ++column)
            cells(0, column).val_.emplace<String_>(HEADERS[column]);
        for (size_t index = 0; index < observations.size(); ++index) {
            const auto& observation = observations[index];
            const int row = static_cast<int>(index) + 1;
            cells(row, 0) = observation.index_;
            cells(row, 1) = DateTime::ToString(observation.fixingTime_);
            cells(row, 2) = observation.historical_;
            cells(row, 3) = OptionalCell(observation.value_);
        }
        return cells;
    }

    inline Matrix_<Cell_> RiskProductCells(const Script::RiskExecutionSnapshot_& execution) {
        Matrix_<Cell_> cells(static_cast<int>(execution.productDates_.size()), 2);
        for (size_t index = 0; index < execution.productDates_.size(); ++index) {
            cells(static_cast<int>(index), 0) = execution.productDates_[index];
            cells(static_cast<int>(index), 1) = execution.productEvents_[index];
        }
        return cells;
    }

} // namespace Dal::Excel
