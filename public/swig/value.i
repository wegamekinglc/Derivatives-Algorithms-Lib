#ifndef DAL_VALUE_I
#define DAL_VALUE_I

%{
#include <public/src/value.hpp>
%}

%include "std_map.i"
%include models.i
%include script.i

%template(Dictionary) std::map<std::string, double>;

%inline %{
    std::map<std::string, double> MonteCarlo_Value(const Handle_<ScriptProduct_>& product,
                                                   const Handle_<ModelData_>& modelData,
                                                   int num_path,
                                                   const std::string& method = "sobol",
                                                   bool use_bb = false,
                                                   bool enable_aad = false,
                                                   double smooth = 0.01) {
    auto res = ValueByMonteCarlo(product, modelData, num_path, String_(method), use_bb, enable_aad, smooth);
    std::map<std::string, double> rtn;
    for (auto& d : res)
        rtn[d.first.c_str()] = d.second;
    return rtn;
}
%}

#endif
