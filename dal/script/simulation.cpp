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
            AAD::Reset(&tape);
            AAD::SetActive(&tape);
            for (Number_* param : clonedMdl.Parameters())
                tape.registerInput(*param);

            for (Number_& param : evaluator.ConstVarVals())
                tape.registerInput(param);

            AAD::NewRecording(&tape);

            clonedMdl.Init(prd.TimeLine(), prd.DefLine());
            InitializePath(path);
            return AAD::GetPosition(tape);
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

#ifdef USE_AADET
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

        Vector_<std::unique_ptr<AAD::Model_<AAD::Number_>>> modelVector(nThreads);
        for (auto& m : modelVector) {
            m = mdl->Clone();
            m->Allocate(product.TimeLine(), product.DefLine());
        }

        Vector_<std::unique_ptr<Random_>> rngVector(nThreads);
        for (auto& random : rngVector)
            random = CreateRNG(rsg, mdl->SimDim(), use_bb);

        Vector_<Vector_<>> gaussVectors(nThreads);
        Vector_<Scenario_<AAD::Number_>> paths(nThreads);
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
            evalStateVector = Vector_<EvalState_<AAD::Number_>>(nThreads, product.BuildEvalState<AAD::Number_>());
        else if (max_nested_ifs > 0)
            fuzzyEvalVector = Vector_<FuzzyEvaluator_<AAD::Number_>>(nThreads, product.BuildFuzzyEvaluator<AAD::Number_>(max_nested_ifs, eps));
        else
            evalVector = Vector_<Evaluator_<AAD::Number_>>(nThreads, product.BuildEvaluator<AAD::Number_>());

        const int batchSize = BATCH_SIZE;
        Vector_<TaskHandle_> futures;
        futures.reserve(n_paths / batchSize + 1);
        Vector_<> simEvals;
        simEvals.reserve(n_paths / batchSize + 1);
        SimResults_<AAD::Number_> results(Dal::Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));

        Vector_<bool> modelInit(nThreads, false);
        Vector_<AAD::Position_> startPositions(nThreads);
        Vector_<AAD::Tape_*> tapes(nThreads, nullptr);

        int firstPath = 0;
        int pathsLeft = static_cast<int>(n_paths);
        size_t loopIndex = 0;
        auto payoffIndex = product.PayOffIdx();

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, batchSize);
            simEvals.emplace_back(0.0);
            auto& simEval = simEvals[loopIndex];
            loopIndex += 1;
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                Scenario_<AAD::Number_>& path = paths[threadNum];

                auto& random = rngVector[threadNum];
                auto& gVec = gaussVectors[threadNum];
                auto& model = modelVector[threadNum];

                random->SkipTo(firstPath);
                AAD::Tape_* tape = tapes[threadNum] = &AAD::GetTape();

                double sumValue = 0.0;
                if (compiled) {
                    EvalState_<AAD::Number_>& evalState = evalStateVector[threadNum];

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
                        Number_ res = evalState.VarVals()[payoffIndex];
                        AAD::SetGradient(res, 1.0);
                        AAD::Evaluate(tape, pos);
                        sumValue += res.value();
                        AAD::ResetToPos(tape, pos);
                    }
                }
                else if (max_nested_ifs > 0) {
                    FuzzyEvaluator_<AAD::Number_>& eval = fuzzyEvalVector[threadNum];

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
                        Number_ res = eval.VarVals()[payoffIndex];
                        AAD::SetGradient(res, 1.0);
                        AAD::Evaluate(tape, pos);
                        sumValue += res.value();
                        AAD::ResetToPos(tape, pos);
                    }
                } else {
                    Evaluator_<AAD::Number_>& eval = evalVector[threadNum];

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
                        Number_ res = eval.VarVals()[payoffIndex];
                        AAD::SetGradient(res, 1.0);
                        AAD::Evaluate(tape, pos);
                        sumValue += res.value();
                        AAD::ResetToPos(tape, pos);
                    }
                }
                simEval = sumValue;
                return true;
            }));
            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);

        // aggregate all the results
        for (const auto& s: simEvals)
            results.aggregated_ += s;

        for (size_t i = 0; i < nThreads; ++i)
            if (modelInit[i])
                AAD::Evaluate(tapes[i]);
                    for (size_t j = 0; j < nParams; ++j)
                        for (size_t i = 0; i < modelVector.size(); ++i)
                            if (modelInit[i])
                                results.risks_[j] += AAD::GetGradient(*modelVector[i]->Parameters()[j]) / static_cast<double>(n_paths);

        if (compiled) {
            for (size_t j = 0; j < nConstVars; ++j)
                for (size_t i = 0; i < evalStateVector.size(); ++i)
                    if (modelInit[i])
                        results.risks_[j + nParams] +=  AAD::GetGradient(evalStateVector[i].ConstVarVals()[j]) / static_cast<double>(n_paths);
        }  else if (max_nested_ifs > 0) {
            for (size_t j = 0; j < nConstVars; ++j)
                for (size_t i = 0; i < fuzzyEvalVector.size(); ++i)
                    if (modelInit[i])
                        results.risks_[j + nParams] +=  AAD::GetGradient(fuzzyEvalVector[i].ConstVarVals()[j]) / static_cast<double>(n_paths);
        } else {
            for (size_t j = 0; j < nConstVars; ++j)
                for (size_t i = 0; i < evalVector.size(); ++i)
                    if (modelInit[i])
                        results.risks_[j + nParams] +=  AAD::GetGradient(evalVector[i].ConstVarVals()[j]) / static_cast<double>(n_paths);
        }
        for (size_t i = 0; i < nThreads; ++i)
            if (modelInit[i])
                tapes[i]->Clear();
        return results;
    }
}
#endif

