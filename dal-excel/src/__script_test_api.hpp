//
// Created by Codex on 2026/9/15.
//

#pragma once

#include "__script_storable.hpp"
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>
#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/math/cell.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/script/detail/simulationobserver.hpp>
#include <dal/storage/globals.hpp>

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_SCRIPT_TEST_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_SCRIPT_TEST_API __declspec(dllimport)
#else
#define DAL_SCRIPT_TEST_API
#endif

namespace Dal {
    struct StorableMarketFixingSnapshot_;
    DAL_SCRIPT_TEST_API void
    ScriptProductSettings_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorableScriptProductSettings_>* productSettings);
    DAL_SCRIPT_TEST_API void ScriptValuationSettings_New(const String_& name,
                                                         const Matrix_<Cell_>& settings,
                                                         const Handle_<StorableMarketFixingSnapshot_>& fixings,
                                                         Handle_<StorableScriptValuationSettings_>* valuation);
    DAL_SCRIPT_TEST_API void
    MonteCarloSettings_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorableMonteCarloSettings_>* simulation);
    DAL_SCRIPT_TEST_API void
    Product_New(const String_& name, const Vector_<Cell_>& dates, const Vector_<String_>& events, Handle_<ScriptProductData_>* product);
    DAL_SCRIPT_TEST_API void Product_NewWithSettings(const String_& name,
                                                     const Vector_<Cell_>& dates,
                                                     const Vector_<String_>& events,
                                                     const Handle_<StorableScriptProductSettings_>& settings,
                                                     Handle_<ScriptProductData_>* product);
    DAL_SCRIPT_TEST_API void MonteCarlo_Value(const Handle_<ScriptProductData_>& product,
                                              const Handle_<ModelData_>& modelData,
                                              double nPaths,
                                              const String_& rsg,
                                              bool useBb,
                                              bool enableAad,
                                              double smooth,
                                              Matrix_<Cell_>* values);
    DAL_SCRIPT_TEST_API void MonteCarlo_ValueWithSettings(const Handle_<ScriptProductData_>& product,
                                                          const Handle_<ModelData_>& modelData,
                                                          double nPaths,
                                                          const Handle_<StorableScriptValuationSettings_>& valuation,
                                                          const Handle_<StorableMonteCarloSettings_>& simulation,
                                                          Matrix_<Cell_>* values);
    DAL_SCRIPT_TEST_API void Product_Describe(const Handle_<ScriptProductData_>& product, Vector_<String_>* json);
    DAL_SCRIPT_TEST_API void ScriptValuation_Explain(const Handle_<ScriptProductData_>& product,
                                                     const Handle_<ModelData_>& modelData,
                                                     const Handle_<StorableScriptValuationSettings_>& valuation,
                                                     Vector_<String_>* json);
    DAL_SCRIPT_TEST_API void ScriptSimulation_Explain(const Handle_<ScriptProductData_>& product,
                                                      const Handle_<ModelData_>& modelData,
                                                      double nPaths,
                                                      const Handle_<StorableScriptValuationSettings_>& valuation,
                                                      const Handle_<StorableMonteCarloSettings_>& simulation,
                                                      Vector_<String_>* json);
    namespace Excel {
        DAL_SCRIPT_TEST_API Vector_<String_> ScriptDiagnosticChunks(const String_& json, const String_& function);
        DAL_SCRIPT_TEST_API void ScriptTestInitialize(int threads);
        DAL_SCRIPT_TEST_API Date_ ScriptTestSetDate(const Date_& date);
        DAL_SCRIPT_TEST_API void ScriptTestStoreFixings(const String_& name, const FixHistory_& history);
        DAL_SCRIPT_TEST_API Detail::FixingReadObserver_*& ScriptTestFixingObserver();
        DAL_SCRIPT_TEST_API Script::Detail::SimulationObserver_*& ScriptTestSimulationObserver();
        DAL_SCRIPT_TEST_API String_ ScriptTestNativeDescribe(const Handle_<ScriptProductData_>& product);
        DAL_SCRIPT_TEST_API String_ ScriptTestNativeExplain(const Handle_<ScriptProductData_>& product,
                                                            const Handle_<ModelData_>& model,
                                                            const ScriptValuationSettings_& valuation);
    } // namespace Excel
} // namespace Dal

#undef DAL_SCRIPT_TEST_API
