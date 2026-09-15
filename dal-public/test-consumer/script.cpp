//
// Created by Codex on 2026/9/15.
//

#include <iostream>
#include <type_traits>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;

namespace {
    class CountingDates_ : public Global::Store_ {
        Matrix_<Cell_> date_{1, 1, Cell_(Date_(2026, 9, 12))};

    public:
        size_t reads_ = 0;
        size_t writes_ = 0;
        MonteCarloSettings_* callerSimulation_ = nullptr;
        void Set(const String_&, const Matrix_<Cell_>& value) override {
            ++writes_;
            date_ = value;
        }
        const Matrix_<Cell_>& Get(const String_&) override {
            ++reads_;
            if (callerSimulation_) {
                callerSimulation_->rsg_ = "changed_during_date_capture";
                callerSimulation_ = nullptr;
            }
            return date_;
        }
    };
} // namespace

int main() {
    InitGlobalData(1);
    // This independent process owns the replacement date store until exit.
    auto* dates = new CountingDates_();
    Global::SetTheDateStore(dates);
    const auto product = NewScriptProduct("legacy", {Cell_(Date_(2026, 9, 12))}, {"pay PAYS SPOT()"});
    const auto model = NewBSModelData("model", 100.0, 0.0, 0.0, 0.0);
    const auto check = [&](const std::map<String_, double>& result) {
        REQUIRE(result.at("PV") == 100.0, "old signature changed its price");
        REQUIRE(dates->reads_ == 1, "default valuation must capture the date exactly once");
        REQUIRE(dates->writes_ == 0, "valuation must not write the global date");
        dates->reads_ = 0;
    };
    check(ValueByMonteCarlo(product, model, 1));
    check(ValueByMonteCarlo(product, model, 1, "sobol"));
    check(ValueByMonteCarlo(product, model, 1, "sobol", true));
    check(ValueByMonteCarlo(product, model, 1, "sobol", true, true));
    check(ValueByMonteCarlo(product, model, 1, "sobol", true, true, 0.02));
    check(ValueByMonteCarlo(product, model, 1, "sobol", true, true, 0.02, true));
    static_assert(std::is_same_v<ScriptValuationSettings_, Script::ScriptValuationSettings_>);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*ScriptValuationSettings_().fixings_)>>);
    ScriptValuationSettings_ valuation{TodayFixingPolicy_::Value_::MODEL, {}};
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    REQUIRE(ValueByMonteCarlo(product, model, 1, valuation).at("PV") == 100.0, "typed valuation failed");
    REQUIRE(dates->reads_ == 0 && dates->writes_ == 0, "explicit valuation must not access the global date");
    REQUIRE(!DescribeScriptProduct(product).empty(), "missing contract description");
    REQUIRE(!ExplainScriptValuation(product, model, valuation).empty(), "missing explicit valuation explanation");
    REQUIRE(dates->reads_ == 0 && dates->writes_ == 0, "explicit diagnostics must not access the global date");
    REQUIRE(!ExplainScriptValuation(product, model).empty(), "missing default valuation explanation");
    REQUIRE(dates->reads_ == 1 && dates->writes_ == 0, "default explanation must capture the date exactly once");
    MonteCarloSettings_ simulation;
    dates->reads_ = 0;
    dates->callerSimulation_ = &simulation;
    check(ValueByMonteCarlo(product, model, 1, ScriptValuationSettings_(), simulation));
    REQUIRE(simulation.rsg_ == "changed_during_date_capture", "date capture seam was not exercised");
    std::cout << "Old 3-8 argument calls and typed settings pass; default date reads=1, explicit/Describe reads=0, writes=0\n";
}
