//
// Created by Codex on 2026/9/28.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/indice/index/ir.hpp>
#include <dal/indice/parser/ir.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/dateutils.hpp>

namespace Dal::Index {
    namespace {
        Vector_<String_> Parts(const String_& body) {
            Vector_<String_> result;
            size_t start = 0;
            while (start <= body.size()) {
                const size_t end = body.find(',', start);
                result.push_back(body.substr(start, end == String_::npos ? String_::npos : end - start));
                if (end == String_::npos)
                    break;
                start = end + 1;
            }
            for (const auto& value : result)
                REQUIRE(!value.empty(), "InvalidIndex: empty IR index field");
            return result;
        }

        Cell_ DateOrIncrement(const String_& value) {
            if (value.size() == 10 && value[4] == '-' && value[7] == '-') {
                const Date_ date = Date::FromString(value);
                REQUIRE(date.IsValid(), "InvalidIndex: invalid IR date " + value);
                return Cell_(date);
            }
            REQUIRE(!Date::ParseIncrement(value).IsEmpty(), "InvalidIndex: invalid IR date increment " + value);
            return Cell_(value);
        }

        std::unique_ptr<Index_> Discount(const Vector_<String_>& values, size_t offset) {
            REQUIRE(values.size() == offset + 2 || values.size() == offset + 3,
                    "InvalidIndex: IR discount requires currency, maturity and optional start");
            const Ccy_ ccy(values[offset]);
            if (values.size() == offset + 2)
                return std::make_unique<DF_>(ccy, DateOrIncrement(values[offset + 1]));
            const Cell_ start = DateOrIncrement(values[offset + 1]);
            return std::make_unique<DF_>(ccy, DateOrIncrement(values[offset + 2]), &start);
        }

        std::unique_ptr<Index_> Rate(const Vector_<String_>& values) {
            REQUIRE(values.size() == 2 || values.size() == 3, "InvalidIndex: IR rate requires currency, tenor and optional start");
            const Ccy_ ccy(values[0]);
            const String_ tenor = values[1];
            const Cell_ start = values.size() == 3 ? DateOrIncrement(values[2]) : Cell_();
            if (IsSwapTenor(tenor))
                return std::make_unique<Swap_>(ccy, tenor, start);
            REQUIRE(IsLiborTenor(tenor), "InvalidIndex: unsupported IR tenor " + tenor);
            return std::make_unique<Libor_>(ccy, TradedRate_(tenor), start);
        }

        std::unique_ptr<Index_> BracketedDiscount(const Vector_<String_>& values) {
            REQUIRE(values.size() == 3 || values.size() == 4, "InvalidIndex: discount requires maturity and optional start");
            Vector_<String_> discount{values[0]};
            if (values.size() == 4)
                discount.push_back(values[3]);
            discount.push_back(values[2]);
            return Discount(discount, 0);
        }

        std::unique_ptr<Index_> BracketedSwap(const Vector_<String_>& values) {
            REQUIRE(values.size() == 3 || values.size() == 4, "InvalidIndex: swap requires a tenor and optional start");
            REQUIRE(!Date::ParseIncrement(values[2]).IsEmpty(), "InvalidIndex: unsupported swap tenor " + values[2]);
            const Cell_ start = values.size() == 4 ? DateOrIncrement(values[3]) : Cell_();
            return std::make_unique<Swap_>(Ccy_(values[0]), values[2], start);
        }

        std::unique_ptr<Index_> Bracketed(const Vector_<String_>& values) {
            REQUIRE(values.size() >= 2, "InvalidIndex: incomplete IR index");
            if (values[1] == "DF")
                return BracketedDiscount(values);
            if (values[1] == "SWAP")
                return BracketedSwap(values);
            return Rate(values);
        }
    } // namespace

    std::unique_ptr<Index_> IRParser(const String_& name) {
        if (name.substr(0, 7) == "IR[DF]:")
            return Discount(Parts(name.substr(7)), 0);
        if (name.substr(0, 3) == "IR:")
            return Rate(Parts(name.substr(3)));
        REQUIRE(name.substr(0, 3) == "IR[" && name.back() == ']',
                "InvalidIndex: expected IR[currency,DF,maturity], IR[currency,rate] or a canonical IR name");
        return Bracketed(Parts(name.substr(3, name.size() - 4)));
    }
} // namespace Dal::Index
