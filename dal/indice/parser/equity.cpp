//
// Created by wegam on 2022/1/23.
//

#include <dal/platform/strict.hpp>
#include <dal/indice/parser/equity.hpp>
#include <dal/indice/index/equity.hpp>
#include <dal/time/dateutils.hpp>

namespace Dal::Index {
    Index_* EquityParser(const String_& name) {
        auto eq_start = name.find_first_of("[");
        auto eq_stop = name.find_first_of("]");

        String_ eq_name = name.substr(eq_start + 1, eq_stop - eq_start - 1);
        auto tenor_start = name.find_first_of("@>", eq_stop + 1);
        if (tenor_start == String_::npos)
            return new Equity_(eq_name, nullptr, nullptr);
        else if (name[tenor_start] == '@') {
            auto delivery_date = Date::FromString(name.substr(tenor_start + 1, name.length() - tenor_start - 1));
            return new Equity_(eq_name, &delivery_date, nullptr);
        } else if (name[tenor_start] == '>') {
            String_ delay_increment = name.substr(tenor_start + 1, name.length() - tenor_start - 1);
            return new Equity_(eq_name, nullptr, &delay_increment);
        } else
            THROW("index pattern is not good");
    }
} // namespace Dal::Index
