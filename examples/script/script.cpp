//
// Created by wegam on 2020/12/21.
//

#include <iostream>
#include <dal/script/event.hpp>
#include <dal/math/aad/models/blackscholes.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/random/brownianbridge.hpp>
#include <dal/storage/globals.hpp>
#include <dal/utilities/timer.hpp>
#include <iomanip>

using namespace std;
using namespace Dal;
using namespace Dal::Script;
using namespace Dal::AAD;


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


int main() {

    XGLOBAL::SetEvaluationDate(Date_(2022, 9, 25));
    using Real_ = double;
    const double spot = 100.0;
    const double vol = 0.15;
    const double rate = 0.0;
    const double div = 0.0;
    const double strike = 120.0;
    const Date_ maturity(2025, 9, 25);

    int n;
    std::cout << "Plz input # of paths (power of 2):";
    std::cin >> n;
    const int n_paths = Pow(2, n);

    const String_ method = "sobol";
    bool use_bb = false;

    ScriptProduct_ product;
    std::map<Date_, String_> events;


    events[maturity] = String_("call pays MAX(spot() - 120.0, 0.0)");

    bool fuzzy = false;
    bool skipDoms = false;

    product.ParseEvents(events.begin(), events.end());
    int maxNestedIfs = product.PreProcess(fuzzy, skipDoms);

    unique_ptr<Scenario_<Real_>> scenario = product.BuildScenario<Real_>();
    unique_ptr<Evaluator_<Real_>> eval = product.BuildEvaluator<Real_>();

    std::unique_ptr<Model_<>> mdl = std::make_unique<BlackScholes_<>>(spot, vol, false, rate, div);
    Matrix_<> results(n_paths, eval->VarVals().size());

    Timer_ timer;

    mdl->Allocate(product.TimeLine(), product.DefLine());
    mdl->Init(product.TimeLine(), product.DefLine());
    std::unique_ptr<Random_> rng = CreateRNG(method, mdl->SimDim(), use_bb);

    Vector_<> gaussVec(mdl->SimDim());
    Scenario_<> path;
    AllocatePath(product.DefLine(), path);
    InitializePath(path);

    for (size_t i = 0; i < n_paths; ++i) {
        rng->FillNormal(&gaussVec);
        mdl->GeneratePath(gaussVec, &path);
        product.Evaluate(path, *eval);
        auto res = results[i];
        for (int k = 0; k != eval->VarVals().size(); ++k)
            res[k] = eval->VarVals()[k];
    }


    auto sum = 0.0;
    for (auto row = 0; row < results.Rows(); ++row)
        sum += results(row, 0);
    auto calculated = sum / static_cast<double>(results.Rows());
    std::cout << "\nEuropean       w. B-S: price " << std::setprecision(8) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;

    return 0;
}