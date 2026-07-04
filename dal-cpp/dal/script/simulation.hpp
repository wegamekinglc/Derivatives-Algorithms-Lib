//
// Created by wegam on 2022/11/6.
//

#pragma once

#include <dal/script/event.hpp>
#include <dal/model/base.hpp>
#include <dal/math/random/brownianbridge.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/utilities/dictionary.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/model/factory.hpp>
#include <dal/utilities/numerics.hpp>
#include <optional>


namespace Dal::Script {

    struct SimResults_ {
        explicit SimResults_(const Vector_<String_>& names) : aggregated_(0.0), risks_(names.size(), 0.0), names_(names) {
            for(auto i = 0; i < names.size(); ++i)
                results_[names[i]] = &risks_[i];
        }
        double aggregated_;
        Vector_<> risks_;
        Vector_<String_> names_;
        std::map<String_, const double*> results_;

        FORCE_INLINE double operator[](const String_& name) {
            return *results_[name];
        }
    };

    constexpr int BATCH_SIZE = 8192;

    template<class E_>
    void InitModel4ParallelAAD(const ScriptProduct_& prd,
                               AAD::Model_<AAD::Number_>& model,
                               Scenario_<AAD::Number_>& path,
                               E_& evaluator) {
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

    //  INTERIM (deleted when the compiled evaluator learns fuzzy smoothing):
    //  detect a conditional statement anywhere in the product's future events
    //  so the <Number_> compiled path can refuse to run instead of silently
    //  producing hard-branched (non-smoothed) greeks.
    inline bool HasConditionalStatement(const Node_& node) {
        if (dynamic_cast<const NodeIf_*>(&node) != nullptr)
            return true;
        for (const auto& arg : node.arguments_)
            if (arg && HasConditionalStatement(*arg))
                return true;
        return false;
    }

    inline bool HasConditionalStatement(const ScriptProduct_& product) {
        for (const auto& evt : product.Events())
            for (const auto& stat : evt)
                if (stat && HasConditionalStatement(*stat))
                    return true;
        return false;
    }

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
        //  Compiled and tree-walk produce the same numbers (pinned by the
        //  ScriptCompiledParity suite): `compiled` is a performance flag and
        //  the <double> path defaults to the faster compiled evaluator.
        const bool useCompiled = compiled.value_or(true);

        //  Compile once per valuation, const-correctly, on the main thread
        //  (throws if PreProcess was not run; ThreadPool_ tasks swallow
        //  exceptions). Never fall back to the tree-walk silently.
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

        Vector_<TaskHandle_> futures;
        const int batchSize = std::min(BATCH_SIZE, static_cast<int>(nPaths / nThreads) + 1);
        futures.reserve(nPaths / batchSize + 1);
        Vector_<> simResults;
        simResults.reserve(nPaths / batchSize + 1);

        int firstPath = 0;
        int pathsLeft = static_cast<int>(nPaths);
        size_t loopIndex = 0;
        auto payoffIndex = product.PayOffIdx();

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, batchSize);
            simResults.emplace_back(0.0);
            auto& simResult = simResults[loopIndex];
            loopIndex += 1;
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                Vector_<>& gaussVec = gaussVectors[threadNum];
                Scenario_<>& path = paths[threadNum];
                auto& random = rngVector[threadNum];
                random->SkipTo(firstPath);
                if (useCompiled) {
                    EvalState_<double>& evalState = evalStateVector[threadNum];
                    for (size_t i = 0; i < pathsInTask; ++i) {
                        random->FillNormal(&gaussVec);
                        mdl->GeneratePath(gaussVec, &path);
                        compiledProduct->Evaluate(path, evalState);
                        simResult += evalState.VarVals()[payoffIndex];
                    }
                } else {
                    Evaluator_<double>& eval = evalVector[threadNum];
                    for (size_t i = 0; i < pathsInTask; ++i) {
                        random->FillNormal(&gaussVec);
                        mdl->GeneratePath(gaussVec, &path);
                        product.Evaluate(path, eval);
                        simResult += eval.VarVals()[payoffIndex];
                    }
                }
                return true;
            }));
            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);

        // aggregate all the results
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
        //  <Number_> defaults to the tree-walk (FuzzyEvaluator_) until the
        //  compiled evaluator learns fuzzy smoothing.
        const bool useCompiled = compiled.value_or(false);

        std::optional<ScriptCompiled_> compiledProduct;
        if (useCompiled) {
            //  INTERIM guard, deleted when the compiled evaluator learns
            //  fuzzy smoothing: the compiled stream hard-branches conditions
            //  while the tree-walk <Number_> path smooths them with
            //  FuzzyEvaluator_; refuse rather than return non-smoothed greeks.
            REQUIRE2(!HasConditionalStatement(product),
                     "compiled <Number_> evaluation cannot smooth conditional statements yet: use compiled=false",
                     ScriptError_);
            compiledProduct.emplace(product.Compile());
        }

        AAD::Activate(*AAD::Tape());
        std::unique_ptr<AAD::Model_<AAD::Number_>> mdl = CreateModel<AAD::Number_>(modelData);
        const auto nParams = mdl->Parameters().size();
        const auto nConstVars = product.ConstVarNames().size();

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThreads = pool->NumThreads();

        Vector_<TaskHandle_> futures;
        const int batchSize = std::min(BATCH_SIZE, static_cast<int>(nPaths / nThreads) + 1);

        int firstPath = 0;
        int pathsLeft = static_cast<int>(nPaths);
        auto payoffIndex = product.PayOffIdx();

        SimResults_ values(Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));
        Vector_<SimResults_> simResults(nThreads, values);

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, batchSize);
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                // Rewind (cursor reset, block reuse) suffices: the tape is immediately
                // re-populated by the per-path recording below. Clear would free and
                // re-allocate every block per batch.
                AAD::Rewind(*AAD::Tape());
                std::unique_ptr<AAD::Model_<AAD::Number_>> model = mdl->Clone();
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

                // Accumulate const-var risks into results.risks_ from whichever evaluator was used
                auto accumulateConstVarRisks = [&](const auto& constVarVals) {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] += Adjoint(constVarVals[j]) / static_cast<double>(nPaths);
                };

                if (useCompiled) {
                    EvalState_<AAD::Number_> evalState = product.BuildEvalState<AAD::Number_>();
                    runPaths(evalState, [&](Scenario_<AAD::Number_>& p, EvalState_<AAD::Number_>& e) {
                        compiledProduct->Evaluate(p, e);
                    });
                    AAD::PropagateMarkToStart(*AAD::Tape());
                    accumulateConstVarRisks(evalState.ConstVarVals());
                } else {
                    FuzzyEvaluator_<AAD::Number_> eval = product.BuildFuzzyEvaluator<AAD::Number_>(maxNestedIfs, eps);
                    runPaths(eval, [&](Scenario_<AAD::Number_>& p, FuzzyEvaluator_<AAD::Number_>& e) {
                        product.Evaluate(p, e);
                    });
                    AAD::PropagateMarkToStart(*AAD::Tape());
                    accumulateConstVarRisks(eval.ConstVarVals());
                }

                for (size_t j = 0; j < nParams; ++j)
                    results.risks_[j] += Adjoint(*model->Parameters()[j]) / static_cast<double>(nPaths);

                results.aggregated_ += sumValue;
                return true;
            }));
            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);

        SimResults_ rtn(Dal::Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));
        for (auto& res: simResults) {
            rtn.aggregated_ += res.aggregated_;
            for (size_t j = 0; j < rtn.risks_.size(); ++j)
                rtn.risks_[j] += res.risks_[j];
        }
        return rtn;
    }
} // namespace Dal::Script
