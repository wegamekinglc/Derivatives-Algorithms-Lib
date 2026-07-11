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
#include <dal/script/event.hpp>
#include <dal/utilities/dictionary.hpp>
#include <dal/utilities/numerics.hpp>

namespace Dal::Script {

    struct SimResults_ {
        explicit SimResults_(const Vector_<String_>& names) : aggregated_(0.0), risks_(names.size(), 0.0), names_(names) {
            for (auto i = 0; i < names.size(); ++i)
                results_[names[i]] = &risks_[i];
        }
        double aggregated_;
        Vector_<> risks_;
        Vector_<String_> names_;
        std::map<String_, const double*> results_;

        [[nodiscard]] FORCE_INLINE double operator[](const String_& name) const {
            const auto it = results_.find(name);
            REQUIRE2(it != results_.end(), "simulation result '" + name + "' is not available", ScriptError_);
            return *it->second;
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

        std::exception_ptr Drain() noexcept {
            std::exception_ptr firstFailure;
            auto captureFailure = [&firstFailure]() noexcept {
                if (!firstFailure)
                    firstFailure = std::current_exception();
            };

            for (auto& future : futures_) {
                if (!future.valid())
                    continue;
                try {
                    pool_->ActiveWait(future);
                } catch (...) {
                    captureFailure();
                    try {
                        future.wait();
                    } catch (...) {
                        captureFailure();
                    }
                }
            }

            for (auto& future : futures_) {
                if (!future.valid())
                    continue;
                try {
                    static_cast<void>(future.get());
                } catch (...) {
                    captureFailure();
                }
            }
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
        }

        void Complete() {
            if (completed_)
                return;
            const std::exception_ptr firstFailure = Drain();
            if (firstFailure)
                std::rethrow_exception(firstFailure);
        }
    };

    template <class E_>
    void InitModel4ParallelAAD(const ScriptProduct_& prd, AAD::Model_<AAD::Number_>& model, Scenario_<AAD::Number_>& path, E_& evaluator) {
        AAD::Rewind(*AAD::Tape());
        for (AAD::Number_* param : model.Parameters())
            PutOnTape(*param);

        for (AAD::Number_& param : evaluator.ConstVarVals())
            PutOnTape(param);

        AAD::NewRecording(*AAD::Tape());

        model.Init(prd.TimeLine(), prd.DefLine());
        InitializePath(path);

        AAD::Mark(*AAD::Tape());
    }

    std::unique_ptr<Random_> CreateRNG(const String_& method, size_t nDim, bool useBb);

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

    template <>
    inline SimResults_ MCSimulation<double>(const ScriptProduct_& product,
                                            const Handle_<ModelData_>& modelData,
                                            size_t nPaths,
                                            const String_& rsg,
                                            bool useBb,
                                            std::optional<bool> compiled,
                                            int maxNestedIfs,
                                            double eps) {
        const bool useCompiled = compiled.value_or(false);

        std::optional<ScriptCompiled_> compiledProduct;
        if (useCompiled)
            compiledProduct.emplace(product.Compile());

        auto mdl = CreateModel<double>(modelData);

        mdl->Allocate(product.TimeLine(), product.DefLine());
        mdl->Init(product.TimeLine(), product.DefLine());

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThreads = pool->NumThreads();

        Vector_<std::unique_ptr<Random_>> rngVector(nThreads);
        for (auto& random : rngVector)
            random = CreateRNG(rsg, mdl->SimDim(), useBb);

        Vector_<Vector_<>> gaussVectors(nThreads);
        Vector_<Scenario_<>> paths(nThreads);

        for (auto& vec : gaussVectors)
            vec.Resize(mdl->SimDim());

        for (auto& path : paths) {
            AllocatePath(product.DefLine(), path);
            InitializePath(path);
        }

        Vector_ evalVector(nThreads, product.BuildEvaluator<double>());
        Vector_ evalStateVector(nThreads, product.BuildEvalState<double>());

        SimResults_ results(Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));

        const BatchPlan_ batchPlan(nPaths, nThreads);
        Vector_<> simResults;
        simResults.reserve(batchPlan.BatchCount());

        auto payoffIndex = product.PayOffIdx();
        // Keep this after every task-captured local so it drains first on unwind.
        SimulationTaskGroup_ tasks(pool, batchPlan.BatchCount());

