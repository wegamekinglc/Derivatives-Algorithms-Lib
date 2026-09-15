//
// Created by Codex on 2026/9/15.
//

#include "__script_test_api.hpp"
#include <dal-public/src/global.hpp>

#if defined(DAL_EXCEL_TEST_API_EXPORTS) || defined(DAL_EXCEL_API_TESTS_PORTABLE)
namespace Dal::Excel {
    // Windows tests and the statically linked XLL own distinct native runtime state.
    void ScriptTestInitialize(int threads) { InitGlobalData(threads); }
    Date_ ScriptTestSetDate(const Date_& date) {
        const auto previous = GetEvaluationDate();
        SetEvaluationDate(date);
        return previous;
    }
    void ScriptTestStoreFixings(const String_& name, const FixHistory_& history) { XGLOBAL::StoreFixings(name, history, false); }
    Detail::FixingReadObserver_*& ScriptTestFixingObserver() { return Detail::FixingReadObserver(); }
    Script::Detail::SimulationObserver_*& ScriptTestSimulationObserver() { return Script::Detail::SimulationObserver(); }
    String_ ScriptTestNativeDescribe(const Handle_<ScriptProductData_>& product) { return DescribeScriptProduct(product); }
    String_
    ScriptTestNativeExplain(const Handle_<ScriptProductData_>& product, const Handle_<ModelData_>& model, const ScriptValuationSettings_& valuation) {
        return ExplainScriptValuation(product, model, valuation);
    }
} // namespace Dal::Excel
#endif
