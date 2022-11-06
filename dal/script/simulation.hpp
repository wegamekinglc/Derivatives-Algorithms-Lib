//
// Created by wegam on 2022/11/6.
//

#pragma once

#include <dal/script/event.hpp>
#include <dal/math/aad/models/base.hpp>
#include <dal/math/random/brownianbridge.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/random/pseudorandom.hpp>


namespace Dal::Script {

    std::unique_ptr<Random_> CreateRNG(const String_& method, size_t n_dim, bool use_bb = false);


    template <class T_ = double>
    struct SimResults_ {
        SimResults_(int nPath, int nParam) : aggregated_(nPath), risks_(nParam) {}
        [[nodiscard]] int Rows() const { return aggregated_.size();  }
        Vector_<> aggregated_;
        Vector_<> risks_;
    };


    template <class T_>
    SimResults_<T_> MCSimulation(const ScriptProduct_& product,
                                 const AAD::Model_<T_>& model,
                                 int n_paths,
                                 const String_& rsg = "sobol",
                                 bool use_bb = false) {
        auto mdl = model.Clone();
        std::unique_ptr<Scenario_<T_>> scenario = product.BuildScenario<T_>();
        std::unique_ptr<Evaluator_<T_>> eval = product.BuildEvaluator<T_>();

        Scenario_<T_> path;
        AllocatePath(product.DefLine(), path);
        mdl->Allocate(product.TimeLine(), product.DefLine());
        std::unique_ptr<Random_> rng = CreateRNG(rsg, mdl->SimDim(), use_bb);

        const Vector_<T_*>& params = mdl->Parameters();
        const size_t nParam = params.size();
        SimResults_<T_> results(n_paths, nParam);

        AAD::Tape_& tape = *T_::tape_;
        auto re_setter = AAD::SetNumResultsForAAD();
        tape.Clear();
        mdl->PutParametersOnTape();
        mdl->Init(product.TimeLine(), product.DefLine());
        InitializePath(path);
        tape.Mark();

        Vector_<> gaussVec(mdl->SimDim());

        for (size_t i = 0; i < n_paths; ++i) {
            tape.RewindToMark();
            rng->FillNormal(&gaussVec);
            mdl->GeneratePath(gaussVec, &path);
            product.Evaluate(path, *eval);
            T_ res = eval->VarVals()[eval->VarVals().size() - 1];
            res.PropagateToMark();
            results.aggregated_[i] = res.Value();
        }

        T_::PropagateMarkToStart();
        Transform(params, [n_paths](const T_* p) { return p->Adjoint() / n_paths; }, &results.risks_);
        tape.Clear();
        return results;
    }
}
