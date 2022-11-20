#ifndef DAL_MODELS_I
#define DAL_MODELS_I

%{
#include <public/src/models.hpp>
%}

%template(BSModelData_) Handle_<BSModelData_>;

%inline %{
    Handle_<BSModelData_> BSModelData_New(double spot,
                                          double vol,
                                          double rate,
                                          double div) {
        return NewBSModelData("bs_model_data", spot, vol, rate, div);
    }
%}

#endif