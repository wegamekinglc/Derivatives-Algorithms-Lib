//
// Created by wegam on 2022/11/6.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/simulation.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/utilities/numerics.hpp>


namespace Dal::Script {

    namespace {
        constexpr const int BATCH_SIZE = 8192;

        template<class E_>
        void InitModel4ParallelAAD(const ScriptProduct_& prd,
                                   AAD::Model_<AAD::Number_>& clonedMdl,
                                   Scenario_<AAD::Number_>& path,
                                   E_& evaluator) {
            AAD::Tape_& tape = *Number_::tape_;
            tape.Rewind();
            for (Number_* param : clonedMdl.Parameters())
                tape.registerInput(*param);

            for (Number_& param : evaluator.ConstVarVals())
                tape.registerInput(param);

            clonedMdl.Init(prd.TimeLine(), prd.DefLine());
            InitializePath(path);
            tape.Mark();
        }

        std::unique_ptr<Random_> CreateRNG(const String_& method, size_t n_dim, bool use_bb) {
            std::unique_ptr<Random_> rsg;
            if (method == "sobol")
                rsg = std::unique_ptr<Random_>(NewSobol(static_cast<int>(n_dim), 2048));
            else if (method == "mrg32")
                rsg = std::unique_ptr<Random_>(New(RNGType_("MRG32"), 1024, n_dim));
            else if (method == "irn")
                rsg = std::unique_ptr<Random_>(New(RNGType_("IRN"), 1024, n_dim));
            else
                THROW("rng method is not known");

            if (use_bb)
                return std::make_unique<BrownianBridge_>(std::move(rsg));
            else
                return rsg;
        }
    }

    SimResults_<> MCSimulation(const ScriptProduct_& product,
                               const AAD::Model_<double>& model,
                               size_t n_paths,
                               const String_& rsg,
                               bool use_bb,
                               bool compiled) {
        auto mdl = model.Clone();

        mdl->Allocate(product.TimeLine(), product.DefLine());
        mdl->Init(product.TimeLine(), product.DefLine());

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThreads = pool->NumThreads();

        Vector_<std::unique_ptr<Random_>> rngVector(nThreads + 1);
        for (auto& random : rngVector)
            random = CreateRNG(rsg, mdl->SimDim(), use_bb);

        Vector_<Vector_<>> gaussVectors(nThreads + 1);
        Vector_<Scenario_<>> paths(nThreads + 1);

        for (auto& vec : gaussVectors)
            vec.Resize(mdl->SimDim());

        for (auto& path : paths) {
            AllocatePath(product.DefLine(), path);
            InitializePath(path);
        }

        Vector_<Evaluator_<double>> evalVector(nThreads + 1, product.BuildEvaluator<double>());
        Vector_<EvalState_<double>> evalStateVector(nThreads + 1, product.BuildEvalState<double>());

        SimResults_<double> results;

        Vector_<TaskHandle_> futures;
        futures.reserve(n_paths / BATCH_SIZE + 1);
        Vector_<> simResults;
        simResults.reserve(n_paths / BATCH_SIZE + 1);

        int firstPath = 0;
        int pathsLeft = static_cast<int>(n_paths);
        size_t loopIndex = 0;
        auto payoffIndex = product.PayOffIdx();

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, BATCH_SIZE);
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

    SimResults_<AAD::Number_> MCSimulation(const ScriptProduct_& product,
                                           const AAD::Model_<AAD::Number_>& model,
                                           size_t n_paths,
                                           const String_& rsg,
                                           bool use_bb,
                                           int max_nested_ifs,
                                           double eps,
                                           bool compiled) {
        auto mdl = model.Clone();
        mdl->Allocate(product.TimeLine(), product.DefLine());
        std::unique_ptr<Random_> rng = CreateRNG(rsg, mdl->SimDim(), use_bb);

        const auto nParams = mdl->Parameters().size();
        const auto nConstVars = product.ConstVarNames().size();

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThreads = pool->NumThreads();

        Vector_<std::unique_ptr<AAD::Model_<AAD::Number_>>> models(nThreads + 1);
        for (auto& m : models) {
            m = mdl->Clone();
            m->Allocate(product.TimeLine(), product.DefLine());
        }

        Vector_<std::unique_ptr<Random_>> rngVector(nThreads + 1);
        for (auto& random : rngVector)
            random = CreateRNG(rsg, mdl->SimDim(), use_bb);

        Vector_<Vector_<>> gaussVectors(nThreads + 1);
        Vector_<Scenario_<AAD::Number_>> paths(nThreads + 1);
        Vector_<Evaluator_<AAD::Number_>> evalVector;
        Vector_<FuzzyEvaluator_<AAD::Number_>> fuzzyEvalVector;
        Vector_<EvalState_<AAD::Number_>> evalStateVector;

        for (auto& vec : gaussVectors)
            vec.Resize(mdl->SimDim());

        for (auto& path : paths) {
            AllocatePath(product.DefLine(), path);
            InitializePath(path);
        }
        if (compiled)
            evalStateVector = Vector_<EvalState_<AAD::Number_>>(nThreads + 1, product.BuildEvalState<AAD::Number_>());
        else if (max_nested_ifs > 0)
            fuzzyEvalVector = Vector_<FuzzyEvaluator_<AAD::Number_>>(nThreads + 1, product.BuildFuzzyEvaluator<AAD::Number_>(max_nested_ifs, eps));
        else
            evalVector = Vector_<Evaluator_<AAD::Number_>>(nThreads + 1, product.BuildEvaluator<AAD::Number_>());

        Vector_<TaskHandle_> futures;
        const int batch_size = std::max(BATCH_SIZE, static_cast<int>(n_paths / (nThreads + 1) + 1));
        futures.reserve(n_paths / batch_size + 1);
        Vector_<> simEvals;
        simEvals.reserve(n_paths / batch_size + 1);

        SimResults_<AAD::Number_> sub_res(Dal::Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));
        Vector_<SimResults_<AAD::Number_>> simResults(nThreads + 1, sub_res);

