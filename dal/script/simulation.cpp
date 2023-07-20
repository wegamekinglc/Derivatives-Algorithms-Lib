//
// Created by wegam on 2022/11/6.
//

#include <dal/platform/strict.hpp>
#include <dal/script/simulation.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/aad.hpp>


namespace Dal::Script {

    AAD::Position_ InitModel4ParallelAAD(AAD::Tape_& tape, const ScriptProduct_& prd, AAD::Model_<AAD::Number_>& clonedMdl, Scenario_<AAD::Number_>& path) {
        tape.reset();
        tape.setActive();
        clonedMdl.PutParametersOnTape(tape);
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
        const size_t n_threads = pool->NumThreads();

        Vector_<std::unique_ptr<Random_>> rng_s(n_threads + 1);
        for (auto& random : rng_s)
            random = CreateRNG(rsg, mdl->SimDim(), use_bb);

        Vector_<Vector_<>> gaussVectors(n_threads + 1);
        Vector_<Scenario_<>> paths(n_threads + 1);

        for (auto& vec : gaussVectors)
            vec.Resize(mdl->SimDim());

        for (auto& path : paths) {
            AllocatePath(product.DefLine(), path);
            InitializePath(path);
        }

        Vector_<Evaluator_<double>> eval_s(n_threads + 1, product.BuildEvaluator<double>());
        Vector_<EvalState_<double>> eval_state_s(n_threads + 1,
                                                 EvalState_<double>(static_cast<int>(product.VarNames().size())));

        SimResults_<double> results;

        Vector_<TaskHandle_> futures;
        futures.reserve(n_paths / BATCH_SIZE + 1);

        size_t firstPath = 0;
        size_t pathsLeft = n_paths;

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, BATCH_SIZE);
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                Vector_<>& gaussVec = gaussVectors[threadNum];
                Scenario_<>& path = paths[threadNum];
                auto& random = rng_s[threadNum];
                random->SkipTo(firstPath);
                double sum_val = 0.0;
                if (compiled) {
                    EvalState_<double>& eval_state = eval_state_s[threadNum];
                    for (size_t i = 0; i < pathsInTask; ++i) {
                        random->FillNormal(&gaussVec);
                        mdl->GeneratePath(gaussVec, &path);
                        product.EvaluateCompiled(path, eval_state);
                        sum_val += eval_state.VarVals()[eval_state.VarVals().size() - 1];
                    }
                } else {
                    Evaluator_<double>& eval = eval_s[threadNum];
                    for (size_t i = 0; i < pathsInTask; ++i) {
                        random->FillNormal(&gaussVec);
                        mdl->GeneratePath(gaussVec, &path);
                        product.Evaluate(path, eval);
                        sum_val += eval.VarVals()[eval.VarVals().size() - 1];
                    }
                }
                results.aggregated_ += sum_val;
                return true;
            }));
            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);

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

        const auto n_params = mdl->Parameters().size();
        SimResults_<AAD::Number_> results(n_params);

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThread = pool->NumThreads();

        Vector_<std::unique_ptr<AAD::Model_<AAD::Number_>>> models(nThread + 1);
        for (auto& m : models) {
            m = mdl->Clone();
            m->Allocate(product.TimeLine(), product.DefLine());
        }

        Vector_<std::unique_ptr<Random_>> rng_s(nThread + 1);
        for (auto& random : rng_s)
            random = CreateRNG(rsg, mdl->SimDim(), use_bb);

        Vector_<Vector_<>> gaussVectors(nThread + 1);
        Vector_<Scenario_<AAD::Number_>> paths(nThread + 1);
        Vector_<Evaluator_<AAD::Number_>> eval_s;
        Vector_<FuzzyEvaluator_<AAD::Number_>> fuzzy_eval_s;
        Vector_<EvalState_<AAD::Number_>> eval_state_s;

        for (auto& vec : gaussVectors)
            vec.Resize(mdl->SimDim());

        for (auto& path : paths) {
            AllocatePath(product.DefLine(), path);
            InitializePath(path);
        }
        if (compiled)
            eval_state_s = Vector_<EvalState_<AAD::Number_>>(nThread + 1,
                                                             EvalState_<AAD::Number_>(static_cast<int>(product.VarNames().size())));
        else if (max_nested_ifs > 0)
            fuzzy_eval_s = Vector_<FuzzyEvaluator_<AAD::Number_>>(nThread + 1, product.BuildFuzzyEvaluator<AAD::Number_>(max_nested_ifs, eps));
        else
            eval_s = Vector_<Evaluator_<AAD::Number_>>(nThread + 1, product.BuildEvaluator<AAD::Number_>());

        Vector_<TaskHandle_> futures;
        futures.reserve(n_paths / BATCH_SIZE + 1);

        size_t firstPath = 0;
        size_t pathsLeft = n_paths;

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, BATCH_SIZE);
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t n_threads = ThreadPool_::ThreadNum();
                AAD::Tape_* tape = &AAD::Number_::getTape();

                //  Initialize once on each thread
                auto pos = InitModel4ParallelAAD(*tape, product, *models[n_threads], paths[n_threads]);
                Scenario_<AAD::Number_>& path = paths[n_threads];

                auto& random = rng_s[n_threads];
                auto& gVec = gaussVectors[n_threads];
                auto& model = models[n_threads];
                random->SkipTo(firstPath);

                double sum_val = 0.0;
                if (compiled) {
                    EvalState_<AAD::Number_>& eval_state = eval_state_s[n_threads];
                    for (size_t i = 0; i < pathsInTask; i++) {
                        random->FillNormal(&gVec);
                        models[n_threads]->GeneratePath(gVec, &path);
                        product.EvaluateCompiled(path, eval_state);
                        AAD::Number_ res = eval_state.VarVals()[eval_state.VarVals().size() - 1];
                        res.setGradient(1.0);
                        tape->evaluate(tape->getPosition(), pos);
                        sum_val += res.value();
                        tape->resetTo(pos);
                    }
                }
                else if (max_nested_ifs > 0) {
                    FuzzyEvaluator_<AAD::Number_>& eval = fuzzy_eval_s[n_threads];
                    for (size_t i = 0; i < pathsInTask; i++) {
                        random->FillNormal(&gVec);
                        models[n_threads]->GeneratePath(gVec, &path);
                        product.Evaluate(path, eval);
                        AAD::Number_ res = eval.VarVals()[eval.VarVals().size() - 1];
                        res.setGradient(1.0);
                        tape->evaluate(tape->getPosition(), pos);
                        sum_val += res.value();
                        tape->resetTo(pos);
                    }
                } else {
                    Evaluator_<AAD::Number_>& eval = eval_s[n_threads];
                    for (size_t i = 0; i < pathsInTask; i++) {
                        random->FillNormal(&gVec);
                        model->GeneratePath(gVec, &path);
                        product.Evaluate(path, eval);
                        AAD::Number_ res = eval.VarVals()[eval.VarVals().size() - 1];
                        res.setGradient(1.0);
                        tape->evaluate(tape->getPosition(), pos);
                        sum_val += res.value();
                        tape->resetTo(pos);
                    }
                }
                results.aggregated_ += sum_val;
                tape->evaluate(pos, tape->getZeroPosition());
                for (size_t j = 0; j < n_params; ++j)
                    results.risks_[j] += models[n_threads]->Parameters()[j]->getGradient();
                tape->reset();
                return true;
            }));

            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);

        for (size_t j = 0; j < n_params; ++j)
            results.risks_[j] /= static_cast<double>(n_paths);
        return results;
    }
}
