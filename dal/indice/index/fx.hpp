//
// Created by wegam on 2023/1/23.
//

#pragma once

#include <dal/indice/index.hpp>
#include <dal/currency/currency.hpp>

namespace Dal::Index {
    class Fx_ : public Index_ {
        Ccy_ dom_, fgn_;
        [[nodiscard]] String_ XName(bool invert) const;

    public:
        Fx_(const Ccy_ dom, const Ccy_ fgn): dom_(dom), fgn_(fgn) {}
        [[nodiscard]] String_ Name() const override { return XName(false);}
        double Fixing(_ENV, const DateTime_& fixing_time) const override;
    };
} // namespace Dal::Index
