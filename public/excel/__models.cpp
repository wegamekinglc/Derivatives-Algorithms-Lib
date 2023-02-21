//
// Created by wegam on 2022/11/20.
//

#include <public/excel/__platform.hpp>
#include <public/src/models.hpp>

/*IF--------------------------------------------------------------------------
public BSModelData_New
    Black - Scholes model's data description
&inputs
name is string
    A name for the object being created
spot is number
    current spot value
vol is number
    volatility of the underlying
rate is number
    risk-free rate
div is number
    dividend rate
&outputs
model is handle ModelData
    The model data
-IF-------------------------------------------------------------------------*/


/*IF--------------------------------------------------------------------------
public DupireModelData_New
    Dupire local volality model's data description
&inputs
name is string
    A name for the object being created
spot is number
    current spot value
rate is number
    risk-free rate
repo is number
    repo rate including dividend
spots is number[]
    local vol surface spots data
times is number[]
    local vol surface times data
vols is number[][]
    local vol surface data
&outputs
model is handle ModelData
    The model data
-IF-------------------------------------------------------------------------*/


namespace Dal {
    using Dal::AAD::ModelData_;
    namespace {
        void BSModelData_New(const String_& name,
                             double spot,
                             double vol,
                             double rate,
                             double div,
                             Handle_<ModelData_>* model) {
            NewBSModelData(name, spot, vol, rate, div).swap(*model);
        }

        void DupireModelData_New(const String_& name,
                                 double spot,
                                 double rate,
                                 double repo,
                                 const Vector_<>& spots,
                                 const Vector_<>& times,
                                 const Matrix_<>& vols,
                                 Handle_<ModelData_>* model) {
            NewDupireModelData(name, spot, rate, repo, spots, times, vols).swap(*model);
        }
    }
#ifdef _WIN32
#include <public/auto/MG_BSModelData_New_public.inc>
#include <public/auto/MG_DupireModelData_New_public.inc>
#endif
}