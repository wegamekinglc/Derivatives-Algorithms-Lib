//
// Created by dal-implementer on 2026/7/13.
//

#pragma once

#include <dal/platform/platform.hpp>

#include <map>
#include <optional>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/datetime.hpp>

namespace Dal {
    class Ccy_;
    struct CurrencyPair_;

    struct FixingRequest_ {
        String_ indexName_;
        DateTime_ fixingTime_;
    };

    class MarketFixingSnapshot_ {
    public:
        using history_t = std::map<DateTime_, double>;
        using values_t = std::map<String_, history_t>;

    private:
        const values_t values_;

    public:
        explicit MarketFixingSnapshot_(const values_t& values = values_t());
        [[nodiscard]] std::optional<double> Find(const String_& indexName, const DateTime_& fixingTime) const;
        [[nodiscard]] double Require(const String_& indexName, const DateTime_& fixingTime, const String_& context) const;
    };

    Handle_<MarketFixingSnapshot_> SnapshotGlobalFixings(const Vector_<FixingRequest_>& requests);
    String_ FxIndexName(const Ccy_& domestic, const Ccy_& foreign);
    String_ FxIndexName(const CurrencyPair_& pair);
    String_ ReverseFxIndexName(const CurrencyPair_& pair);
} // namespace Dal
