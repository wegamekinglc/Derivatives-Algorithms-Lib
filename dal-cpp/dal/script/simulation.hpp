//
// Created by wegam on 2022/11/6.
//

#pragma once

#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/random/brownianbridge.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/model/base.hpp>
#include <dal/model/factory.hpp>
#include <dal/script/detail/simulationobserver.hpp>
#include <dal/script/event.hpp>
#include <dal/script/preparation.hpp>
#include <dal/utilities/dictionary.hpp>
#include <dal/utilities/numerics.hpp>

namespace Dal::Script {

    struct SimResults_ {
        explicit SimResults_(const Vector_<String_>& names) : aggregated_(0.0), risks_(names.size(), 0.0), names_(names) {
            for (size_t i = 0; i < names.size(); ++i)
                results_[names[i]] = i;
        }
        double aggregated_;
        Vector_<> risks_;
        Vector_<String_> names_;
        std::map<String_, size_t> results_;

        [[nodiscard]] FORCE_INLINE double operator[](const String_& name) const {
            const auto it = results_.find(name);
            REQUIRE2(it != results_.end(), "simulation result '" + name + "' is not available", ScriptError_);
            return risks_[it->second];
        }
    };

    constexpr size_t BATCH_SIZE = 8192;

    struct PathBatch_ {
        size_t firstPath_;
        size_t pathCount_;
    };

    class BatchPlan_ {
        size_t nPaths_;
        size_t batchSize_;

    public:
        BatchPlan_(size_t nPaths, size_t nThreads) : nPaths_(nPaths), batchSize_(0) {
            REQUIRE(nThreads > 0, "number of Monte Carlo threads must be positive");
            if (nPaths_ > 0) {
                const size_t pathsPerThread = nPaths_ / nThreads + static_cast<size_t>(nPaths_ % nThreads != 0);
                batchSize_ = std::min(BATCH_SIZE, pathsPerThread);
            }
        }

        [[nodiscard]] size_t BatchSize() const { return batchSize_; }

        [[nodiscard]] size_t BatchCount() const {
            return batchSize_ == 0 ? 0 : nPaths_ / batchSize_ + static_cast<size_t>(nPaths_ % batchSize_ != 0);
        }

        [[nodiscard]] PathBatch_ BatchAt(size_t index) const {
            REQUIRE(index < BatchCount(), "Monte Carlo batch index is out of range");
            const size_t firstPath = index * batchSize_;
            return {firstPath, std::min(batchSize_, nPaths_ - firstPath)};
        }
    };

    // Owns every accepted task future. Construction reserves all future storage,
    // and destruction drains without replacing an exception already in flight.
    class SimulationTaskGroup_ {
        ThreadPool_* pool_;
        size_t taskCount_;
        Vector_<TaskHandle_> futures_;
        bool completed_ = false;

        static void CaptureCurrentFailure(std::exception_ptr* firstFailure) noexcept {
            if (!*firstFailure)
                *firstFailure = std::current_exception();
        }

        void WaitUntilReady(TaskHandle_& future, std::exception_ptr* firstFailure) noexcept {
            if (!future.valid())
                return;
            try {
                pool_->ActiveWait(future);
            } catch (...) {
                CaptureCurrentFailure(firstFailure);
                try {
                    future.wait();
                } catch (...) {
                    CaptureCurrentFailure(firstFailure);
                }
            }
        }

        static void ConsumeResult(TaskHandle_& future, std::exception_ptr* firstFailure) noexcept {
            if (!future.valid())
                return;
            try {
                static_cast<void>(future.get());
            } catch (...) {
                CaptureCurrentFailure(firstFailure);
            }
        }

        std::exception_ptr Drain() noexcept {
            std::exception_ptr firstFailure;
            for (auto& future : futures_)
                WaitUntilReady(future, &firstFailure);
            for (auto& future : futures_)
                ConsumeResult(future, &firstFailure);
            completed_ = true;
            return firstFailure;
        }

