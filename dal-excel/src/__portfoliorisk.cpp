//
// Created by Codex on 2026/10/7.
//

#include "__portfoliorisk.hpp"

#include <array>
#include <limits>
#include <type_traits>

#include <dal-public/src/repository.hpp>

#include "__platform.hpp"
#include "__portfolioinput.hpp"
#include "__script_test_api.hpp"
#include "__settingskeys.hpp"

namespace Dal {
    namespace {
        template <class T_> Handle_<T_> PortfolioTableHandle(const Environment_* environment, const String_& tag, const String_& context) {
            try {
                const auto value = handle_cast<T_>(FetchRepository(environment, tag));
                REQUIRE(value, "expected typed repository handle");
                return value;
            } catch (const Exception_& error) {
                THROW(context + "; cause=" + String_(error.what()));
            }
        }

        template <class F_> auto VisitPortfolioResult(const Handle_<Storable_>& handle, F_ function) {
            if (const auto* value = dynamic_cast<const StorablePortfolioWeightedRiskResult_*>(handle.get()))
                return function(value->val_);
            const auto* value = dynamic_cast<const StorablePortfolioJacobianRiskResult_*>(handle.get());
            REQUIRE(value, "InvalidPortfolioRiskResult: field=result; expected completed weighted or Jacobian portfolio result");
            return function(value->val_);
        }

        Cell_ PortfolioInteger(size_t value) { return value <= 9007199254740991ULL ? Cell_(double(value)) : Cell_(String_(std::to_string(value))); }

        template <class R_> void ReadPortfolioCapacityCell(const String_& key, const Cell_& cell, const String_& context, R_* value) {
            if (key == "recording_capacity_budget_bytes")
                value->recordingCapacityBudgetBytes_ = Excel::PayloadBudget(cell, context, "recording_capacity_budget_bytes");
            else if (key == "scratch_capacity_budget_bytes")
                value->scratchCapacityBudgetBytes_ = Excel::PayloadBudget(cell, context, "scratch_capacity_budget_bytes");
            else
                Excel::ReadRiskSelectionCell(key, cell, context, &value->selection_);
        }

        template <class R_, auto VALUE_>
        auto EvaluatePortfolio(const Handle_<ScriptPortfolioData_>& portfolio,
                               double paths,
                               const Handle_<Excel::StorableRiskValue_<R_>>& request,
                               const Handle_<StorableScriptValuationSettings_>& valuation,
                               const Handle_<StorableMonteCarloSettings_>& simulation,
                               const char* function) {
            const int count = Excel::CheckedMonteCarloPathCount(paths, function);
            const auto requested = request ? request->val_ : R_();
            const auto settings = valuation ? valuation->val_ : ScriptValuationSettings_();
            const auto execution = simulation ? simulation->val_ : DefaultRiskMonteCarloSettings();
            return VALUE_(portfolio, count, requested, settings, execution);
        }

        const Script::RiskResultProvenance_& TradeProvenance(const PortfolioRiskProvenance_& provenance, double trade) {
            const auto ordinal = Excel::PayloadBudget(Cell_(trade), "InvalidPortfolioRiskResult: ", "trade");
            REQUIRE(ordinal < provenance.trades_.size(), "InvalidPortfolioRiskResult: field=trade; original trade ordinal out of range");
            return provenance.trades_[ordinal];
        }

        template <class R_> Matrix_<Cell_> PortfolioValueCells(const R_& result) {
            auto cells = Excel::OutputCoordinateCells(result.OutputAxis(), 5);
            cells(0, 3) = "mean";
            cells(0, 4) = "weight";
            const auto& values = [&]() -> const Vector_<double>& {
                if constexpr (std::is_same_v<R_, PortfolioWeightedRiskResult_>)
                    return result.ComponentMeans();
                else
                    return result.Values();
            }();
            for (size_t row = 0; row < values.size(); ++row) {
                cells(static_cast<int>(row) + 1, 3) = values[row];
                if constexpr (std::is_same_v<R_, PortfolioWeightedRiskResult_>)
                    cells(static_cast<int>(row) + 1, 4) = result.Weights()[row];
            }
            return cells;
        }

        using SamplingRow_ = std::array<Cell_, 6>;

