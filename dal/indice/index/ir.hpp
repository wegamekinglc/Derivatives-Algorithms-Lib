//
// Created by wegam on 2022/1/28.
//

#pragma once

#include <dal/indice/index.hpp>
#include <dal/math/cell.hpp>
#include <dal/protocol/currency.hpp>
#include <dal/protocol/couponrate.hpp>

namespace Dal::Index {
    class IRForward_: public Index_ {
    public:
        const Ccy_ ccy_;
        const Cell_ start_;

        IRForward_(const Ccy_& ccy, const Cell_& start = Cell_())
        : ccy_(ccy), start_(start) {}

        Date_ StartDate(const DateTime_& fixing_time) const;
    };

    class Libor_: public IRForward_ {

    };
}