    public:
        SimulationTaskGroup_(ThreadPool_* pool, size_t taskCount) : pool_(pool), taskCount_(taskCount) {
            static_assert(std::is_nothrow_move_constructible_v<TaskHandle_>, "task futures must move without throwing after submission");
            REQUIRE(pool_ != nullptr, "simulation task group requires a thread pool");
            futures_.reserve(taskCount_);
        }

        ~SimulationTaskGroup_() {
            if (!completed_)
                static_cast<void>(Drain());
        }

        SimulationTaskGroup_(const SimulationTaskGroup_&) = delete;
        SimulationTaskGroup_& operator=(const SimulationTaskGroup_&) = delete;
        SimulationTaskGroup_(SimulationTaskGroup_&&) = delete;
        SimulationTaskGroup_& operator=(SimulationTaskGroup_&&) = delete;

        template <class C_> void Spawn(C_&& task) {
            REQUIRE(!completed_, "cannot submit to a completed simulation task group");
            REQUIRE(futures_.size() < taskCount_, "simulation task group submission count exceeds its reservation");
            TaskHandle_ future = pool_->SpawnTask(std::forward<C_>(task));
            futures_.push_back(std::move(future));
            if (auto* observer = Detail::SimulationObserver())
                observer->AfterSubmission();
        }

        void Complete() {
            if (completed_)
                return;
            const std::exception_ptr firstFailure = Drain();
            if (firstFailure)
                std::rethrow_exception(firstFailure);
        }
    };

    template <class P_, class E_>
    void
    InitModel4ParallelAAD(const P_& prd, AAD::Model_<AAD::Number_>& model, Scenario_<AAD::Number_>& path, E_& evaluator, AAD::Number_* payoffZero) {
        AAD::Rewind(*AAD::Tape());
        for (AAD::Number_* param : model.Parameters())
            PutOnTape(*param);

        for (AAD::Number_& param : evaluator.ConstVarVals())
            PutOnTape(param);

        PutOnTape(*payoffZero);

        AAD::NewRecording(*AAD::Tape());

        model.Init(prd.TimeLine(), prd.DefLine());
        InitializePath(path);

        if constexpr (std::is_base_of_v<PreparedScript_, P_>)
            prd.InitializeHistoricalState(&evaluator);

        AAD::Mark(*AAD::Tape());
    }

    std::unique_ptr<Random_> CreateRNG(const String_& method, size_t nDim, bool useBb);

    namespace Detail {
        template <class T_> void DiagnoseInvalidSimulationPath(const Scenario_<T_>& path) {
            for (const auto& sample : path) {
                REQUIRE2(std::isfinite(Value(sample.spot_)), "InvalidModelPath: non-finite spot", ScriptError_);
                REQUIRE2(std::isfinite(Value(sample.numeraire_)) && Value(sample.numeraire_) > 0.0,
                         "InvalidModelPath: non-finite or nonpositive numeraire", ScriptError_);
                for (const auto& observation : sample.observations_)
                    REQUIRE2(std::isfinite(Value(observation)), "InvalidModelPath: non-finite observation", ScriptError_);
            }
        }
    } // namespace Detail

    template <class T_> FORCE_INLINE void ValidateSimulationPath(const Scenario_<T_>& path) {
        // Only invalid paths enter the diagnostic function and its exception setup.
        if (!AAD::IsValidModelPath(path))
            Detail::DiagnoseInvalidSimulationPath(path);
    }

    namespace Detail {
        template <class T_, class F_> auto WithPathGenerator(const AAD::Model_<T_>& model, const F_& work) {
            if (typeid(model) == typeid(AAD::BlackScholes_<T_>)) {
                const auto& bs = static_cast<const AAD::BlackScholes_<T_>&>(model);
                return work([&](const Vector_<>& gauss, Scenario_<T_>* path) {
                    if (!bs.GeneratePathAndValidate(gauss, path))
                        DiagnoseInvalidSimulationPath(*path);
                });
            }
            return work([&](const Vector_<>& gauss, Scenario_<T_>* path) {
                model.GeneratePath(gauss, path);
                ValidateSimulationPath(*path);
            });
        }