        void AppendDefinition(Vector_<SamplingRow_>* rows, size_t group, size_t sample, const AAD::SampleDef_& definition) {
            const auto add = [&](const char* field, Cell_ value, Cell_ ordinal = {}, Cell_ subordinal = {}) {
                rows->push_back(
                    {PortfolioInteger(group), PortfolioInteger(sample), Cell_(field), std::move(ordinal), std::move(subordinal), std::move(value)});
            };
            add("numeraire", Cell_(definition.numeraire_));
            add("index_count", PortfolioInteger(definition.indexNames_.size()));
            add("discount_count", PortfolioInteger(definition.discountMats_.size()));
            add("libor_count", PortfolioInteger(definition.liborDefs_.size()));
            add("forward_rows", PortfolioInteger(definition.forwardMats_.size()));
            for (size_t i = 0; i < definition.indexNames_.size(); ++i)
                add("index", Cell_(definition.indexNames_[i]), PortfolioInteger(i));
            for (size_t i = 0; i < definition.discountMats_.size(); ++i)
                add("discount_maturity", Cell_(definition.discountMats_[i]), PortfolioInteger(i));
            for (size_t i = 0; i < definition.liborDefs_.size(); ++i) {
                add("libor_start", Cell_(definition.liborDefs_[i].start_), PortfolioInteger(i));
                add("libor_end", Cell_(definition.liborDefs_[i].end_), PortfolioInteger(i));
                add("libor_curve", Cell_(definition.liborDefs_[i].curve_), PortfolioInteger(i));
            }
            for (size_t i = 0; i < definition.forwardMats_.size(); ++i) {
                add("forward_columns", PortfolioInteger(definition.forwardMats_[i].size()), PortfolioInteger(i));
                for (size_t j = 0; j < definition.forwardMats_[i].size(); ++j)
                    add("forward_maturity", Cell_(definition.forwardMats_[i][j]), PortfolioInteger(i), PortfolioInteger(j));
            }
        }

        Matrix_<Cell_> SamplingCells(const PortfolioRiskExecution_& execution) {
            Vector_<SamplingRow_> rows{{Cell_("group"), Cell_("sample"), Cell_("field"), Cell_("ordinal"), Cell_("subordinal"), Cell_("value")}};
            for (size_t group = 0; group < execution.groups_.size(); ++group) {
                const auto& data = execution.groups_[group];
                for (size_t i = 0; i < data.sampleDates_.size(); ++i)
                    rows.push_back({PortfolioInteger(group), PortfolioInteger(i), Cell_("sample_date"), {}, {}, Cell_(data.sampleDates_[i])});
                for (size_t i = 0; i < data.timeLine_.size(); ++i)
                    rows.push_back({PortfolioInteger(group), PortfolioInteger(i), Cell_("timeline"), {}, {}, Cell_(data.timeLine_[i])});
                for (size_t i = 0; i < data.sampleDefinitions_.size(); ++i)
                    AppendDefinition(&rows, group, i, data.sampleDefinitions_[i]);
            }
            REQUIRE(rows.size() <= static_cast<size_t>(std::numeric_limits<int>::max()), "InvalidPortfolioRiskResult: sampling spill is too large");
            Matrix_<Cell_> cells(static_cast<int>(rows.size()), 6);
            for (int row = 0; row < cells.Rows(); ++row)
                for (int column = 0; column < cells.Cols(); ++column)
                    cells(row, column) = rows[row][column];
            return cells;
        }

