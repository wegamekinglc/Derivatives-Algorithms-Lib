//
// Created by Codex on 2026/10/7.
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal-public/src/value.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;
using namespace Dal::Script;

namespace {
    constexpr const char* REQUEST_ERROR = "InvalidPortfolioRiskRequest";

    template <class T_> std::shared_ptr<T_> PortfolioHandleInput(const py::handle& value, const std::string& field, const char* type) {
        if (!py::isinstance<T_>(value))
            throw py::type_error(InputContext(value, field, type, "InvalidScriptPortfolio"));
        return py::cast<std::shared_ptr<T_>>(value);
    }

    std::shared_ptr<ScriptPortfolioData_> PortfolioInput(const py::object& ids, const py::object& products, const py::object& models) {
        const auto tradeIds = RequestIds(ids, {"ScriptPortfolio_New; trade_ids", "InvalidScriptPortfolio"});
        const auto tradeProducts = RequestSequence(products, {"ScriptPortfolio_New; products", "InvalidScriptPortfolio"});
        const auto tradeModels = RequestSequence(models, {"ScriptPortfolio_New; modelData", "InvalidScriptPortfolio"});
        REQUIRE2(tradeIds && !tradeIds->empty() && tradeIds->size() == tradeProducts.size() && tradeIds->size() == tradeModels.size(),
                 "InvalidScriptPortfolio: trade_ids, products and modelData must have equal nonzero lengths", ScriptError_);
        Vector_<PortfolioTrade_> trades;
        trades.reserve(tradeIds->size());
        for (size_t trade = 0; trade < tradeIds->size(); ++trade) {
            const auto field = "ScriptPortfolio_New; trade=" + Text((*tradeIds)[trade]);
            trades.push_back({(*tradeIds)[trade],
                              Handle_<ScriptProductData_>(
                                  PortfolioHandleInput<ScriptProductData_>(tradeProducts[trade], field + "; products", "ScriptProductData_")),
                              Handle_<ModelData_>(PortfolioHandleInput<ModelData_>(tradeModels[trade], field + "; modelData", "ModelData_"))});
        }
        py::gil_scoped_release release;
        return std::make_shared<ScriptPortfolioData_>("", trades);
    }

    RiskRequest_ PortfolioSelection(const py::object& inputs, const py::object& outputs, const py::object& factors, const py::object& budget) {
        return {RequestIds(inputs, {"portfolio request; inputs", REQUEST_ERROR}), RequestIds(outputs, {"portfolio request; outputs", REQUEST_ERROR}),
                RequestFactors(factors, {"portfolio request; report_factors", REQUEST_ERROR}),
                RequestBudget(budget, {"portfolio request; numeric_payload_budget_bytes", REQUEST_ERROR})};
    }

    template <class R_> py::class_<R_> BindPortfolioRequest(py::module_& m, const char* name) {
        return WithCopies(py::class_<R_>(m, name))
            .def_property_readonly("inputs", [](const R_& value) { return RiskTextVector(value.selection_.inputs_); })
            .def_property_readonly("outputs", [](const R_& value) { return RiskTextVector(value.selection_.outputs_); })
            .def_property_readonly("report_factors",
                                   [](const R_& value) {
                                       return value.selection_.reportFactors_
                                                  ? std::optional<std::vector<double>>(CopyRiskVector(*value.selection_.reportFactors_))
                                                  : std::nullopt;
                                   })
            .def_property_readonly("numeric_payload_budget_bytes", [](const R_& value) { return value.selection_.numericPayloadBudgetBytes_; })
            .def_property_readonly("recording_capacity_budget_bytes", [](const R_& value) { return value.recordingCapacityBudgetBytes_; })
            .def_property_readonly("scratch_capacity_budget_bytes", [](const R_& value) { return value.scratchCapacityBudgetBytes_; });
    }

