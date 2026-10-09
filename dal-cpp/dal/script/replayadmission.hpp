//
// Created by Codex on 2026/10/06.
//

#pragma once

#include <dal/script/passivereplay.hpp>

namespace Dal::Script::Detail {
    template <class T_> Vector_<Vector_<T_>> AdmissionVectorSeed(const PreparedScript_& product, Vector_<Vector_<T_>> seed) {
        if (!product.Product().PastEvents().empty()) {
            const auto& capacities = product.Product().VectorCapacities();
            for (size_t i = 0; i < capacities.size(); ++i)
                seed[i].Resize(capacities[i]);
        }
        return seed;
    }

    class ReplayAdmissionProduct_ {
        const PreparedScript_& product_;
        const Vector_<> variables_;

        template <class T_, class E_> E_ WithVectorSeed(E_ evaluator) const {
            evaluator.SetHistoricalVectorSeed(AdmissionVectorSeed(product_, TypedVectorValues<T_>(product_.Product().VectorValues())));
            return evaluator;
        }

    public:
        explicit ReplayAdmissionProduct_(const PreparedScript_& product) : product_(product), variables_(product.Product().VarNames().size(), 0.0) {}
        [[nodiscard]] const Vector_<AAD::SampleDef_>& DefLine() const { return product_.DefLine(); }
        template <class T_> Evaluator_<T_> BuildEvaluator() const {
            return WithVectorSeed<T_>(Evaluator_<T_>(variables_, Apply([](double value) { return T_(value); }, product_.Product().ConstVarValues()),
                                                     product_.Product().VectorCapacities()));
        }
        template <class T_> EvalState_<T_> BuildEvalState() const {
            return WithVectorSeed<T_>(EvalState_<T_>(variables_, Apply([](double value) { return T_(value); }, product_.Product().ConstVarValues()),
                                                     product_.MaxNestedIfs(), product_.Simulation().smooth_, product_.Product().VectorCapacities()));
        }
        template <class T_> FuzzyEvaluator_<T_> BuildFuzzyEvaluator() const {
            return WithVectorSeed<T_>(
                FuzzyEvaluator_<T_>(variables_, Apply([](double value) { return T_(value); }, product_.Product().ConstVarValues()),
                                    product_.MaxNestedIfs(), product_.Simulation().smooth_, product_.Product().VectorCapacities()));
        }
    };

    template <class T_> void AdmitKnownPathScratch(const AAD::Model_<T_>& model, Scenario_<T_>* path) {
        REQUIRE2(path && !path->empty(), "InvalidJacobianPreflight: model path is empty", ScriptError_);
        if (typeid(model) == typeid(AAD::CorrelatedBlackScholes_<T_>))
            (*path)[0].modelScratch_.Resize(model.NumAssets());
        if (const auto* hybrid = dynamic_cast<const AAD::HybridModel_<T_>*>(&model)) {
            (*path)[0].modelScratch_.Resize(hybrid->StateDim());
            (*path)[0].modelFactorScratch_.Resize(hybrid->NumFactors());
        }
    }

    template <class E_>
    void AdmitNativeInitialization(const PreparedScript_& product,
                                   AAD::Model_<AAD::Number_>* model,
                                   Scenario_<AAD::Number_>* path,
                                   E_* evaluator,
                                   AAD::RecordingScope_* recording) {
        AAD::Rewind(*AAD::Tape());
        for (auto* parameter : model->Parameters())
            PutOnTape(*parameter);
        for (auto& parameter : evaluator->ConstVarVals())
            PutOnTape(parameter);
        AAD::Number_ zero = 0.0;
        PutOnTape(zero);
        recording->StartRecording();
        model->Init(product.TimeLine(), product.DefLine());
        InitializePath(*path);
        auto retainHistoryShape = [&](const auto& past) {
            evaluator->SetHistoricalSeed(past.VarVals());
            evaluator->SetHistoricalVectorSeed(AdmissionVectorSeed(product, past.VectorVals()));
        };
        if (product.Simulation().compiled_.value_or(false)) {
            const EvalState_<AAD::Number_> past(Vector_<>(product.Product().VarNames().size(), 0.0), evaluator->ConstVarVals(), 0, 0.0,
                                                product.Product().VectorCapacities());
            retainHistoryShape(past);
        } else {
            const PastEvaluator_<AAD::Number_> past(Vector_<>(product.Product().VarNames().size(), 0.0), evaluator->ConstVarVals(),
                                                    product.Product().VectorCapacities());
            retainHistoryShape(past);
        }
    }

    inline void AdmitNativeWorker(const PreparedScript_& product,
                                  const Handle_<ModelData_>& modelData,
                                  const Vector_<RiskOutputCoordinate_>& outputs,
                                  const Vector_<size_t>& inputs,
                                  size_t completeModelInputs,
                                  size_t width,
                                  size_t scratchQuota,
                                  size_t tapeQuota) {
        const ReplayAdmissionProduct_ admissionProduct(product);
        BufferCapacityBudget_ scratch(scratchQuota);
        BufferCapacityScope_ scratchScope(&scratch, AADBlockBatchFixedPayloadBytes(product.Simulation().compiled_.value_or(false)));
        AAD::TapeCapacityBudget_ tape(tapeQuota);
        AAD::TapeCapacityScope_ tapeScope(&tape, true);
        auto mode = AAD::SetNumResultsForAAD(true, width);
        const auto& simulation = product.Simulation();
        const AADBatchSettings_ settings{
            simulation.rsg_,     simulation.useBb_,          -1, simulation.smooth_, 1, completeModelInputs, product.ConstVarNames().size(),
            product.PayOffIdx(), simulation.normalPrecision_};
        const BlockPayoffCollector_ collector(outputs, {0, width, width}, settings, inputs, nullptr);
        AAD::RecordingScope_ recording;
        auto model = CreateModel<AAD::Number_>(modelData);
        model->Allocate(product.TimeLine(), product.DefLine());
        const auto random = CreateRNG(simulation.rsg_, *model, simulation.useBb_, std::nullopt, simulation.normalPrecision_);
        const Vector_<> gauss(model->SimDim());
        Scenario_<AAD::Number_> path;
        AllocatePath(product.DefLine(), path);
        InitializePath(path);
        const auto run = [&](auto evaluator) {
            AdmitNativeInitialization(product, model.get(), &path, &evaluator, &recording);
            AdmitKnownPathScratch(*model, &path);
            recording.FinishRecording();
        };
        if (simulation.compiled_.value_or(false))
            run(admissionProduct.BuildEvalState<AAD::Number_>());
        else
            run(admissionProduct.BuildFuzzyEvaluator<AAD::Number_>());
        recording.Close();
    }

