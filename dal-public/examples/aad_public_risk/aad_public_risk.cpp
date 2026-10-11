//
// Created by Codex on 2026/10/11.
//

#include <iostream>

#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/platform/initall.hpp>

#include "../../../dal-cpp/examples/aadreport.hpp"

using namespace Dal;

int main() {
    RegisterAll_::Init();
    ThreadPool_::GetInstance()->Start(1, true);
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 10, 10);
    auto model = NewBSModelData("facade", 100.0, 0.2, 0.0, 0.0);
    auto product = NewScriptProduct("polynomial", {Cell_("X"), Cell_("Y"), Cell_(Date_(2027, 10, 10))}, {"2", "3", "a = X * Y b = X + Y pay PAYS 5"});
    Script::WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "output:1", "payoff"};
    request.selection_.inputs_ = Vector_<String_>{"constant:1", "constant:0"};
    request.selection_.reportFactors_ = Vector_<>{0.01, 0.001};
    request.weights_ = Vector_<>{2.0, -1.0, 0.5};
    const auto result = ValueByMonteCarloWithWeightedRisk(product, model, 33, request, valuation);
    model = {};
    product = {};
    Examples::PrintReferenceHeader("Public facade: owning weighted risk with reordered/scaled coordinates");
    Examples::Compare("Owning objective value", result.WeightedValue(), 9.5);
    Examples::Compare("Raw Y derivative", result.Jacobian()(0, 0), 3.0);
    Examples::Compare("Raw X derivative", result.Jacobian()(0, 1), 5.0);
    const auto reported = result.ReportedJacobian();
    Examples::Compare("Y derivative per 0.01", reported(0, 0), 0.03);
    Examples::Compare("X derivative per 0.001", reported(0, 1), 0.005);
    REQUIRE(result.InputAxis()[0].id_ == "constant:1" && result.InputAxis()[1].id_ == "constant:0", "Expected requested coordinate order");
    REQUIRE(result.Provenance().execution_ && result.Provenance().execution_->pathsPerReplicate_ == 33,
            "Expected retained complete execution snapshot");
    std::cout << "Result and execution metadata remain readable after releasing caller product/model handles.\n";
    return 0;
}
