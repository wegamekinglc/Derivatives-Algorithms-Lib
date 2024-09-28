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

    constexpr int BATCH_SIZE = 1024;

    template<class E_>
    void InitModel4ParallelAAD(const ScriptProduct_& prd,
                               AAD::Model_<AAD::Number_>& model,
                               Scenario_<AAD::Number_>& path,
                               E_& evaluator) {
        Number_::Tape()->Rewind();
        for (Number_* param : model.Parameters())
            param->PutOnTape();

        for (Number_& param : evaluator.ConstVarVals())
            param.PutOnTape();

        model.Init(prd.TimeLine(), prd.DefLine());
        InitializePath(path);
        Number_::Tape()->Mark();
    }

    std::unique_ptr<Random_> CreateRNG(const String_& method, size_t n_dim, bool use_bb);

    template <class T_>
    SimResults_ MCSimulation(const ScriptProduct_& product,
                             const Handle_<ModelData_>& model_data,
                             size_t n_paths,
                             const String_& rsg = "sobol",
                             bool use_bb = false,
                             bool compiled = false,
                             int max_nested_ifs = -1,
                             double eps = 0.01) {
        THROW("not implemented");
    }

    template <>
    inline SimResults_ MCSimulation<double>(const ScriptProduct_& product,
                             const Handle_<ModelData_>& model_data,
                             size_t n_paths,
                             const String_& rsg,
                             bool use_bb,
                             bool compiled,
                             int max_nested_ifs,
                             double eps) {
        std::unique_ptr<AAD::Model_<double>> mdl = CreateModel<double>(model_data);

        mdl->Allocate(product.TimeLine(), product.DefLine());
        mdl->Init(product.TimeLine(), product.DefLine());

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThreads = pool->NumThreads();

        Vector_<std::unique_ptr<Random_>> rngVector(nThreads);
        for (auto& random : rngVector)
            random = CreateRNG(rsg, mdl->SimDim(), use_bb);

        Vector_<Vector_<>> gaussVectors(nThreads);
        Vector_<Scenario_<>> paths(nThreads);

        for (auto& vec : gaussVectors)
            vec.Resize(mdl->SimDim());

        for (auto& path : paths) {
            AllocatePath(product.DefLine(), path);
            InitializePath(path);
        }

        Vector_<Evaluator_<double>> evalVector(nThreads, product.BuildEvaluator<double>());
        Vector_<EvalState_<double>> evalStateVector(nThreads, product.BuildEvalState<double>());

        SimResults_ results(Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));

        Vector_<TaskHandle_> futures;
        const int batch_size = std::max(BATCH_SIZE, static_cast<int>(n_paths / nThreads) + 1);
        futures.reserve(n_paths / batch_size + 1);
        Vector_<> simResults;
        simResults.reserve(n_paths / batch_size + 1);

        int firstPath = 0;
        int pathsLeft = static_cast<int>(n_paths);
        size_t loopIndex = 0;
        auto payoffIndex = product.PayOffIdx();

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, batch_size);
            simResults.emplace_back(0.0);
            auto& simResult = simResults[loopIndex];
            loopIndex += 1;
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                Vector_<>& gaussVec = gaussVectors[threadNum];
                Scenario_<>& path = paths[threadNum];
                auto& random = rngVector[threadNum];
                random->SkipTo(firstPath);
                if (compiled) {
                    EvalState_<double>& evalState = evalStateVector[threadNum];
                    for (size_t i = 0; i < pathsInTask; ++i) {
                        random->FillNormal(&gaussVec);
                        mdl->GeneratePath(gaussVec, &path);
                        product.EvaluateCompiled(path, evalState);
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
                             const Handle_<ModelData_>& model_data,
                             size_t n_paths,
                             const String_& rsg,
                             bool use_bb,
                             bool compiled,
                             int max_nested_ifs,
                             double eps) {
        std::unique_ptr<AAD::Model_<Number_>> mdl = CreateModel<Number_>(model_data);
        const auto nParams = mdl->Parameters().size();
        const auto nConstVars = product.ConstVarNames().size();

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThreads = pool->NumThreads();

        Vector_<TaskHandle_> futures;
        const int batchSize = std::max(BATCH_SIZE, static_cast<int>(n_paths / nThreads) + 1);

        int firstPath = 0;
        int pathsLeft = static_cast<int>(n_paths);
        auto payoffIndex = product.PayOffIdx();
        Vector_<AAD::Tape_> tapes(nThreads);
        AAD::Tape_* mainThreadPtr = Number_::Tape();

        SimResults_ values(Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));
        Vector_<SimResults_> simResults(nThreads, values);

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, batchSize);
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                Number_::SetTape(tapes[threadNum]);
                Number_::Tape()->Rewind();
                std::unique_ptr<AAD::Model_<AAD::Number_>> model = mdl->Clone();
                model->Allocate(product.TimeLine(), product.DefLine());

                std::unique_ptr<Random_> random = CreateRNG(rsg, model->SimDim(), use_bb);
                Vector_<> gVec(model->SimDim());

                Scenario_<AAD::Number_> path;
                AllocatePath(product.DefLine(), path);
                InitializePath(path);
                random->SkipTo(firstPath);

                double sumValue = 0.0;
                auto& results = simResults(threadNum);
                if (compiled) {
                    EvalState_<AAD::Number_> evalState = product.BuildEvalState<AAD::Number_>();
                    InitModel4ParallelAAD(product, *model, path, evalState);

                    for (size_t i = 0; i < pathsInTask; i++) {
                        Number_::Tape()->RewindToMark();
                        random->FillNormal(&gVec);
                        model->GeneratePath(gVec, &path);
                        product.EvaluateCompiled(path, evalState);
                        Number_ res = evalState.VarVals()[payoffIndex];
                        res.PropagateToMark();
                        sumValue += res.value();
                    }

                    Number_::PropagateMarkToStart();
                    for (size_t j = 0; j < nParams; ++j)
                        results.risks_[j] += model->Parameters()[j]->Adjoint() / static_cast<double>(n_paths);

                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  evalState.ConstVarVals()[j].Adjoint() / static_cast<double>(n_paths);
                }
                else {
                    FuzzyEvaluator_<AAD::Number_> eval = product.BuildFuzzyEvaluator<AAD::Number_>(max_nested_ifs, eps);
                    InitModel4ParallelAAD(product, *model, path, eval);

                    for (size_t i = 0; i < pathsInTask; i++) {
                        Number_::Tape()->RewindToMark();
                        random->FillNormal(&gVec);
                        model->GeneratePath(gVec, &path);
                        product.Evaluate(path, eval);
                        Number_ res = eval.VarVals()[payoffIndex];
                        res.PropagateToMark();
                        sumValue += res.value();
                    }

                    Number_::PropagateMarkToStart();
                    for (size_t j = 0; j < nParams; ++j)
                        results.risks_[j] += model->Parameters()[j]->Adjoint() / static_cast<double>(n_paths);

                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  eval.ConstVarVals()[j].Adjoint() / static_cast<double>(n_paths);

                }
                results.aggregated_ += sumValue;
                return true;
            }));
            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);

        Number_::SetTape(*mainThreadPtr);
        SimResults_ rtn(Dal::Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));
        for (auto& res: simResults) {
            rtn.aggregated_ += res.aggregated_;
            for (size_t j = 0; j < rtn.risks_.size(); ++j)
                rtn.risks_[j] += res.risks_[j];
        }
        return rtn;
    }
}
