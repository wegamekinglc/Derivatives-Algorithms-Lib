//
// Created by Codex on 2026/9/23.
//

#pragma once

#include <utility>

#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>

namespace Dal::Script {
    //  Backward liveness for the recording program. Exercise expressions and the
    //  selected receiver's payments are roots; both IF arms contribute dependencies.
    class LsmcProcessor_ : public Visitor_<LsmcProcessor_> {
        Vector_<char> live_;
        size_t payoffIdx_;
        bool keep_ = true;

        void ProcessRange(Event_* statements, size_t first, size_t last) {
            for (size_t i = last; i-- > first;) {
                keep_ = true;
                VisitNode(*(*statements)[i]);
                if (!keep_)
                    (*statements)[i].reset();
            }
        }

        static void Compact(Event_* statements) {
            size_t next = 0;
            for (size_t i = 0; i < statements->size(); ++i)
                if ((*statements)[i]) {
                    if (i != next)
                        (*statements)[next] = std::move((*statements)[i]);
                    ++next;
                }
            statements->Resize(next);
        }

    public:
        using Visitor_<LsmcProcessor_>::Visit;

        LsmcProcessor_(size_t nVariables, size_t payoffIdx) : live_(nVariables, 0), payoffIdx_(payoffIdx) {
            if (payoffIdx < nVariables)
                live_[payoffIdx] = 1;
        }

        void Process(Vector_<Event_>* events) {
            for (size_t e = events->size(); e-- > 0;) {
                ProcessRange(&(*events)[e], 0, (*events)[e].size());
                Compact(&(*events)[e]);
            }
        }

        void Visit(NodeVar_& node) { live_[node.index_] = 1; }

        void Visit(NodeAssign_& node) {
            const size_t index = Downcast<NodeVar_>(node.arguments_[0])->index_;
            keep_ = live_[index] != 0;
            if (keep_) {
                live_[index] = 0;
                VisitNode(*node.arguments_[1]);
            }
        }

        void Visit(NodePays_& node) {
            const size_t index = Downcast<NodeVar_>(node.arguments_[0])->index_;
            keep_ = index == payoffIdx_ || live_[index];
            if (keep_)
                VisitNode(*node.arguments_[1]);
        }

        void Visit(NodeIf_& node) {
            const auto after = live_;
            const size_t firstElse = node.HasElse() ? static_cast<size_t>(node.firstElse_) : node.arguments_.size();
            ProcessRange(&node.arguments_, firstElse, node.arguments_.size());
            const auto falseLive = live_;
            live_ = after;
            ProcessRange(&node.arguments_, 1, firstElse);
            for (size_t i = 0; i < live_.size(); ++i)
                live_[i] |= falseLive[i];
            size_t trueCount = 0;
            for (size_t i = 1; i < firstElse; ++i)
                trueCount += node.arguments_[i] != nullptr;
            if (node.HasElse())
                node.firstElse_ = static_cast<int>(1 + trueCount);
            Compact(&node.arguments_);
            keep_ = node.arguments_.size() > 1;
            if (keep_)
                VisitNode(*node.arguments_[0]);
        }

        void Visit(NodeCollect_& node) {
            ProcessRange(&node.arguments_, 0, node.arguments_.size());
            Compact(&node.arguments_);
            keep_ = !node.arguments_.empty();
        }
    };
} // namespace Dal::Script
