//
// Created by Codex on 2026/10/4.
//

#include <array>

#include <dal/platform/platform.hpp>
#include <dal/script/blockedreplay.hpp>
#include <dal/script/replayadmission.hpp>
#include <dal/storage/globals.hpp>
#include <dal/storage/json.hpp>

#include <dal-public/src/riskvalueinternal.hpp>
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

        Script::RiskRequest_ PlanRiskValuation(const Script::ScriptProduct_&,
                                               const Vector_<Script::RiskCoordinate_>& axis,
                                               ScriptValuationSettings_*,
                                               const Script::RiskRequest_& request,
                                               bool enableAad) {
            return Script::PlanScalarRiskRequest(axis, request, enableAad);
        }

        Script::WeightedRiskPlan_ PlanRiskValuation(const Script::ScriptProduct_& product,
                                                    const Vector_<Script::RiskCoordinate_>& axis,
                                                    ScriptValuationSettings_* valuation,
                                                    const Script::WeightedRiskRequest_& request,
                                                    bool enableAad) {
            const auto date = valuation->evaluationDate_ ? *valuation->evaluationDate_ : Script::CaptureScriptEvaluationDate();
            valuation->evaluationDate_ = date;
            return Script::PlanWeightedRiskRequest(product, axis, date, request, enableAad);
        }

        Script::JacobianRiskPlan_ PlanRiskValuation(const Script::ScriptProduct_& product,
                                                    const Vector_<Script::RiskCoordinate_>& axis,
                                                    ScriptValuationSettings_* valuation,
                                                    const Script::JacobianRiskRequest_& request,
                                                    bool enableAad) {
            const auto date = valuation->evaluationDate_ ? *valuation->evaluationDate_ : Script::CaptureScriptEvaluationDate();
            valuation->evaluationDate_ = date;
            return Script::PlanJacobianRiskRequest(product, axis, date, request, enableAad);
        }

        Script::Detail::AADBlockReplaySettings_ ReplayLimits(const Script::JacobianRiskRequest_& request) {
            return {request.maxBlockWidth_, request.selection_.numericPayloadBudgetBytes_, request.scratchCapacityBudgetBytes_,
                    request.recordingCapacityBudgetBytes_};
        }

        template <class P_> void PreflightRiskValuation(const P_&, int, const MonteCarloSettings_&, const AAD::Model_<double>&) {}

        void PreflightRiskValuation(const Script::JacobianRiskPlan_& plan,
                                    int paths,
                                    const MonteCarloSettings_& simulation,
                                    const AAD::Model_<double>& model) {
            const auto threads = ThreadPool_::GetInstance()->NumThreads();
            const Script::BatchPlan_ batches(static_cast<size_t>(paths), threads);
            if (plan.EnableAad())
                static_cast<void>(Script::Detail::PlanAADBlockReplay(plan.OutputAxis().size(), plan.InputAxis().size(),
                                                                     plan.CompleteInputAxis().size(), batches, threads,
                                                                     simulation.compiled_.value_or(false), ReplayLimits(plan.Request())));
            else
                Script::Detail::PreflightPassiveReplay(plan.OutputAxis().size(), batches, threads,
                                                       typeid(model) == typeid(AAD::BlackScholes_<double>), ReplayLimits(plan.Request()));
        }

        template <class R_> Handle_<ScriptProductData_> SnapshotRiskProduct(const Handle_<ScriptProductData_>& product, const R_&) { return product; }

        Handle_<ScriptProductData_> SnapshotRiskProduct(const Handle_<ScriptProductData_>& product, const Script::JacobianRiskRequest_&) {
            REQUIRE2(product, "InvalidJacobianRiskRequest: product must not be null; field=product", ScriptError_);
            return Handle_<ScriptProductData_>(new ScriptProductData_(product->Name(), product->Dates(), product->EventTexts(), product->Settings()));
        }

        template <class R_> Handle_<ModelData_> SnapshotRiskModel(const Handle_<ModelData_>& model, const R_&) { return model; }

        Handle_<ModelData_> SnapshotRiskModel(const Handle_<ModelData_>& model, const Script::JacobianRiskRequest_&) {
            REQUIRE2(model, "InvalidJacobianRiskRequest: model must not be null; field=model", ScriptError_);
            const auto snapshot = JSON::WriteString(*model);
            JSONReadOptions_ options;
            options.maxInputBytes_ = snapshot.size();
            const auto copy = handle_cast<ModelData_>(JSON::ReadString(snapshot.data(), snapshot.size(), options));
            REQUIRE2(copy && typeid(*copy) == typeid(*model), "UnsupportedJacobianRisk: model snapshot type changed; field=model", ScriptError_);
            return copy;
        }

        void ValidatePreparedRiskPlan(const Script::RiskRequest_&,
                                      const Vector_<Script::RiskCoordinate_>& axis,
                                      const Script::PreparedScript_& prepared,
                                      const AAD::Model_<double>& model) {
            CheckPreparedAxis(axis, InputAxis(model, prepared.Product()));
        }

        void ValidatePreparedRiskPlan(const Script::WeightedRiskPlan_& plan,
                                      const Vector_<Script::RiskCoordinate_>&,
                                      const Script::PreparedScript_& prepared,
                                      const AAD::Model_<double>& model) {
            Script::ValidateWeightedRiskPreparedAxes(plan, prepared.Product(), InputAxis(model, prepared.Product()));
        }

        void ValidatePreparedRiskPlan(const Script::JacobianRiskPlan_& plan,
                                      const Vector_<Script::RiskCoordinate_>&,
                                      const Script::PreparedScript_& prepared,
                                      const AAD::Model_<double>& model) {
            Script::ValidateJacobianRiskPreparedAxes(plan, prepared.Product(), InputAxis(model, prepared.Product()));
        }

        template <class P_>
        Script::PreparedScript_ PrepareRiskValuation(const ScriptProductData_& product,
                                                     AAD::Model_<double>* model,
                                                     const Handle_<ModelData_>&,
                                                     const ScriptValuationSettings_& valuation,
                                                     const MonteCarloSettings_& simulation,
                                                     int,
                                                     const P_&,
                                                     size_t*) {
            return Script::PrepareScript(product, model, valuation, simulation);
        }

        Script::PreparedScript_ PrepareRiskValuation(const ScriptProductData_& product,
                                                     AAD::Model_<double>* model,
                                                     const Handle_<ModelData_>& modelData,
                                                     const ScriptValuationSettings_& valuation,
                                                     const MonteCarloSettings_& simulation,
                                                     int paths,
                                                     const Script::JacobianRiskPlan_& plan,
                                                     size_t* admittedWidth) {
            return Script::Detail::PrepareScriptWithAdmission(product, model, valuation, simulation, [&](const auto& prepared) {
                *admittedWidth =
                    Script::Detail::PreflightPreparedReplay(prepared, modelData, static_cast<size_t>(paths), plan.OutputAxis(), plan.InputPositions(),
                                                            model->Parameters().size(), ReplayLimits(plan.Request()));
            });
        }

        Script::Detail::AADBlockReplayResult_ EvaluateJacobianRiskSource(const Script::PreparedScript_& prepared,
                                                                         const AAD::Model_<double>& model,
                                                                         const Handle_<ModelData_>& modelData,
                                                                         const MonteCarloSettings_& simulation,
                                                                         size_t paths,
                                                                         const Script::JacobianRiskPlan_& plan,
                                                                         size_t admittedWidth) {
            if (!simulation.enableAad_)
                return Script::Detail::EvaluatePassiveOutputReplay(prepared, modelData, paths, plan.OutputAxis(), ReplayLimits(plan.Request()));
            const Script::Detail::AADBatchSettings_ settings{
                simulation.rsg_,     simulation.useBb_, -1, simulation.smooth_, paths, model.Parameters().size(), prepared.ConstVarNames().size(),
                prepared.PayOffIdx()};
            auto limits = ReplayLimits(plan.Request());
            limits.maxWidth_ = admittedWidth;
            return Script::Detail::EvaluateAADBlockReplay(prepared, modelData, settings, plan.OutputAxis(), plan.InputPositions(), limits);
        }

        template <class R_, class F_>
        auto EvaluateScriptRisk(const Handle_<ScriptProductData_>& product,
                                const Handle_<ModelData_>& modelData,
                                int numPath,
                                const R_& request,
                                const ScriptValuationSettings_& valuation,
                                const MonteCarloSettings_& simulation,
                                const F_& evaluate) {
            XGLOBAL::ValuationMutationGuard_ valuationGuard;
            REQUIRE2(numPath > 0, "InvalidPathCount: numPath must be a positive integer; numPath=" + String_(std::to_string(numPath)), ScriptError_);
            const auto execution = simulation;
            const auto requested = request;
            auto valuationCopy = valuation;
            const auto productCopy = SnapshotRiskProduct(product, requested);
            const auto modelCopy = SnapshotRiskModel(modelData, requested);
            Detail::CheckScriptValuationInputs(productCopy, modelCopy);
            Script::ValidateSimulationSettings(execution);
            auto model = CreateModel<double>(modelCopy);
            auto parsed = productCopy->Product();
            parsed.IndexVariables();
            const auto axis = InputAxis(*model, parsed);
            const auto planned = PlanRiskValuation(parsed, axis, &valuationCopy, requested, execution.enableAad_);
            PreflightRiskValuation(planned, numPath, execution, *model);
            const auto settings = Script::ResolveValuationSettings(valuationCopy);
            size_t admittedWidth = 0;
            const auto prepared = PrepareRiskValuation(*productCopy, model.get(), modelCopy, settings, execution, numPath, planned, &admittedWidth);
            ValidatePreparedRiskPlan(planned, axis, prepared, *model);
            const auto provenance = Provenance(prepared, *productCopy, *modelCopy, numPath);
            return evaluate(prepared, model.get(), modelCopy, execution, axis, planned, provenance, admittedWidth);
        }
    } // namespace

    namespace Detail {
        Vector_<Script::RiskCoordinate_> ScriptRiskInputAxis(const AAD::Model_<double>& model, const Script::ScriptProduct_& product) {
            return InputAxis(model, product);
        }

        Script::RiskResultProvenance_
        CaptureScriptRiskProvenance(const Script::PreparedScript_& prepared, const ScriptProductData_& product, const ModelData_& model, int paths) {
            return Provenance(prepared, product, model, paths);
        }
    } // namespace Detail

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
        return EvaluateScriptRisk(product, modelData, numPath, request, valuation, simulation,
                                  [&](const auto& prepared, auto* model, const auto& modelCopy, const auto& execution, const auto& axis,
                                      const auto& planned, const auto& provenance, size_t) {
                                      const size_t paths = static_cast<size_t>(numPath);
                                      const auto source =
                                          execution.enableAad_
                                              ? Script::MCSimulation<AAD::Number_>(prepared, modelCopy, paths, execution.rsg_, execution.useBb_,
                                                                                   execution.compiled_, -1, execution.smooth_)
                                              : Script::MCDoubleSimulation(prepared, model, paths, execution.rsg_, execution.useBb_,
                                                                           execution.compiled_, true);
                                      return Script::ProjectMonteCarloRiskResult(source, numPath, axis, planned, provenance);
                                  });
    }

    Script::WeightedRiskResult_ ValueByMonteCarloWithWeightedRisk(const Handle_<ScriptProductData_>& product,
                                                                  const Handle_<ModelData_>& modelData,
                                                                  int numPath,
                                                                  const Script::WeightedRiskRequest_& request,
                                                                  const ScriptValuationSettings_& valuation,
                                                                  const MonteCarloSettings_& simulation) {
        return EvaluateScriptRisk(product, modelData, numPath, request, valuation, simulation,
                                  [&](const auto& prepared, auto* model, const auto& modelCopy, const auto& execution, const auto&,
                                      const auto& planned, const auto& provenance, size_t) {
                                      const size_t paths = static_cast<size_t>(numPath);
                                      const Script::Detail::WeightedSimulationObjective_ objective(planned);
                                      const auto source =
                                          execution.enableAad_
                                              ? Script::MCAADSimulationWithObjective(prepared, modelCopy, paths, execution.rsg_, execution.useBb_,
                                                                                     execution.compiled_, -1, execution.smooth_, objective)
                                              : Script::MCDoubleSimulationWithObjective(prepared, model, paths, execution.rsg_, execution.useBb_,
                                                                                        execution.compiled_, true, objective);
                                      return Script::ProjectWeightedMonteCarloRiskResult(source, source.componentSums_, numPath, planned, provenance);
                                  });
    }

    Script::JacobianRiskResult_ ValueByMonteCarloWithJacobianRisk(const Handle_<ScriptProductData_>& product,
                                                                  const Handle_<ModelData_>& modelData,
                                                                  int numPath,
                                                                  const Script::JacobianRiskRequest_& request,
                                                                  const ScriptValuationSettings_& valuation,
                                                                  const MonteCarloSettings_& simulation) {
        return EvaluateScriptRisk(product, modelData, numPath, request, valuation, simulation,
                                  [&](const auto& prepared, auto* model, const auto& modelCopy, const auto& execution, const auto&, const auto& plan,
                                      const auto& provenance, size_t admittedWidth) {
                                      auto source = EvaluateJacobianRiskSource(prepared, *model, modelCopy, execution, static_cast<size_t>(numPath),
                                                                               plan, admittedWidth);
                                      return Script::ProjectJacobianRiskResult(std::move(source), numPath, plan, provenance);
                                  });
    }
} // namespace Dal
