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

struct AADResults_ {
    AADResults_(int nPath, int nParam) : aggregated_(nPath), risks_(nParam) {}
    int Rows() const { return aggregated_.size();  }
    Vector_<> aggregated_;
    Vector_<> risks_;
};


const auto DEFAULT_AGGREGATOR = [](const Vector_<Number_>& v) { return v[0]; };


int main() {

    XGLOBAL::SetEvaluationDate(Date_(2022, 9, 25));
    Timer_ timer;
    using Real_ = Number_;
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

    bool use_parallel = true;
    std::cout << "Use parallel?:";
    std::cin >> use_parallel;

    bool use_bb = false;
    std::cout << "Use brownian bridge?:";
    std::cin >> use_bb;

    std::string rsg_type = "sobol";
    std::cout << "Type of rsg:";
    std::cin >> rsg_type;

    ScriptProduct_ product;
    std::map<Date_, String_> events;


    events[maturity] = String_("call pays MAX(spot() - 120.0, 0.0)");

    bool fuzzy = false;
    bool skipDoms = false;

    product.ParseEvents(events.begin(), events.end());
    int maxNestedIfs = product.PreProcess(fuzzy, skipDoms);

    unique_ptr<Scenario_<Real_>> scenario = product.BuildScenario<Real_>();
    unique_ptr<Evaluator_<Real_>> eval = product.BuildEvaluator<Real_>();


    std::unique_ptr<Model_<Real_>> mdl = std::make_unique<BlackScholes_<Real_>>(spot, vol, false, rate, div);

    timer.Reset();
    Scenario_<Real_> path;
    AllocatePath(product.DefLine(), path);
    mdl->Allocate(product.TimeLine(), product.DefLine());
    std::unique_ptr<Random_> rng = CreateRNG(String_(rsg_type), mdl->SimDim(), use_bb);

    const Vector_<Number_*>& params = mdl->Parameters();
    const size_t nParam = params.size();
    AADResults_ results(n_paths, nParam);

    Tape_& tape = *Number_::tape_;
    auto re_setter = SetNumResultsForAAD();
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
        Number_ res = DEFAULT_AGGREGATOR(eval->VarVals());
        res.PropagateToMark();
        results.aggregated_[i] = res.Value();
    }

    Number_::PropagateMarkToStart();
    Transform(params, [n_paths](const Number_* p) { return p->Adjoint() / n_paths; }, &results.risks_);
    tape.Clear();

    auto sum = 0.0;
    for (auto row = 0; row < results.Rows(); ++row)
        sum += results.aggregated_[row];
    auto calculated = sum / static_cast<double>(results.Rows());
    std::cout << "\nEuropean       w. B-S: price " << std::setprecision(8) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    std::cout << "                     : delta " << std::setprecision(8) << results.risks_[0] << std::endl;
    std::cout << "                     : vega  " << std::setprecision(8) << results.risks_[1] << std::endl;
    std::cout << "                     : rho   " << std::setprecision(8) << results.risks_[2] << std::endl;

    return 0;
}