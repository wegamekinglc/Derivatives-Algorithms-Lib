//
// Created by wegam on 2023/1/23.
//

#pragma once

#include <dal/indice/index.hpp>
#include <dal/currency/currency.hpp>

namespace Dal {
    String_ FxIndexName(const Ccy_& domestic, const Ccy_& foreign);

    namespace Index {
        class Fx_ : public Index_ {
            Ccy_ dom_, fgn_;

        public:
            Fx_(const Ccy_ dom, const Ccy_ fgn) : dom_(dom), fgn_(fgn) {}
            [[nodiscard]] String_ Name() const override { return FxIndexName(dom_, fgn_); }
            double Fixing(_ENV, const DateTime_& fixingTime) const override;
        };
    } // namespace Index
} // namespace Dal
