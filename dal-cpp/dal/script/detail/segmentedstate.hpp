//
// Created by Codex on 2026/10/9.
//

#pragma once

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <tuple>
#include <utility>
#include <vector>

#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/script/observationplan.hpp>

namespace Dal::Script::Detail {
    class SegmentedObservationPlan_ {
        struct Interval_ {
            size_t first_;
            size_t last_;
            size_t request_;
        };

        std::shared_ptr<const ObservationPlan_> plan_;
        Vector_<std::optional<size_t>> slots_;
        Vector_<Vector_<size_t>> produced_;
        size_t count_ = 0;

        [[nodiscard]] size_t LastUse(const ObservationRequest_& request) const {
            size_t last = request.modelSlot_->sampleId_;
            for (const auto& use : request.uses_) {
                const auto& events = plan_->LiveEventIds();
                const auto found = std::lower_bound(events.begin(), events.end(), use.eventId_);
                if (found == events.end() || *found != use.eventId_)
                    continue;
                const size_t event = static_cast<size_t>(found - events.begin());
                const size_t sample = plan_->EventToSample()[event];
                REQUIRE2(sample >= request.modelSlot_->sampleId_ && sample < produced_.size(),
                         "SegmentedObservation: observation use precedes production or is outside the timeline", ScriptError_);
                last = std::max(last, sample);
            }
            return last;
        }

        [[nodiscard]] Vector_<Interval_> Intervals() const {
            Vector_<Interval_> result;
            for (size_t i = 0; i < slots_.size(); ++i) {
                const auto& request = plan_->Request(i);
                if (!request.modelSlot_)
                    continue;
                const auto& slot = *request.modelSlot_;
                REQUIRE2(slot.sampleId_ < produced_.size() && slot.outputId_ < plan_->DefLine()[slot.sampleId_].indexNames_.size(),
                         "SegmentedObservation: model output is outside the sample definition", ScriptError_);
                result.push_back({slot.sampleId_, LastUse(request), i});
            }
            std::sort(result.begin(), result.end(), [](const Interval_& lhs, const Interval_& rhs) {
                return std::tie(lhs.first_, lhs.request_) < std::tie(rhs.first_, rhs.request_);
            });
            return result;
        }

        void AllocateSlots(const Vector_<Interval_>& intervals) {
            using EndSlot_ = std::pair<size_t, size_t>;
            std::priority_queue<EndSlot_, std::vector<EndSlot_>, std::greater<EndSlot_>> live;
            std::priority_queue<size_t, std::vector<size_t>, std::greater<size_t>> available;
            for (const auto& interval : intervals) {
                // The last consuming event must finish before its slot is reused.
                while (!live.empty() && live.top().first < interval.first_) {
                    available.push(live.top().second);
                    live.pop();
                }
                const size_t slot = available.empty() ? count_++ : available.top();
                if (!available.empty())
                    available.pop();
                slots_[interval.request_] = slot;
                produced_[interval.first_].push_back(interval.request_);
                live.emplace(interval.last_, slot);
            }
        }

    public:
        explicit SegmentedObservationPlan_(std::shared_ptr<const ObservationPlan_> plan) : plan_(std::move(plan)) {
            REQUIRE2(plan_, "SegmentedObservation: plan must be present", ScriptError_);
            REQUIRE2(plan_->TimeLine().size() == plan_->DefLine().size() && plan_->LiveEventIds().size() == plan_->EventToSample().size(),
                     "SegmentedObservation: inconsistent prepared timeline or event mapping", ScriptError_);
            slots_.Resize(plan_->Requests().size());
            produced_.Resize(plan_->TimeLine().size());
            AllocateSlots(Intervals());
        }

        [[nodiscard]] size_t Slots() const { return count_; }

        [[nodiscard]] std::optional<size_t> Slot(size_t requestId) const {
            (void)plan_->Request(requestId);
            return slots_[requestId];
        }

        template <class T_> [[nodiscard]] T_ Read(size_t requestId, const Vector_<T_>& values) const {
            REQUIRE2(values.size() == Slots(), "SegmentedObservation: storage size must equal Slots", ScriptError_);
            const auto& request = plan_->Request(requestId);
            if (request.historyValueId_)
                return T_(plan_->KnownValue(*request.historyValueId_));
            REQUIRE2(slots_[requestId], "SegmentedObservation: unresolved model observation", ScriptError_);
            return values[*slots_[requestId]];
        }

        template <class T_> void Capture(size_t sampleId, const AAD::Sample_<T_>& sample, Vector_<T_>* values) const {
            REQUIRE2(values && values->size() == Slots(), "SegmentedObservation: storage size must equal Slots", ScriptError_);
            REQUIRE2(sampleId < produced_.size(), "SegmentedObservation: sampleId is outside the timeline", ScriptError_);
            for (const size_t request : produced_[sampleId])
                REQUIRE2(plan_->Request(request).modelSlot_->outputId_ < sample.observations_.size(),
                         "SegmentedObservation: emitted observation is outside the sample", ScriptError_);
            for (const size_t request : produced_[sampleId])
                (*values)[*slots_[request]] = sample.observations_[plan_->Request(request).modelSlot_->outputId_];
        }
    };

    class SegmentedEvalLayout_ {
        struct VectorBound_ {
            size_t indexed_ = 0;
            size_t appends_ = 0;
        };

        size_t scalars_;
        size_t observations_;
        Vector_<size_t> bounds_;
        Vector_<size_t> offsets_;
        size_t observationOffset_;
        size_t size_;

        [[nodiscard]] static size_t Sum(size_t lhs, size_t rhs) {
            REQUIRE2(rhs <= std::numeric_limits<size_t>::max() - lhs, "SegmentedState: state dimension overflow", ScriptError_);
            return lhs + rhs;
        }

