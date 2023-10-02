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
        AAD::Position_ InitModel4ParallelAAD(AAD::Tape_& tape,
                                             const ScriptProduct_& prd,
                                             AAD::Model_<AAD::Number_>& clonedMdl,
                                             Scenario_<AAD::Number_>& path,
                                             E_& evaluator) {
            tape.reset();
            tape.setActive();
            for (Number_* param : clonedMdl.Parameters())
                tape.registerInput(*param);

            for (Number_& param : evaluator.ConstVarVals())
                tape.registerInput(param);

            clonedMdl.Init(prd.TimeLine(), prd.DefLine());
            InitializePath(path);
            return tape.getPosition();
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

        Vector_<std::unique_ptr<Random_>> rngs(nThreads);
        for (auto& random : rngs)
            random = CreateRNG(rsg, mdl->SimDim(), use_bb);

        Vector_<Vector_<>> gaussVectors(nThreads);
        Vector_<Scenario_<>> paths(nThreads);

        for (auto& vec : gaussVectors)
            vec.Resize(mdl->SimDim());

        for (auto& path : paths) {
            AllocatePath(product.DefLine(), path);
            InitializePath(path);
        }

        Vector_<Evaluator_<double>> evals(nThreads, product.BuildEvaluator<double>());
        Vector_<EvalState_<double>> evalStates(nThreads, product.BuildEvalState<double>());

        SimResults_<double> results;

        Vector_<TaskHandle_> futures;
        futures.reserve(n_paths / BATCH_SIZE + 1);
        Vector_<> simResults;
        simResults.reserve(n_paths / BATCH_SIZE + 1);

        int firstPath = 0;
        int pathsLeft = n_paths;
        size_t loop_i = 0;
        auto payoff_idx = product.PayOffIdx();

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, BATCH_SIZE);
            simResults.emplace_back(0.0);
            auto& simResult = simResults[loop_i];
            loop_i += 1;
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                Vector_<>& gaussVec = gaussVectors[threadNum];
                Scenario_<>& path = paths[threadNum];
                auto& random = rngs[threadNum];
                random->SkipTo(firstPath);
                if (compiled) {
                    EvalState_<double>& eval_state = evalStates[threadNum];
                    for (size_t i = 0; i < pathsInTask; ++i) {
                        random->FillNormal(&gaussVec);
                        mdl->GeneratePath(gaussVec, &path);
                        product.EvaluateCompiled(path, eval_state);
                        simResult += eval_state.VarVals()[payoff_idx];
                    }
                } else {
                    Evaluator_<double>& eval = evals[threadNum];
                    for (size_t i = 0; i < pathsInTask; ++i) {
                        random->FillNormal(&gaussVec);
                        mdl->GeneratePath(gaussVec, &path);
                        product.Evaluate(path, eval);
                        simResult += eval.VarVals()[payoff_idx];
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
        SimResults_<AAD::Number_> results(Dal::Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThreads = pool->NumThreads();

        Vector_<std::unique_ptr<AAD::Model_<AAD::Number_>>> models(nThreads);
        for (auto& m : models) {
            m = mdl->Clone();
            m->Allocate(product.TimeLine(), product.DefLine());
        }

        Vector_<std::unique_ptr<Random_>> rngs(nThreads);
        for (auto& random : rngs)
            random = CreateRNG(rsg, mdl->SimDim(), use_bb);

        Vector_<Vector_<>> gaussVectors(nThreads);
        Vector_<Scenario_<AAD::Number_>> paths(nThreads);
        Vector_<Evaluator_<AAD::Number_>> evals;
        Vector_<FuzzyEvaluator_<AAD::Number_>> fuzzyEvals;
        Vector_<EvalState_<AAD::Number_>> evalStates;

        for (auto& vec : gaussVectors)
            vec.Resize(mdl->SimDim());

        for (auto& path : paths) {
            AllocatePath(product.DefLine(), path);
            InitializePath(path);
        }
        if (compiled)
            evalStates = Vector_<EvalState_<AAD::Number_>>(nThreads, product.BuildEvalState<AAD::Number_>());
        else if (max_nested_ifs > 0)
            fuzzyEvals = Vector_<FuzzyEvaluator_<AAD::Number_>>(nThreads, product.BuildFuzzyEvaluator<AAD::Number_>(max_nested_ifs, eps));
        else
            evals = Vector_<Evaluator_<AAD::Number_>>(nThreads, product.BuildEvaluator<AAD::Number_>());

#ifndef USE_AADET
        const int batchSize = std::max(BATCH_SIZE, n_paths / nThreads + 1);
#else
        const int batchSize = BATCH_SIZE;
#endif
        Vector_<TaskHandle_> futures;
        futures.reserve(n_paths / batchSize + 1);
        Vector_<> simResults;
        simResults.reserve(n_paths / batchSize + 1);

        Vector_<bool> modelInit(nThreads, false);
        Vector_<AAD::Position_> startPositions(nThreads);
        Vector_<AAD::Tape_*> tapes(nThreads, nullptr);

        int firstPath = 0;
        int pathsLeft = n_paths;
        size_t loop_i = 0;
        auto payoffIdx = product.PayOffIdx();

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, batchSize);
            simResults.emplace_back(0.0);
            auto& simResult = simResults[loop_i];
            loop_i += 1;
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                Scenario_<AAD::Number_>& path = paths[threadNum];

                auto& random = rngs[threadNum];
                auto& gVec = gaussVectors[threadNum];
                auto& model = models[threadNum];
                random->SkipTo(firstPath);

                if (!tapes[threadNum])
                    tapes[threadNum] = &AAD::Number_::getTape();
                AAD::Tape_* tape = tapes[threadNum];

                double sum_val = 0.0;
                if (compiled) {
                    EvalState_<AAD::Number_>& evalState = evalStates[threadNum];

                    //  Initialize once on each thread
                    if (!modelInit[threadNum]) {
                        startPositions[threadNum] = InitModel4ParallelAAD(*tape, product, *model, path, evalState);
                        modelInit[threadNum] = true;
                    }
                    auto& pos = startPositions[threadNum];

                    for (size_t i = 0; i < pathsInTask; i++) {
                        random->FillNormal(&gVec);
                        model->GeneratePath(gVec, &path);
                        product.EvaluateCompiled(path, evalState);
                        AAD::Number_ res = evalState.VarVals()[payoffIdx];
                        res.setGradient(1.0);
                        tape->evaluate(tape->getPosition(), pos);
                        sum_val += res.value();
                        tape->resetTo(pos, false);
                    }
                }
                else if (max_nested_ifs > 0) {
                    FuzzyEvaluator_<AAD::Number_>& eval = fuzzyEvals[threadNum];

                    //  Initialize once on each thread
                    if (!modelInit[threadNum]) {
                        startPositions[threadNum] = InitModel4ParallelAAD(*tape, product, *model, path, eval);
                        modelInit[threadNum] = true;
                    }
                    auto& pos = startPositions[threadNum];

                    for (size_t i = 0; i < pathsInTask; i++) {
                        random->FillNormal(&gVec);
                        model->GeneratePath(gVec, &path);
                        product.Evaluate(path, eval);
                        AAD::Number_ res = eval.VarVals()[payoffIdx];
                        res.setGradient(1.0);
                        tape->evaluate(tape->getPosition(), pos);
                        sum_val += res.value();
                        tape->resetTo(pos, false);
                    }
                } else {
                    Evaluator_<AAD::Number_>& eval = evals[threadNum];

                    //  Initialize once on each thread
                    if (!modelInit[threadNum]) {
                        startPositions[threadNum] = InitModel4ParallelAAD(*tape, product, *model, path, eval);
                        modelInit[threadNum] = true;
                    }
                    auto& pos = startPositions[threadNum];

                    for (size_t i = 0; i < pathsInTask; i++) {
                        random->FillNormal(&gVec);
                        model->GeneratePath(gVec, &path);
                        product.Evaluate(path, eval);
                        AAD::Number_ res = eval.VarVals()[payoffIdx];
                        res.setGradient(1.0);
                        tape->evaluate(tape->getPosition(), pos);
                        sum_val += res.value();
                        tape->resetTo(pos, false);
                    }
                }
                simResult = sum_val;
#ifndef USE_AADET
                tape->evaluate(pos, tape->getZeroPosition());
                for (size_t j = 0; j < nParams; ++j)
                    results.risks_[j] += model->Parameters()[j]->getGradient() / static_cast<double>(n_paths);

                if (compiled) {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  evalStates[threadNum].ConstVarVals()[j].getGradient() / static_cast<double>(n_paths);
                }  else if (max_nested_ifs > 0) {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  fuzzyEvals[threadNum].ConstVarVals()[j].getGradient() / static_cast<double>(n_paths);
                } else {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  evals[threadNum].ConstVarVals()[j].getGradient() / static_cast<double>(n_paths);
                }
                tape->reset();
#endif
                return true;
            }));
            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);

        // aggregate all the results
        for (const auto& s: simResults)
            results.aggregated_ += s;

