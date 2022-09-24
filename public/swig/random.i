#ifndef DAL_RANDOM_I
#define DAL_RANDOM_I

%{
#include <public/src/random.hpp>
%}

    %template(PseudoRSG_) Handle_<PseudoRSG_>;

    %inline %{
        Handle_<PseudoRSG_> PseudoRSG_New(const std::string& name, int seed, int ndim = 1) {
            return NewPseudoRSG(String_(name), seed, ndim);
        }

        Matrix_<> PseudoRSG_Get_Uniform(const Handle_<PseudoRSG_>& rsg, int num_path) {
            Matrix_<> m;
            GetPseudoRSGUniform(rsg, num_path, &m);
            return m;
        }
    %}

#endif