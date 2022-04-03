//
// Created by wegam on 2020/12/19.
//

#pragma once

#include <dal/math/random/base.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF--------------------------------------------------------------------------
enumeration RNGType
    random number generator types
alternative IRN ShuffledIRN
alternative MRG32 MRG32k32a
-IF-------------------------------------------------------------------------*/

namespace Dal {
    class PseudoRandom_ : public Random_ {
        bool anti_ = false;

    protected:
        Vector_<> cache_;

    public:
        explicit PseudoRandom_(size_t n_dim) : cache_(n_dim) {}
        virtual ~PseudoRandom_() = default;
        virtual double NextUniform() = 0;
        virtual void FillUniform(Vector_<>* deviates);
        virtual void FillNormal(Vector_<>* deviates);
        [[nodiscard]] virtual PseudoRandom_* Clone() const = 0;
        virtual void SkipTo(size_t n_points) = 0;
        [[nodiscard]] size_t NDim() const override { return cache_.size(); }
        [[nodiscard]] virtual PseudoRandom_* Branch(int i_child) const = 0;
    };

#include <dal/auto/MG_RNGType_enum.hpp>
    PseudoRandom_* New(const RNGType_& type, int seed, size_t n_dim = 1);
} // namespace Dal
