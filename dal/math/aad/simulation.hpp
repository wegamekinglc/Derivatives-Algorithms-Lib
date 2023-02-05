//
// Created by wegam on 2021/7/11.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/aad/products/base.hpp>
#include <dal/math/aad/models/base.hpp>
#include <dal/math/random/brownianbridge.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>

namespace Dal::AAD {
    /*
     * Template algorithms
     * check compatibility of model and products
     * At the moment, only check that assets are the same in both cases
     * may be easily extended in the future
     */

    template <class T_ = double> inline bool CheckCompatibility(const Product_<T_>& prd, const Model_<T_>& mdl) {
        return prd.AssetNames() == mdl.AssetNames();
    }

    Matrix_<> MCSimulation(const Product_<double>& prd,
                           const Model_<double>& mdl,
                           const String_& method,
                           int nPath,
                           bool use_bb = false);

    constexpr const size_t BATCH_SIZE = 8192;

    Matrix_<> MCParallelSimulation(const Product_<double>& prd,
                                   const Model_<double>& mdl,
                                   const String_& method,
                                   int nPath,
                                   bool use_bb = false);


    inline std::unique_ptr<Random_> CreateRNG(const String_& method, size_t n_dim, bool use_bb = false) {
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

    struct AADResults_ {
        AADResults_(int nPath, int nParam) : aggregated_(nPath), risks_(nParam) {}
        int Rows() const { return aggregated_.size();  }
        Vector_<> aggregated_;
        Vector_<> risks_;
    };

    inline AADResults_ MCSimulationAAD(const Product_<Number_>& prd,
                                const Model_<Number_>& mdl,
                                const String_& method,
                                int nPath,
                                bool use_bb = false) {
        REQUIRE(CheckCompatibility(prd, mdl), "model and products are not compatible");
        auto cMdl = mdl.Clone();

        Scenario_<Number_> path;
        AllocatePath(prd.DefLine(), path);
        cMdl->Allocate(prd.TimeLine(), prd.DefLine());
        std::unique_ptr<Random_> rng = CreateRNG(method, cMdl->SimDim(), use_bb);

        const size_t nPay = prd.PayoffLabels().size();
        const Vector_<Number_*>& params = cMdl->Parameters();
        const size_t nParam = params.size();

        AAD::Tape_* tape = &AAD::Number_::getTape();
        tape->reset();
        tape->setActive();
        cMdl->PutParametersOnTape(*tape);

        cMdl->Init(prd.TimeLine(), prd.DefLine());
        InitializePath(path);
        auto begin = tape->getPosition();

        Vector_<Number_> nPayoffs(nPay);
        Vector_<> gaussVec(cMdl->SimDim());
        AADResults_ results(nPath, nParam);

        for (size_t i = 0; i < nPath; i++) {
            tape->resetTo(begin);
            rng->FillNormal(&gaussVec);
            cMdl->GeneratePath(gaussVec, &path);
            prd.Payoffs(path, &nPayoffs);
            Number_ result = nPayoffs[0];
            result.setGradient(1.0);
            tape->evaluate(tape->getPosition(), begin);
            results.aggregated_[i] = result.value();
        }

        tape->evaluate(begin, tape->getZeroPosition());
        Transform(params, [nPath](const Number_* p) { return p->getGradient() / nPath; }, &results.risks_);
        tape->reset();
        return results;
    }

    typename AAD::Position_ InitModel4ParallelAAD(AAD::Tape_& tape, const Product_<Number_>& prd, Model_<Number_>& clonedMdl, Scenario_<Number_>& path);

    inline AADResults_ MCParallelSimulationAAD(const Product_<Number_>& prd,
                                        const Model_<Number_>& mdl,
                                        const String_& method,
                                        int nPath,
                                        bool use_bb = false) {
        REQUIRE(CheckCompatibility(prd, mdl), "model and products are not compatible");

        const size_t nPay = prd.PayoffLabels().size();
        const size_t nParam = mdl.NumParams();

        AADResults_ results(nPath, nParam);

        AAD::Tape_* tape = &AAD::Number_::getTape();
        tape->reset();

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThread = pool->NumThreads();

        Vector_<std::unique_ptr<Model_<Number_>>> models(nThread + 1);
        for (auto& model : models) {
            model = mdl.Clone();
            model->Allocate(prd.TimeLine(), prd.DefLine());
        }
        std::unique_ptr<Random_> rng = CreateRNG(method, models[0]->SimDim(), use_bb);

        Vector_<Scenario_<Number_>> paths(nThread + 1);
        for (auto& path : paths) {
            AllocatePath(prd.DefLine(), path);
        }

        Vector_<Vector_<Number_>> payoffs(nThread + 1, Vector_<Number_>(nPay));

        Vector_<std::unique_ptr<Random_>> rngs(nThread + 1);
        for (auto& random : rngs)
            random.reset(rng->Clone());

        Vector_<Vector_<double>> gaussVecs(nThread + 1, Vector_<double>(models[0]->SimDim()));

        Vector_<TaskHandle_> futures;
        futures.reserve(nPath / BATCH_SIZE + 1);

        size_t firstPath = 0;
        size_t pathsLeft = nPath;
        while (pathsLeft > 0) {
            size_t pathsInTask = std::min(pathsLeft, BATCH_SIZE);
            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = pool->ThreadNum();
                AAD::Tape_* tape = &AAD::Number_::getTape();

                //  Initialize once on each thread
                auto pos = InitModel4ParallelAAD(*tape, prd, *models[threadNum], paths[threadNum]);
                auto& random = rngs[threadNum];
                random->SkipTo(firstPath);
                for (size_t i = 0; i < pathsInTask; ++i) {
                    random->FillNormal(&gaussVecs[threadNum]);
                    models[threadNum]->GeneratePath(gaussVecs[threadNum], &paths[threadNum]);
                    prd.Payoffs(paths[threadNum], &payoffs[threadNum]);
                    Number_ result = payoffs[threadNum][0];
                    result.setGradient(1.0);
                    tape->evaluate(tape->getPosition(), pos);
                    results.aggregated_[firstPath + i] = result.value();
                    tape->resetTo(pos);
                }
                tape->evaluate(pos, tape->getZeroPosition());
                for (size_t j = 0; j < nParam; ++j)
                    results.risks_[j] += models[threadNum]->Parameters()[j]->getGradient();
                tape->reset();
                return true;
            }));

            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }
        for (auto& future : futures)
            pool->ActiveWait(future);

        for (size_t j = 0; j < nParam; ++j)
            results.risks_[j] /= nPath;
        tape->reset();
        return results;
    }

} // namespace Dal::AAD