#ifdef USE_XAD
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

        Vector_<std::unique_ptr<AAD::Model_<AAD::Number_>>> modelVector(nThreads);
        for (auto& m : modelVector) {
            m = mdl->Clone();
            m->Allocate(product.TimeLine(), product.DefLine());
        }

        Vector_<std::unique_ptr<Random_>> rngVector(nThreads);
        for (auto& random : rngVector)
            random = CreateRNG(rsg, mdl->SimDim(), use_bb);

        Vector_<Vector_<>> gaussVectors(nThreads);
        Vector_<Scenario_<AAD::Number_>> paths(nThreads);
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
            evalStateVector = Vector_<EvalState_<AAD::Number_>>(nThreads, product.BuildEvalState<AAD::Number_>());
        else if (max_nested_ifs > 0)
            fuzzyEvalVector = Vector_<FuzzyEvaluator_<AAD::Number_>>(nThreads, product.BuildFuzzyEvaluator<AAD::Number_>(max_nested_ifs, eps));
        else
            evalVector = Vector_<Evaluator_<AAD::Number_>>(nThreads, product.BuildEvaluator<AAD::Number_>());

        const int batchSize = std::max(BATCH_SIZE, static_cast<int>(n_paths / nThreads + 1));\
        Vector_<TaskHandle_> futures;
        futures.reserve(n_paths / batchSize + 1);
        Vector_<> simEvals;
        simEvals.reserve(n_paths / batchSize + 1);

        SimResults_<AAD::Number_> sub_res(Dal::Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));
        Vector_<SimResults_<AAD::Number_>> simResults(nThreads, sub_res);

        Vector_<bool> modelInit(nThreads, false);
        Vector_<AAD::Position_> startPositions(nThreads);
        Vector_<AAD::Tape_*> tapes(nThreads, nullptr);

        int firstPath = 0;
        int pathsLeft = static_cast<int>(n_paths);
        size_t loopIndex = 0;
        auto payoffIndex = product.PayOffIdx();

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, batchSize);
            simEvals.emplace_back(0.0);
            auto& simEval = simEvals[loopIndex];
            loopIndex += 1;
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                Scenario_<AAD::Number_>& path = paths[threadNum];

                auto& random = rngVector[threadNum];
                auto& gVec = gaussVectors[threadNum];
                auto& model = modelVector[threadNum];
                auto& results = simResults[threadNum];
                random->SkipTo(firstPath);
                auto this_tap = AAD::GetTape();
                AAD::Tape_* tape = &this_tap;

                double sumValue = 0.0;
                if (compiled) {
                    EvalState_<AAD::Number_>& evalState = evalStateVector[threadNum];

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
                        Number_ res = evalState.VarVals()[payoffIndex];
                        AAD::SetGradient(res, 1.0);
                        AAD::Evaluate(tape, pos);
                        sumValue += res.value();
                        AAD::ResetToPos(tape, pos);
                    }
                }
                else if (max_nested_ifs > 0) {
                    FuzzyEvaluator_<AAD::Number_>& eval = fuzzyEvalVector[threadNum];

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
                        Number_ res = eval.VarVals()[payoffIndex];
                        AAD::SetGradient(res, 1.0);
                        AAD::Evaluate(tape, pos);
                        sumValue += res.value();
                        AAD::ResetToPos(tape, pos);
                    }
                } else {
                    Evaluator_<AAD::Number_>& eval = evalVector[threadNum];

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
                        Number_ res = eval.VarVals()[payoffIndex];
                        AAD::SetGradient(res, 1.0);
                        AAD::Evaluate(tape, pos);
                        sumValue += res.value();
                        AAD::ResetToPos(tape, pos);
                    }
                }
                simEval = sumValue;

                AAD::Evaluate(tape);
                results.aggregated_ += simEval;
                for (size_t j = 0; j < nParams; ++j)
                    results.risks_[j] += AAD::GetGradient(*model->Parameters()[j]) / static_cast<double>(n_paths);

                if (compiled) {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  AAD::GetGradient(evalStateVector[threadNum].ConstVarVals()[j]) / static_cast<double>(n_paths);
                }  else if (max_nested_ifs > 0) {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  AAD::GetGradient(fuzzyEvalVector[threadNum].ConstVarVals()[j]) / static_cast<double>(n_paths);
                } else {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  AAD::GetGradient(evalVector[threadNum].ConstVarVals()[j]) / static_cast<double>(n_paths);
                }
                AAD::Reset(tape);
                return true;
            }));
            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);


        SimResults_<AAD::Number_> results(Dal::Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));
        // aggregate all the results
        for (auto k = 0; k < simResults.size(); ++k) {
            if (modelInit[k]) {
                const auto& s = simResults[k];
                results.aggregated_ += s.aggregated_;
                for (auto i = 0; i < results.risks_.size(); ++i)
                    results.risks_[i] += s.risks_[i];
            }
        }
        return results;
    }

