//
// Created by wegam on 2021/7/11.
//

#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/models/base.hpp>
#include <dal/math/aad/products/base.hpp>
#include <dal/math/aad/simulation.hpp>
#include <dal/platform/strict.hpp>

namespace Dal::AAD {

    Matrix_<> MCSimulation(const Product_<>& prd, const Model_<>& mdl, const String_& method, int nPath, bool use_bb) {
        REQUIRE(CheckCompatibility(prd, mdl), "model and products are not compatible");
        auto cMdl = mdl.Clone();

        const size_t nPay = prd.PayoffLabels().size();
        Matrix_<> results(nPath, nPay);

        cMdl->Allocate(prd.TimeLine(), prd.DefLine());
        cMdl->Init(prd.TimeLine(), prd.DefLine());
        std::unique_ptr<Random_> rng = CreateRNG(method, cMdl->SimDim(), use_bb);

        Vector_<> gaussVec(cMdl->SimDim());
        Scenario_<> path;
        AllocatePath(prd.DefLine(), path);
        InitializePath(path);

        for (size_t i = 0; i < nPath; ++i) {
            rng->FillNormal(&gaussVec);
            cMdl->GeneratePath(gaussVec, &path);
            auto res = results[i];
            prd.Payoffs(path, &res);
        }
        return results;
    }


    Matrix_<> MCParallelSimulation(const Product_<>& prd,
                                   const Model_<>& mdl,
                                   const String_& method,
                                   int nPath,
                                   bool use_bb) {
        REQUIRE(CheckCompatibility(prd, mdl), "model and products are not compatible");
        auto cMdl = mdl.Clone();

        const size_t nPay = prd.PayoffLabels().size();
        Matrix_<> results(nPath, nPay);

        cMdl->Allocate(prd.TimeLine(), prd.DefLine());
        cMdl->Init(prd.TimeLine(), prd.DefLine());
        std::unique_ptr<Random_> rng = CreateRNG(method, cMdl->SimDim(), use_bb);

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThread = pool->NumThreads();
        Vector_<Vector_<>> gaussVecs(nThread + 1);
        Vector_<Scenario_<>> paths(nThread + 1);

        for (auto& vec : gaussVecs)
            vec.Resize(cMdl->SimDim());
        for (auto& path : paths) {
            AllocatePath(prd.DefLine(), path);
            InitializePath(path);
        }

        Vector_<std::unique_ptr<Random_>> rng_s(nThread + 1);
        for (auto& random : rng_s)
            random = std::unique_ptr<Random_>(rng->Clone());

        Vector_<TaskHandle_> futures;
        futures.reserve(nPath / BATCH_SIZE + 1);

        size_t firstPath = 0;
        size_t pathsLeft = nPath;

        while (pathsLeft > 0) {
            size_t pathsInTask = std::min<size_t>(pathsLeft, BATCH_SIZE);
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = pool->ThreadNum();
                Vector_<>& gaussVec = gaussVecs[threadNum];
                Scenario_<>& path = paths[threadNum];

                auto& random = rng_s[threadNum];
                random->SkipTo(firstPath);

                for (size_t i = 0; i < pathsInTask; ++i) {
                    random->FillNormal(&gaussVec);
                    cMdl->GeneratePath(gaussVec, &path);
                    auto res = results[firstPath + i];
                    prd.Payoffs(path, &res);
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

    void InitModel4ParallelAAD(const Product_<Number_>& prd, Model_<Number_>& clonedMdl, Scenario_<Number_>& path) {
        Tape_& tape = *Number_::tape_;
        tape.Rewind();
        clonedMdl.PutParametersOnTape();
        clonedMdl.Init(prd.TimeLine(), prd.DefLine());
        InitializePath(path);
        tape.Mark();
    }

} // namespace Dal::AAD