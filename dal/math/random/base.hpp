//
// Created by wegam on 2021/12/26.
//

#pragma once
#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>

namespace Dal {
    class Random_ {
    public:
        virtual ~Random_() = default;
        virtual void FillUniform(Vector_<>* deviates) = 0;
        virtual void FillNormal(Vector_<>* deviates) = 0;
        virtual Random_* Clone() const = 0;
        virtual size_t NDim() const = 0;
    };
}

