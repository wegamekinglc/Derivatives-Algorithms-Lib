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
            if (Cell::IsEmpty(c))
                return fix;
            else if (Cell::IsInt(c))
                return fix.AddDays(Cell::ToInt(c));
            else if (Cell::IsDate(c))
                return Cell::ToDate(c);
            else if (Cell::IsDateTime(c))
                return Cell::ToDateTime(c).Date();
            else if (Cell::IsString(c))
                return Date::ParseIncrement(Cell::ToString(c))->FwdFrom(fix);
            else
                THROW("invalid type for date offset");
        }

        String_ MatPostfix(const Cell_& start) {
            // TODO: incomplete implementation
            if (Cell::IsString(start))
                return Cell::ToString(start);
            else if (Cell::IsDate(start))
                return Date::ToString(Cell::ToDate(start));
            else if (Cell::IsEmpty(start))
                THROW("maturity may not be empty");
            else
                THROW("unsupported start/mat type in index");
        }

        String_ StartPostfix(const Cell_& start) {
            if (Cell::IsEmpty(start))
                return {};
            else
                return "," + MatPostfix(start);
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
