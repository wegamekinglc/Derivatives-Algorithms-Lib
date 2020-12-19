//
// Created by wegam on 2020/12/19.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
    class Random_ {
    public:
        virtual ~Random_() = default;
        virtual double NextUniform() = 0;
        virtual void FillUniform(Vector_<>* deviates) = 0;
        virtual void FillNormal(Vector_<>* deviates) = 0;
        virtual Random_* Branch(int i_child = 0) const = 0;
    };

    namespace Random {
        Random_* New(int seed);
    }
}