        for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
            const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
            const size_t firstPath = batch.firstPath_;
            const size_t pathsInTask = batch.pathCount_;
            simResults.emplace_back(0.0);
            tasks.Spawn([&, batchIndex, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                Vector_<>& gaussVec = gaussVectors[threadNum];
                Scenario_<>& path = paths[threadNum];
                auto& random = rngVector[threadNum];
                random->SkipTo(firstPath);
                double sumValue = 0.0;
                if (useCompiled) {
                    EvalState_<double>& evalState = evalStateVector[threadNum];
                    for (size_t i = 0; i < pathsInTask; ++i) {
                        random->FillNormal(&gaussVec);
                        mdl->GeneratePath(gaussVec, &path);
                        compiledProduct->Evaluate(path, evalState);
                        sumValue += evalState.VarVals()[payoffIndex];
                    }
                } else {
                    Evaluator_<double>& eval = evalVector[threadNum];
                    for (size_t i = 0; i < pathsInTask; ++i) {
                        random->FillNormal(&gaussVec);
                        mdl->GeneratePath(gaussVec, &path);
                        product.Evaluate(path, eval);
                        sumValue += eval.VarVals()[payoffIndex];
                    }
                }
                simResults[batchIndex] = sumValue;
                return true;
            });
        }

        tasks.Complete();

        results.aggregated_ = Accumulate(simResults);
        return results;
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
        const bool useCompiled = compiled.value_or(false);

        std::optional<ScriptCompiled_> compiledProduct;
        if (useCompiled)
            compiledProduct.emplace(product.Compile(true));

        const std::unique_ptr<AAD::Model_<double>> metadataModel = CreateModel<double>(modelData);
        const auto nParams = metadataModel->Parameters().size();
        const auto nConstVars = product.ConstVarNames().size();

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThreads = pool->NumThreads();

        const BatchPlan_ batchPlan(nPaths, nThreads);

        auto payoffIndex = product.PayOffIdx();

        SimResults_ values(Vector::Join(metadataModel->ParameterLabels(), product.ConstVarNames()));
        Vector_<SimResults_> simResults(nThreads, values);
        // Keep this after every task-captured local so it drains first on unwind.
        SimulationTaskGroup_ tasks(pool, batchPlan.BatchCount());

        for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
            const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
            const size_t firstPath = batch.firstPath_;
            const size_t pathsInTask = batch.pathCount_;
            tasks.Spawn([&, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                AAD::Activate(*AAD::Tape());
                AAD::Rewind(*AAD::Tape());
                std::unique_ptr<AAD::Model_<AAD::Number_>> model = CreateModel<AAD::Number_>(modelData);
                model->Allocate(product.TimeLine(), product.DefLine());

                std::unique_ptr<Random_> random = CreateRNG(rsg, model->SimDim(), useBb);
                Vector_<> gVec(model->SimDim());

                Scenario_<AAD::Number_> path;
                AllocatePath(product.DefLine(), path);
                InitializePath(path);
                random->SkipTo(firstPath);

                double sumValue = 0.0;
                auto& results = simResults(threadNum);

                auto runPaths = [&](auto& evaluator, auto evaluate) {
                    InitModel4ParallelAAD(product, *model, path, evaluator);
                    for (size_t i = 0; i < pathsInTask; i++) {
                        AAD::RewindToMark(*AAD::Tape());
                        random->FillNormal(&gVec);
                        model->GeneratePath(gVec, &path);
                        evaluate(path, evaluator);
                        AAD::Number_ res = evaluator.VarVals()[payoffIndex];
                        Adjoint(res) = 1.0;
                        AAD::PropagateToMark(*AAD::Tape());
                        sumValue += Value(res);
                    }
                };

                auto accumulateConstVarRisks = [&](const auto& constVarVals) {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] += Adjoint(constVarVals[j]) / static_cast<double>(nPaths);
                };

                if (useCompiled) {
                    EvalState_<AAD::Number_> evalState = product.BuildEvalState<AAD::Number_>(static_cast<size_t>(std::max(maxNestedIfs, 0)), eps);
                    runPaths(evalState, [&](Scenario_<AAD::Number_>& p, EvalState_<AAD::Number_>& e) { compiledProduct->Evaluate(p, e); });
                    AAD::PropagateMarkToStart(*AAD::Tape());
                    accumulateConstVarRisks(evalState.ConstVarVals());
                } else {
                    FuzzyEvaluator_<AAD::Number_> eval = product.BuildFuzzyEvaluator<AAD::Number_>(maxNestedIfs, eps);
                    runPaths(eval, [&](Scenario_<AAD::Number_>& p, FuzzyEvaluator_<AAD::Number_>& e) { product.Evaluate(p, e); });
                    AAD::PropagateMarkToStart(*AAD::Tape());
                    accumulateConstVarRisks(eval.ConstVarVals());
                }

                for (size_t j = 0; j < nParams; ++j)
                    results.risks_[j] += Adjoint(*model->Parameters()[j]) / static_cast<double>(nPaths);

                results.aggregated_ += sumValue;
                return true;
            });
        }

        tasks.Complete();

        SimResults_ rtn(Dal::Vector::Join(metadataModel->ParameterLabels(), product.ConstVarNames()));
        for (auto& res : simResults) {
            rtn.aggregated_ += res.aggregated_;
            for (size_t j = 0; j < rtn.risks_.size(); ++j)
                rtn.risks_[j] += res.risks_[j];
        }
        return rtn;
    }
} // namespace Dal::Script
