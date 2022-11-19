//
// Created by wegam on 2022/11/6.
//

#include <dal/platform/strict.hpp>
#include <dal/script/simulation.hpp>

namespace Dal::Script {
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


    SimResults_<> MCSimulation(const ScriptProduct_& product,
                               const AAD::Model_<double>& model,
                               int n_paths,
                               const String_& rsg,
                               bool use_bb) {
        auto mdl = model.Clone();
        std::unique_ptr<Scenario_<double>> scenario = product.BuildScenario<double>();
        std::unique_ptr<Evaluator_<double>> eval = product.BuildEvaluator<double>();

        Scenario_<double> path;
        AllocatePath(product.DefLine(), path);
        mdl->Allocate(product.TimeLine(), product.DefLine());
        std::unique_ptr<Random_> rng = CreateRNG(rsg, mdl->SimDim(), use_bb);

        SimResults_<double> results(n_paths);
        mdl->Init(product.TimeLine(), product.DefLine());
        InitializePath(path);

        Vector_<> gaussVec(mdl->SimDim());

        for (size_t i = 0; i < n_paths; ++i) {
            rng->FillNormal(&gaussVec);
            mdl->GeneratePath(gaussVec, &path);
            product.Evaluate(path, *eval);
            results.aggregated_[i] = eval->VarVals()[eval->VarVals().size() - 1];
        }
        return results;
    }


    SimResults_<AAD::Number_> MCSimulation(const ScriptProduct_& product,
                                           const AAD::Model_<AAD::Number_>& model,
                                           int n_paths,
                                           const String_& rsg,
                                           bool use_bb) {
        auto mdl = model.Clone();
        std::unique_ptr<Scenario_<AAD::Number_>> scenario = product.BuildScenario<AAD::Number_>();
        std::unique_ptr<Evaluator_<AAD::Number_>> eval = product.BuildEvaluator<AAD::Number_>();

        Scenario_<AAD::Number_> path;
        AllocatePath(product.DefLine(), path);
        mdl->Allocate(product.TimeLine(), product.DefLine());
        std::unique_ptr<Random_> rng = CreateRNG(rsg, mdl->SimDim(), use_bb);

        const Vector_<AAD::Number_*>& params = mdl->Parameters();
        const size_t nParam = params.size();
        SimResults_<AAD::Number_> results(n_paths, nParam);

        AAD::Tape_& tape = *AAD::Number_::tape_;
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
            AAD::Number_ res = eval->VarVals()[eval->VarVals().size() - 1];
            res.PropagateToMark();
            results.aggregated_[i] = res.Value();
        }

        AAD::Number_::PropagateMarkToStart();
        Transform(params, [n_paths](const AAD::Number_* p) { return p->Adjoint() / n_paths; }, &results.risks_);
        tape.Clear();
        return results;
    }
}