    inline void AdmitPassiveWorker(const PreparedScript_& product, const Handle_<ModelData_>& modelData, size_t scratchQuota) {
        const ReplayAdmissionProduct_ admissionProduct(product);
        BufferCapacityBudget_ scratch(scratchQuota);
        BufferCapacityScope_ scope(&scratch, PassiveWorkerFixedPayloadBytes(typeid(*modelData) == typeid(BSModelData_)));
        auto model = CreateModel<double>(modelData);
        model->Allocate(product.TimeLine(), product.DefLine());
        model->Init(product.TimeLine(), product.DefLine());
        const auto& simulation = product.Simulation();
        DoubleSimulationState_<ReplayAdmissionProduct_> state(admissionProduct, *model, simulation.rsg_, simulation.useBb_,
                                                              simulation.normalPrecision_);
        if (!state.bsPaths_)
            AdmitKnownPathScratch(*model, &state.path_);
    }

    inline size_t ReplayCoordinatorBytes(size_t outputs, size_t inputs, size_t width, size_t batches, bool native) {
        const auto rowBytes = ReplayExtentProduct(ReplayExtentSum(inputs, 1), sizeof(double));
        const auto result = ReplayExtentSum(sizeof(AADBlockReplayResult_), ReplayExtentProduct(outputs, rowBytes));
        const auto slotBytes = native ? sizeof(AADBlockBatchResult_) : sizeof(Vector_<>);
        const auto batch = ReplayExtentSum(ReplayExtentProduct(native ? width : outputs, rowBytes), slotBytes + sizeof(TaskHandle_));
        return ReplayExtentSum(result, ReplayExtentProduct(batches, batch));
    }

    inline void AdmitReplayWidth(const PreparedScript_& product,
                                 const Handle_<ModelData_>& modelData,
                                 const Vector_<RiskOutputCoordinate_>& outputs,
                                 const Vector_<size_t>& inputs,
                                 size_t modelInputs,
                                 size_t width,
                                 size_t batches,
                                 size_t workers,
                                 const AADBlockReplaySettings_& limits) {
        const bool native = product.Simulation().enableAad_;
        const auto fixed = ReplayCoordinatorBytes(outputs.size(), inputs.size(), width, batches, native);
        RequireReplayCapacity(limits.scratchBudgetBytes_, fixed, "Scratch buffer");
        const auto scratchQuota = limits.scratchBudgetBytes_ ? (*limits.scratchBudgetBytes_ - fixed) / workers : std::numeric_limits<size_t>::max();
        const auto tapeQuota = limits.tapeBudgetBytes_ ? *limits.tapeBudgetBytes_ / workers : std::numeric_limits<size_t>::max();
        if (native)
            AdmitNativeWorker(product, modelData, outputs, inputs, modelInputs, width, scratchQuota, tapeQuota);
        else
            AdmitPassiveWorker(product, modelData, scratchQuota);
    }

    inline bool HasPreparedReplayBudget(bool native, const AADBlockReplaySettings_& limits) {
        return limits.scratchBudgetBytes_.has_value() || (native && limits.tapeBudgetBytes_.has_value());
    }

    inline size_t PreflightPreparedReplay(const PreparedScript_& product,
                                          const Handle_<ModelData_>& modelData,
                                          size_t paths,
                                          const Vector_<RiskOutputCoordinate_>& outputs,
                                          const Vector_<size_t>& inputs,
                                          size_t modelInputs,
                                          const AADBlockReplaySettings_& limits) {
        const auto threads = ThreadPool_::GetInstance()->NumThreads();
        const BatchPlan_ batches(paths, threads);
        const auto workers = std::min(threads, batches.BatchCount());
        const bool native = product.Simulation().enableAad_;
        size_t width = native ? PlanAADBlockReplay(outputs.size(), inputs.size(), ReplayExtentSum(modelInputs, product.ConstVarNames().size()),
                                                   batches, threads, product.Simulation().compiled_.value_or(false), limits)
                                    .Width()
                              : 1;
        if (!HasPreparedReplayBudget(native, limits))
            return width;
        for (;;) {
            try {
                AdmitReplayWidth(product, modelData, outputs, inputs, modelInputs, width, batches.BatchCount(), workers, limits);
                return width;
            } catch (const Exception_& error) {
                if (std::string(error.what()).find("capacity budget exceeded") == std::string::npos)
                    throw;
                if (width == 1)
                    THROW2("JacobianPreflightFailed: width=1; output=" + outputs.front().id_ + "; workers=" + String_(std::to_string(workers)) +
                               "; cause=" + String_(error.what()),
                           ScriptError_);
                --width;
            }
        }
    }
} // namespace Dal::Script::Detail