#endif


#ifdef USE_CODI
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

        Vector_<std::unique_ptr<AAD::Model_<AAD::Number_>>> modelVector(nThreads);
        for (auto& m : modelVector) {
            m = mdl->Clone();
            m->Allocate(product.TimeLine(), product.DefLine());
        }

        Vector_<std::unique_ptr<Random_>> rngVector(nThreads);
        for (auto& random : rngVector)
            random = CreateRNG(rsg, mdl->SimDim(), use_bb);

        Vector_<Vector_<>> gaussVectors(nThreads);
        Vector_<Scenario_<AAD::Number_>> paths(nThreads);
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
            evalStateVector = Vector_<EvalState_<AAD::Number_>>(nThreads, product.BuildEvalState<AAD::Number_>());
        else if (max_nested_ifs > 0)
            fuzzyEvalVector = Vector_<FuzzyEvaluator_<AAD::Number_>>(nThreads, product.BuildFuzzyEvaluator<AAD::Number_>(max_nested_ifs, eps));
        else
            evalVector = Vector_<Evaluator_<AAD::Number_>>(nThreads, product.BuildEvaluator<AAD::Number_>());

        const int batchSize = std::max(BATCH_SIZE, static_cast<int>(n_paths / nThreads + 1));\
        Vector_<TaskHandle_> futures;
        futures.reserve(n_paths / batchSize + 1);
        Vector_<> simEvals;
        simEvals.reserve(n_paths / batchSize + 1);

        SimResults_<AAD::Number_> sub_res(Dal::Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));
        Vector_<SimResults_<AAD::Number_>> simResults(nThreads, sub_res);

        Vector_<bool> modelInit(nThreads, false);
        Vector_<AAD::Position_> startPositions(nThreads);
        Vector_<AAD::Tape_*> tapes(nThreads, nullptr);

        int firstPath = 0;
        int pathsLeft = static_cast<int>(n_paths);
        size_t loopIndex = 0;
        auto payoffIndex = product.PayOffIdx();

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, batchSize);
            simEvals.emplace_back(0.0);
            auto& simEval = simEvals[loopIndex];
            loopIndex += 1;
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                Scenario_<AAD::Number_>& path = paths[threadNum];

                auto& random = rngVector[threadNum];
                auto& gVec = gaussVectors[threadNum];
                auto& model = modelVector[threadNum];
                auto& results = simResults[threadNum];
                random->SkipTo(firstPath);
                AAD::Tape_* tape = &AAD::GetTape();

                double sumValue = 0.0;
                if (compiled) {
                    EvalState_<AAD::Number_>& evalState = evalStateVector[threadNum];

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
                        Number_ res = evalState.VarVals()[payoffIndex];
                        AAD::SetGradient(res, 1.0);
                        AAD::Evaluate(tape, pos);
                        sumValue += res.value();
                        AAD::ResetToPos(tape, pos);
                    }
                }
                else if (max_nested_ifs > 0) {
                    FuzzyEvaluator_<AAD::Number_>& eval = fuzzyEvalVector[threadNum];

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
                        Number_ res = eval.VarVals()[payoffIndex];
                        AAD::SetGradient(res, 1.0);
                        AAD::Evaluate(tape, pos);
                        sumValue += res.value();
                        AAD::ResetToPos(tape, pos);
                    }
                } else {
                    Evaluator_<AAD::Number_>& eval = evalVector[threadNum];

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
                        Number_ res = eval.VarVals()[payoffIndex];
                        AAD::SetGradient(res, 1.0);
                        AAD::Evaluate(tape, pos);
                        sumValue += res.value();
                        AAD::ResetToPos(tape, pos);
                    }
                }
                simEval = sumValue;

                AAD::Evaluate(tape);
                results.aggregated_ += simEval;
                for (size_t j = 0; j < nParams; ++j)
                    results.risks_[j] += AAD::GetGradient(*model->Parameters()[j]) / static_cast<double>(n_paths);

                if (compiled) {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  AAD::GetGradient(evalStateVector[threadNum].ConstVarVals()[j]) / static_cast<double>(n_paths);
                }  else if (max_nested_ifs > 0) {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  AAD::GetGradient(fuzzyEvalVector[threadNum].ConstVarVals()[j]) / static_cast<double>(n_paths);
                } else {
                    for (size_t j = 0; j < nConstVars; ++j)
                        results.risks_[j + nParams] +=  AAD::GetGradient(evalVector[threadNum].ConstVarVals()[j]) / static_cast<double>(n_paths);
                }
                AAD::Reset(tape);
                return true;
            }));
            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);

        SimResults_<AAD::Number_> results(Dal::Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));
        // aggregate all the results
        for (auto k = 0; k < simResults.size(); ++k) {
            if (modelInit[k]) {
                const auto& s = simResults[k];
                results.aggregated_ += s.aggregated_;
                for (auto i = 0; i < results.risks_.size(); ++i)
                    results.risks_[i] += s.risks_[i];
            }
        }
        return results;
    }
#endif
}