//
// Created by Codex on 2026/9/13.
//

#pragma once

#include <tuple>

#include <dal/math/aad/sample.hpp>
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
            String_ indexOriginal_;
            std::optional<Date_> fixingDate_;
            bool legacySpot_ = false;
        };

        struct ModelObservation_ {
            size_t sampleId_;
            size_t outputId_;
        };

        //  Shared field context for observation errors; order is pinned by substring-matching tests
        [[nodiscard]] inline String_ ObservationErrorContext(const String_& original,
                                                             const String_& canonical,
                                                             const DateTime_& fixingTime,
                                                             const SourceLocation_& source,
                                                             size_t statementId,
                                                             size_t nodeId) {
            return "; index=" + canonical + "; fixing=" + DateTime::ToString(fixingTime) + "; " + source.Describe() + "; original=" + original +
                   "; canonical=" + canonical + "; statement=" + String_(std::to_string(statementId)) + "; node=n" + String_(std::to_string(nodeId));
        }

        [[nodiscard]] inline String_ LookAheadObservationError(const String_& original,
                                                               const String_& canonical,
                                                               const Date_& fixingDate,
                                                               const SourceLocation_& source,
                                                               size_t statementId,
                                                               size_t nodeId) {
            return "LookAheadObservation: expected fixing <= event" +
                   ObservationErrorContext(original, canonical, DateTime_(fixingDate, 0.0), source, statementId, nodeId);
        }

        struct ObservationRequest_ {
            Handle_<Index_> index_;
            ObservationKey_ key_;
            Vector_<ObservationUse_> uses_;
            std::optional<size_t> historyValueId_;
            std::optional<ModelObservation_> modelSlot_;
            bool historical_ = false;
        };

        class ObservationPlan_ {
            Vector_<ObservationRequest_> requests_;
            Vector_<> knownValues_;
            Vector_<Date_> sampleDates_;
            Vector_<> timeLine_;
            Vector_<AAD::SampleDef_> defLine_;
            Vector_<size_t> eventToSample_;
            Vector_<size_t> liveEventIds_;
            Vector_<String_> modelBindingNames_;

            friend class PreparedScript_;
            friend class PreparedScriptBuilder_;

        public:
            ObservationPlan_(Vector_<ObservationRequest_>&& requests, Vector_<>&& knownValues)
                : requests_(std::move(requests)), knownValues_(std::move(knownValues)) {}
            [[nodiscard]] const Vector_<ObservationRequest_>& Requests() const { return requests_; }
            [[nodiscard]] const Vector_<>& KnownValues() const { return knownValues_; }
            [[nodiscard]] const Vector_<Date_>& SampleDates() const { return sampleDates_; }
            [[nodiscard]] const Vector_<>& TimeLine() const { return timeLine_; }
            [[nodiscard]] const Vector_<AAD::SampleDef_>& DefLine() const { return defLine_; }
            [[nodiscard]] const Vector_<size_t>& EventToSample() const { return eventToSample_; }
            [[nodiscard]] const Vector_<size_t>& LiveEventIds() const { return liveEventIds_; }
            [[nodiscard]] const Vector_<String_>& ModelBindingNames() const { return modelBindingNames_; }

            template <class T_> T_ Read(size_t requestId, const AAD::Scenario_<T_>* scenario) const {
                const auto& request = Request(requestId);
                if (request.historyValueId_)
                    return T_(KnownValue(*request.historyValueId_));
                REQUIRE2(request.modelSlot_ && scenario, "UnresolvedModelObservation", ScriptError_);
                const auto& slot = *request.modelSlot_;
                REQUIRE2(slot.sampleId_ < scenario->size() && slot.outputId_ < (*scenario)[slot.sampleId_].observations_.size(),
                         "ModelObservationOutOfRange", ScriptError_);
                return (*scenario)[slot.sampleId_].observations_[slot.outputId_];
            }
            [[nodiscard]] const ObservationRequest_& Request(size_t requestId) const {
                REQUIRE2(requestId < requests_.size(), "ObservationIdOutOfRange", ScriptError_);
                return requests_[requestId];
            }
            [[nodiscard]] double KnownValue(size_t valueId) const {
                REQUIRE2(valueId < knownValues_.size(), "HistoryValueIdOutOfRange", ScriptError_);
                return knownValues_[valueId];
            }
            //  Resolved historical value for a request, when one was frozen at preparation
            [[nodiscard]] std::optional<double> TryKnownValue(size_t requestId) const {
                const auto& request = Request(requestId);
                if (!request.historyValueId_)
                    return std::nullopt;
                return KnownValue(*request.historyValueId_);
            }
        };
    } // namespace Script
} // namespace Dal
