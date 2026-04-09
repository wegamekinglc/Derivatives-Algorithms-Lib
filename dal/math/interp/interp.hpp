//
// Created by wegam on 2020/10/25.
//

#pragma once


#include <dal/storage/archive.hpp>
#include <dal/math/operators.hpp>

namespace Dal {

    class BASE_EXPORT Interp1_ : public Storable_ {
    public:
        explicit Interp1_(const String_& name);
        virtual double operator()(double x) const = 0;
        [[nodiscard]] virtual bool IsInBounds(double x) const { return true; }
    };

} // namespace Dal