//
// Created by wegam on 2022/9/24.
//

#pragma once

#include <dal/string/strings.hpp>
#include <dal-public/src/random.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/random/sobol.hpp>

namespace Dal {

    FORCE_INLINE Handle_<PseudoRSG_> NewPseudoRSG(const String_& name, int seed, int ndim = 1) {
        return Handle_<PseudoRSG_>(new PseudoRSG_(name, seed, ndim));
    }

    FORCE_INLINE Handle_<SobolRSG_> NewSobolRSG(const String_& name, int iPath, int ndim = 1) {
        return Handle_<SobolRSG_>(new SobolRSG_(name, iPath, ndim));
    }

    FORCE_INLINE void GetPseudoRSGUniform(const Handle_<PseudoRSG_>& f, int numPath, Matrix_<>* y) {
        int n_dim = f->NDim();
        y->Resize(numPath, n_dim);
        Vector_<> deviates(n_dim);
        for(int i = 0; i < numPath; ++i) {
            f->FillUniform(&deviates);
            for(int j = 0; j < n_dim; ++j)
                (*y)(i, j) = deviates[j];
        }
    }

    FORCE_INLINE void GetSobolRSGUniform(const Handle_<SobolRSG_>& f, int numPath, Matrix_<>* y) {
        int n_dim = f->NDim();
        y->Resize(numPath, n_dim);
        Vector_<> deviates(n_dim);
        for(int i = 0; i < numPath; ++i) {
            f->FillUniform(&deviates);
            for(int j = 0; j < n_dim; ++j)
                (*y)(i, j) = deviates[j];
        }
    }
    FORCE_INLINE void GetPseudoRSGNormal(const Handle_<PseudoRSG_>& f, int numPath, Matrix_<>* y) {
        int n_dim = f->NDim();
        y->Resize(numPath, n_dim);
        Vector_<> deviates(n_dim);
        for(int i = 0; i < numPath; ++i) {
            f->FillNormal(&deviates);
            for(int j = 0; j < n_dim; ++j)
                (*y)(i, j) = deviates[j];
        }
    }

    FORCE_INLINE void GetSobolRSGNormal(const Handle_<SobolRSG_>& f, int numPath, Matrix_<>* y) {
        int n_dim = f->NDim();
        y->Resize(numPath, n_dim);
        Vector_<> deviates(n_dim);
        for(int i = 0; i < numPath; ++i) {
            f->FillNormal(&deviates);
            for(int j = 0; j < n_dim; ++j)
                (*y)(i, j) = deviates[j];
        }
    }
} // namespace Dal