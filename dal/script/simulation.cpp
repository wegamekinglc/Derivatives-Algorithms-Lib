//
// Created by wegam on 2022/11/6.
//

#include <dal/platform/strict.hpp>
#include <dal/script/simulation.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/utilities/numerics.hpp>


namespace Dal::Script {

    namespace {
        constexpr const size_t BATCH_SIZE = 8192;

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
        Vector_<EvalState_<double>> eval_state_s(n_threads + 1, product.BuildEvalState<double>());

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
                    auto pay_num = eval_state.VarVals().size() - 1;
                    for (size_t i = 0; i < pathsInTask; ++i) {
                        random->FillNormal(&gaussVec);
                        mdl->GeneratePath(gaussVec, &path);
                        product.EvaluateCompiled(path, eval_state);
                        sim_result += eval_state.VarVals()[pay_num];
                    }
                } else {
                    Evaluator_<double>& eval = eval_s[threadNum];
                    auto pay_num = eval.VarVals().size() - 1;
                    for (size_t i = 0; i < pathsInTask; ++i) {
                        random->FillNormal(&gaussVec);
                        mdl->GeneratePath(gaussVec, &path);
                        product.Evaluate(path, eval);
                        sim_result += eval.VarVals()[pay_num];
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
        const auto n_const_vars = product.ConstVarNames().size();
        SimResults_<AAD::Number_> results(Dal::Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));

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
            eval_state_s = Vector_<EvalState_<AAD::Number_>>(n_thread + 1, product.BuildEvalState<AAD::Number_>());
        else if (max_nested_ifs > 0)
            fuzzy_eval_s = Vector_<FuzzyEvaluator_<AAD::Number_>>(n_thread + 1, product.BuildFuzzyEvaluator<AAD::Number_>(max_nested_ifs, eps));
        else
            eval_s = Vector_<Evaluator_<AAD::Number_>>(n_thread + 1, product.BuildEvaluator<AAD::Number_>());

#ifndef USE_AADET
        const size_t batch_size = std::max(BATCH_SIZE, n_paths / (n_thread + 1) + 1);
#else
        const size_t batch_size = BATCH_SIZE;
#endif
        Vector_<TaskHandle_> futures;
        futures.reserve(n_paths / batch_size + 1);
        Vector_<> sim_results;
        sim_results.reserve(n_paths / batch_size + 1);

        Vector_<bool> model_init(n_thread + 1, false);
        Vector_<AAD::Position_> start_positions(n_thread + 1);
        Vector_<AAD::Tape_*> tapes(n_thread + 1, nullptr);

        size_t firstPath = 0;
        size_t pathsLeft = n_paths;
        size_t loop_i = 0;

        while (pathsLeft > 0) {
            auto pathsInTask = std::min(pathsLeft, batch_size);
            sim_results.emplace_back(0.0);
            auto& sim_result = sim_results[loop_i];
            loop_i += 1;
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t n_threads = ThreadPool_::ThreadNum();
                Scenario_<AAD::Number_>& path = paths[n_threads];

                auto& random = rng_s[n_threads];
                auto& gVec = gaussVectors[n_threads];
                auto& model = models[n_threads];
                random->SkipTo(firstPath);

                if (!tapes[n_threads])
                    tapes[n_threads] = &AAD::Number_::getTape();
                AAD::Tape_* tape = tapes[n_threads];

                auto& pos = start_positions[n_threads];

                double sum_val = 0.0;
                if (compiled) {
                    EvalState_<AAD::Number_>& eval_state = eval_state_s[n_threads];

                    //  Initialize once on each thread
                    if (!model_init[n_threads]) {
                        start_positions[n_threads] = InitModel4ParallelAAD(*tape, product, *model, path, eval_state);
                        model_init[n_threads] = true;
                    }

                    auto pay_num = eval_state.VarVals().size() - 1;
                    for (size_t i = 0; i < pathsInTask; i++) {
                        random->FillNormal(&gVec);
                        model->GeneratePath(gVec, &path);
                        product.EvaluateCompiled(path, eval_state);
                        AAD::Number_ res = eval_state.VarVals()[pay_num];
                        res.setGradient(1.0);
                        tape->evaluate(tape->getPosition(), pos);
                        sum_val += res.value();
                        tape->resetTo(pos, false);
                    }
                }
                else if (max_nested_ifs > 0) {
                    FuzzyEvaluator_<AAD::Number_>& eval = fuzzy_eval_s[n_threads];

                    //  Initialize once on each thread
                    if (!model_init[n_threads]) {
                        start_positions[n_threads] = InitModel4ParallelAAD(*tape, product, *model, path, eval);
                        model_init[n_threads] = true;
                    }

                    auto pay_num = eval.VarVals().size() - 1;
                    for (size_t i = 0; i < pathsInTask; i++) {
                        random->FillNormal(&gVec);
                        model->GeneratePath(gVec, &path);
                        product.Evaluate(path, eval);
                        AAD::Number_ res = eval.VarVals()[pay_num];
                        res.setGradient(1.0);
                        tape->evaluate(tape->getPosition(), pos);
                        sum_val += res.value();
                        tape->resetTo(pos, false);
                    }
                } else {
                    Evaluator_<AAD::Number_>& eval = eval_s[n_threads];

                    //  Initialize once on each thread
                    if (!model_init[n_threads]) {
                        start_positions[n_threads] = InitModel4ParallelAAD(*tape, product, *model, path, eval);
                        model_init[n_threads] = true;
                    }

                    auto pay_num = eval.VarVals().size() - 1;
                    for (size_t i = 0; i < pathsInTask; i++) {
                        random->FillNormal(&gVec);
                        model->GeneratePath(gVec, &path);
                        product.Evaluate(path, eval);
                        AAD::Number_ res = eval.VarVals()[pay_num];
                        res.setGradient(1.0);
                        tape->evaluate(tape->getPosition(), pos);
                        sum_val += res.value();
                        tape->resetTo(pos, false);
                    }
                }
                sim_result = sum_val;
#ifndef USE_AADET
                tape->evaluate(pos, tape->getZeroPosition());
                for (size_t j = 0; j < n_params; ++j)
                    results.risks_[j] += model->Parameters()[j]->getGradient() / static_cast<double>(n_paths);

                if (compiled) {
                    for (size_t j = 0; j < n_const_vars; ++j)
                        results.risks_[j + n_params] +=  eval_state_s[n_threads].ConstVarVals()[j].getGradient() / static_cast<double>(n_paths);
                }  else if (max_nested_ifs > 0) {
                    for (size_t j = 0; j < n_const_vars; ++j)
                        results.risks_[j + n_params] +=  fuzzy_eval_s[n_threads].ConstVarVals()[j].getGradient() / static_cast<double>(n_paths);
                } else {
                    for (size_t j = 0; j < n_const_vars; ++j)
                        results.risks_[j + n_params] +=  eval_s[n_threads].ConstVarVals()[j].getGradient() / static_cast<double>(n_paths);
                }
                tape->reset(false);
#endif
                return true;
            }));
            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);

        // aggregate all the results
        for (const auto& s: sim_results)
            results.aggregated_ += s;

