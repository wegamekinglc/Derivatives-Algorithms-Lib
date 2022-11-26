//
// Created by wegam on 2022/11/6.
//

#include <dal/concurrency/threadpool.hpp>
#include <dal/script/simulation.hpp>
#include <dal/platform/strict.hpp>


namespace Dal::Script {

    void InitModel4ParallelAAD(const ScriptProduct_& prd, AAD::Model_<AAD::Number_>& clonedMdl, Scenario_<AAD::Number_>& path) {
        AAD::Tape_& tape = *AAD::Number_::tape_;
        tape.Rewind();
        clonedMdl.PutParametersOnTape();
        clonedMdl.Init(prd.TimeLine(), prd.DefLine());
        InitializePath(path);
        tape.Mark();
    }

    std::unique_ptr<Random_> CreateRNG(const String_& method, size_t n_dim, bool use_bb) {
        std::unique_ptr<Random_> rsg;
        if (method == "sobol")
            rsg = std::unique_ptr<Random_>(NewSobol(n_dim, 2048));
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

    void InitModel4ParallelAAD(const ScriptProduct_& prd, AAD::Model_<AAD::Number_>& clonedMdl, Scenario_<AAD::Number_>& path);

    SimResults_<> MCSimulation(const ScriptProduct_& product,
                               const AAD::Model_<double>& model,
                               int n_paths,
                               const String_& rsg,
                               bool use_bb) {
        auto mdl = model.Clone();

        mdl->Allocate(product.TimeLine(), product.DefLine());
        mdl->Init(product.TimeLine(), product.DefLine());

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThread = pool->NumThreads();

        Vector_<std::unique_ptr<Random_>> rng_s(nThread + 1);
        for (auto& random : rng_s)
            random = CreateRNG(rsg, mdl->SimDim(), use_bb);

        Vector_<Vector_<>> gaussVecs(nThread + 1);
        Vector_<Scenario_<>> paths(nThread + 1);
        Vector_<std::unique_ptr<Evaluator_<double>>> eval_s(nThread + 1);

        for (auto& vec : gaussVecs)
            vec.Resize(mdl->SimDim());

        for (auto& path : paths) {
            AllocatePath(product.DefLine(), path);
            InitializePath(path);
        }

        for (auto& eval : eval_s)
            eval = product.BuildEvaluator<double>();

        SimResults_<double> results(n_paths);

        Vector_<TaskHandle_> futures;
        futures.reserve(n_paths / BATCH_SIZE + 1);

        int firstPath = 0;
        int pathsLeft = n_paths;

        while (pathsLeft > 0) {
            int pathsInTask = Min(pathsLeft, BATCH_SIZE);
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = pool->ThreadNum();
                Vector_<>& gaussVec = gaussVecs[threadNum];
                Scenario_<>& path = paths[threadNum];
                std::unique_ptr<Evaluator_<double>>& eval = eval_s[threadNum];

                auto& random = rng_s[threadNum];
                random->SkipTo(firstPath);

                for (size_t i = 0; i < pathsInTask; ++i) {
                    random->FillNormal(&gaussVec);
                    mdl->GeneratePath(gaussVec, &path);
                    product.Evaluate(path, *eval);
                    results.aggregated_[firstPath + i] = eval->VarVals()[eval->VarVals().size() - 1];
                }
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
                                           int n_paths,
                                           const String_& rsg,
                                           bool use_bb,
                                           int max_nested_ifs,
                                           double eps) {
        auto mdl = model.Clone();
        mdl->Allocate(product.TimeLine(), product.DefLine());
        std::unique_ptr<Random_> rng = CreateRNG(rsg, mdl->SimDim(), use_bb);

        const Vector_<AAD::Number_*>& params = mdl->Parameters();
        const size_t nParam = params.size();
        SimResults_<AAD::Number_> results(n_paths, nParam);

        AAD::Tape_& tape = *AAD::Number_::tape_;
        tape.Clear();

        auto re_setter = AAD::SetNumResultsForAAD();
        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThread = pool->NumThreads();

        Vector_<std::unique_ptr<AAD::Model_<AAD::Number_>>> models(nThread + 1);
        for (auto& model : models) {
            model = mdl->Clone();
            model->Allocate(product.TimeLine(), product.DefLine());
        }

        Vector_<std::unique_ptr<Random_>> rng_s(nThread + 1);
        for (auto& random : rng_s)
            random = CreateRNG(rsg, mdl->SimDim(), use_bb);

        Vector_<Vector_<>> gaussVecs(nThread + 1);
        Vector_<Scenario_<AAD::Number_>> paths(nThread + 1);
        Vector_<std::unique_ptr<Evaluator_<AAD::Number_>>> eval_s(nThread + 1);

        for (auto& vec : gaussVecs)
            vec.Resize(mdl->SimDim());

        for (auto& path : paths) {
            AllocatePath(product.DefLine(), path);
            InitializePath(path);
        }

        for (auto& eval : eval_s) {
            if (max_nested_ifs >= 0)
                eval = product.BuildFuzzyEvaluator<AAD::Number_>(max_nested_ifs, eps);
            else
                eval = product.BuildEvaluator<AAD::Number_>();
        }

        Vector_<AAD::Tape_> tapes(nThread);
        Vector_<bool> mdlInit(nThread + 1, false);
        InitModel4ParallelAAD(product, *models[0], paths[0]);

        mdlInit[0] = true;

        Vector_<TaskHandle_> futures;
        futures.reserve(n_paths / BATCH_SIZE + 1);

        int firstPath = 0;
        int pathsLeft = n_paths;

        while (pathsLeft > 0) {
            int pathsInTask = Min(pathsLeft, BATCH_SIZE);
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = pool->ThreadNum();
                if (threadNum > 0)
                    AAD::Number_::tape_ = &tapes[threadNum - 1];

                //  Initialize once on each thread
                if (!mdlInit[threadNum]) {
                    InitModel4ParallelAAD(product, *models[threadNum], paths[threadNum]);
                    mdlInit[threadNum] = true;
                }

                Scenario_<AAD::Number_>& path = paths[threadNum];

                auto& random = rng_s[threadNum];
                random->SkipTo(firstPath);
                std::unique_ptr<Evaluator_<AAD::Number_>>& eval = eval_s[threadNum];

                for (size_t i = 0; i < pathsInTask; i++) {
                    AAD::Number_::tape_->RewindToMark();
                    random->FillNormal(&gaussVecs[threadNum]);
                    models[threadNum]->GeneratePath(gaussVecs[threadNum], &paths[threadNum]);
                    product.Evaluate(path, *eval);
                    AAD::Number_ res = eval->VarVals()[eval->VarVals().size() - 1];
                    res.PropagateToMark();
                    results.aggregated_[firstPath + i] = res.Value();
                }
                return true;
            }));

            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);

        AAD::Number_::PropagateMarkToStart();
        AAD::Tape_* mainThreadPtr = AAD::Number_::tape_;
        for (size_t i = 0; i < nThread; ++i) {
            if (mdlInit[i + 1]) {
                AAD::Number_::tape_ = &tapes[i];
                AAD::Number_::PropagateMarkToStart();
            }
        }
        AAD::Number_::tape_ = mainThreadPtr;

        for (size_t j = 0; j < nParam; ++j) {
            results.risks_[j] = 0.0;
            for (size_t i = 0; i < models.size(); ++i) {
                if (mdlInit[i])
                    results.risks_[j] += models[i]->Parameters()[j]->Adjoint();
            }
            results.risks_[j] /= n_paths;
        }
        AAD::Number_::tape_->Clear();
        return results;
    }
}
