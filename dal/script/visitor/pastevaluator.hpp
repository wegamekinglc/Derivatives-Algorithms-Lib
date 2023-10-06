//
// Created by wegam on 2023/6/24.
//

#pragma once

#include <dal/script/visitor/evaluator.hpp>


namespace Dal::Script {

    template <class T_>
    class PastEvaluator_: public EvaluatorBase_<T_, PastEvaluator_> {
    public:
        using Base = EvaluatorBase_<T_, PastEvaluator_>;
        explicit PastEvaluator_(const Vector_<T_>& variables, const Vector_<T_>& const_variables = Vector_<T_>())
            : Base(variables, const_variables) {}

        using Base::Visit;
        using Base::VisitNode;
        using Base::variables_;
        using Base::dStack_;

        FORCE_INLINE void Visit(const NodePays_& node) {
            //	Visit the RHS expression
            VisitNode(*node.arguments_[1]);
        }

        FORCE_INLINE void Visit(const NodeSpot_&) {
            // TODO: just push 0 as now
            dStack_.Push(30.0);
        }

        [[nodiscard]] FORCE_INLINE const Vector_<>& Variables() const {
            return variables_;
        }
    };

} // namespace Dal::Script

