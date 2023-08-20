//
// Created by wegam on 2023/6/24.
//

#pragma once

#include <dal/script/visitor/evaluator.hpp>


namespace Dal::Script {

    class PastEvaluator_: public Evaluator_<double> {
    public:
        using Base = Evaluator_<double>;
        explicit PastEvaluator_(size_t nVar, const Vector_<double>& const_variables = Vector_<double>())
            : Evaluator_<double>(nVar, const_variables) {}

        using Evaluator_<double>::Visit;

        FORCE_INLINE void Visit(const NodePays_& node) {
            //	Visit the RHS expression
            VisitNode(*node.arguments_[1]);

            // no need to save to the left as it is passed
        }
    };

} // namespace Dal::Script

