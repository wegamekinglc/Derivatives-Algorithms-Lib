#include <iostream>
#include <type_traits>

#ifdef DAL_TEST_PUBLIC_PDE_RISK
#include <dal-public/src/earlyexercisepderisk.hpp>
using namespace Dal;
static_assert(std::is_same_v<EarlyExercisePdeRiskRecord_, Dal::PDE::EarlyExercisePdeRiskRecord_>);
#else
#include <dal/math/pde/earlyexercisepderisk.hpp>
using namespace Dal;
using namespace Dal::PDE;
#endif

int main() {
    EarlyExercisePdeRiskRequest_ request;
    const Date_ today(2026, 10, 18), maturity(2027, 10, 18);
    request.valuation_.contract_ = {today, maturity, 100.0, OptionType_("PUT"), EarlyExerciseStyle_("BERMUDAN"), {maturity}};
    request.valuation_.market_ = {100.0, 0.2, {{maturity, 0.05, 0.03}}};
    request.valuation_.settings_.gridPoints_ = 41;
    request.valuation_.settings_.ordinarySteps_ = 2;
    request.valuation_.settings_.rannacherIntervals_ = 0;
    ValidateEarlyExercisePdeRiskSettings(request.risk_);
    ValidateEarlyExercisePdeRiskRequest(request);
    const auto result = EvaluateEarlyExercisePdeRisk(request);
    if (!result.PriceAvailable() || result.SchemaVersion() != 1 || result.Scenarios().size() != 17 || result.Risks().size() != 5 ||
        result.Execution().workUnits_ != 1326 || result.AadUsed())
        return 1;
    std::cout << result.Status().String() << ' ' << result.Price() << '\n';
    return 0;
}
