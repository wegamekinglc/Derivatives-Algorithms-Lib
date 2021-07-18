//
// Created by wegam on 2021/7/11.
//

#include <dal/platform/strict.hpp>
#include <dal/math/aad/mcbase.hpp>

namespace Dal {
    Time_  SYSTEM_TIME = 0.0;

    namespace {
        template <class T_>
        void PutParametersOnTapeT(const Vector_<T_*>&) {}

        template <>
        void PutParametersOnTapeT<Number_>(const Vector_<Number_*>& parameters) {
            for (Number_* param : parameters)
                param->PutOnTape();
        }
    }

    template <class T_>
    void Model_<T_>::PutParametersOnTape() {
        PutParametersOnTapeT<T_>(Parameters());
    }

    Vector_<Vector_<>> MCSimulation(
        const Product_<double>& prd,
        const Model_<double>& mdl,
        const RNG_& rng,
        const size_t& nPath
    ) {
        REQUIRE(CheckCompatibility(prd, mdl), "model and product are not compatible");
        auto cMdl = mdl.Clone();
        auto cRng = rng.Clone();

        const size_t nPay = prd.PayoffLabels().size();
        Vector_<Vector_<>> results(nPath, Vector_<>(nPay));

        cMdl->Allocate(prd.TimeLine(), prd.DefLine());
        cMdl->Init(prd.TimeLine(), prd.DefLine());
        cRng->Init(cMdl->SimDim());
        Vector_<> gaussVec(cMdl->SimDim());
        Scenario_<double> path;
        AllocatePath(prd.DefLine(), path);
        InitializePath(path);

        for (size_t i =0; i < nPath; ++i) {
            cRng->NextG(gaussVec);
            cMdl->GeneratePath(gaussVec, path);
            prd.Payoffs(path, results[i]);
        }
        return results;
    }

    Vector_<Vector_<>> MCParallelSimulation(
        const Product_<double>& prd,
        const Model_<double>& mdl,
        const RNG_& rng,
        const size_t& nPath
    ) {
        REQUIRE(CheckCompatibility(prd, mdl), "model and product are not compatible");
        auto cMdl = mdl.Clone();

        const size_t nPay = prd.PayoffLabels().size();
        Vector_<Vector_<>> results(nPath, Vector_<>(nPay));

        cMdl->Allocate(prd.TimeLine(), prd.DefLine());
        cMdl->Init(prd.TimeLine(), prd.DefLine());
    }
}