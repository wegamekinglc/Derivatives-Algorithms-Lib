//
// Created by wegam on 2021/12/26.
//

#pragma once

#include <dal/math/vectors.hpp>

namespace Dal {
    class Random_ {
    public:
        virtual ~Random_() = default;
        virtual void FillUniform(Vector_<>* deviates) = 0;
        virtual void FillNormal(Vector_<>* deviates) = 0;
        virtual void SkipTo(size_t n_points) = 0;
        [[nodiscard]] virtual Random_* Clone() const = 0;
        [[nodiscard]] virtual size_t NDim() const = 0;
    };
} // namespace Dal
