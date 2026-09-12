//
// Created by Codex on 2026/9/13.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/script/lexer.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    class Index_;

    namespace Script {
        struct ObservationKey_ {
            String_ canonicalIndex_;
            DateTime_ fixingTime_;
            bool operator<(const ObservationKey_& rhs) const {
                return std::tie(canonicalIndex_, fixingTime_) < std::tie(rhs.canonicalIndex_, rhs.fixingTime_);
            }
        };

        struct ObservationUse_ {
            SourceLocation_ source_;
            size_t eventId_;
            size_t statementId_;
            size_t nodeId_;
        };

        struct ObservationRequest_ {
            Handle_<Index_> index_;
            ObservationKey_ key_;
            Vector_<ObservationUse_> uses_;
            std::optional<size_t> historyValueId_;
        };

        class ObservationPlan_ {
            Vector_<ObservationRequest_> requests_;
            Vector_<> knownValues_;

        public:
            ObservationPlan_(Vector_<ObservationRequest_>&& requests, Vector_<>&& knownValues)
                : requests_(std::move(requests)), knownValues_(std::move(knownValues)) {}
            [[nodiscard]] const Vector_<ObservationRequest_>& Requests() const { return requests_; }
            [[nodiscard]] const Vector_<>& KnownValues() const { return knownValues_; }
            [[nodiscard]] const ObservationRequest_& Request(size_t requestId) const {
                REQUIRE2(requestId < requests_.size(), "ObservationIdOutOfRange", ScriptError_);
                return requests_[requestId];
            }
            [[nodiscard]] double KnownValue(size_t valueId) const {
                REQUIRE2(valueId < knownValues_.size(), "HistoryValueIdOutOfRange", ScriptError_);
                return knownValues_[valueId];
            }
        };
    } // namespace Script
} // namespace Dal
