//
// Created by wegam on 2022/9/24.
//


#include <public/src/random.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/random/sobol.hpp>

namespace Dal {
    Handle_<PseudoRSG_> NewPseudoRSG(const String_& name, int seed, int ndim) {
        return Handle_<PseudoRSG_>(new PseudoRSG_(name, seed, ndim));
    }

    Handle_<SobolRSG_> NewSobolRSG(const String_& name, int i_path, int ndim) {
        return Handle_<SobolRSG_>(new SobolRSG_(name, i_path, ndim));
    }

    void GetPseudoRSGUniform(const Handle_<PseudoRSG_>& f, int num_path, Matrix_<>* y) {
        int n_dim = f->NDim();
        y->Resize(num_path, n_dim);
        Vector_<> deviates(n_dim);
        for(int i = 0; i < num_path; ++i) {
            f->FillUniform(&deviates);
            for(int j = 0; j < n_dim; ++j)
                (*y)(i, j) = deviates[j];
        }
    }
}