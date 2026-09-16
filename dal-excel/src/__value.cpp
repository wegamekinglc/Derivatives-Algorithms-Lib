//
// Created by wegam on 2022/11/20.
//

#include "__value.hpp"
#include "__platform.hpp"
#include "__script_test_api.hpp"
#include "__scriptinput.hpp"
#include <dal-public/src/value.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/platform/strict.hpp>

/*IF--------------------------------------------------------------------------
public MonteCarlo_Value
    valuation with monte carlo by a script product and a dedicated model
&inputs
product is handle ScriptProductData
    a product's data
modelData is handle ModelData
    a model's data
n_paths is number
    # of paths need for valuation
rsg is string
    method of random number generation
use_bb is boolean
    whether to use brownian bridge to generate path
enable_aad is boolean
    whether to enable aad mode
smooth is number
    smooth factor for non-continuous
&outputs
values is cell[][]
    the output values
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MonteCarlo_ValueWithSettings
    Value a script product with valuation and simulation settings
&inputs
product is handle ScriptProductData
    Script product handle
modelData is handle ModelData
    BS or Dupire model data handle
n_paths is number
    Finite integer from 1 to INT_MAX
+xl_valuation = Excel::ScriptScalarInput(xl_valuation); xl_simulation = Excel::ScriptScalarInput(xl_simulation);
&optional
valuation is handle StorableScriptValuationSettings
    Valuation settings handle; blank captures the global date and required history for this call
simulation is handle StorableMonteCarloSettings
    Monte Carlo settings handle; blank uses sobol, no BB/AAD, smooth 0.01 and tree
&outputs
values is cell[][]
    Two columns: PV and optional d_ parameter keys, then numeric values; no header
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptValuation_Explain
    Prepare once with default price settings; no paths, workers or cache
&inputs
product is handle ScriptProductData
    Script product handle
modelData is handle ModelData
    BS or Dupire model data handle
+xl_valuation = Excel::ScriptScalarInput(xl_valuation);
&optional
valuation is handle StorableScriptValuationSettings
    Valuation settings handle; blank selects per-call defaults
&outputs
json is string[]
    JSON chunks in one column; concatenate without separators for dal.script-valuation/1
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        Matrix_<Cell_> PriceTable(const std::map<String_, double>& prices) {
            Matrix_<Cell_> values(prices.size(), 2);
            int row = 0;
            for (const auto& price : prices) {
                values(row, 0) = price.first;
                values(row, 1) = price.second;
                ++row;
            }
            return values;
        }
    } // namespace

    void MonteCarlo_Value(const Handle_<ScriptProductData_>& product,
                          const Handle_<ModelData_>& modelData,
                          double n_paths,
                          const String_& rsg,
                          bool use_bb,
                          bool enable_aad,
                          double smooth,
                          Matrix_<Cell_>* values) {
        const int nPaths = Excel::CheckedMonteCarloPathCount(n_paths);
        *values = PriceTable(ValueByMonteCarlo(product, modelData, nPaths, rsg, use_bb, enable_aad, smooth));
    }

    void MonteCarlo_ValueWithSettings(const Handle_<ScriptProductData_>& product,
                                      const Handle_<ModelData_>& modelData,
                                      double nPaths,
                                      const Handle_<StorableScriptValuationSettings_>& valuation,
                                      const Handle_<StorableMonteCarloSettings_>& simulation,
                                      Matrix_<Cell_>* values) {
        const auto settings = valuation ? valuation->val_ : ScriptValuationSettings_();
        const auto execution = simulation ? simulation->val_ : MonteCarloSettings_();
        int count;
        try {
            count = Excel::CheckedMonteCarloPathCount(nPaths);
        } catch (const Exception_& error) {
            THROW("InvalidPathCount: MonteCarlo_ValueWithSettings; n_paths; " + String_(error.what()));
        }
        *values = PriceTable(ValueByMonteCarlo(product, modelData, count, settings, execution));
    }

    void ScriptValuation_Explain(const Handle_<ScriptProductData_>& product,
                                 const Handle_<ModelData_>& modelData,
                                 const Handle_<StorableScriptValuationSettings_>& valuation,
                                 Vector_<String_>* json) {
        const auto settings = valuation ? valuation->val_ : ScriptValuationSettings_();
        *json = Excel::ScriptDiagnosticChunks(ExplainScriptValuation(product, modelData, settings), "ScriptValuation_Explain");
    }

#ifdef _WIN32
#include <dal-excel/auto/MG_MonteCarlo_ValueWithSettings_public.inc>
#include <dal-excel/auto/MG_MonteCarlo_Value_public.inc>
#include <dal-excel/auto/MG_ScriptValuation_Explain_public.inc>
#endif
} // namespace Dal
