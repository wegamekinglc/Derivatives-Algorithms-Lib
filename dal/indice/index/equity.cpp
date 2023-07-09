//
// Created by wegam on 2022/1/21.
//

#include <dal/platform/strict.hpp>
#include <dal/indice/index/equity.hpp>
#include <dal/time/dateincrement.hpp>

namespace Dal::Index {
    String_ Equity_::Name() const {
        String_ ret = "EQ[" + eqName_ + "]";
        if (Cell::IsString(delivery_))
            ret += ">" + Cell::ToString(delivery_);
        else if (Cell::IsDate(delivery_))
            ret += "@" + Date::ToString(Cell::ToDate(delivery_));
        return ret;
    }

    Equity_::Equity_(const String_& eq_name, const Date_* delivery_date, const String_* delay_increment)
        : eqName_(eq_name) {
        if (delivery_date)
            delivery_ = Cell_(*delivery_date);
        else if (delay_increment)
            delivery_ = Cell_(*delay_increment);
        else
            delivery_ = Cell_();
    }

    Date_ Equity_::Delivery(const DateTime_& fixing_time) const {
        Date_ ret_val;
        if (Cell::IsString(delivery_)) {
            auto di = Date::ParseIncrement(Cell::ToString(delivery_));
            ret_val = (fixing_time.Date() + (*di));
        } else if (Cell::IsDate(delivery_))
            ret_val = Cell::ToDate(delivery_);
        else
            ret_val = Date::Maximum();
        return ret_val;
    }
} // namespace Dal::Index
