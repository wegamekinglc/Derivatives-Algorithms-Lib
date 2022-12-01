#ifndef DAL_MODELS_I
#define DAL_MODELS_I

%{
#include <public/src/models.hpp>
%}

%template(ModelData_) Handle_<ModelData_>;

%inline %{
    Handle_<ModelData_> BSModelData_New(double spot,
                                        double vol,
                                        double rate,
                                        double div) {
        return NewBSModelData("bs_model_data", spot, vol, rate, div);
    }
%}

#endif