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
        explicit PastEvaluator_(size_t nVar, const Vector_<double>& const_variables = Vector_<double>())
            : Base(nVar, const_variables) {}

        using Base::Visit;
        using Base::VisitNode;

        FORCE_INLINE void Visit(const NodePays_& node) {
            //	Visit the RHS expression
            VisitNode(*node.arguments_[1]);

            // no need to save to the left as it is passed
        }

        FORCE_INLINE void Visit(const NodeSpot_&) {
            // TODO: just push 0 as now
            dStack_.Push(0.0);
        }

        [[nodiscard]] FORCE_INLINE const Vector_<>& Variables() const {
            return variables_;
        }
    };

} // namespace Dal::Script