        Matrix_<Cell_> ExecutionCells(const PortfolioRiskExecution_& execution) {
            const Vector_<String_> headers{"group",
                                           "model_owner",
                                           "trade_positions",
                                           "random_dimension",
                                           "factors",
                                           "deterministic_numeraire",
                                           "generated_scenarios",
                                           "evaluator_calls",
                                           "suffix_reversals",
                                           "prefix_reversals",
                                           "requested_max_block_width",
                                           "actual_widths",
                                           "replay_attempts",
                                           "peak_recording_bytes",
                                           "peak_scratch_bytes"};
            Matrix_<Cell_> cells(static_cast<int>(execution.groups_.size()) + 1, static_cast<int>(headers.size()));
            for (int column = 0; column < cells.Cols(); ++column)
                cells(0, column) = headers[column];
            for (size_t group = 0; group < execution.groups_.size(); ++group) {
                const auto& data = execution.groups_[group];
                const auto positions = [](const Vector_<size_t>& values) {
                    Vector_<String_> text;
                    for (const auto value : values)
                        text.push_back(String_(std::to_string(value)));
                    return String::Accumulate(text, ";");
                };
                const Vector_<Cell_> row{PortfolioInteger(group),
                                         PortfolioInteger(data.modelOwner_),
                                         Cell_(positions(data.tradePositions_)),
                                         PortfolioInteger(data.randomDimension_),
                                         PortfolioInteger(data.factors_),
                                         Cell_(data.deterministicNumeraire_),
                                         PortfolioInteger(data.generatedScenarios_),
                                         PortfolioInteger(data.evaluatorCalls_),
                                         PortfolioInteger(data.suffixReversals_),
                                         PortfolioInteger(data.prefixReversals_),
                                         PortfolioInteger(execution.requestedMaxBlockWidth_),
                                         Cell_(positions(data.actualWidths_)),
                                         PortfolioInteger(data.replayAttempts_),
                                         PortfolioInteger(execution.peakRecordingBytes_),
                                         PortfolioInteger(execution.peakScratchBytes_)};
                for (int column = 0; column < cells.Cols(); ++column)
                    cells(static_cast<int>(group) + 1, column) = row[column];
            }
            return cells;
        }
    } // namespace

    void ScriptPortfolio_New(const String_& name, const Matrix_<Cell_>& table, Handle_<ScriptPortfolioData_>* portfolio) {
        REQUIRE(name.find('\0') == String_::npos, "InvalidScriptPortfolio: field=name; embedded NUL is unsupported");
        REQUIRE(table.Rows() > 0 && table.Cols() == 3, "InvalidScriptPortfolio: trades; expected nonempty physical three-column table");
        ENV_SEED_TYPE(ObjectAccess_);
        Vector_<Script::PortfolioTrade_> trades;
        for (int row = 0; row < table.Rows(); ++row) {
            Vector_<String_> fields;
            for (int column = 0; column < 3; ++column) {
                const auto context = Excel::ScriptSettingLocation("ScriptPortfolio_New", "trades", row + 1, column + 1);
                const auto text = Excel::TextValue(table(row, column), context);
                REQUIRE(!text.empty() && text.find('\0') == String_::npos, context + "expected nonempty text without NUL");
                fields.push_back(text);
            }
            const auto context = "InvalidScriptPortfolio: trade=" + fields[0] + "; trades row=" + String_(std::to_string(row + 1));
            const auto product = PortfolioTableHandle<ScriptProductData_>(_env, fields[1], context + " column=2; field=products");
            const auto model = PortfolioTableHandle<ModelData_>(_env, fields[2], context + " column=3; field=modelData");
            trades.push_back({fields[0], product, model});
        }
        portfolio->reset(new ScriptPortfolioData_(name, trades));
    }

