//
// Created by wegam on 2020/12/19.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/math/vectors.hpp>

namespace Dal {
    class Random_ {
        bool anti_ = false;
    protected:
        Vector_<> cache_;
    public:
        explicit Random_(size_t n_dim): cache_(n_dim) {}
        virtual ~Random_() = default;
        virtual double NextUniform() = 0;
        virtual void FillUniform(Vector_<>* deviates) = 0;
        virtual void FillNormal(Vector_<>* deviates) = 0;
        [[nodiscard]]virtual Random_* Clone() const = 0;
        virtual void SkipTo(size_t n_points) = 0;
        [[nodiscard]]virtual Random_* Branch(int i_child) const = 0;
    };

/*IF--------------------------------------------------------------------------
enumeration RNGType
    random number generator types
alternative IRN ShuffledIRN
alternative MRG32 MRG32k32a
-IF-------------------------------------------------------------------------*/

    #include <dal/auto/MG_RNGType_enum.hpp>
    Random_* New(const RNGType_& type, int seed, size_t n_dim = 1);
}
