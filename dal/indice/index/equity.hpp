//
// Created by wegam on 2022/1/21.
//

#pragma once

#include <dal/indice/index.hpp>
#include <dal/math/cell.hpp>

namespace Dal::Index {
    class Equity_ : public Index_ {
        Cell_ delivery_;

    public:
        String_ Name() const;
        const String_ eqName_;
        Date_ Delivery(const DateTime_& fixing_time) const;
        Equity_(const String_& eq_name, const Date_* delivery_date = nullptr, const String_* delay_increment = nullptr);
    };
} // namespace Dal::Index