#ifdef USE_AADET
        for (size_t i = 0; i < n_thread + 1; ++i)
            if (model_init[i])
                tapes[i]->evaluate();

        for (size_t j = 0; j < n_params; ++j)
            for (size_t i = 0; i < models.size(); ++i)
                if (model_init[i])
                    results.risks_[j] += models[i]->Parameters()[j]->getGradient() / static_cast<double>(n_paths);

        if (compiled) {
            for (size_t j = 0; j < n_const_vars; ++j)
                for (size_t i = 0; i < eval_state_s.size(); ++i)
                    if (model_init[i])
                        results.risks_[j + n_params] +=  eval_state_s[i].ConstVarVals()[j].getGradient() / static_cast<double>(n_paths);
        }  else if (max_nested_ifs > 0) {
            for (size_t j = 0; j < n_const_vars; ++j)
                for (size_t i = 0; i < fuzzy_eval_s.size(); ++i)
                    if (model_init[i])
                        results.risks_[j + n_params] +=  fuzzy_eval_s[i].ConstVarVals()[j].getGradient() / static_cast<double>(n_paths);
        } else {
            for (size_t j = 0; j < n_const_vars; ++j)
                for (size_t i = 0; i < eval_s.size(); ++i)
                    if (model_init[i])
                        results.risks_[j + n_params] +=  eval_s[i].ConstVarVals()[j].getGradient() / static_cast<double>(n_paths);
        }
        for (size_t i = 0; i < n_thread + 1; ++i)
            if (model_init[i])
                tapes[i]->reset();
#endif
        return results;
    }
}
