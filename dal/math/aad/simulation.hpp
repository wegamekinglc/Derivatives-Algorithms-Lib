//
// Created by wegam on 2021/7/11.
//

#pragma once

#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/aad/operators.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/random/pseudorandom.hpp>


namespace Dal::AAD {
    /*
     * random number generators
     */

    template <class T_> class Product_;

    template <class T_> class Model_;

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
                           int nPath);

    constexpr const int BATCH_SIZE = 65536;

    Matrix_<> MCParallelSimulation(const Product_<double>& prd,
                                   const Model_<double>& mdl,
                                   const String_& method,
                                   int nPath);


    inline std::unique_ptr<Random_> CreateRNG(const String_& method, size_t n_dim) {
        if (method == "sobol")
            return std::unique_ptr<Random_>(NewSobol(n_dim, 2048));
        else if (method == "mrg32")
            return std::unique_ptr<Random_>(New(RNGType_("MRG32"), 1024, n_dim));
        else if (method == "irn")
            return std::unique_ptr<Random_>(New(RNGType_("IRN"), 1024, n_dim));
        else
            THROW("rng method is not known");
    }

    struct AADResults_ {
        AADResults_(int nPath, int nPay, int nParam) : payoffs_(nPath, nPay), aggregated_(nPath), risks_(nParam) {}
        Matrix_<> payoffs_;
        Vector_<> aggregated_;
        Vector_<> risks_;
    };

    const auto DEFAULT_AGGREGATOR = [](const Vector_<Number_>& v) { return v[0]; };

    template <class F_ = decltype(DEFAULT_AGGREGATOR)>
    AADResults_ MCSimulationAAD(const Product_<Number_>& prd,
                                const Model_<Number_>& mdl,
                                const String_& method,
                                int nPath,
                                const F_& aggFun = DEFAULT_AGGREGATOR) {
        REQUIRE(CheckCompatibility(prd, mdl), "model and products are not compatible");
        auto cMdl = mdl.Clone();

        Scenario_<Number_> path;
        AllocatePath(prd.DefLine(), path);
        cMdl->Allocate(prd.TimeLine(), prd.DefLine());
        std::unique_ptr<Random_> rng = CreateRNG(method, cMdl->SimDim());

        const size_t nPay = prd.PayoffLabels().size();
        const Vector_<Number_*>& params = cMdl->Parameters();
        const size_t nParam = params.size();

        Tape_& tape = *Number_::tape_;
        tape.Clear();
        auto re_setter = SetNumResultsForAAD();
        cMdl->PutParametersOnTape();
        cMdl->Init(prd.TimeLine(), prd.DefLine());
        InitializePath(path);
        tape.Mark();

        Vector_<Number_> nPayoffs(nPay);
        Vector_<> gaussVec(cMdl->SimDim());
        AADResults_ results(nPath, nPay, nParam);

        for (size_t i = 0; i < nPath; i++) {
            tape.RewindToMark();
            rng->FillNormal(&gaussVec);
            cMdl->GeneratePath(gaussVec, &path);
            prd.Payoffs(path, &nPayoffs);
            Number_ result = aggFun(nPayoffs);

            result.PropagateToMark();
            results.aggregated_[i] = result.Value();
            ConvertCollection(nPayoffs.begin(), nPayoffs.end(), results.payoffs_[i].begin());
        }

        Number_::PropagateMarkToStart();
        Transform(
            params, [nPath](const Number_* p) { return p->Adjoint() / nPath; }, &results.risks_);
        tape.Clear();
        return results;
    }

    void InitModel4ParallelAAD(const Product_<Number_>& prd, Model_<Number_>& clonedMdl, Scenario_<Number_>& path);

    template <class F_ = decltype(DEFAULT_AGGREGATOR)>
    AADResults_ MCParallelSimulationAAD(const Product_<Number_>& prd,
                                        const Model_<Number_>& mdl,
                                        const String_& method,
                                        int nPath,
                                        const F_& aggFun = DEFAULT_AGGREGATOR) {
        REQUIRE(CheckCompatibility(prd, mdl), "model and products are not compatible");

        const size_t nPay = prd.PayoffLabels().size();
        const size_t nParam = mdl.NumParams();

        AADResults_ results(nPath, nPay, nParam);

        Number_::tape_->Clear();
        auto re_setter = SetNumResultsForAAD();

        ThreadPool_* pool = ThreadPool_::GetInstance();
        const size_t nThread = pool->NumThreads();

        Vector_<std::unique_ptr<Model_<Number_>>> models(nThread + 1);
        for (auto& model : models) {
            model = mdl.Clone();
            model->Allocate(prd.TimeLine(), prd.DefLine());
        }
        std::unique_ptr<Random_> rng = CreateRNG(method, models[0]->SimDim());

        Vector_<Scenario_<Number_>> paths(nThread + 1);
        for (auto& path : paths) {
            AllocatePath(prd.DefLine(), path);
        }

        Vector_<Vector_<Number_>> payoffs(nThread + 1, Vector_<Number_>(nPay));

        Vector_<Tape_> tapes(nThread);
        Vector_<bool> mdlInit(nThread + 1, false);

        InitModel4ParallelAAD(prd, *models[0], paths[0]);

        mdlInit[0] = true;

        Vector_<std::unique_ptr<Random_>> rngs(nThread + 1);
        for (auto& random : rngs)
            random.reset(rng->Clone());

        Vector_<Vector_<double>> gaussVecs(nThread + 1, Vector_<double>(models[0]->SimDim()));

        Vector_<TaskHandle_> futures;
        futures.reserve(nPath /  + 1);

        size_t firstPath = 0;
        size_t pathsLeft = nPath;
        while (pathsLeft > 0) {
            size_t pathsInTask = Min<size_t>(pathsLeft, BATCH_SIZE);

            futures.push_back(pool->SpawnTask([&, firstPath, pathsInTask]() {
                const size_t threadNum = pool->ThreadNum();
                if (threadNum > 0)
                    Number_::tape_ = &tapes[threadNum - 1];

                //  Initialize once on each thread
                if (!mdlInit[threadNum]) {
                    InitModel4ParallelAAD(prd, *models[threadNum], paths[threadNum]);
                    mdlInit[threadNum] = true;
                }

                auto& random = rngs[threadNum];
                random->SkipTo(firstPath);

                for (size_t i = 0; i < pathsInTask; i++) {
                    Number_::tape_->RewindToMark();
                    random->FillNormal(&gaussVecs[threadNum]);
                    models[threadNum]->GeneratePath(gaussVecs[threadNum], &paths[threadNum]);
                    prd.Payoffs(paths[threadNum], &payoffs[threadNum]);

                    Number_ result = aggFun(payoffs[threadNum]);
                    result.PropagateToMark();
                    results.aggregated_[firstPath + i] = result.Value();
                    ConvertCollection(payoffs[threadNum].begin(), payoffs[threadNum].end(), results.payoffs_[firstPath + i].begin());
                }
                return true;
            }));

            pathsLeft -= pathsInTask;
            firstPath += pathsInTask;
        }

        for (auto& future : futures)
            pool->ActiveWait(future);

        Number_::PropagateMarkToStart();
        Tape_* mainThreadPtr = Number_::tape_;
        for (size_t i = 0; i < nThread; ++i) {
            if (mdlInit[i + 1]) {
                Number_::tape_ = &tapes[i];
                Number_::PropagateMarkToStart();
            }
        }
        Number_::tape_ = mainThreadPtr;

        for (size_t j = 0; j < nParam; ++j) {
            results.risks_[j] = 0.0;
            for (size_t i = 0; i < models.size(); ++i) {
                if (mdlInit[i])
                    results.risks_[j] += models[i]->Parameters()[j]->Adjoint();
            }
            results.risks_[j] /= nPath;
        }
        Number_::tape_->Clear();
        return results;
    }

} // namespace Dal::AAD