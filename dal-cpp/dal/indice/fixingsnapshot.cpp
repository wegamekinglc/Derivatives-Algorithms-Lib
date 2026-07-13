//
// Created by dal-implementer on 2026/7/13.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <dal/indice/fixingsnapshot.hpp>
#include <dal/storage/globals.hpp>

namespace Dal {
    namespace {
        std::optional<String_> ReverseCanonicalFxName(const String_& name) {
            if (name.size() < 8 || name.compare(0, 3, "FX[") != 0 || name.back() != ']')
                return std::nullopt;
            const auto slash = name.find('/', 3);
            if (slash == String_::npos || slash == 3 || slash + 1 >= name.size() - 1 || name.find('/', slash + 1) != String_::npos)
                return std::nullopt;
            const String_ numerator(name.begin() + 3, name.begin() + slash);
            const String_ denominator(name.begin() + slash + 1, name.end() - 1);
            return String_(String_("FX[") + denominator + String_("/") + numerator + String_("]"));
        }

        const double* FindValue(const FixHistory_& history, const DateTime_& fixingTime) {
            for (const auto& item : history.vals_) {
                if (item.first == fixingTime)
                    return &item.second;
            }
            return nullptr;
        }
    } // namespace

    MarketFixingSnapshot_::MarketFixingSnapshot_(const values_t& values) : values_(values) {
        for (const auto& indexHistory : values_) {
            const String_& indexName = indexHistory.first;
            REQUIRE(!indexName.empty(), "Market fixing snapshot requires non-empty index names");
            for (const auto& fixing : indexHistory.second) {
                REQUIRE(fixing.first.IsValid(), "Market fixing snapshot requires valid fixing timestamps");
                REQUIRE(std::isfinite(fixing.second) && fixing.second > 0.0,
                        "Market fixing snapshot requires positive finite values for " + indexName + " at " + DateTime::ToString(fixing.first));
            }

            const std::optional<String_> reverseName = ReverseCanonicalFxName(indexName);
            if (!reverseName.has_value())
                continue;
            const auto reverseHistory = values_.find(*reverseName);
            if (reverseHistory == values_.end())
                continue;
            for (const auto& direct : indexHistory.second) {
                const auto reverse = reverseHistory->second.find(direct.first);
                if (reverse == reverseHistory->second.end())
                    continue;
                REQUIRE(std::fabs(direct.second * reverse->second - 1.0) <= 1.0e-10,
                        "Inconsistent direct/reverse FX fixings for " + indexName + " at " + DateTime::ToString(direct.first));
            }
        }
    }

    std::optional<double> MarketFixingSnapshot_::Find(const String_& indexName, const DateTime_& fixingTime) const {
        const auto indexHistory = values_.find(indexName);
        if (indexHistory != values_.end()) {
            const auto fixing = indexHistory->second.find(fixingTime);
            if (fixing != indexHistory->second.end())
                return fixing->second;
        }

        const std::optional<String_> reverseName = ReverseCanonicalFxName(indexName);
        if (!reverseName.has_value())
            return std::nullopt;
        const auto reverseHistory = values_.find(*reverseName);
        if (reverseHistory == values_.end())
            return std::nullopt;
        const auto reverse = reverseHistory->second.find(fixingTime);
        if (reverse == reverseHistory->second.end())
            return std::nullopt;
        return 1.0 / reverse->second;
    }

    double MarketFixingSnapshot_::Require(const String_& indexName, const DateTime_& fixingTime, const String_& context) const {
        const std::optional<double> value = Find(indexName, fixingTime);
        REQUIRE(value.has_value(), "Missing fixing for " + indexName + " at " + DateTime::ToString(fixingTime) + " (" + context + ")");
        return *value;
    }

    Handle_<MarketFixingSnapshot_> SnapshotGlobalFixings(const Vector_<FixingRequest_>& requests) {
        using request_key_t = std::pair<String_, DateTime_>;
        std::set<request_key_t> uniqueRequests;
        for (const auto& request : requests)
            uniqueRequests.emplace(request.indexName_, request.fixingTime_);
        std::map<String_, FixHistory_> histories;
        MarketFixingSnapshot_::values_t values;

        auto history = [&](const String_& indexName) -> const FixHistory_& {
            auto found = histories.find(indexName);
            if (found == histories.end())
                found = histories.emplace(indexName, Global::Fixings_().History(indexName)).first;
            return found->second;
        };
        auto copyAt = [&](const String_& indexName, const DateTime_& fixingTime) {
            const double* value = FindValue(history(indexName), fixingTime);
            if (value)
                values[indexName][fixingTime] = *value;
        };

        for (const auto& request : uniqueRequests) {
            REQUIRE(!request.first.empty(), "Market fixing snapshot request requires a non-empty index name");
            REQUIRE(request.second.IsValid(), "Market fixing snapshot request requires a valid fixing timestamp");
            copyAt(request.first, request.second);
            const std::optional<String_> reverseName = ReverseCanonicalFxName(request.first);
            if (reverseName.has_value())
                copyAt(*reverseName, request.second);
        }
        return Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(values));
    }
} // namespace Dal
