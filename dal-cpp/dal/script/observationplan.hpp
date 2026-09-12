//
// Created by Codex on 2026/9/13.
//

#pragma once

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
        };

        struct ModelObservation_ {
            size_t sampleId_;
            size_t outputId_;
        };

        struct ObservationRequest_ {
            Handle_<Index_> index_;
            ObservationKey_ key_;
            Vector_<ObservationUse_> uses_;
            std::optional<size_t> historyValueId_;
            std::optional<ModelObservation_> modelSlot_;
        };

        class ObservationPlan_ {
            Vector_<ObservationRequest_> requests_;
            Vector_<> knownValues_;
            Vector_<Date_> sampleDates_;
            Vector_<> timeLine_;
            Vector_<AAD::SampleDef_> defLine_;
            Vector_<size_t> eventToSample_;

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
        };
    } // namespace Script
} // namespace Dal
