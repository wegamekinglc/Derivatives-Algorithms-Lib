//
// Created by wegam on 2023/1/23.
//

#pragma once

#include <dal/indice/index.hpp>
#include <dal/currency/currency.hpp>
#include <dal/platform/strict.hpp>

namespace Dal::Index {
    class Fx_ : public Index_ {
        Ccy_ dom_, fgn_;
        String_ XName(bool invert) const;

    public:
        String_ Name() const override {
            return XName(false);
        }
        double Fixing(_ENV, const DateTime_& fixing_time) const override;
    };
} // namespace Dal::Index
