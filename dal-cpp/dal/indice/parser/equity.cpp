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
    namespace {
        size_t EquityNameEnd(const String_& name) {
            const auto stop = name.find(']');
            REQUIRE(name.substr(0, 3) == "EQ[" && stop != String_::npos && stop > 3, "equity index must contain a nonempty EQ[name]");
            REQUIRE(name.substr(3, stop - 3).find_first_of("[]\"'\r\n") == String_::npos && name.find(']', stop + 1) == String_::npos,
                    "malformed equity index brackets or name");
            return stop;
        }

        void ValidateDeliveryIncrement(const String_& increment) {
            REQUIRE(!increment.empty() && increment.front() != '&' && increment.back() != '&' && increment.find("&&") == String_::npos,
                    "equity delivery increment contains an empty component");
            REQUIRE(!Date::ParseIncrement(increment).IsEmpty(), "invalid equity delivery increment");
        }

        [[noreturn]] void InvalidEquity(const String_& name, const std::exception& error) {
            THROW("InvalidIndex: " + name + "; " + String_(error.what()));
        }
    } // namespace

    std::unique_ptr<Index_> EquityParser(const String_& name) try {
        NOTICE(name);
        const auto eq_stop = EquityNameEnd(name);
        const String_ eq_name = name.substr(3, eq_stop - 3);
        const auto tenor_start = eq_stop + 1;
        if (tenor_start == name.size())
            return std::make_unique<Equity_>(eq_name, nullptr, nullptr);

        if (name[tenor_start] == '@') {
            auto delivery_date = Date::FromString(name.substr(tenor_start + 1, name.length() - tenor_start - 1));
            REQUIRE(delivery_date.IsValid(), "invalid equity delivery date");
            return std::make_unique<Equity_>(eq_name, &delivery_date, nullptr);
        }

        if (name[tenor_start] == '>') {
            String_ delay_increment = name.substr(tenor_start + 1, name.length() - tenor_start - 1);
            ValidateDeliveryIncrement(delay_increment);
            return std::make_unique<Equity_>(eq_name, nullptr, &delay_increment);
        }
        THROW("unexpected trailing equity index characters");
    } catch (const Exception_& error) {
        InvalidEquity(name, error);
    } catch (const std::logic_error& error) {
        InvalidEquity(name, error);
    }
} // namespace Dal::Index
