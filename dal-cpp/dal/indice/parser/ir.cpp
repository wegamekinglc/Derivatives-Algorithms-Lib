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

        std::unique_ptr<Index_> Rate(const Vector_<String_>& values, bool explicitSwap) {
            const size_t offset = explicitSwap ? 1 : 0;
            REQUIRE(values.size() == offset + 2 || values.size() == offset + 3, "InvalidIndex: IR rate requires currency, tenor and optional start");
            const Ccy_ ccy(values[offset]);
            const String_ tenor = values[offset + 1];
            const Cell_ start = values.size() == offset + 3 ? DateOrIncrement(values[offset + 2]) : Cell_();
            if (explicitSwap || IsSwapTenor(tenor)) {
                REQUIRE(IsSwapTenor(tenor), "InvalidIndex: unsupported swap tenor " + tenor);
                return std::make_unique<Swap_>(ccy, tenor, start);
            }
            REQUIRE(IsLiborTenor(tenor), "InvalidIndex: unsupported IR tenor " + tenor);
            return std::make_unique<Libor_>(ccy, TradedRate_(tenor), start);
        }
    } // namespace

    std::unique_ptr<Index_> IRParser(const String_& name) {
        if (name.substr(0, 7) == "IR[DF]:")
            return Discount(Parts(name.substr(7)), 0);
        if (name.substr(0, 3) == "IR:")
            return Rate(Parts(name.substr(3)), false);
        REQUIRE(name.substr(0, 3) == "IR[" && name.back() == ']',
                "InvalidIndex: expected IR[currency,DF,maturity], IR[currency,rate] or a canonical IR name");
        const auto values = Parts(name.substr(3, name.size() - 4));
        REQUIRE(values.size() >= 2, "InvalidIndex: incomplete IR index");
        if (values[1] == "DF") {
            REQUIRE(values.size() == 3 || values.size() == 4, "InvalidIndex: discount requires maturity and optional start");
            Vector_<String_> discount{values[0]};
            for (size_t i = 2; i < values.size(); ++i)
                discount.push_back(values[i]);
            return Discount(discount, 0);
        }
        if (values[1] == "SWAP") {
            REQUIRE(values.size() == 3 || values.size() == 4, "InvalidIndex: swap requires a tenor and optional start");
            Vector_<String_> rate{values[0], values[2]};
            if (values.size() == 4)
                rate.push_back(values[3]);
            return Rate(rate, false);
        }
        return Rate(values, false);
    }
} // namespace Dal::Index
