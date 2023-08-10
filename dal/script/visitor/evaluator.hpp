//
// Created by wegam on 2022/6/3.
//

#pragma once

#include <dal/math/aad/sample.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/stacks.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>

namespace Dal::Script {

    template <class T_, template <typename> class EVAL_> class EvaluatorBase_ : public ConstVisitor_<EVAL_<T_>> {
    protected:
        // State
        Vector_<T_> variables_;
        Vector_<T_> const_variables_;

        // Stacks
        StaticStack_<T_> dStack_;
        StaticStack_<bool> bStack_;

        // Reference to current scenario
        const AAD::Scenario_<T_>* scenario_;

        // Index of current event
        size_t curEvt_;

    public:
        using ConstVisitor_<EVAL_<T_>>::Visit;
        using ConstVisitor_<EVAL_<T_>>::VisitNode;

        // Constructor, nVar = number of variables, from Product after parsing and variable indexation
        EvaluatorBase_(size_t nVar, const Vector_<T_>& const_variables) : variables_(nVar), const_variables_(const_variables) {}

        // Copy/Move
        EvaluatorBase_(const EvaluatorBase_& rhs) : variables_(rhs.variables_), const_variables_(rhs.const_variables_) {}
        EvaluatorBase_& operator=(const EvaluatorBase_& rhs) {
            if (this == &rhs)
                return *this;
            variables_ = rhs.variables_;
            const_variables_ = rhs.const_variables_;
            return *this;
        }

        EvaluatorBase_(EvaluatorBase_&& rhs) noexcept : variables_(std::move(rhs.variables_)), const_variables_(std::move(rhs.const_variables_)) {}
        EvaluatorBase_& operator=(EvaluatorBase_&& rhs) noexcept {
            variables_ = std::move(rhs.variables_);
            const_variables_ = std::move(rhs.const_variables_);
            return *this;
        }

        Vector_<T_>& ConstVarVals() {
            return const_variables_;
        }
        
        const Vector_<T_>& ConstVarVals() const {
            return const_variables_;
        }

        // (Re-)initialize before evaluation in each scenario
        void Init() {
            for (auto& var: variables_)
               var = T_(0.0);
            //	Stacks should be empty, if this is not the case the empty them
            //	without affecting capacity for added performance
            dStack_.Reset();
            bStack_.Reset();
        }

        // Accessors
        // Access to variable values after evaluation
        [[nodiscard]] FORCE_INLINE const Vector_<T_>& VarVals() const { return variables_; }

        // Set generated scenarios and current event
        // Set reference to current scenario
        FORCE_INLINE void SetScenario(const AAD::Scenario_<T_>* scenario) { scenario_ = scenario; }

        // Set index of current event
        FORCE_INLINE void SetCurEvt(size_t curEvt) { curEvt_ = curEvt; }

        // Visitors
        // Expressions
        // Binaries
        template <class OP> FORCE_INLINE void VisitBinary(const ExprNode_& node, OP op) {
            VisitNode(*node.arguments_[0]);
            VisitNode(*node.arguments_[1]);
            op(dStack_[1], dStack_.Top());
            dStack_.Pop();
        }

        FORCE_INLINE void Visit(const NodeAdd_& node) {
            VisitBinary(node, [](T_& x, const T_ y) { x += y; });
        }

        FORCE_INLINE void Visit(const NodeSub_& node) {
            VisitBinary(node, [](T_& x, const T_ y) { x -= y; });
        }

        FORCE_INLINE void Visit(const NodeMult_& node) {
            VisitBinary(node, [](T_& x, const T_ y) { x *= y; });
        }

        FORCE_INLINE void Visit(const NodeDiv_& node) {
            VisitBinary(node, [](T_& x, const T_ y) { x /= y; });
        }

        FORCE_INLINE void Visit(const NodePow_& node) {
            VisitBinary(node, [](T_& x, const T_ y) { x = pow(x, y); });
        }

        FORCE_INLINE void Visit(const NodeMax_& node) {
            VisitBinary(node, [](T_& x, const T_ y) {
                if (x < y)
                    x = y;
            });
        }

        FORCE_INLINE void Visit(const NodeMin_& node) {
            VisitBinary(node, [](T_& x, const T_ y) {
                if (x > y)
                    x = y;
            });
        }

        // Unaries
        template <class OP> FORCE_INLINE void VisitUnary(const ExprNode_& node, OP op) {
            VisitNode(*node.arguments_[0]);
            op(dStack_.Top());
        }

        FORCE_INLINE void Visit(const NodeUplus_& node) {
            VisitUnary(node, [](T_& x) {});
        }

        FORCE_INLINE void Visit(const NodeUminus_& node) {
            VisitUnary(node, [](T_& x) { x = -x; });
        }

        // Functions
        FORCE_INLINE void Visit(const NodeLog_& node) {
            VisitUnary(node, [](T_& x) { x = log(x); });
        }

        FORCE_INLINE void Visit(const NodeSqrt_& node) {
            VisitUnary(node, [](T_& x) { x = sqrt(x); });
        }

        FORCE_INLINE void Visit(const NodeExp_& node) {
            VisitUnary(node, [](T_& x) { x = exp(x); });
        }

        // Conditions
        template <class OP> FORCE_INLINE void VisitCondition(const BoolNode_& node, OP op) {
            VisitNode(*node.arguments_[0]);
            bStack_.Push(op(dStack_.TopAndPop()));
        }

        FORCE_INLINE void Visit(const NodeEqual_& node) {
            VisitCondition(node, [](const T_ x) { return x == 0; });
        }

        FORCE_INLINE void Visit(const NodeSup_& node) {
            VisitCondition(node, [](const T_ x) { return x > 0; });
        }

        FORCE_INLINE void Visit(const NodeSupEqual_& node) {
            VisitCondition(node, [](const T_ x) { return x >= 0; });
        }

        FORCE_INLINE void Visit(const NodeAnd_& node) {
            VisitNode(*node.arguments_[0]);
            if (bStack_.Top()) {
                bStack_.Pop();
                VisitNode(*node.arguments_[1]);
            }
        }

        FORCE_INLINE void Visit(const NodeOr_& node) {
            VisitNode(*node.arguments_[0]);
            if (!bStack_.Top()) {
                bStack_.Pop();
                VisitNode(*node.arguments_[1]);
            }
        }

        FORCE_INLINE void Visit(const NodeNot_& node) {
            VisitNode(*node.arguments_[0]);
            auto& b = bStack_.Top();
            b = !b;
        }

        // Instructions
        void Visit(const NodeIf_& node) {
            //	Eval the condition
            VisitNode(*node.arguments_[0]);

            //	Pick the result
            const auto isTrue = bStack_.TopAndPop();

            //	Evaluate the relevant statements
            if (isTrue) {
                const auto lastTrue = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;
                for (unsigned i = 1; i <= lastTrue; ++i) {
                    VisitNode(*node.arguments_[i]);
                }
            } else if (node.firstElse_ != -1) {
                const size_t n = node.arguments_.size();
                for (unsigned i = node.firstElse_; i < n; ++i) {
                    VisitNode(*node.arguments_[i]);
                }
            }
        }

        FORCE_INLINE void Visit(const NodeAssign_& node) {
            const auto varIdx = Downcast<NodeVar_>(node.arguments_[0])->index_;

            //	Visit the RHS expression
            VisitNode(*node.arguments_[1]);

            //	Write result into variable
            variables_[varIdx] = dStack_.TopAndPop();
        }

        FORCE_INLINE void Visit(const NodePays_& node) {
            const auto varIdx = Downcast<NodeVar_>(node.arguments_[0])->index_;

            //	Visit the RHS expression
            VisitNode(*node.arguments_[1]);

            //	Write result into variable
            variables_[varIdx] += dStack_.TopAndPop() / (*scenario_)[curEvt_].numeraire_;
        }

        // Variables and constants
        FORCE_INLINE void Visit(const NodeVar_& node) {
            //	Push value onto the stack
            dStack_.Push(variables_[node.index_]);
        }

        FORCE_INLINE void Visit(const NodeConstVar_& node) {
            //	Push value onto the stack
            dStack_.Push(const_variables_[node.index_]);
        }


        FORCE_INLINE void Visit(const NodeConst_& node) { dStack_.Push(node.constVal_); }

        FORCE_INLINE void Visit(const NodeTrue_& node) { bStack_.Push(true); }
        FORCE_INLINE void Visit(const NodeFalse_& node) { bStack_.Push(false); }

        // Scenario related
        FORCE_INLINE void Visit(const NodeSpot_& node) { dStack_.Push((*scenario_)[curEvt_].spot_); }
    };

    //  Concrete Evaluator_
    template <class T_> class Evaluator_ : public EvaluatorBase_<T_, Evaluator_> {

    public:
        using Base = EvaluatorBase_<T_, Evaluator_>;

        explicit Evaluator_(size_t nVar, const Vector_<T_>& const_variables = Vector_<T_>()) : Base(nVar, const_variables) {}
        Evaluator_(const Evaluator_& rhs) : Base(rhs) {}
        Evaluator_(Evaluator_&& rhs) noexcept: Base(std::move(rhs)) {}
        Evaluator_& operator=(const Evaluator_& rhs) {
            return Base::operator=(rhs);
        }
        Evaluator_& operator=(Evaluator_&& rhs) noexcept {
            return Base::operator=(std::move(rhs));
        }
    };

} // namespace Dal::Script
