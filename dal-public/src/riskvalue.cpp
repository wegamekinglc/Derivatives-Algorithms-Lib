//
// Created by Codex on 2026/10/4.
//

#include <array>

#include <dal/platform/platform.hpp>
#include <dal/storage/globals.hpp>
#include <dal/storage/json.hpp>

#include <dal-public/src/value.hpp>
#include <dal-public/src/valuevalidation.hpp>

namespace Dal {
    namespace {
        std::optional<String_> ModelPhysicalUnit(const AAD::Model_<double>& model, size_t ordinal) {
            static constexpr std::array<const char*, 4> BS_UNITS = {nullptr, "year^-1/2", "year^-1", "year^-1"};
            static constexpr std::array<const char*, 3> CORRELATED_UNITS = {nullptr, "year^-1/2", "year^-1"};
            const char* unit = nullptr;
            if (typeid(model) == typeid(AAD::BlackScholes_<double>)) {
                unit = BS_UNITS.at(ordinal);
            } else if (typeid(model) == typeid(AAD::CorrelatedBlackScholes_<double>)) {
                unit = ordinal == 3 * model.NumAssets() ? "year^-1" : CORRELATED_UNITS.at(ordinal % 3);
            }
            return unit ? std::optional<String_>(unit) : std::nullopt;
        }

        Vector_<Script::RiskCoordinate_> InputAxis(const AAD::Model_<double>& model, const Script::ScriptProduct_& product) {
            Vector_<Script::RiskCoordinate_> axis;
            const auto& parameters = model.Parameters();
            const auto& labels = model.ParameterLabels();
            REQUIRE2(parameters.size() == labels.size(), "InvalidRiskResult: model parameter/label dimensions disagree", ScriptError_);
            axis.reserve(parameters.size() + product.ConstVarNames().size());
            for (size_t ordinal = 0; ordinal < parameters.size(); ++ordinal)
                axis.push_back({"model:" + String_(std::to_string(ordinal)), labels[ordinal], "model", ordinal, *parameters[ordinal],
                                "model-coordinate", ModelPhysicalUnit(model, ordinal), 1.0});
            for (size_t ordinal = 0; ordinal < product.ConstVarNames().size(); ++ordinal)
                axis.push_back({"constant:" + String_(std::to_string(ordinal)), product.ConstVarNames()[ordinal], "constant", ordinal,
                                product.ConstVarValues()[ordinal], "script-number", std::nullopt, 1.0});
            return axis;
        }

        void CheckPreparedAxis(const Vector_<Script::RiskCoordinate_>& before, const Vector_<Script::RiskCoordinate_>& prepared) {
            REQUIRE2(before.size() == prepared.size(), "InvalidRiskResult: prepared input axis extent changed", ScriptError_);
            for (size_t index = 0; index < before.size(); ++index)
                REQUIRE2(before[index].id_ == prepared[index].id_ && before[index].label_ == prepared[index].label_ &&
                             before[index].value_ == prepared[index].value_,
                         "InvalidRiskResult: prepared input coordinate changed; input=" + before[index].id_, ScriptError_);
        }

        String_ RiskMethod(const Script::PreparedScript_& prepared) {
            if (prepared.AllExpired())
                return "Expired";
            if (!prepared.Simulation().enableAad_)
                return "PriceOnly";
            if (prepared.Product().ContainsExercise() && prepared.Simulation().lsmcPolicyRiskMode_ == "RetrainedBump")
                return "NativeAADWithRetrainedPolicySecant";
            return "NativeAAD";
        }

        Script::RiskResultProvenance_
        Provenance(const Script::PreparedScript_& prepared, const ScriptProductData_& product, const ModelData_& model, int paths) {
            const auto& simulation = prepared.Simulation();
            const bool exercise = prepared.Product().ContainsExercise();
            Script::RiskResultProvenance_ provenance;
            provenance.method_ = RiskMethod(prepared);
            provenance.modelType_ = model.Type();
            provenance.evaluationDate_ = prepared.EvaluationDate();
            Script::RiskExecutionSnapshot_ execution;
            execution.pathsPerReplicate_ = paths;
            execution.pricingReplicates_ = exercise ? simulation.lsmcRqmcReplicates_.value_or(1) : 1;
            execution.allExpired_ = prepared.AllExpired();
            execution.simulation_ = simulation;
            execution.productSettings_ = product.Settings();
            execution.productDates_ = product.Dates();
            execution.productEvents_ = product.EventTexts();
            execution.todayFixingPolicy_ = prepared.Settings().todayFixingPolicy_.String();
            execution.fixingSource_ = Script::FixingSourceKind(prepared.Settings());
            execution.modelSnapshotJson_ = JSON::WriteString(model);
            for (const auto& request : prepared.Plan().Requests())
                execution.observations_.push_back(
                    {request.key_.canonicalIndex_, request.key_.fixingTime_, request.historical_,
                     request.historyValueId_ ? std::optional<double>(prepared.Plan().KnownValue(*request.historyValueId_)) : std::nullopt});
            provenance.execution_ = std::move(execution);
            return provenance;
        }
    } // namespace

    MonteCarloSettings_ DefaultRiskMonteCarloSettings() {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        return simulation;
    }

    Script::RiskResult_ ValueByMonteCarloWithRisk(const Handle_<ScriptProductData_>& product,
                                                  const Handle_<ModelData_>& modelData,
                                                  int numPath,
                                                  const Script::RiskRequest_& request,
                                                  const ScriptValuationSettings_& valuation,
                                                  const MonteCarloSettings_& simulation) {
        XGLOBAL::ValuationMutationGuard_ valuationGuard;
        REQUIRE2(numPath > 0, "InvalidPathCount: numPath must be a positive integer; numPath=" + String_(std::to_string(numPath)), ScriptError_);
        const auto execution = simulation;
        const auto requested = request;
        const auto valuationCopy = valuation;
        const auto productCopy = product;
        const auto modelCopy = modelData;
        Detail::CheckScriptValuationInputs(productCopy, modelCopy);
        Script::ValidateSimulationSettings(execution);
        auto model = CreateModel<double>(modelCopy);
        auto parsed = productCopy->Product();
        parsed.IndexVariables();
        const auto axis = InputAxis(*model, parsed);
        const auto planned = Script::PlanScalarRiskRequest(axis, requested, execution.enableAad_);
        const auto settings = Script::ResolveValuationSettings(valuationCopy);
        const auto prepared = Script::PrepareScript(*productCopy, model.get(), settings, execution);
        CheckPreparedAxis(axis, InputAxis(*model, prepared.Product()));
        const auto provenance = Provenance(prepared, *productCopy, *modelCopy, numPath);
        const size_t paths = static_cast<size_t>(numPath);
        const auto source = execution.enableAad_ ? Script::MCSimulation<AAD::Number_>(prepared, modelCopy, paths, execution.rsg_, execution.useBb_,
                                                                                      execution.compiled_, -1, execution.smooth_)
                                                 : Script::MCDoubleSimulation(prepared, model.get(), paths, execution.rsg_, execution.useBb_,
                                                                              execution.compiled_, true);
        return Script::ProjectMonteCarloRiskResult(source, numPath, axis, planned, provenance);
    }
} // namespace Dal
