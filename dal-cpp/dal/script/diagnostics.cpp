//
// Created by Codex on 2026/9/15.
//

#include <dal/model/factory.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/diagnostics.hpp>
#include <dal/script/event.hpp>
#include <dal/script/lsmc.hpp>
#include <dal/script/preparation.hpp>
#include <dal/script/simulation.hpp>
#include <dal/script/visitor/debugger.hpp>

namespace Dal::Script {
    namespace {
        template <class R_, class F_> void WriteArray(std::ostream& out, const R_& values, F_ write) {
            out << '[';
            size_t i = 0;
            for (const auto& value : values) {
                if (i)
                    out << ',';
                write(value, i++);
            }
            out << ']';
        }

        void WriteCell(std::ostream& out, const Cell_& value) {
            value.Visit([&](const auto& cell) {
                using T_ = std::decay_t<decltype(cell)>;
                if constexpr (std::is_same_v<T_, bool>)
                    out << (cell ? "true" : "false");
                else if constexpr (std::is_same_v<T_, double>)
                    out << DebugNumber(cell);
                else if constexpr (std::is_same_v<T_, Date_>)
                    JsonWriteString(Date::ToString(cell), out);
                else if constexpr (std::is_same_v<T_, DateTime_>)
                    JsonWriteString(DateTime::ToString(cell), out);
                else if constexpr (std::is_same_v<T_, String_>)
                    JsonWriteString(cell, out);
                else
                    JsonWriteString("", out);
            });
        }

        void
        WriteObservationRequest(std::ostream& out, const ObservationPlan_& plan, const ObservationRequest_& request, size_t id, bool allExpired) {
            out << "{\"request_id\":" << id << ",\"index_canonical\":";
            JsonWriteString(request.key_.canonicalIndex_, out);
            out << ",\"fixing_time\":";
            JsonWriteString(DateTime::ToString(request.key_.fixingTime_), out);
            out << ",\"source\":\"" << (request.historical_ ? "Historical" : "Model") << "\",\"resolution\":\""
                << (allExpired ? "SkippedExpired" : "Resolved") << "\",\"uses\":";
            WriteArray(out, request.uses_, [&](const auto& use, size_t) {
                out << "{\"event_id\":" << use.eventId_ << ",\"statement_id\":" << use.statementId_ << ",\"node_id\":\"n" << use.nodeId_
                    << "\",\"source\":";
                JsonWriteSource(use.source_, out);
                out << ",\"index_original\":";
                JsonWriteString(use.indexOriginal_, out);
                JsonWriteFixingDate(use.fixingDate_, out);
                out << ",\"observation_type\":\"" << (use.legacySpot_ ? "Spot" : "Fix") << "\"}";
            });
            out << ",\"history_value_id\":";
            if (request.historyValueId_)
                out << *request.historyValueId_;
            else
                out << "null";
            out << ",\"value\":";
            if (request.historyValueId_)
                out << DebugNumber(plan.KnownValue(*request.historyValueId_));
            else
                out << "null";
            out << ",\"model_slot\":";
            if (request.modelSlot_)
                out << "{\"sample_id\":" << request.modelSlot_->sampleId_ << ",\"output_id\":" << request.modelSlot_->outputId_ << '}';
            else
                out << "null";
            out << '}';
        }
    } // namespace

    String_ DescribeScriptProductData(const ScriptProductData_& data) {
        auto product = data.Product();
        product.IndexVariables();
        const auto& defaultName = data.Settings().defaultIndex_;
        const auto defaultIndex = defaultName.empty() ? Handle_<Index_>() : ParseSettingIndex(defaultName, "product.defaultIndex_");
        std::ostringstream out;
        out << "{\"schema\":\"dal.script-product/2\",\"name\":";
        JsonWriteString(data.Name(), out);
        out << ",\"default_index\":{\"original\":";
        JsonWriteString(defaultName, out);
        out << ",\"canonical\":";
        if (defaultIndex)
            JsonWriteString(defaultIndex->Name(), out);
        else
            out << "null";
        out << "},\"input_rows\":";
        WriteArray(out, data.Dates(), [&](const auto& date, size_t i) {
            out << "{\"row\":" << i + 1 << ",\"date_or_definition\":";
            WriteCell(out, date);
            out << ",\"text\":";
            JsonWriteString(data.EventTexts()[i], out);
            out << '}';
        });
        out << ",\"variables\":";
        WriteArray(out, product.VarNames(), [&](const auto& name, size_t i) {
            out << "{\"index\":" << i << ",\"name\":";
            JsonWriteString(name, out);
            out << '}';
        });
        out << ",\"constants\":";
        WriteArray(out, product.ConstVarNames(), [&](const auto& name, size_t i) {
            out << "{\"index\":" << i << ",\"name\":";
            JsonWriteString(name, out);
            out << ",\"value\":" << DebugNumber(product.ConstVarValues()[i]) << '}';
        });
        out << ",\"payoff_index\":";
        // The receiver slot exists only when a PAYS statement defines it; EXERCISE-only
        // products keep the null key (the LSMC driver aggregates the path payoff itself)
        if (product.HasPays() && !product.VarNames().empty())
            out << product.PayOffIdx();
        else
            out << "null";
        out << ",\"events\":";
        size_t nodeId = 0;
        WriteArray(out, product.Events(), [&](const auto& event, size_t i) {
            out << "{\"event_id\":" << i << ",\"date\":";
            JsonWriteString(Date::ToString(product.EventDates()[i]), out);
            out << ",\"origins\":";
            WriteArray(out, product.ParsedEventSources()[i], [&](const auto& source, size_t) {
                out << "{\"row\":" << source.row_ << ",\"offset\":" << source.offset_ << ",\"event_date\":";
                JsonWriteString(Date::ToString(source.eventDate_), out);
                out << '}';
            });
            out << ",\"statements\":";
            WriteArray(out, event, [&](const auto& statement, size_t statementId) {
                Debugger_ debugger(defaultName, defaultIndex, statementId);
                statement->Accept(debugger);
                DebugNodeJson(debugger.Top(), nodeId, out);
            });
            out << '}';
        });
        out << '}';
        return String_(out.str());
    }