        template <class S_, class E_, class F_>
        double EvaluateDoubleBatch(
            const AAD::Model_<double>& model, S_* state, E_* evaluator, const PathBatch_& batch, size_t payoffIndex, const F_& evaluate) {
            if (state->random_)
                state->random_->SkipTo(batch.firstPath_);
            auto run = [&](const auto& generate) {
                double sumValue = 0.0;
                for (size_t i = 0; i < batch.pathCount_; ++i) {
                    if (state->random_)
                        state->random_->FillNormal(&state->gauss_);
                    evaluate(generate(state->gauss_), *evaluator);
                    REQUIRE2(std::isfinite(evaluator->VarVals()[payoffIndex]), "InvalidPayoff: non-finite path value", ScriptError_);
                    sumValue += evaluator->VarVals()[payoffIndex];
                }
                return sumValue;
            };
            if (state->bsPaths_) {
                auto& paths = *state->bsPaths_;
                return run([&](const Vector_<>& gauss) -> const Scenario_<>& {
                    if (!paths.Generate(gauss))
                        DiagnoseInvalidSimulationPath(paths.Path());
                    return paths.Path();
                });
            }
            return run([&](const Vector_<>& gauss) -> const Scenario_<>& {
                model.GeneratePath(gauss, &state->path_);
                ValidateSimulationPath(state->path_);
                return state->path_;
            });
        }
    } // namespace Detail

    template <class T_>
    SimResults_ MCSimulation(const ScriptProduct_& product,
                             const Handle_<ModelData_>& modelData,
                             size_t nPaths,
                             const String_& rsg = "sobol",
                             bool useBb = false,
                             std::optional<bool> compiled = std::nullopt,
                             int maxNestedIfs = -1,
                             double eps = 0.01) {
        THROW("not implemented");
    }

    template <class P_>
    SimResults_ MCDoubleSimulation(const P_& product,
                                   AAD::Model_<double>* mdl,
                                   size_t nPaths,
                                   const String_& rsg,
                                   bool useBb,
                                   std::optional<bool> compiled,
                                   bool initialized = false) {
        product.RequireExecutable();
        ValidateRNG(rsg);
        const bool useCompiled = compiled.value_or(false);

        if (product.EventDates().empty())
            return SimResults_(Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));

        if constexpr (std::is_base_of_v<PreparedScript_, P_>)
            REQUIRE2(!product.Simulation().enableAad_ && useCompiled == product.Simulation().compiled_.value_or(false),
                     "UnsupportedExecutionMode: execution differs from preparation", ScriptError_);

        std::optional<ScriptCompiled_> compiledProduct;
        if (useCompiled)
            compiledProduct.emplace(product.Compile());

        if (!initialized) {
            mdl->Allocate(product.TimeLine(), product.DefLine());
            mdl->Init(product.TimeLine(), product.DefLine());
        }

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThreads = pool->NumThreads();

        // Each worker constructs and reuses its own writable buffers. Keeping hot
        // evaluator state in adjacent arrays made timing sensitive to allocation layout.
        // Isolate snapshot metadata from another worker's adjacent writable sample.
        struct alignas(64) LocalCheckedPaths_ : AAD::BlackScholes_<double>::CheckedPaths_ {
            using AAD::BlackScholes_<double>::CheckedPaths_::CheckedPaths_;
        };
        struct ThreadState_ {
            std::unique_ptr<Random_> random_;
            Vector_<> gauss_;
            Scenario_<> path_;
            std::unique_ptr<LocalCheckedPaths_> bsPaths_;
            Evaluator_<double> evaluator_;
            EvalState_<double> compiledState_;

            ThreadState_(const P_& product, const AAD::Model_<double>& model, const String_& rsg, bool useBb)
                : random_(CreateRNG(rsg, model.SimDim(), useBb)), gauss_(model.SimDim()), evaluator_(product.template BuildEvaluator<double>()),
                  compiledState_(product.template BuildEvalState<double>()) {
                if (typeid(model) == typeid(AAD::BlackScholes_<double>))
                    bsPaths_ = std::make_unique<LocalCheckedPaths_>(static_cast<const AAD::BlackScholes_<double>&>(model));
                else {
                    AllocatePath(product.DefLine(), path_);
                    InitializePath(path_);
                }
            }
        };
        Vector_<std::unique_ptr<ThreadState_>> threadStates(nThreads);
        // Preserve caller-side input validation, including the zero-path case.
        threadStates[0] = std::make_unique<ThreadState_>(product, *mdl, rsg, useBb);

