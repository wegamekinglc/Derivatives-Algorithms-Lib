//
// Created by wegam on 2022/11/6.
//

#include <dal/platform/strict.hpp>
#include <dal/script/simulation.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/utilities/numerics.hpp>


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
        Vector_<> sim_results;
        sim_results.reserve(n_paths / BATCH_SIZE + 1);

        size_t firstPath = 0;
        size_t pathsLeft = n_paths;
        size_t loop_i = 0;

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, BATCH_SIZE);
            sim_results.emplace_back(0.0);
            auto& sim_result = sim_results[loop_i];
            loop_i += 1;
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = ThreadPool_::ThreadNum();
                Vector_<>& gaussVec = gaussVectors[threadNum];
                Scenario_<>& path = paths[threadNum];
                auto& random = rng_s[threadNum];
                random->SkipTo(firstPath);
                if (compiled) {
                    EvalState_<double>& eval_state = eval_state_s[threadNum];
                    for (size_t i = 0; i < pathsInTask; ++i) {
                        random->FillNormal(&gaussVec);
                        mdl->GeneratePath(gaussVec, &path);
                        product.EvaluateCompiled(path, eval_state);
                        sim_result += eval_state.VarVals()[eval_state.VarVals().size() - 1];
                    }
                } else {
                    Evaluator_<double>& eval = eval_s[threadNum];
                    for (size_t i = 0; i < pathsInTask; ++i) {
                        random->FillNormal(&gaussVec);
                        mdl->GeneratePath(gaussVec, &path);
                        product.Evaluate(path, eval);
                        sim_result += eval.VarVals()[eval.VarVals().size() - 1];
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
        results.aggregated_ = Accumulate(sim_results);
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
        const size_t n_thread = pool->NumThreads();

        Vector_<std::unique_ptr<AAD::Model_<AAD::Number_>>> models(n_thread + 1);
        for (auto& m : models) {
            m = mdl->Clone();
            m->Allocate(product.TimeLine(), product.DefLine());
        }

        Vector_<std::unique_ptr<Random_>> rng_s(n_thread + 1);
        for (auto& random : rng_s)
            random = CreateRNG(rsg, mdl->SimDim(), use_bb);

        Vector_<Vector_<>> gaussVectors(n_thread + 1);
        Vector_<Scenario_<AAD::Number_>> paths(n_thread + 1);
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
            eval_state_s = Vector_<EvalState_<AAD::Number_>>(n_thread + 1, EvalState_<AAD::Number_>(static_cast<int>(product.VarNames().size())));
        else if (max_nested_ifs > 0)
            fuzzy_eval_s = Vector_<FuzzyEvaluator_<AAD::Number_>>(n_thread + 1, product.BuildFuzzyEvaluator<AAD::Number_>(max_nested_ifs, eps));
        else
            eval_s = Vector_<Evaluator_<AAD::Number_>>(n_thread + 1, product.BuildEvaluator<AAD::Number_>());

        Vector_<TaskHandle_> futures;
        futures.reserve(n_paths / BATCH_SIZE + 1);
        Vector_<> sim_results;
        sim_results.reserve(n_paths / BATCH_SIZE + 1);

        Vector_<bool> model_init(n_thread + 1, false);
        Vector_<AAD::Position_> start_positions(n_thread + 1);
        Vector_<AAD::Tape_*> tapes(n_thread + 1);

        size_t firstPath = 0;
        size_t pathsLeft = n_paths;
        size_t loop_i = 0;

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, BATCH_SIZE);
            sim_results.emplace_back(0.0);
            auto& sim_result = sim_results[loop_i];
            loop_i += 1;
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t n_threads = ThreadPool_::ThreadNum();
                AAD::Tape_* tape = &AAD::Number_::getTape();
                tapes[n_threads] = tape;

                //  Initialize once on each thread
                if (!model_init[n_threads]) {
                    start_positions[n_threads] = InitModel4ParallelAAD(*tape, product, *models[n_threads], paths[n_threads]);
                    model_init[n_threads] = true;
                }
                auto& pos = start_positions[n_threads];

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
                sim_result = sum_val;
                return true;
            }));
            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);

        for (size_t i = 0; i < n_thread + 1; ++i)
            if (model_init[i])
                tapes[i]->evaluate(start_positions[i], tapes[i]->getZeroPosition());

        // aggregate all the results
        for (const auto& s: sim_results)
            results.aggregated_ += s;

        for (size_t j = 0; j < n_params; ++j)
            for (size_t i = 0; i < models.size(); ++i)
                if (model_init[i])
                    results.risks_[j] += models[i]->Parameters()[j]->getGradient() / static_cast<double>(n_paths);

        for (size_t i = 0; i < n_thread + 1; ++i)
            if (model_init[i])
                tapes[i]->reset();
        return results;
    }
}
