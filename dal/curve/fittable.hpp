//
// Created by wegam on 2023/3/26.
//

#pragma once

#include <dal/math/vectors.hpp>

namespace Dal {

    class FittableCurve_ {
    public:
        virtual ~FittableCurve_() = default;
        [[nodiscard]] virtual int NX() const = 0;
        virtual void ApplyDX(Vector_<>::const_iterator dx, double leverage) = 0;
    };

} // namespace Dal
