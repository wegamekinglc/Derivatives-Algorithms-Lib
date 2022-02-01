//
// Created by wegam on 2021/7/11.
//

#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/models/base.hpp>
#include <dal/math/aad/products/base.hpp>
#include <dal/math/aad/simulation.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {

    Matrix_<> MCSimulation(const Product_<>& prd, const Model_<>& mdl, const std::unique_ptr<Random_>& rng, int nPath) {
        REQUIRE(CheckCompatibility(prd, mdl), "model and products are not compatible");
        auto cMdl = mdl.Clone();

        const size_t nPay = prd.PayoffLabels().size();
        Matrix_<> results(nPath, nPay);

        cMdl->Allocate(prd.TimeLine(), prd.DefLine());
        cMdl->Init(prd.TimeLine(), prd.DefLine());
        Vector_<> gaussVec(cMdl->SimDim());
        Scenario_<> path;
        AllocatePath(prd.DefLine(), path);
        InitializePath(path);

        for (size_t i = 0; i < nPath; ++i) {
            rng->FillNormal(&gaussVec);
            cMdl->GeneratePath(gaussVec, &path);
            prd.Payoffs(path, results[i]);
        }
        return results;
    }

    constexpr const int BATCH_SIZE = 65536;

    Matrix_<> MCParallelSimulation(const Product_<>& prd,
                                   const Model_<>& mdl,
                                   const std::unique_ptr<PseudoRandom_>& rng,
                                   int nPath) {
        REQUIRE(CheckCompatibility(prd, mdl), "model and products are not compatible");
        auto cMdl = mdl.Clone();

        const size_t nPay = prd.PayoffLabels().size();
        Matrix_<> results(nPath, nPay);

        cMdl->Allocate(prd.TimeLine(), prd.DefLine());
        cMdl->Init(prd.TimeLine(), prd.DefLine());

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

        Vector_<std::unique_ptr<PseudoRandom_>> rng_s(nThread + 1);
        for (auto& random : rng_s)
            random = std::unique_ptr<PseudoRandom_>(rng->Clone());

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
                random->SkipTo(firstPath * nPay);

                for (size_t i = 0; i < pathsInTask; ++i) {
                    random->FillNormal(&gaussVec);
                    cMdl->GeneratePath(gaussVec, &path);
                    prd.Payoffs(path, results[firstPath + i]);
                }
                return true;
            }));
            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWaite(future);
        return results;
    }
} // namespace Dal