#ifdef USE_AADET
        for (size_t i = 0; i < nThreads; ++i)
            if (modelInit[i])
                tapes[i]->evaluate();

        for (size_t j = 0; j < nParams; ++j)
            for (size_t i = 0; i < models.size(); ++i)
                if (modelInit[i])
                    results.risks_[j] += models[i]->Parameters()[j]->getGradient() / static_cast<double>(n_paths);

        if (compiled) {
            for (size_t j = 0; j < nConstVars; ++j)
                for (size_t i = 0; i < evalStates.size(); ++i)
                    if (modelInit[i])
                        results.risks_[j + nParams] +=  evalStates[i].ConstVarVals()[j].getGradient() / static_cast<double>(n_paths);
        }  else if (max_nested_ifs > 0) {
            for (size_t j = 0; j < nConstVars; ++j)
                for (size_t i = 0; i < fuzzyEvals.size(); ++i)
                    if (modelInit[i])
                        results.risks_[j + nParams] +=  fuzzyEvals[i].ConstVarVals()[j].getGradient() / static_cast<double>(n_paths);
        } else {
            for (size_t j = 0; j < nConstVars; ++j)
                for (size_t i = 0; i < evals.size(); ++i)
                    if (modelInit[i])
                        results.risks_[j + nParams] +=  evals[i].ConstVarVals()[j].getGradient() / static_cast<double>(n_paths);
        }
        for (size_t i = 0; i < nThreads; ++i)
            if (modelInit[i])
                tapes[i]->Clear();
#endif
        return results;
    }
}