    String_ ExplainPreparedScript(const PreparedScript_& prepared) {
        prepared.RequireExecutable();
        const auto& plan = prepared.Plan();
        const auto& settings = prepared.Settings();
        const auto& simulation = prepared.Simulation();
        std::ostringstream out;
        out << "{\"schema\":\"dal.script-valuation/1\",\"evaluation_date\":";
        JsonWriteString(Date::ToString(prepared.EvaluationDate()), out);
        out << ",\"today_fixing\":\"" << (settings.todayFixingPolicy_ == TodayFixingPolicy_::Value_::MODEL ? "Model" : "RequireHistorical")
            << "\",\"source_kind\":\"" << FixingSourceKind(settings) << "\",\"simulation\":{\"rsg\":";
        JsonWriteString(simulation.rsg_, out);
        out << ",\"use_bb\":" << (simulation.useBb_ ? "true" : "false") << ",\"enable_aad\":" << (simulation.enableAad_ ? "true" : "false")
            << ",\"smooth\":" << DebugNumber(simulation.smooth_) << ",\"compiled\":" << (simulation.compiled_.value_or(false) ? "true" : "false")
            << "},\"all_expired\":" << (prepared.AllExpired() ? "true" : "false") << ",\"observation_mode\":\""
            << (plan.Requests().empty() ? "Legacy" : "Named") << "\",\"model_bindings\":";
        WriteArray(out, plan.ModelBindingNames(), [&](const auto& canonical, size_t) {
            out << "{\"asset\":\"spot\",\"index_original\":";
            JsonWriteString(canonical, out);
            out << ",\"index_canonical\":";
            JsonWriteString(canonical, out);
            out << '}';
        });
        out << ",\"requests\":";
        WriteArray(out, plan.Requests(),
                   [&](const auto& request, size_t id) { WriteObservationRequest(out, plan, request, id, prepared.AllExpired()); });
        out << ",\"sample_dates\":";
        WriteArray(out, plan.SampleDates(), [&](const auto& date, size_t) { JsonWriteString(Date::ToString(date), out); });
        out << ",\"timeline\":";
        WriteArray(out, plan.TimeLine(), [&](double time, size_t) { out << DebugNumber(time); });
        out << ",\"event_to_sample\":";
        WriteArray(out, plan.EventToSample(), [&](size_t sample, size_t) { out << sample; });
        out << ",\"live_events\":";
        WriteArray(out, plan.LiveEventIds(),
                   [&](size_t event, size_t i) { out << "{\"event_id\":" << event << ",\"future_event_index\":" << i << '}'; });
        out << ",\"sample_definitions\":";
        WriteArray(out, plan.DefLine(), [&](const auto& def, size_t id) {
            out << "{\"sample_id\":" << id << ",\"numeraire\":" << (def.numeraire_ ? "true" : "false") << ",\"index_names\":";
            WriteArray(out, def.indexNames_, [&](const auto& name, size_t) { JsonWriteString(name, out); });
            out << ",\"discount_maturities\":";
            WriteArray(out, def.discountMats_, [&](double time, size_t) { out << DebugNumber(time); });
            out << ",\"forward_maturities\":";
            WriteArray(out, def.forwardMats_,
                       [&](const auto& row, size_t) { WriteArray(out, row, [&](double time, size_t) { out << DebugNumber(time); }); });
            out << ",\"libor_definitions\":";
            WriteArray(out, def.liborDefs_, [&](const auto& rate, size_t) {
                out << "{\"start\":" << DebugNumber(rate.start_) << ",\"end\":" << DebugNumber(rate.end_) << ",\"curve\":";
                JsonWriteString(rate.curve_, out);
                out << '}';
            });
            out << '}';
        });
        out << ",\"numeraire_requests\":";
        WriteArray(out, plan.EventToSample(), [&](size_t sample, size_t i) {
            out << "{\"event_id\":" << plan.LiveEventIds()[i] << ",\"future_event_index\":" << i << ",\"sample_id\":" << sample << '}';
        });
        out << '}';
        return String_(out.str());
    }

