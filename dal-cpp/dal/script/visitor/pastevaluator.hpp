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
        explicit PastEvaluator_(const Vector_<T_>& variables, const Vector_<T_>& constVariables = Vector_<T_>())
            : Base(variables, constVariables) {}

        using Base::Visit;
        using Base::VisitNode;
        using Base::variables_;
        using Base::dStack_;

        FORCE_INLINE void Visit(const NodePays_& node) {
            //	Visit the RHS expression
            VisitNode(*node.arguments_[1]);
            dStack_.Pop();
        }

        FORCE_INLINE void Visit(const NodeSpot_& node) {
            REQUIRE2(node.observationId_ && this->observations_, "UnboundHistoricalSpot: SPOT() requires a default index", ScriptError_);
            Base::Visit(node);
        }

        [[nodiscard]] FORCE_INLINE const Vector_<>& Variables() const {
            return variables_;
        }
    };

} // namespace Dal::Script