    void BindPortfolioRequests(py::module_& m) {
        BindPortfolioRequest<PortfolioWeightedRiskRequest_>(m, "PortfolioWeightedRiskRequest_")
            .def(py::init([](const py::object& inputs, const py::object& outputs, const py::object& weights, const py::object& factors,
                             const py::object& resultBudget, const py::object& recordingBudget, const py::object& scratchBudget) {
                     return PortfolioWeightedRiskRequest_{
                         PortfolioSelection(inputs, outputs, factors, resultBudget),
                         RequestNumbers(weights, {"PortfolioWeightedRiskRequest_; weights", REQUEST_ERROR}, "weights"),
                         RequestBudget(recordingBudget, {"portfolio request; recording_capacity_budget_bytes", REQUEST_ERROR}),
                         RequestBudget(scratchBudget, {"portfolio request; scratch_capacity_budget_bytes", REQUEST_ERROR})};
                 }),
                 py::kw_only(), py::arg("inputs") = py::none(), py::arg("outputs") = py::none(), py::arg("weights") = py::none(),
                 py::arg("report_factors") = py::none(), py::arg("numeric_payload_budget_bytes") = py::none(),
                 py::arg("recording_capacity_budget_bytes") = py::none(), py::arg("scratch_capacity_budget_bytes") = py::none())
            .def_property_readonly("weights", [](const PortfolioWeightedRiskRequest_& value) {
                return value.weights_ ? std::optional<std::vector<double>>(CopyRiskVector(*value.weights_)) : std::nullopt;
            });
        BindPortfolioRequest<PortfolioJacobianRiskRequest_>(m, "PortfolioJacobianRiskRequest_")
            .def(py::init([](const py::object& inputs, const py::object& outputs, const py::object& factors, const py::object& width,
                             const py::object& resultBudget, const py::object& recordingBudget, const py::object& scratchBudget) {
                     const auto maximum = RequestUnsigned(width, {"PortfolioJacobianRiskRequest_; max_block_width", REQUEST_ERROR});
                     REQUIRE2(maximum > 0 && maximum <= AAD::ADJ_SIZE,
                              "InvalidPortfolioRiskRequest: max_block_width must be in [1, " + String_(std::to_string(AAD::ADJ_SIZE)) + "]",
                              ScriptError_);
                     return PortfolioJacobianRiskRequest_{
                         PortfolioSelection(inputs, outputs, factors, resultBudget), maximum,
                         RequestBudget(recordingBudget, {"portfolio request; recording_capacity_budget_bytes", REQUEST_ERROR}),
                         RequestBudget(scratchBudget, {"portfolio request; scratch_capacity_budget_bytes", REQUEST_ERROR})};
                 }),
                 py::kw_only(), py::arg("inputs") = py::none(), py::arg("outputs") = py::none(), py::arg("report_factors") = py::none(),
                 py::arg("max_block_width") = 1, py::arg("numeric_payload_budget_bytes") = py::none(),
                 py::arg("recording_capacity_budget_bytes") = py::none(), py::arg("scratch_capacity_budget_bytes") = py::none())
            .def_property_readonly("max_block_width", [](const PortfolioJacobianRiskRequest_& value) { return value.maxBlockWidth_; });
    }

    template <class R_> py::class_<R_> BindPortfolioResult(py::module_& m, const char* name) {
        return WithCopies(py::class_<R_>(m, name))
            .def_property_readonly("jacobian", [](const R_& value) { return value.Jacobian(); })
            .def_property_readonly("reported_jacobian", [](const R_& value) { return value.ReportedJacobian(); })
            .def_property_readonly("output_axis", [](const R_& value) { return CopyRiskVector(value.OutputAxis()); })
            .def_property_readonly("complete_output_axis", [](const R_& value) { return CopyRiskVector(value.CompleteOutputAxis()); })
            .def_property_readonly("input_axis", [](const R_& value) { return CopyRiskVector(value.InputAxis()); })
            .def_property_readonly("complete_input_axis", [](const R_& value) { return CopyRiskVector(value.CompleteInputAxis()); })
            .def_property_readonly("provenance", [](const R_& value) { return value.Provenance(); })
            .def_property_readonly("execution", [](const R_& value) { return value.Execution(); });
    }

    void BindPortfolioProvenance(py::module_& m) {
        WithCopies(py::class_<PortfolioRiskProvenance_>(m, "PortfolioRiskProvenance_"))
            .def_property_readonly("method", [](const PortfolioRiskProvenance_& value) { return Text(value.method_); })
            .def_property_readonly("engine", [](const PortfolioRiskProvenance_& value) { return Text(value.engine_); })
            .def_property_readonly("normalization", [](const PortfolioRiskProvenance_& value) { return Text(value.normalization_); })
            .def_property_readonly("calibration", [](const PortfolioRiskProvenance_& value) { return Text(value.calibration_); })
            .def_property_readonly("evaluation_date", [](const PortfolioRiskProvenance_& value) { return value.evaluationDate_; })
            .def_property_readonly("trade_ids", [](const PortfolioRiskProvenance_& value) { return RiskTextVector(value.tradeIds_); })
            .def_property_readonly("model_owners", [](const PortfolioRiskProvenance_& value) { return CopyRiskVector(value.modelOwners_); })
            .def_property_readonly("trades", [](const PortfolioRiskProvenance_& value) { return CopyRiskVector(value.trades_); });
    }