        SimResults_ results(Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));

        const BatchPlan_ batchPlan(nPaths, nThreads);
        Vector_<> simResults;
        simResults.reserve(batchPlan.BatchCount());

        auto payoffIndex = product.PayOffIdx();
        // Keep this after every task-captured local so it drains first on unwind.
        SimulationTaskGroup_ tasks(pool, batchPlan.BatchCount());

        for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
            const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
            simResults.emplace_back(0.0);
            tasks.Spawn([&, batchIndex, batch]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                auto& state = threadStates[threadNum];
                if (!state)
                    state = std::make_unique<ThreadState_>(product, *mdl, rsg, useBb);
                auto runPaths = [&](auto& evaluator, const auto& evaluate) {
                    return Detail::EvaluateDoubleBatch(*mdl, state.get(), &evaluator, batch, payoffIndex, evaluate);
                };
                if (useCompiled)
                    simResults[batchIndex] = runPaths(state->compiledState_, [&](const auto& p, auto& e) { compiledProduct->Evaluate(p, e); });
                else
                    simResults[batchIndex] = runPaths(state->evaluator_, [&](const auto& p, auto& e) { product.Evaluate(p, e); });
                return true;
            });
        }

        tasks.Complete();

        results.aggregated_ = Accumulate(simResults);
        return results;
    }

    template <>
    inline SimResults_ MCSimulation<double>(const ScriptProduct_& product,
                                            const Handle_<ModelData_>& modelData,
                                            size_t nPaths,
                                            const String_& rsg,
                                            bool useBb,
                                            std::optional<bool> compiled,
                                            int maxNestedIfs,
                                            double eps) {
        product.RequireExecutable();
        auto model = CreateModel<double>(modelData);
        return MCDoubleSimulation(product, model.get(), nPaths, rsg, useBb, compiled);
    }

    namespace Detail {
        struct AADBatchSettings_ {
            const String_& rsg_;
            bool useBb_;
            int maxNestedIfs_;
            double eps_;
            size_t nPaths_;
            size_t nParams_;
            size_t nConstVars_;
            size_t payoffIndex_;
        };

        template <class P_>
        void EvaluateAADBatch(const P_& product,
                              const Handle_<ModelData_>& modelData,
                              const AADBatchSettings_& settings,
                              const std::optional<ScriptCompiled_>& compiledProduct,
                              const PathBatch_& batch,
                              SimResults_* results) {
            AAD::Activate(*AAD::Tape());
            AAD::Rewind(*AAD::Tape());
            std::unique_ptr<AAD::Model_<AAD::Number_>> model = CreateModel<AAD::Number_>(modelData);
            model->Allocate(product.TimeLine(), product.DefLine());

            std::unique_ptr<Random_> random = CreateRNG(settings.rsg_, model->SimDim(), settings.useBb_);
            Vector_<> gVec(model->SimDim());

            Scenario_<AAD::Number_> path;
            AllocatePath(product.DefLine(), path);
            InitializePath(path);
            if (random)
                random->SkipTo(batch.firstPath_);

            double sumValue = 0.0;

            auto runPaths = [&](auto& evaluator, auto evaluate) {
                AAD::Number_ payoffZero = 0.0;
                InitModel4ParallelAAD(product, *model, path, evaluator, &payoffZero);
                WithPathGenerator(*model, [&](const auto& generate) {
                    for (size_t i = 0; i < batch.pathCount_; i++) {
                        AAD::RewindToMark(*AAD::Tape());
                        if (random)
                            random->FillNormal(&gVec);
                        generate(gVec, &path);
                        evaluate(path, evaluator);
                        AAD::Number_ res = AAD::PayoffRoot(evaluator.VarVals()[settings.payoffIndex_], payoffZero);
                        REQUIRE2(std::isfinite(Value(res)), "InvalidPayoff: non-finite path value", ScriptError_);
                        Adjoint(res) = 1.0;
                        AAD::PropagateToMark(*AAD::Tape());
                        sumValue += Value(res);
                    }
                });
            };

            auto accumulateConstVarRisks = [&](const auto& constVarVals) {
                for (size_t j = 0; j < settings.nConstVars_; ++j)
                    results->risks_[j + settings.nParams_] += Adjoint(constVarVals[j]) / static_cast<double>(settings.nPaths_);
            };

            if (compiledProduct) {
                EvalState_<AAD::Number_> evalState =
                    product.template BuildEvalState<AAD::Number_>(static_cast<size_t>(std::max(settings.maxNestedIfs_, 0)), settings.eps_);
                runPaths(evalState, [&](Scenario_<AAD::Number_>& p, EvalState_<AAD::Number_>& e) { compiledProduct->Evaluate(p, e); });
                AAD::PropagateMarkToStart(*AAD::Tape());
                accumulateConstVarRisks(evalState.ConstVarVals());
            } else {
                FuzzyEvaluator_<AAD::Number_> eval = product.template BuildFuzzyEvaluator<AAD::Number_>(settings.maxNestedIfs_, settings.eps_);
                runPaths(eval, [&](Scenario_<AAD::Number_>& p, FuzzyEvaluator_<AAD::Number_>& e) { product.Evaluate(p, e); });
                AAD::PropagateMarkToStart(*AAD::Tape());
                accumulateConstVarRisks(eval.ConstVarVals());
            }

            for (size_t j = 0; j < settings.nParams_; ++j)
                results->risks_[j] += Adjoint(*model->Parameters()[j]) / static_cast<double>(settings.nPaths_);

            results->aggregated_ += sumValue;
        }

        inline SimResults_ AggregateAADResults(const Vector_<String_>& names, const Vector_<SimResults_>& simResults) {
            SimResults_ rtn(names);
            for (const auto& res : simResults) {
                rtn.aggregated_ += res.aggregated_;
                for (size_t j = 0; j < rtn.risks_.size(); ++j)
                    rtn.risks_[j] += res.risks_[j];
            }
            return rtn;
        }
    } // namespace Detail

    template <class P_>
    SimResults_ MCAADSimulation(const P_& product,
                                const Handle_<ModelData_>& modelData,
                                size_t nPaths,
                                const String_& rsg,
                                bool useBb,
                                std::optional<bool> compiled,
                                int maxNestedIfs,
                                double eps) {
        product.RequireExecutable();
        ValidateRNG(rsg);
        if constexpr (!std::is_base_of_v<PreparedScript_, P_>)
            REQUIRE2(product.PastEvents().empty() || product.EventDates().empty(),
                     "UnsupportedExecutionMode: historical AAD replay requires preparation", ScriptError_);
        const bool useCompiled = compiled.value_or(false);

        const std::unique_ptr<AAD::Model_<double>> metadataModel = CreateModel<double>(modelData);
        if (product.EventDates().empty())
            return SimResults_(Vector::Join(metadataModel->ParameterLabels(), product.ConstVarNames()));
        if constexpr (std::is_base_of_v<PreparedScript_, P_>)
            REQUIRE2(product.Simulation().enableAad_ && eps == product.Simulation().smooth_ &&
                         useCompiled == product.Simulation().compiled_.value_or(false),
                     "UnsupportedExecutionMode: AAD mode or smoothing differs from preparation", ScriptError_);

        std::optional<ScriptCompiled_> compiledProduct;
        if (useCompiled)
            compiledProduct.emplace(product.Compile(true));

        metadataModel->Allocate(product.TimeLine(), product.DefLine());
        metadataModel->Init(product.TimeLine(), product.DefLine());
        const auto nParams = metadataModel->Parameters().size();
        const auto nConstVars = product.ConstVarNames().size();

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThreads = pool->NumThreads();

        const BatchPlan_ batchPlan(nPaths, nThreads);

        auto payoffIndex = product.PayOffIdx();

        SimResults_ values(Vector::Join(metadataModel->ParameterLabels(), product.ConstVarNames()));
        Vector_<SimResults_> simResults(nThreads, values);
        const Detail::AADBatchSettings_ settings{rsg, useBb, maxNestedIfs, eps, nPaths, nParams, nConstVars, payoffIndex};
        // Keep this after every task-captured local so it drains first on unwind.
        SimulationTaskGroup_ tasks(pool, batchPlan.BatchCount());

        for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
            const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
            tasks.Spawn([&, batch]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                Detail::EvaluateAADBatch(product, modelData, settings, compiledProduct, batch, &simResults(threadNum));
                return true;
            });
        }

        tasks.Complete();

        return Detail::AggregateAADResults(Vector::Join(metadataModel->ParameterLabels(), product.ConstVarNames()), simResults);
    }

    template <>
    inline SimResults_ MCSimulation<AAD::Number_>(const ScriptProduct_& product,
                                                  const Handle_<ModelData_>& modelData,
                                                  size_t nPaths,
                                                  const String_& rsg,
                                                  bool useBb,
                                                  std::optional<bool> compiled,
                                                  int maxNestedIfs,
                                                  double eps) {
        return MCAADSimulation(product, modelData, nPaths, rsg, useBb, compiled, maxNestedIfs, eps);
    }
    template <class T_>
    SimResults_ MCSimulation(const PreparedScript_& prepared,
                             const Handle_<ModelData_>& modelData,
                             size_t nPaths,
                             const String_& rsg = "sobol",
                             bool useBb = false,
                             std::optional<bool> compiled = std::nullopt,
                             int maxNestedIfs = -1,
                             double eps = 0.01) {
        prepared.RequireExecutable();
        REQUIRE2(nPaths > 0, "InvalidPathCount: number of paths must be positive", ScriptError_);
        ValidateRNG(rsg);
        REQUIRE2((prepared.AllExpired() || prepared.Simulation().enableAad_ == !std::is_same_v<T_, double>),
                 "UnsupportedExecutionMode: evaluation mode differs from preparation", ScriptError_);
        if constexpr (!std::is_same_v<T_, double>)
            return MCAADSimulation(prepared, modelData, nPaths, rsg, useBb, compiled, maxNestedIfs, eps);
        auto model = CreateModel<double>(modelData);
        if (prepared.AllExpired())
            return SimResults_(Vector::Join(model->ParameterLabels(), prepared.Product().ConstVarNames()));
        return MCDoubleSimulation(prepared, model.get(), nPaths, rsg, useBb, compiled);
    }

    template <class T_>
    SimResults_ MCSimulation(const ScriptProductData_& data,
                             const Handle_<ModelData_>& modelData,
                             size_t nPaths,
                             const ScriptValuationSettings_& settings,
                             const MonteCarloSettings_& simulation = {},
                             const Handle_<MarketFixingSnapshot_>& snapshot = {},
                             const ScriptProductSettings_& contract = {}) {
        auto execution = simulation;
        REQUIRE2(nPaths > 0, "InvalidPathCount: number of paths must be positive", ScriptError_);
        ValidateRNG(execution.rsg_);
        execution.enableAad_ = !std::is_same_v<T_, double> || execution.enableAad_;
        REQUIRE2((!std::is_same_v<T_, double> || !execution.enableAad_), "UnsupportedExecutionMode: double simulation requested AAD", ScriptError_);
        auto model = CreateModel<double>(modelData);
        const auto prepared = PrepareScript(data, model.get(), settings, execution, snapshot, contract);
        if constexpr (!std::is_same_v<T_, double>)
            return MCAADSimulation(prepared, modelData, nPaths, execution.rsg_, execution.useBb_, execution.compiled_, -1, execution.smooth_);
        return MCDoubleSimulation(prepared, model.get(), nPaths, execution.rsg_, execution.useBb_, execution.compiled_, true);
    }
} // namespace Dal::Script