        static void BoundNode(const Node_& node, Vector_<VectorBound_>* bounds) {
            if (const auto* entry = dynamic_cast<const NodeVectorEntry_*>(&node)) {
                REQUIRE2(entry->index_ >= 0 && static_cast<size_t>(entry->index_) < bounds->size(),
                         "SegmentedState: vector entry has no prepared index", ScriptError_);
                auto& bound = (*bounds)[entry->index_];
                bound.indexed_ = std::max(bound.indexed_, Sum(entry->entry_, 1));
            }
            if (const auto* append = dynamic_cast<const NodeVectorAppend_*>(&node)) {
                REQUIRE2(append->index_ >= 0 && static_cast<size_t>(append->index_) < bounds->size(),
                         "SegmentedState: vector append has no prepared index", ScriptError_);
                auto& bound = (*bounds)[append->index_];
                bound.appends_ = Sum(bound.appends_, 1);
            }
            for (const auto& child : node.arguments_)
                BoundNode(*child, bounds);
        }

        [[nodiscard]] static Vector_<size_t> VectorBounds(const ScriptProduct_& product) {
            Vector_<VectorBound_> proof(product.VectorNames().size());
            for (const auto* events : {&product.PastEvents(), &product.Events()})
                for (const auto& event : *events)
                    for (const auto& statement : event)
                        BoundNode(*statement, &proof);
            REQUIRE2(product.VectorValues().size() == proof.size(), "SegmentedState: historical vector shape mismatch", ScriptError_);
            Vector_<size_t> result(proof.size());
            for (size_t i = 0; i < proof.size(); ++i) {
                result[i] = std::max(Sum(proof[i].indexed_, proof[i].appends_), product.VectorValues()[i].size());
                REQUIRE2(result[i] <= 9007199254740992ULL, "SegmentedState: vector length exceeds exact integer storage", ScriptError_);
            }
            return result;
        }

        template <class T_> void ValidateShape(const EvalState_<T_>& state, const Vector_<T_>& observations) const {
            REQUIRE2(state.variables_.size() == scalars_ && state.vectors_.size() == bounds_.size() && observations.size() == observations_,
                     "SegmentedState: evaluator or observation shape mismatch", ScriptError_);
        }

        template <class T_> [[nodiscard]] size_t Length(const Vector_<T_>& packed, size_t vector) const {
            const double length = AAD::Value(packed[offsets_[vector]]);
            REQUIRE2(std::isfinite(length) && length >= 0.0 && length <= static_cast<double>(bounds_[vector]) && std::floor(length) == length,
                     "SegmentedState: vector logical length must be an integer within its proved bound", ScriptError_);
            return static_cast<size_t>(length);
        }

    public:
        SegmentedEvalLayout_(const ScriptProduct_& product, size_t observations)
            : scalars_(product.VarNames().size()), observations_(observations), bounds_(VectorBounds(product)), offsets_(bounds_.size()) {
            size_t offset = Sum(1, scalars_);
            for (size_t i = 0; i < bounds_.size(); ++i) {
                offsets_[i] = offset;
                offset = Sum(Sum(offset, 1), bounds_[i]);
            }
            observationOffset_ = offset;
            size_ = Sum(offset, observations_);
        }

        [[nodiscard]] size_t Size() const { return size_; }
        [[nodiscard]] const Vector_<size_t>& VectorBounds() const { return bounds_; }

        template <class T_> [[nodiscard]] Vector_<T_> Pack(const T_& logSpot, const EvalState_<T_>& state, const Vector_<T_>& observations) const {
            ValidateShape(state, observations);
            Vector_<T_> result(Size(), T_(0.0));
            result[0] = logSpot;
            if (scalars_)
                std::copy(state.variables_.begin(), state.variables_.end(), result.begin() + 1);
            for (size_t i = 0; i < bounds_.size(); ++i) {
                REQUIRE2(state.vectors_[i].size() <= bounds_[i], "SegmentedState: vector exceeds its proved bound", ScriptError_);
                result[offsets_[i]] = T_(static_cast<double>(state.vectors_[i].size()));
                if (!state.vectors_[i].empty())
                    std::copy(state.vectors_[i].begin(), state.vectors_[i].end(), result.begin() + offsets_[i] + 1);
            }
            if (observations_)
                std::copy(observations.begin(), observations.end(), result.begin() + observationOffset_);
            return result;
        }

        template <class T_> void Restore(const Vector_<T_>& packed, T_* logSpot, EvalState_<T_>* state, Vector_<T_>* observations) const {
            REQUIRE2(logSpot && state && observations, "SegmentedState: restore outputs must be present", ScriptError_);
            REQUIRE2(packed.size() == Size(), "SegmentedState: packed state size mismatch", ScriptError_);
            ValidateShape(*state, *observations);
            Vector_<size_t> lengths(bounds_.size());
            for (size_t i = 0; i < bounds_.size(); ++i)
                lengths[i] = Length(packed, i);
            *logSpot = packed[0];
            if (scalars_)
                std::copy(packed.begin() + 1, packed.begin() + 1 + scalars_, state->variables_.begin());
            for (size_t i = 0; i < bounds_.size(); ++i) {
                state->vectors_[i].Resize(lengths[i]);
                if (lengths[i])
                    std::copy(packed.begin() + offsets_[i] + 1, packed.begin() + offsets_[i] + 1 + lengths[i], state->vectors_[i].begin());
            }
            if (observations_)
                std::copy(packed.begin() + observationOffset_, packed.end(), observations->begin());
            state->dStack_.Reset();
            state->bStack_.Reset();
            state->nestedIfLvl_ = 0;
        }
    };
} // namespace Dal::Script::Detail
