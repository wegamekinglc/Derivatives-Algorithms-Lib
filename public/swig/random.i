#ifndef DAL_RANDOM_I
#define DAL_RANDOM_I

%{
#include <public/src/random.hpp>
%}

    %template(PseudoRSG_) Handle_<PseudoRSG_>;
    %template(SobolRSG_) Handle_<SobolRSG_>;

    %inline %{
        Handle_<PseudoRSG_> PseudoRSG_New(int seed, int ndim = 1) {
            return NewPseudoRSG(String_("PseudoRSG_"), seed, ndim);
        }

        Matrix_<> PseudoRSG_Get_Uniform(const Handle_<PseudoRSG_>& rsg, int num_path) {
            Matrix_<> m;
            GetPseudoRSGUniform(rsg, num_path, &m);
            return m;
        }

        Matrix_<> PseudoRSG_Get_Normal(const Handle_<PseudoRSG_>& rsg, int num_path) {
            Matrix_<> m;
            GetPseudoRSGNormal(rsg, num_path, &m);
            return m;
        }

        Handle_<SobolRSG_> SobolRSG_New(int i_path, int ndim = 1) {
            return NewSobolRSG(String_("SobolRSG_"), i_path, ndim);
        }

        Matrix_<> SobolRSG_Get_Uniform(const Handle_<SobolRSG_>& rsg, int num_path) {
            Matrix_<> m;
            GetSobolRSGUniform(rsg, num_path, &m);
            return m;
        }

        Matrix_<> SobolRSG_Get_Normal(const Handle_<SobolRSG_>& rsg, int num_path) {
            Matrix_<> m;
            GetSobolRSGNormal(rsg, num_path, &m);
            return m;
        }
    %}

#endif