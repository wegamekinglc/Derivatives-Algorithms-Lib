//
// Created by Codex on 2026/10/7.
//

#include <dal/platform/platform.hpp>
#include <dal/script/portfoliobatch.hpp>
#include <dal/script/portfoliopassive.hpp>
#include <dal/script/replayadmission.hpp>

namespace Dal::Script::Detail {
    namespace {
        struct AdmissionSelection_ {
            Vector_<double> weights_;
            Vector_<String_> contexts_;
            Vector_<size_t> evaluators_;
            Vector_<size_t> positions_;
        };

        template <class G_> struct AdmissionRoots_ {
            explicit AdmissionRoots_(size_t) {}
            AAD::Number_* Zero() { return nullptr; }
        };

        template <> struct AdmissionRoots_<Matrix_<double>> {
            size_t width_;
            AAD::Number_ zero_;
            Vector_<AAD::Number_> roots_;
            explicit AdmissionRoots_(size_t width) : width_(width) { roots_.reserve(width); }
            AAD::Number_* Zero() { return &zero_; }
        };

        AdmissionSelection_ AdmitSelection(const Vector_<PortfolioBatchOutput_>& outputs, size_t trades) {
            AdmissionSelection_ selection;
            for (const auto& output : outputs) {
                selection.weights_.push_back(output.weight_);
                selection.contexts_.push_back(output.coordinate_.id_);
                selection.evaluators_.push_back(output.tradePosition_);
            }
            for (size_t trade = 0; trade < trades; ++trade)
                selection.positions_.push_back(trade);
            return selection;
        }

        template <class F_> auto BuildAdmissionStates(const Vector_<const PreparedScript_*>& trades, const F_& build) {
            using E_ = decltype(build(ReplayAdmissionProduct_(*trades.front())));
            Vector_<std::unique_ptr<E_>> states;
            states.reserve(trades.size());
            for (const auto* trade : trades)
                states.push_back(std::unique_ptr<E_>(new E_(build(ReplayAdmissionProduct_(*trade)))));
            return states;
        }

        template <class E_> void AdmitPrivateHistory(const PreparedScript_& trade, E_* evaluator) {
            const auto retain = [&](const auto& past) {
                evaluator->SetHistoricalSeed(past.VarVals());
                evaluator->SetHistoricalVectorSeed(AdmissionVectorSeed(trade, past.VectorVals()));
            };
            if (trade.Simulation().compiled_.value_or(false)) {
                const EvalState_<AAD::Number_> past(Vector_<>(trade.Product().VarNames().size(), 0.0), evaluator->ConstVarVals(), 0, 0.0,
                                                    trade.Product().VectorCapacities());
                retain(past);
            } else {
                const PastEvaluator_<AAD::Number_> past(Vector_<>(trade.Product().VarNames().size(), 0.0), evaluator->ConstVarVals(),
                                                        trade.Product().VectorCapacities());
                retain(past);
            }
        }