    namespace {
        //  The diagnostic explicitly runs the full valuation in the prepared mode
        //  (tree-walk or compiled; AAD exercise valuation arrives with the fuzzy milestone)
        void RunSimulationDiagnostic(const PreparedScript_& prepared, AAD::Model_<double>* model, size_t nPaths, LsmcDiagnostics_* diagnostics) {
            if (prepared.AllExpired())
                return;
            const auto& simulation = prepared.Simulation();
            if (prepared.Product().ContainsExercise())
                MCLsmcSimulation(prepared, model, nPaths, diagnostics);
            else
                MCDoubleSimulation(prepared, model, nPaths, simulation.rsg_, simulation.useBb_, simulation.compiled_, true);
        }

        void WriteSimulationSettings(std::ostream& out, const MonteCarloSettings_& simulation) {
            out << ",\"simulation\":{\"rsg\":";
            JsonWriteString(simulation.rsg_, out);
            out << ",\"use_bb\":" << (simulation.useBb_ ? "true" : "false") << ",\"enable_aad\":" << (simulation.enableAad_ ? "true" : "false")
                << ",\"smooth\":" << DebugNumber(simulation.smooth_) << ",\"compiled\":" << (simulation.compiled_.value_or(false) ? "true" : "false")
                << ",\"lsmc_basis_degree\":" << simulation.lsmcBasisDegree_ << "}";
        }

        void JsonWriteStringOrNull(const String_& text, std::ostream& out) {
            if (text.empty())
                out << "null";
            else
                JsonWriteString(text, out);
        }

        void WriteExerciseEvent(std::ostream& out, const ExerciseEventStats_& event) {
            out << "{\"event_id\":" << event.eventId_ << ",\"date\":";
            JsonWriteString(Date::ToString(event.date_), out);
            out << ",\"basis_degree\":" << event.basisDegree_ << ",\"regressor_index\":";
            JsonWriteStringOrNull(event.regressorIndex_, out);
            out << ",\"num_cond_true_paths\":" << event.numCondTruePaths_ << ",\"num_coefficients\":" << event.coefficients_.size()
                << ",\"coefficients\":";
            WriteArray(out, event.coefficients_, [&](double coefficient, size_t) { out << DebugNumber(coefficient); });
            out << ",\"degenerate\":" << (event.degenerate_ ? "true" : "false") << ",\"degenerate_reason\":";
            JsonWriteStringOrNull(event.degenerateReason_, out);
            out << ",\"exercise_rate\":" << DebugNumber(event.exerciseRate_) << '}';
        }
    } // namespace

    String_ ExplainScriptSimulation(const ScriptProductData_& data,
                                    const Handle_<ModelData_>& modelData,
                                    size_t nPaths,
                                    const ScriptValuationSettings_& valuation,
                                    const MonteCarloSettings_& requestedSimulation) {
        REQUIRE2(modelData, "InvalidSetting: modelData=null; expected a non-null model", ScriptError_);
        REQUIRE2(nPaths > 0, "InvalidPathCount: number of Monte Carlo paths must be positive", ScriptError_);
        //  The diagnostic explicitly runs the full three-phase valuation, so its cost is
        //  the cost of a simulation; the double driver covers tree-walk and compiled
        //  (AAD exercise valuation arrives with the fuzzy milestone)
        REQUIRE2(!requestedSimulation.enableAad_,
                 "UnsupportedExecutionMode: the simulation diagnostic runs the double LSMC driver (tree-walk or compiled); AAD exercise valuation "
                 "is not implemented yet",
                 ScriptError_);
        const auto simulation = requestedSimulation;
        const auto settings = ResolveValuationSettings(valuation);
        auto model = CreateModel<double>(modelData);
        const auto prepared = PrepareScript(data, model.get(), settings, simulation);
        ValidateSimulationSettings(simulation);

        LsmcDiagnostics_ diagnostics;
        diagnostics.nPaths_ = nPaths;
        RunSimulationDiagnostic(prepared, model.get(), nPaths, &diagnostics);

        std::ostringstream out;
        out << "{\"schema\":\"dal.script-simulation/1\",\"evaluation_date\":";
        JsonWriteString(Date::ToString(prepared.EvaluationDate()), out);
        WriteSimulationSettings(out, simulation);
        out << ",\"n_paths\":" << nPaths << ",\"exercise_events\":";
        WriteArray(out, diagnostics.events_, [&](const auto& event, size_t) { WriteExerciseEvent(out, event); });
        out << '}';
        return String_(out.str());
    }
} // namespace Dal::Script