        Vector_<bool> modelInit(nThreads + 1, false);

        if (compiled)
            InitModel4ParallelAAD(product, *models[0], paths[0], evalStateVector[0]);
        else if (max_nested_ifs > 0)
            InitModel4ParallelAAD(product, *models[0], paths[0], fuzzyEvalVector[0]);
        else
            InitModel4ParallelAAD(product, *models[0], paths[0], evalVector[0]);
        modelInit[0] = true;

        int firstPath = 0;
        int pathsLeft = static_cast<int>(n_paths);
        size_t loopIndex = 0;
        auto payoffIndex = product.PayOffIdx();

        Vector_<AAD::Tape_> tapes(nThreads);

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, batch_size);
            simEvals.emplace_back(0.0);
            auto& simEval = simEvals[loopIndex];
            loopIndex += 1;
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                Scenario_<AAD::Number_>& path = paths[threadNum];

                auto& random = rngVector[threadNum];
                auto& gVec = gaussVectors[threadNum];
                auto& model = models[threadNum];
                auto& results = simResults[threadNum];
                random->SkipTo(firstPath);

                if (threadNum > 0)
                    Number_::tape_ = &tapes[threadNum - 1];

                double sumValue = 0.0;
                if (compiled) {
                    EvalState_<AAD::Number_>& evalState = evalStateVector[threadNum];

                    //  Initialize once on each thread
                    if (!modelInit[threadNum]) {
                        InitModel4ParallelAAD(product, *model, path, evalState);
                        modelInit[threadNum] = true;
                    }

                    for (size_t i = 0; i < pathsInTask; i++) {
                        Number_::tape_->RewindToMark();
                        random->FillNormal(&gVec);
                        model->GeneratePath(gVec, &path);
                        product.EvaluateCompiled(path, evalState);
                        Number_ res = evalState.VarVals()[payoffIndex];
                        res.PropagateToMark();
                        sumValue += res.value();
                    }
                }
                else if (max_nested_ifs > 0) {
                    FuzzyEvaluator_<AAD::Number_>& eval = fuzzyEvalVector[threadNum];

                    //  Initialize once on each thread
                    if (!modelInit[threadNum]) {
                        InitModel4ParallelAAD(product, *model, path, eval);
                        modelInit[threadNum] = true;
                    }

                    for (size_t i = 0; i < pathsInTask; i++) {
                        Number_::tape_->RewindToMark();
                        random->FillNormal(&gVec);
                        model->GeneratePath(gVec, &path);
                        product.Evaluate(path, eval);
                        Number_ res = eval.VarVals()[payoffIndex];
                        res.PropagateToMark();
                        sumValue += res.value();
                    }
                } else {
                    Evaluator_<AAD::Number_>& eval = evalVector[threadNum];

                    //  Initialize once on each thread
                    if (!modelInit[threadNum]) {
                        InitModel4ParallelAAD(product, *model, path, eval);
                        modelInit[threadNum] = true;
                    }

                    for (size_t i = 0; i < pathsInTask; i++) {
                        Number_::tape_->RewindToMark();
                        random->FillNormal(&gVec);
                        model->GeneratePath(gVec, &path);
                        product.Evaluate(path, eval);
                        Number_ res = eval.VarVals()[payoffIndex];
                        res.PropagateToMark();
                        sumValue += res.value();
                    }
                }
                simEval = sumValue;
                results.aggregated_ += simEval;
                return true;
            }));
            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);

        Number_::PropagateMarkToStart();
        //  And on the worker thread's tapes
        AAD::Tape_* mainThreadPtr = Number_::tape_;
        for (size_t i = 0; i < nThreads; ++i) {
            if (modelInit[i + 1]) {
                Number_::tape_ = &tapes[i];
                Number_::PropagateMarkToStart();
            }
        }

        SimResults_<AAD::Number_> results(Dal::Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));
        // aggregate all the results
        for (auto k = 0; k < simResults.size(); ++k) {
            if (modelInit[k]) {
                auto& this_model = models[k];
                const auto& s = simResults[k];
                results.aggregated_ += s.aggregated_;
                for (auto i = 0; i < results.risks_.size(); ++i)
                    results.risks_[i] += s.risks_[i];

                for (size_t j = 0; j < nParams; ++j)
                    results.risks_[j] += AAD::GetGradient(*this_model->Parameters()[j]) / static_cast<double>(n_paths);

                if (compiled) {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  AAD::GetGradient(evalStateVector[k].ConstVarVals()[j]) / static_cast<double>(n_paths);
                }  else if (max_nested_ifs > 0) {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  AAD::GetGradient(fuzzyEvalVector[k].ConstVarVals()[j]) / static_cast<double>(n_paths);
                } else {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  AAD::GetGradient(evalVector[k].ConstVarVals()[j]) / static_cast<double>(n_paths);
                }
            }
        }

        Number_::tape_ = mainThreadPtr;
        Number_::tape_->Clear();
        return results;
    }
}