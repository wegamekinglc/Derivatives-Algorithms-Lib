//
// Created by wegam on 2022/9/24.
//

#include <public/excel/__platform.hpp>
#include <public/src/random.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/random/sobol.hpp>

/*IF--------------------------------------------------------------------------
public PseudoRSG_New
    Create a pseudo random sequence generator
&inputs
name is string
    A name for the object being created
seed is number
    The seed for the generator
n_dim is number
    The dimension of the generator
&outputs
f is handle PseudoRSG
    The pseudo random sequence generator
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PseudoRSG_Get_Uniform
    get a pseudo random uniform number generated matrix (num_path x n_dim)
&inputs
f is handle PseudoRSG
    The pseudo random sequence generator
num_path is number
    The number of random sequences
&outputs
y is number[][]
    random number generated matrix (num_path x n_dim)
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PseudoRSG_Get_Normal
    get a pseudo random normal number generated matrix (num_path x n_dim)
&inputs
f is handle PseudoRSG
    The pseudo random sequence generator
num_path is number
    The number of random sequences
&outputs
y is number[][]
    random number generated matrix (num_path x n_dim)
-IF-------------------------------------------------------------------------*/


/*IF--------------------------------------------------------------------------
public SobolRSG_New
    Create a sobol quasi-random sequence generator
&inputs
name is string
    A name for the object being created
i_path is number
    The number of path to skip
n_dim is number
    The dimension of the generator
&outputs
f is handle SobolRSG
    The sobol quasi-random sequence generator
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public SobolRSG_Get_Uniform
    get a psobol quasi-random uniform number generated matrix (num_path x n_dim)
&inputs
f is handle SobolRSG
    The pseudo random sequence generator
num_path is number
    The number of random sequences
&outputs
y is number[][]
    random number generated matrix (num_path x n_dim)
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public SobolRSG_Get_Normal
    get a sobol quasi-random normal number generated matrix (num_path x n_dim)
&inputs
f is handle SobolRSG
    The pseudo random sequence generator
num_path is number
    The number of random sequences
&outputs
y is number[][]
    random number generated matrix (num_path x n_dim)
-IF-------------------------------------------------------------------------*/


namespace Dal {
    namespace {
        void PseudoRSG_New(const String_& name, double seed, double ndim, Handle_<PseudoRSG_>* f) {
            NewPseudoRSG(name, seed, ndim).swap(*f);
        }

        void PseudoRSG_Get_Uniform(const Handle_<PseudoRSG_>& f, double num_path, Matrix_<>* y) {
            int n_dim = f->NDim();
            y->Resize(static_cast<int>(num_path), n_dim);
            Vector_<> deviates(n_dim);
            for(int i = 0; i < num_path; ++i) {
                f->FillUniform(&deviates);
                for(int j = 0; j < n_dim; ++j)
                    (*y)(i, j) = deviates[j];
            }
        }

        void PseudoRSG_Get_Normal(const Handle_<PseudoRSG_>& f, double num_path, Matrix_<>* y) {
            int n_dim = f->NDim();
            y->Resize(static_cast<int>(num_path), n_dim);
            Vector_<> deviates(n_dim);
            for(int i = 0; i < num_path; ++i) {
                f->FillNormal(&deviates);
                for(int j = 0; j < n_dim; ++j)
                    (*y)(i, j) = deviates[j];
            }
        }

        void SobolRSG_New(const String_& name, double i_path, double ndim, Handle_<SobolRSG_>* f) {
            NewSobolRSG(name, i_path, ndim).swap(*f);
        }

        void SobolRSG_Get_Uniform(const Handle_<SobolRSG_>& f, double num_path, Matrix_<>* y) {
            int n_dim = f->NDim();
            y->Resize(static_cast<int>(num_path), n_dim);
            Vector_<> deviates(n_dim);
            for(int i = 0; i < num_path; ++i) {
                f->FillUniform(&deviates);
                for(int j = 0; j < n_dim; ++j)
                    (*y)(i, j) = deviates[j];
            }
        }

        void SobolRSG_Get_Normal(const Handle_<SobolRSG_>& f, double num_path, Matrix_<>* y) {
            int n_dim = f->NDim();
            y->Resize(static_cast<int>(num_path), n_dim);
            Vector_<> deviates(n_dim);
            for(int i = 0; i < num_path; ++i) {
                f->FillNormal(&deviates);
                for(int j = 0; j < n_dim; ++j)
                    (*y)(i, j) = deviates[j];
            }
        }
    }

#ifdef _WIN32
#include <public/auto/MG_PseudoRSG_New_public.inc>
#include <public/auto/MG_PseudoRSG_Get_Uniform_public.inc>
#include <public/auto/MG_PseudoRSG_Get_Normal_public.inc>
#include <public/auto/MG_SobolRSG_New_public.inc>
#include <public/auto/MG_SobolRSG_Get_Uniform_public.inc>
#include <public/auto/MG_SobolRSG_Get_Normal_public.inc>
#endif
}
