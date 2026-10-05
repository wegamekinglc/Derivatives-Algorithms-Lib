//
// Created by Codex on 2026/9/15.
//

#include <dal-excel/src/__script_test_api.hpp>
#include <dal-public/src/global.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/script/detail/simulationobserver.hpp>
#include <dal/storage/globals.hpp>

#if defined(DAL_EXCEL_TEST_API_EXPORTS) || defined(DAL_EXCEL_API_TESTS_PORTABLE)
namespace Dal::Excel {
    // Windows tests and the statically linked XLL own distinct native runtime state.
    void ScriptTestInitialize(int threads) { InitGlobalData(threads); }
    std::pair<size_t, bool> ScriptTestStartWorkers(size_t threads) {
        auto* pool = ThreadPool_::GetInstance();
        const auto previous = std::make_pair(pool->NumThreads(), pool->IsActive());
        pool->Start(threads, true);
        return previous;
    }
    void ScriptTestRestoreWorkers(const std::pair<size_t, bool>& state) {
        auto* pool = ThreadPool_::GetInstance();
        pool->Start(state.first, true);
        if (!state.second)
            pool->Stop();
    }
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