        template <class F_>
        void AdmitStates(const Vector_<const PreparedScript_*>& trades,
                         AAD::Model_<AAD::Number_>* model,
                         Scenario_<AAD::Number_>* path,
                         AAD::RecordingScope_* recording,
                         const F_& build,
                         AAD::Number_* zero) {
            auto states = BuildAdmissionStates(trades, build);
            AAD::Rewind(*AAD::Tape());
            for (auto* parameter : model->Parameters())
                recording->RegisterInput(*parameter, Value(*parameter));
            for (auto& state : states)
                for (auto& constant : state->ConstVarVals())
                    recording->RegisterInput(constant, Value(constant));
            if (zero)
                recording->RegisterInput(*zero, 0.0);
            recording->StartRecording();
            model->Init(trades.front()->TimeLine(), trades.front()->DefLine());
            InitializePath(*path);
            for (size_t trade = 0; trade < trades.size(); ++trade)
                AdmitPrivateHistory(*trades[trade], states[trade].get());
            AdmitKnownPathScratch(*model, path);
            recording->FinishRecording();
        }
        template <class G_>
        void AdmitNativePortfolioWorker(const Vector_<const PreparedScript_*>& trades,
                                        const Handle_<ModelData_>& modelData,
                                        const Vector_<PortfolioBatchOutput_>& requestedOutputs,
                                        const PortfolioAdmissionSettings_& settings) {
            constexpr bool BLOCKED = std::is_same_v<G_, Matrix_<double>>;
            REQUIRE2(!trades.empty(), "InvalidPortfolioAdmission: selected trades must not be empty", ScriptError_);
            const bool compiled = trades.front()->Simulation().compiled_.value_or(false);
            const auto width = settings.width_;
            BufferCapacityBudget_ scratch(settings.scratchQuota_);
            const auto fixed =
                BLOCKED ? PortfolioJacobianWorkerFixedBytes(compiled, trades.size()) : PortfolioWeightedWorkerFixedBytes(compiled, trades.size());
            BufferCapacityScope_ buffers(&scratch, fixed);
            const auto outputs = requestedOutputs;
            const auto selection = AdmitSelection(outputs, trades.size());
            AAD::TapeCapacityBudget_ tape(settings.tapeQuota_);
            AAD::TapeCapacityScope_ tapeScope(&tape, true);
            auto mode = AAD::SetNumResultsForAAD(BLOCKED, width);
            AdmissionRoots_<G_> roots(width);
            AAD::RecordingScope_ recording;
            auto model = CreateModel<AAD::Number_>(modelData);
            model->Allocate(trades.front()->TimeLine(), trades.front()->DefLine());
            const auto random = CreateRNG(trades.front()->Simulation().rsg_, *model, trades.front()->Simulation().useBb_, std::nullopt,
                                          trades.front()->Simulation().normalPrecision_);
            const Vector_<double> gauss(model->SimDim());
            Scenario_<AAD::Number_> path;
            AllocatePath(trades.front()->DefLine(), path);
            const Vector_<AAD::Number_> values(outputs.size());
            PortfolioBatchResult_<G_> result(outputs.size());
            auto modelInputs = model->Parameters().size();
            if (settings.gradients_) {
                const auto& columns = settings.gradients_->Model(settings.modelOwner_);
                columns.ValidateExtent(modelInputs);
                modelInputs = columns.Size();
                REQUIRE2(settings.tradePositions_ && settings.tradePositions_->size() == trades.size(),
                         "InvalidPortfolioAdmission: original trade positions must match live trades", ScriptError_);
            }
            ResizePortfolioGradientStorage(&result.modelGradientSums_, width, modelInputs);
            result.tradePositions_.Resize(trades.size());
            result.constantGradientSums_.Resize(trades.size());
            for (size_t trade = 0; trade < trades.size(); ++trade) {
                auto inputs = trades[trade]->ConstVarNames().size();
                if (settings.gradients_) {
                    const auto& columns = settings.gradients_->Constants((*settings.tradePositions_)[trade]);
                    columns.ValidateExtent(inputs);
                    inputs = columns.Size();
                }
                ResizePortfolioGradientStorage(&result.constantGradientSums_[trade], width, inputs);
            }
            if (compiled)
                AdmitStates(
                    trades, model.get(), &path, &recording, [](const auto& product) { return product.template BuildEvalState<AAD::Number_>(); },
                    roots.Zero());
            else
                AdmitStates(
                    trades, model.get(), &path, &recording, [](const auto& product) { return product.template BuildFuzzyEvaluator<AAD::Number_>(); },
                    roots.Zero());
            recording.Close();
        }
    } // namespace

    void AdmitPortfolioWeightedWorker(const Vector_<const PreparedScript_*>& trades,
                                      const Handle_<ModelData_>& model,
                                      const Vector_<PortfolioBatchOutput_>& outputs,
                                      const PortfolioAdmissionSettings_& settings) {
        AdmitNativePortfolioWorker<Vector_<double>>(trades, model, outputs, settings);
    }

    void AdmitPortfolioJacobianWorker(const Vector_<const PreparedScript_*>& trades,
                                      const Handle_<ModelData_>& model,
                                      const Vector_<PortfolioBatchOutput_>& outputs,
                                      const PortfolioAdmissionSettings_& settings) {
        AdmitNativePortfolioWorker<Matrix_<double>>(trades, model, outputs, settings);
    }

    void AdmitPortfolioPassiveWorker(const Vector_<const PreparedScript_*>& trades,
                                     const Handle_<ModelData_>& modelData,
                                     const Vector_<PortfolioBatchOutput_>& requestedOutputs,
                                     size_t scratchQuota) {
        REQUIRE2(!trades.empty(), "InvalidPortfolioAdmission: selected trades must not be empty", ScriptError_);
        const bool compiled = trades.front()->Simulation().compiled_.value_or(false);
        BufferCapacityBudget_ scratch(scratchQuota);
        BufferCapacityScope_ buffers(&scratch,
                                     PortfolioWeightedWorkerFixedBytes(compiled, trades.size(), false, typeid(*modelData) == typeid(BSModelData_)));
        const auto outputs = requestedOutputs;
        const auto selection = AdmitSelection(outputs, trades.size());
        auto model = CreateModel<double>(modelData);
        model->Allocate(trades.front()->TimeLine(), trades.front()->DefLine());
        model->Init(trades.front()->TimeLine(), trades.front()->DefLine());
        PortfolioPassivePathState_ pathState(*trades.front(), *model);
        if (!pathState.bsPaths_)
            AdmitKnownPathScratch(*model, &pathState.path_);
        PortfolioWeightedBatchResult_ result(outputs.size());
        result.tradePositions_.Resize(trades.size());
        if (compiled)
            static_cast<void>(BuildAdmissionStates(trades, [](const auto& product) { return product.template BuildEvalState<double>(); }));
        else
            static_cast<void>(BuildAdmissionStates(trades, [](const auto& product) { return product.template BuildEvaluator<double>(); }));
    }
} // namespace Dal::Script::Detail