    void BindPortfolioSampling(py::module_& m) {
        WithCopies(py::class_<AAD::SampleDef_::RateDef_>(m, "PortfolioRateDefinition_"))
            .def_property_readonly("start", [](const AAD::SampleDef_::RateDef_& value) { return value.start_; })
            .def_property_readonly("end", [](const AAD::SampleDef_::RateDef_& value) { return value.end_; })
            .def_property_readonly("curve", [](const AAD::SampleDef_::RateDef_& value) { return Text(value.curve_); });
        WithCopies(py::class_<AAD::SampleDef_>(m, "PortfolioSampleDefinition_"))
            .def_property_readonly("numeraire", [](const AAD::SampleDef_& value) { return value.numeraire_; })
            .def_property_readonly("index_names", [](const AAD::SampleDef_& value) { return RiskTextVector(value.indexNames_); })
            .def_property_readonly("discount_maturities", [](const AAD::SampleDef_& value) { return CopyRiskVector(value.discountMats_); })
            .def_property_readonly("libor_definitions", [](const AAD::SampleDef_& value) { return CopyRiskVector(value.liborDefs_); })
            .def_property_readonly("forward_maturities", [](const AAD::SampleDef_& value) {
                std::vector<std::vector<double>> rows;
                for (const auto& row : value.forwardMats_)
                    rows.push_back(CopyRiskVector(row));
                return rows;
            });
    }

    void BindPortfolioExecution(py::module_& m) {
        WithCopies(py::class_<PortfolioGroupExecution_>(m, "PortfolioGroupExecution_"))
            .def_property_readonly("model_owner", [](const PortfolioGroupExecution_& value) { return value.modelOwner_; })
            .def_property_readonly("trade_positions", [](const PortfolioGroupExecution_& value) { return CopyRiskVector(value.tradePositions_); })
            .def_property_readonly("random_dimension", [](const PortfolioGroupExecution_& value) { return value.randomDimension_; })
            .def_property_readonly("factors", [](const PortfolioGroupExecution_& value) { return value.factors_; })
            .def_property_readonly("deterministic_numeraire", [](const PortfolioGroupExecution_& value) { return value.deterministicNumeraire_; })
            .def_property_readonly("sample_dates", [](const PortfolioGroupExecution_& value) { return CopyRiskVector(value.sampleDates_); })
            .def_property_readonly("timeline", [](const PortfolioGroupExecution_& value) { return CopyRiskVector(value.timeLine_); })
            .def_property_readonly("sample_definitions",
                                   [](const PortfolioGroupExecution_& value) { return CopyRiskVector(value.sampleDefinitions_); })
            .def_property_readonly("simulation", [](const PortfolioGroupExecution_& value) { return value.simulation_; })
            .def_property_readonly("generated_scenarios", [](const PortfolioGroupExecution_& value) { return value.generatedScenarios_; })
            .def_property_readonly("evaluator_calls", [](const PortfolioGroupExecution_& value) { return value.evaluatorCalls_; })
            .def_property_readonly("suffix_reversals", [](const PortfolioGroupExecution_& value) { return value.suffixReversals_; })
            .def_property_readonly("prefix_reversals", [](const PortfolioGroupExecution_& value) { return value.prefixReversals_; })
            .def_property_readonly("actual_widths", [](const PortfolioGroupExecution_& value) { return CopyRiskVector(value.actualWidths_); })
            .def_property_readonly("replay_attempts", [](const PortfolioGroupExecution_& value) { return value.replayAttempts_; });
        WithCopies(py::class_<PortfolioRiskExecution_>(m, "PortfolioRiskExecution_"))
            .def_property_readonly("groups", [](const PortfolioRiskExecution_& value) { return CopyRiskVector(value.groups_); })
            .def_property_readonly("peak_recording_bytes", [](const PortfolioRiskExecution_& value) { return value.peakRecordingBytes_; })
            .def_property_readonly("peak_scratch_bytes", [](const PortfolioRiskExecution_& value) { return value.peakScratchBytes_; })
            .def_property_readonly("requested_max_block_width", [](const PortfolioRiskExecution_& value) { return value.requestedMaxBlockWidth_; });
    }

