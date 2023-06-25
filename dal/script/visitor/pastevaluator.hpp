//
// Created by wegam on 2023/6/24.
//

#pragma once

#include <dal/script/visitor/evaluator.hpp>


namespace Dal::Script {

    class PastEvaluator_: public Evaluator_<double> {
    public:
        using Base = Evaluator_<double>;

        using Evaluator_<double>::Visit;

        FORCE_INLINE void Visit(const NodePays_& node) {
            //	Visit the RHS expression
            VisitNode(*node.arguments_[1]);
        }
    };

} // namespace Dal::Script

