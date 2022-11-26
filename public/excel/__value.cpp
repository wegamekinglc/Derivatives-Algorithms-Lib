//
// Created by wegam on 2022/11/20.
//

#include <public/excel/__platform.hpp>
#include <public/src/value.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/platform/strict.hpp>

/*IF--------------------------------------------------------------------------
public MonteCarlo_Value
    valuation with monte carlo by a script product and a dedicated model
&inputs
product is handle ScriptProduct
    a product
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


namespace Dal {
    namespace {
        void MonteCarlo_Value(const Handle_<ScriptProduct_>& product,
                              const Handle_<ModelData_>& modelData,
                              double n_paths,
                              const String_& rsg,
                              bool use_bb,
                              bool enable_aad,
                              double smooth,
                              Matrix_<Cell_>* values) {
            auto val = ValueByMonteCarlo(product, modelData, (int)n_paths, rsg, use_bb, enable_aad, smooth);
            values->Resize(val.size(), 2);
            int i = 0;
            for (auto& d : val) {
                (*values)(i, 0) = d.first;
                (*values)(i, 1) = d.second;
                ++i;
            }
        }
    }

#ifdef _WIN32
#include <public/auto/MG_MonteCarlo_Value_public.inc>
#endif
}