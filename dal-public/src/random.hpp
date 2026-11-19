//
// Created by wegam on 2022/9/24.
//

#pragma once

#include <limits>
#include <type_traits>

#include <dal/string/strings.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/random/sobol.hpp>

namespace Dal {

    FORCE_INLINE Handle_<PseudoRSG_> NewPseudoRSG(const String_& name, int seed, int ndim = 1) {
        REQUIRE(ndim >= 0, "InvalidRandomInput: field=/n_dim; expected=integer in [0,INT_MAX]; actual=" + String_(std::to_string(ndim)));
        return Handle_<PseudoRSG_>(new PseudoRSG_(name, seed, ndim));
    }

    FORCE_INLINE Handle_<SobolRSG_> NewSobolRSG(
        const String_& name, int iPath, int ndim = 1, bool precise = false, bool polish = false) {
        return Handle_<SobolRSG_>(new SobolRSG_(name, iPath, ndim, precise, polish));
    }

    namespace RandomBinding {
        Handle_<PseudoRSG_> NewPseudoRSG(const String_& name, double seed, double ndim);
        Handle_<SobolRSG_> NewSobolRSG(const String_& name, double iPath, double ndim, bool precise, bool polish);
    } // namespace RandomBinding

    template <class RSG_>
    FORCE_INLINE void FillRSG_(const Handle_<RSG_>& f, int numPath, void (RSG_::*fill)(Vector_<>* ) const, Matrix_<>* y) {
        REQUIRE(f.get() != nullptr, "Random generator handle must not be null");
        REQUIRE(y != nullptr, "Random generation requires an output matrix");
        REQUIRE(numPath >= 0, "Random path count must be nonnegative");
        REQUIRE(f->NDim() <= static_cast<size_t>(std::numeric_limits<int>::max()),
                "InvalidRandomInput: field=/n_dim; expected=dimension fitting matrix int range; actual=" + String_(std::to_string(f->NDim())));
        int n_dim = static_cast<int>(f->NDim());
        if constexpr (std::is_same_v<RSG_, SobolRSG_>)
            Detail::CheckSobolCapacity(*f, static_cast<size_t>(numPath));
        y->Resize(numPath, n_dim);
        if constexpr (std::is_same_v<RSG_, SobolRSG_>) {
            if (numPath == 0)
                return;
        }
        Vector_<> deviates(n_dim);
        for(int i = 0; i < numPath; ++i) {
            (f.get()->*fill)(&deviates);
            for(int j = 0; j < n_dim; ++j)
                (*y)(i, j) = deviates[j];
        }
    }

    FORCE_INLINE void GetPseudoRSGUniform(const Handle_<PseudoRSG_>& f, int numPath, Matrix_<>* y) {
        FillRSG_(f, numPath, &PseudoRSG_::FillUniform, y);
    }

    FORCE_INLINE void GetSobolRSGUniform(const Handle_<SobolRSG_>& f, int numPath, Matrix_<>* y) {
        FillRSG_(f, numPath, &SobolRSG_::FillUniform, y);
    }

    FORCE_INLINE void GetPseudoRSGNormal(const Handle_<PseudoRSG_>& f, int numPath, Matrix_<>* y) {
        FillRSG_(f, numPath, &PseudoRSG_::FillNormal, y);
    }

    FORCE_INLINE void GetSobolRSGNormal(const Handle_<SobolRSG_>& f, int numPath, Matrix_<>* y) {
        FillRSG_(f, numPath, &SobolRSG_::FillNormal, y);
    }
} // namespace Dal
