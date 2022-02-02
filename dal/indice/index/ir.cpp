//
// Created by wegam on 2022/2/2.
//

#include <dal/platform/platform.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/protocol/conventions.hpp>

namespace Dal {
    namespace {
        Date_ DateFromCell(const Cell_& c, const Date_& fix) {
            switch (c.type_) {
            case Cell_::Type_::EMPTY:
                return fix;
            case Cell_::Type_::NUMBER:
                REQUIRE(static_cast<int>(c.d_) == c.d_ && c.d_ >= 0,
                        "start delay days must be a non-negative integer");
                return fix.AddDays(static_cast<int>(c.d_));
            case Cell_::Type_::DATE:
                return c.dt_.Date();
            case Cell_::Type_ ::STRING:
                return Date::ParseIncrement(c.s_)->FwdFrom(fix);
            default:
                THROW("invalid type for date offset");
            }
        }

        String_ MatPostfix(const Cell_& start) {
            // TODO: incomplete implementation
            switch (start.type_) {
            case Cell_::Type_::STRING:
                return start.s_;
            case Cell_::Type_::DATE:
                return Date::ToString(start.dt_.Date());
            case Cell_::Type_::EMPTY:
                THROW("maturity may not be empty");
            default:
                THROW("unsupported start/mat type in index");
            }
        }

        String_ StartPostfix(const Cell_& start) {
            switch (start.type_) {
            case Cell_::Type_::EMPTY:
                return String_();
            default:
                return "," + MatPostfix(start);
            }
        }
    }

    namespace Index {
        Date_ IRForward_::StartDate(const DateTime_& fixing_time) const {
            Date_ temp = DateFromCell(start_, fixing_time.Date());
            if (Cell::IsDate(start_))
                return temp;
            return Libor::StartFromFix(ccy_, temp);
        }

        String_ Index::Libor_::Name() const {
            return "IR:" + String_(ccy_.String()) + "," + String_(tenor_.String()) + StartPostfix(start_);
        }

        String_ Index::Swap_::Name() const {
            // note ",5Y" is a swap, ",Libor3M" is a Libor -- numeric first digit indicates a swap
            return "IR:" + String_(ccy_.String()) + "," + tenor_ + StartPostfix(start_);
        }

        String_ Index::DF_::Name() const {
            return "IR[DF]:" + String_(ccy_.String()) + StartPostfix(start_) + "," + MatPostfix(maturity_);
        }

        Date_ Index::DF_::StartDate(const DateTime_& fixing_time) const {
            return DateFromCell(start_, fixing_time.Date());
        }

        Date_ Index::DF_::Maturity(const DateTime_& fixing_time) const {
            return DateFromCell(maturity_, fixing_time.Date());
        }

    }
}