    template <class R_, auto VALUE_>
    auto EvaluatePortfolioRisk(const py::object& portfolio,
                               const py::object& paths,
                               const py::object& request,
                               const py::object& valuation,
                               const py::object& simulation,
                               const RiskValuationContext_& context) {
        const int count = PathCount(paths, context.function_);
        const Handle_<ScriptPortfolioData_> data(PortfolioHandleInput<ScriptPortfolioData_>(portfolio, context.function_, "ScriptPortfolioData_"));
        const std::string function(context.function_);
        const auto requested = SettingsInput<R_>(request, function + "; request", context.requestType_);
        const auto settings = SettingsInput<ScriptValuationSettings_>(valuation, function + "; valuation", "ScriptValuationSettings_");
        const auto execution = simulation.is_none()
                                   ? DefaultRiskMonteCarloSettings()
                                   : SettingsInput<MonteCarloSettings_>(simulation, function + "; simulation", "MonteCarloSettings_");
        py::gil_scoped_release release;
        return VALUE_(data, count, requested, settings, execution);
    }
} // namespace

void init_bindings_portfoliorisk(py::module_& m) {
    py::class_<ScriptPortfolioData_, Storable_, std::shared_ptr<ScriptPortfolioData_>>(m, "ScriptPortfolioData_")
        .def_property_readonly("trade_ids", [](const ScriptPortfolioData_& value) { return RiskTextVector(value.TradeIds()); })
        .def_property_readonly("model_owners", [](const ScriptPortfolioData_& value) { return CopyRiskVector(value.ModelOwners()); });
    m.def("ScriptPortfolio_New", &PortfolioInput, py::arg("trade_ids"), py::arg("products"), py::arg("modelData"));
    BindPortfolioRequests(m);
    BindPortfolioProvenance(m);
    BindPortfolioSampling(m);
    BindPortfolioExecution(m);
    BindPortfolioResult<PortfolioWeightedRiskResult_>(m, "PortfolioWeightedRiskResult_")
        .def_property_readonly("weighted_value", &PortfolioWeightedRiskResult_::WeightedValue)
        .def_property_readonly("component_means", [](const PortfolioWeightedRiskResult_& value) { return CopyRiskVector(value.ComponentMeans()); })
        .def_property_readonly("weights", [](const PortfolioWeightedRiskResult_& value) { return CopyRiskVector(value.Weights()); });
    BindPortfolioResult<PortfolioJacobianRiskResult_>(m, "PortfolioJacobianRiskResult_")
        .def_property_readonly("values", [](const PortfolioJacobianRiskResult_& value) { return CopyRiskVector(value.Values()); });
    m.def(
        "PortfolioMonteCarlo_ValueWithWeightedRisk",
        [](const py::object& portfolio, const py::object& paths, const py::object& request, const py::object& valuation,
           const py::object& simulation) {
            return EvaluatePortfolioRisk<PortfolioWeightedRiskRequest_, &ValuePortfolioByMonteCarloWithWeightedRisk>(
                portfolio, paths, request, valuation, simulation, {"PortfolioMonteCarlo_ValueWithWeightedRisk", "PortfolioWeightedRiskRequest_"});
        },
        py::arg("portfolio"), py::arg("num_path"), py::kw_only(), py::arg("request") = py::none(), py::arg("valuation") = py::none(),
        py::arg("simulation") = py::none());
    m.def(
        "PortfolioMonteCarlo_ValueWithJacobianRisk",
        [](const py::object& portfolio, const py::object& paths, const py::object& request, const py::object& valuation,
           const py::object& simulation) {
            return EvaluatePortfolioRisk<PortfolioJacobianRiskRequest_, &ValuePortfolioByMonteCarloWithJacobianRisk>(
                portfolio, paths, request, valuation, simulation, {"PortfolioMonteCarlo_ValueWithJacobianRisk", "PortfolioJacobianRiskRequest_"});
        },
        py::arg("portfolio"), py::arg("num_path"), py::kw_only(), py::arg("request") = py::none(), py::arg("valuation") = py::none(),
        py::arg("simulation") = py::none());
}
