//
// Created by wegam on 2022/1/23.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/indice/index/equity.hpp>
#include <dal/indice/parser/equity.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/dateutils.hpp>

namespace Dal::Index {
    std::unique_ptr<Index_> EquityParser(const String_& name) try {
        NOTICE(name);
        const auto eq_stop = name.find(']');
        REQUIRE(name.substr(0, 3) == "EQ[" && eq_stop != String_::npos && eq_stop > 3, "InvalidIndex: equity index must contain a nonempty EQ[name]");
        const String_ eq_name = name.substr(3, eq_stop - 3);
        REQUIRE(eq_name.find_first_of("[]\"'\r\n") == String_::npos && name.find(']', eq_stop + 1) == String_::npos,
                "InvalidIndex: malformed equity index brackets or name");
        const auto tenor_start = eq_stop + 1;
        if (tenor_start == name.size())
            return std::make_unique<Equity_>(eq_name, nullptr, nullptr);

        if (name[tenor_start] == '@') {
            auto delivery_date = Date::FromString(name.substr(tenor_start + 1, name.length() - tenor_start - 1));
            REQUIRE(delivery_date.IsValid(), "InvalidIndex: invalid equity delivery date");
            return std::make_unique<Equity_>(eq_name, &delivery_date, nullptr);
        }

        if (name[tenor_start] == '>') {
            String_ delay_increment = name.substr(tenor_start + 1, name.length() - tenor_start - 1);
            REQUIRE(!delay_increment.empty() && delay_increment.front() != '&' && delay_increment.back() != '&' &&
                        delay_increment.find("&&") == String_::npos,
                    "InvalidIndex: equity delivery increment contains an empty component");
            REQUIRE(!Date::ParseIncrement(delay_increment).IsEmpty(), "InvalidIndex: invalid equity delivery increment");
            return std::make_unique<Equity_>(eq_name, nullptr, &delay_increment);
        }
        THROW("InvalidIndex: unexpected trailing equity index characters");
    } catch (const std::logic_error& error) {
        THROW("InvalidIndex: " + name + "; " + String_(error.what()));
    }
} // namespace Dal::Index
