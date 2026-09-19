//
// Created by dal-implementer on 2026/9/20.
//

#pragma once

#include <dal/script/visitor/evaluator.hpp>

namespace Dal::Script {

    //  Tree-walk evaluator that records the LSMC driver's forward-pass data:
    //  raw (undiscounted) payments per PAYS event and the (x, h, condition)
    //  triple per exercise event. EXERCISE stays a no-op on the script state;
    //  the arithmetic of the accumulated payoff variable mirrors EvaluatorBase_
    //  statement for statement, so never-exercised paths stay bitwise identical
    //  to the plain evaluation of the same script.
    template <class T_> class LsmcEvaluator_ : public EvaluatorBase_<T_, LsmcEvaluator_> {
        static_assert(std::is_same_v<T_, double>, "the LSMC recording evaluator exists for the double hard-decision mode only");

    public:
        using Base = EvaluatorBase_<T_, LsmcEvaluator_>;
        using Base::bStack_;
        using Base::curEvt_;
        using Base::dStack_;
        using Base::scenario_;
        using Base::variables_;
        using Base::Visit;
        using Base::VisitNode;

        LsmcEvaluator_(const Vector_<>& variables, const Vector_<T_>& constVariables) : Base(variables, constVariables) {}

        //  Driver-installed recording sinks; storage rows are indexed by global path slot
        const Vector_<size_t>* eventToPays_ = nullptr;
        const Vector_<size_t>* eventToExercise_ = nullptr;
        Vector_<Vector_<>>* paysStorage_ = nullptr;
        Vector_<Vector_<>>* xStorage_ = nullptr;
        Vector_<Vector_<>>* hStorage_ = nullptr;
        Vector_<Vector_<char>>* condStorage_ = nullptr; //  empty row = unconditional day
        size_t pathSlot_ = 0;
        size_t eventOrdinal_ = 0;

        void SetEventOrdinal(size_t event) { eventOrdinal_ = event; }

        FORCE_INLINE void Visit(const NodePays_& node) {
            const auto varIdx = Downcast<NodeVar_>(node.arguments_[0])->index_;
            VisitNode(*node.arguments_[1]);
            const T_ payment = dStack_.TopAndPop();
            (*paysStorage_)[(*eventToPays_)[eventOrdinal_]][pathSlot_] += payment;
            variables_[varIdx] += payment / (*scenario_)[curEvt_].numeraire_;
        }

        void Visit(const NodeExercise_& node) {
            VisitNode(*node.arguments_[0]);
            const double value = dStack_.TopAndPop();
            double cond = 1.0;
            if (node.arguments_.size() > 1) {
                VisitNode(*node.arguments_[1]);
                cond = bStack_.TopAndPop() ? 1.0 : 0.0;
            }
            const size_t slot = (*eventToExercise_)[eventOrdinal_];
            (*xStorage_)[slot][pathSlot_] = (*scenario_)[curEvt_].spot_;
            (*hStorage_)[slot][pathSlot_] = value;
            if (condStorage_) {
                auto& row = (*condStorage_)[slot];
                if (!row.empty())
                    row[pathSlot_] = static_cast<char>(cond);
            }
        }
    };
} // namespace Dal::Script