    void
    PortfolioWeightedRiskRequest_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorablePortfolioWeightedRiskRequest_>* request) {
        REQUIRE(name.find('\0') == String_::npos, "InvalidPortfolioRiskRequest: name; embedded NUL is unsupported");
        PortfolioWeightedRiskRequest_ value;
        Excel::ReadRows(
            settings, "PortfolioWeightedRiskRequest_New", "settings",
            [&](const String_& key, const Cell_& cell, const String_&, const String_& context) {
                RequireKnownSettingsKey(key, {"inputs", "outputs", "weights", "report_factors", "numeric_payload_budget_bytes",
                                              "recording_capacity_budget_bytes", "scratch_capacity_budget_bytes"});
                if (key == "weights")
                    value.weights_ = Excel::WeightList(cell, context);
                else
                    ReadPortfolioCapacityCell(key, cell, context, &value);
            },
            true);
        request->reset(new StorablePortfolioWeightedRiskRequest_(name, std::move(value)));
    }

    void
    PortfolioJacobianRiskRequest_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorablePortfolioJacobianRiskRequest_>* request) {
        REQUIRE(name.find('\0') == String_::npos, "InvalidPortfolioRiskRequest: name; embedded NUL is unsupported");
        PortfolioJacobianRiskRequest_ value;
        Excel::ReadRows(
            settings, "PortfolioJacobianRiskRequest_New", "settings",
            [&](const String_& key, const Cell_& cell, const String_&, const String_& context) {
                RequireKnownSettingsKey(key, {"inputs", "outputs", "report_factors", "max_block_width", "numeric_payload_budget_bytes",
                                              "recording_capacity_budget_bytes", "scratch_capacity_budget_bytes"});
                if (key == "max_block_width") {
                    const auto width = Excel::PayloadBudget(cell, context, "max_block_width");
                    REQUIRE(width > 0 && width <= AAD::ADJ_SIZE, context + "max_block_width outside native bounds");
                    value.maxBlockWidth_ = width;
                } else
                    ReadPortfolioCapacityCell(key, cell, context, &value);
            },
            true);
        request->reset(new StorablePortfolioJacobianRiskRequest_(name, std::move(value)));
    }

    void PortfolioMonteCarlo_ValueWithWeightedRisk(const Handle_<ScriptPortfolioData_>& portfolio,
                                                   double paths,
                                                   const Handle_<StorablePortfolioWeightedRiskRequest_>& request,
                                                   const Handle_<StorableScriptValuationSettings_>& valuation,
                                                   const Handle_<StorableMonteCarloSettings_>& simulation,
                                                   Handle_<StorablePortfolioWeightedRiskResult_>* result) {
        auto value = EvaluatePortfolio<PortfolioWeightedRiskRequest_, &ValuePortfolioByMonteCarloWithWeightedRisk>(
            portfolio, paths, request, valuation, simulation, "PortfolioMonteCarlo_ValueWithWeightedRisk");
        result->reset(new StorablePortfolioWeightedRiskResult_("", std::move(value)));
    }

    void PortfolioMonteCarlo_ValueWithJacobianRisk(const Handle_<ScriptPortfolioData_>& portfolio,
                                                   double paths,
                                                   const Handle_<StorablePortfolioJacobianRiskRequest_>& request,
                                                   const Handle_<StorableScriptValuationSettings_>& valuation,
                                                   const Handle_<StorableMonteCarloSettings_>& simulation,
                                                   Handle_<StorablePortfolioJacobianRiskResult_>* result) {
        auto value = EvaluatePortfolio<PortfolioJacobianRiskRequest_, &ValuePortfolioByMonteCarloWithJacobianRisk>(
            portfolio, paths, request, valuation, simulation, "PortfolioMonteCarlo_ValueWithJacobianRisk");
        result->reset(new StorablePortfolioJacobianRiskResult_("", std::move(value)));
    }

    void PortfolioRiskResult_Get_Objective(const Handle_<Storable_>& handle, double* objective) {
        const auto* value = dynamic_cast<const StorablePortfolioWeightedRiskResult_*>(handle.get());
        REQUIRE(value, "InvalidPortfolioRiskResult: field=result; objective requires weighted portfolio result");
        *objective = value->val_.WeightedValue();
    }

    void PortfolioRiskResult_Get_Values(const Handle_<Storable_>& result, Matrix_<Cell_>* values) {
        *values = VisitPortfolioResult(result, [](const auto& value) { return PortfolioValueCells(value); });
    }

    void PortfolioRiskResult_Get_Jacobian(const Handle_<Storable_>& result, bool reported, Matrix_<Cell_>* jacobian) {
        *jacobian = VisitPortfolioResult(result, [&](const auto& value) {
            return reported ? Excel::NumericCells(value.ReportedJacobian()) : Excel::NumericCells(value.Jacobian());
        });
    }

    void PortfolioRiskResult_Get_Shape(const Handle_<Storable_>& result, Matrix_<Cell_>* shape) {
        *shape = VisitPortfolioResult(result, [](const auto& value) {
            using Result_ = std::decay_t<decltype(value)>;
            Matrix_<Cell_> cells(1, 2);
            cells(0, 0) = PortfolioInteger(std::is_same_v<Result_, PortfolioWeightedRiskResult_> ? 1 : value.OutputAxis().size());
            cells(0, 1) = PortfolioInteger(value.InputAxis().size());
            return cells;
        });
    }

    void PortfolioRiskResult_Get_Outputs(const Handle_<Storable_>& result, bool complete, Matrix_<Cell_>* outputs) {
        *outputs = VisitPortfolioResult(
            result, [&](const auto& value) { return Excel::OutputCoordinateCells(complete ? value.CompleteOutputAxis() : value.OutputAxis()); });
    }

    void PortfolioRiskResult_Get_Inputs(const Handle_<Storable_>& result, bool complete, Matrix_<Cell_>* inputs) {
        *inputs = VisitPortfolioResult(
            result, [&](const auto& value) { return Excel::RiskCoordinateCells(complete ? value.CompleteInputAxis() : value.InputAxis()); });
    }

    void PortfolioRiskResult_Get_Execution(const Handle_<Storable_>& result, Matrix_<Cell_>* execution) {
        *execution = VisitPortfolioResult(result, [](const auto& value) { return ExecutionCells(value.Execution()); });
    }

    void PortfolioRiskResult_Get_Sampling(const Handle_<Storable_>& result, Matrix_<Cell_>* sampling) {
        *sampling = VisitPortfolioResult(result, [](const auto& value) { return SamplingCells(value.Execution()); });
    }

    void PortfolioRiskResult_Get_Provenance(const Handle_<Storable_>& result, Matrix_<Cell_>* provenance) {
        *provenance = VisitPortfolioResult(result, [](const auto& value) {
            const auto& data = value.Provenance();
            return Excel::FieldCells({{"method", Cell_(data.method_)},
                                      {"engine", Cell_(data.engine_)},
                                      {"normalization", Cell_(data.normalization_)},
                                      {"calibration", Cell_(data.calibration_)},
                                      {"evaluation_date", Cell_(data.evaluationDate_)},
                                      {"trade_count", PortfolioInteger(data.tradeIds_.size())}});
        });
    }

    void PortfolioRiskResult_Get_Trades(const Handle_<Storable_>& result, Matrix_<Cell_>* trades) {
        *trades = VisitPortfolioResult(result, [](const auto& value) {
            const auto& data = value.Provenance();
            Matrix_<Cell_> cells(static_cast<int>(data.tradeIds_.size()) + 1, 4);
            const Vector_<String_> headers{"trade", "id", "model_owner", "model_type"};
            for (int column = 0; column < 4; ++column)
                cells(0, column) = headers[column];
            for (size_t trade = 0; trade < data.tradeIds_.size(); ++trade) {
                cells(static_cast<int>(trade) + 1, 0) = PortfolioInteger(trade);
                cells(static_cast<int>(trade) + 1, 1) = data.tradeIds_[trade];
                cells(static_cast<int>(trade) + 1, 2) = PortfolioInteger(static_cast<size_t>(data.modelOwners_[trade]));
                cells(static_cast<int>(trade) + 1, 3) = data.trades_[trade].modelType_;
            }
            return cells;
        });
    }

    void PortfolioRiskResult_Get_TradeProvenance(const Handle_<Storable_>& result, double trade, Matrix_<Cell_>* provenance) {
        *provenance =
            VisitPortfolioResult(result, [&](const auto& value) { return Excel::RiskProvenanceCells(TradeProvenance(value.Provenance(), trade)); });
    }

    void PortfolioRiskResult_Get_History(const Handle_<Storable_>& result, double trade, Matrix_<Cell_>* history) {
        *history = VisitPortfolioResult(
            result, [&](const auto& value) { return Excel::RiskHistoryCells(Excel::RiskExecution(TradeProvenance(value.Provenance(), trade))); });
    }

    void PortfolioRiskResult_Get_Product(const Handle_<Storable_>& result, double trade, Matrix_<Cell_>* product) {
        *product = VisitPortfolioResult(
            result, [&](const auto& value) { return Excel::RiskProductCells(Excel::RiskExecution(TradeProvenance(value.Provenance(), trade))); });
    }

    void PortfolioRiskResult_Get_ModelSnapshot(const Handle_<Storable_>& result, double trade, Vector_<String_>* json) {
        *json = VisitPortfolioResult(result, [&](const auto& value) {
            return Excel::ScriptDiagnosticChunks(Excel::RiskExecution(TradeProvenance(value.Provenance(), trade)).modelSnapshotJson_,
                                                 "PortfolioRiskResult_Get_ModelSnapshot");
        });
    }

#if defined(DAL_EXCEL_TEST_API_EXPORTS) || defined(DAL_EXCEL_API_TESTS_PORTABLE)
    String_ Excel::PortfolioTestStore(const Handle_<Storable_>& object) {
        ENV_SEED_TYPE(ObjectAccess_);
        return StoreRepository(_env, object);
    }
#endif
} // namespace Dal
