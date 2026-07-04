//
// Created by wegam on 2023/1/26.
//

/*
Written by Antoine Savine in 2018

This code is the strict IP of Antoine Savine

License to use and alter this code for personal and commercial applications
is freely granted to any person or company who purchased a copy of the book

Modern Computational Finance: Scripting for Derivatives and XVA
Jesper Andreasen & Antoine Savine
Wiley, 2018

As long as this comment is preserved at the Top of the file
*/

#pragma once

#include <iostream>
#include <algorithm>
#include <functional>
#include <dal/math/aad/sample.hpp>
#include <dal/math/stacks.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>
#include <dal/script/visitor/smoothing.hpp>

/*IF--------------------------------------------------------------------------
enumeration NodeType
    node type list
switchable
alternative Add
alternative AddConst
alternative Sub
alternative SubConst
alternative ConstSub
alternative Mult
alternative MultConst
alternative Div
alternative DivConst
alternative ConstDiv
alternative Pow
alternative PowConst
alternative ConstPow
alternative Max2
alternative Max2Const
alternative Min2
alternative Min2Const
alternative Spot
alternative Var
alternative Const
alternative Assign
alternative AssignConst
alternative Pays
alternative PaysConst
alternative If
alternative IfElse
alternative Equal
alternative Sup
alternative SupEqual
alternative And
alternative Or
alternative Smooth
alternative Sqrt
alternative Log
alternative Not
alternative Uminus
alternative True
alternative False
alternative ConstVar
-IF-------------------------------------------------------------------------*/

namespace Dal::Script {
    template <class T_> struct EvalState_ {
        // State
        Vector_<T_> variables_;
        Vector_<> variablesInit_;
        Vector_<T_> constVariables_;

        //  Fuzzy state (compiled fuzzy If): default smoothing eps for
        //  conditions that don't override it, current nested-if level and
        //  the per-level variable stores used by the dt-blend.
        double defEps_ = 0.0;
        size_t nestedIfLvl_ = 0;
        Vector_<Vector_<T_>> varStore0_;
        Vector_<Vector_<T_>> varStore1_;

        //  Constructor
        explicit EvalState_(const Vector_<>& variables,
                            const Vector_<T_>& constVariables = Vector_<T_>(),
                            size_t maxNestedIfs = 0,
                            double defEps = 0.0)
            : variablesInit_(variables), constVariables_(constVariables), defEps_(defEps),
              varStore0_(maxNestedIfs), varStore1_(maxNestedIfs) {
            variables_.Resize(variablesInit_.size());
            for (auto i = 0; i < variables_.size(); ++i)
                variables_[i] = T_(variablesInit_[i]);
            for (auto& varStore : varStore0_)
                varStore.Resize(variables_.size());
            for (auto& varStore : varStore1_)
                varStore.Resize(variables_.size());
        }

        //  Initializer
        void Init() {
            for (auto i = 0; i < variables_.size(); ++i)
                variables_[i] = T_(variablesInit_[i]);
            nestedIfLvl_ = 0;
        }


        const Vector_<T_>& VarVals() const { return variables_; }
        Vector_<T_>& ConstVarVals() {
            return constVariables_;
        }
        const Vector_<T_>& ConstVarVals() const {
            return constVariables_;
        }
    };

    // NOTE: NodeType_ kept hand-written (not dal/auto/MG_NodeType_enum); the generated form
    // is a class wrapper (not an enum) and cannot serve as a non-type template parameter for
    // VisitBinary/VisitUnary/VisitCondition. Migrating is high-risk (~167 bare-opcode refs +
    // the compiled nodeStream_ integer contract) and is deferred to a dedicated PR.
    enum NodeType_ {
        Add = 0,
        AddConst = 1,
        Sub = 2,
        SubConst = 3,
        ConstSub = 4,
        Multi = 5,
        MultiConst = 6,
        Div = 7,
        DivConst = 8,
        ConstDiv = 9,
        Pow = 10,
        PowConst = 11,
        ConstPow = 12,
        Max2 = 13,
        Max2Const = 14,
        Min2 = 15,
        Min2Const = 16,
        Spot = 17,
        Var = 18,
        Const = 19,
        Assign = 20,
        AssignConst = 21,
        Pays = 22,
        PaysConst = 23,
        If = 24,
        IfElse = 25,
        Equal = 26,
        Sup = 27,
        SupEqual = 28,
        And = 29,
        Or = 30,
        //  31 retired: dead Smooth opcode (defect #7), replaced by the
        //  dedicated fuzzy opcodes 40+ below.
        Sqrt = 32,
        Log = 33,
        Exp = 34,
        Not = 35,
        UMinus = 36,
        True = 37,
        False = 38,
        ConstVar = 39,
        //  Fuzzy opcodes (compiled counterpart of FuzzyEvaluator_): smoothed
        //  condition results are T-valued probabilities on the value stack.
        FuzzyEqual = 40,         //  operand: const idx (eps; < 0 means state.defEps_)
        FuzzyEqualDiscrete = 41, //  operands: const idx (lb), const idx (rb)
        FuzzyComp = 42,          //  operand: const idx (eps; < 0 means state.defEps_)
        FuzzyCompDiscrete = 43,  //  operands: const idx (lb), const idx (rb)
        FuzzyAnd = 44,           //  a * b
        FuzzyOr = 45,            //  a + b - a * b
        FuzzyNot = 46,           //  1 - x
        FuzzyTrue = 47,
        FuzzyFalse = 48,
        FuzzyIf = 49             //  operands: lastTrue, lastFalse, nAff, aff...
    };

    class Compiler_ : public ConstVisitor_<Compiler_> {
        // State
        Vector_<int> nodeStream_;
        Vector_<double> constStream_;
        Vector_<const void*> dataStream_;
        //  Fuzzy mode: conditions compile to smoothed (CSpr/BFly) opcodes and
        //  If compiles to the dt-blend FuzzyIf, mirroring FuzzyEvaluator_.
        const bool fuzzy_;

    public:
        explicit Compiler_(bool fuzzy = false) : fuzzy_(fuzzy) {}

        using ConstVisitor_<Compiler_>::Visit;
        // Accessors
        // Access the streams after traversal
        [[nodiscard]] const Vector_<int>& NodeStream() const { return nodeStream_; }
        [[nodiscard]] const Vector_<double>& ConstStream() const { return constStream_; }
        [[nodiscard]] const Vector_<const void*>& DataStream() const { return dataStream_; }

        // Visitors
        // Expressions
        //  Binaries
        template <NodeType_ IfBin, NodeType_ IfConstLeft, NodeType_ IfConstRight> void VisitBinary(const ExprNode_& node) {
            if (node.isConst_) {
                nodeStream_.emplace_back(Const);
                nodeStream_.emplace_back(int(constStream_.size()));
                constStream_.emplace_back(node.constVal_);
            } else {
                const auto* lhs = Downcast<ExprNode_>(node.arguments_[0]);
                const auto* rhs = Downcast<ExprNode_>(node.arguments_[1]);

                if (lhs->isConst_) {
                    node.arguments_[1]->Accept(*this);
                    nodeStream_.emplace_back(IfConstLeft);
                    nodeStream_.emplace_back(int(constStream_.size()));
                    constStream_.emplace_back(lhs->constVal_);
                } else if (rhs->isConst_) {
                    node.arguments_[0]->Accept(*this);
                    nodeStream_.emplace_back(IfConstRight);
                    nodeStream_.emplace_back(int(constStream_.size()));
                    constStream_.emplace_back(rhs->constVal_);
                } else {
                    node.arguments_[0]->Accept(*this);
                    node.arguments_[1]->Accept(*this);
                    nodeStream_.emplace_back(IfBin);
                }
            }
        }

        void Visit(const NodeAdd_& node) { VisitBinary<Add, AddConst, AddConst>(node); }
        void Visit(const NodeSub_& node) { VisitBinary<Sub, ConstSub, SubConst>(node); }
        void Visit(const NodeMulti_& node) { VisitBinary<Multi, MultiConst, MultiConst>(node); }
        void Visit(const NodeDiv_& node) { VisitBinary<Div, ConstDiv, DivConst>(node); }
        void Visit(const NodePow_& node) { VisitBinary<Pow, ConstPow, PowConst>(node); }

        void Visit(const NodeMax_& node) { VisitBinary<Max2, Max2Const, Max2Const>(node); }

        void Visit(const NodeMin_& node) { VisitBinary<Min2, Min2Const, Min2Const>(node); }

        // unary
        template <NodeType_ NT> void VisitUnary(const ExprNode_& node) {
            if (node.isConst_) {
                nodeStream_.emplace_back(Const);
                nodeStream_.emplace_back(int(constStream_.size()));
                constStream_.emplace_back(node.constVal_);
            } else {
                node.arguments_[0]->Accept(*this);
                nodeStream_.emplace_back(NT);
            }
        }

        void Visit(const NodeUPlus_& node) { node.arguments_[0]->Accept(*this); }

        void Visit(const NodeUMinus_& node) { VisitUnary<UMinus>(node); }
        void Visit(const NodeLog_& node) { VisitUnary<Log>(node); }
        void Visit(const NodeSqrt_& node) { VisitUnary<Sqrt>(node); }
        void Visit(const NodeExp_& node) { VisitUnary<Exp>(node); }

        // Conditions
        template <NodeType_ NT, typename OP> void VisitCondition(const CompNode_& node, OP op) {
            if (fuzzy_) {
                //  #12: never hard-fold conditions on the fuzzy path — a
                //  const argument compiles to a plain value push and the
                //  smoothed comparison below computes exactly what
                //  FuzzyEvaluator_ computes (CSpr/BFly), fractional dt
                //  included when |const| < eps / 2.
                node.arguments_[0]->Accept(*this);
                if (node.isDiscrete_) {
                    nodeStream_.emplace_back(NT == Equal ? FuzzyEqualDiscrete : FuzzyCompDiscrete);
                    nodeStream_.emplace_back(int(constStream_.size()));
                    constStream_.emplace_back(node.lb_);
                    nodeStream_.emplace_back(int(constStream_.size()));
                    constStream_.emplace_back(node.rb_);
                } else {
                    nodeStream_.emplace_back(NT == Equal ? FuzzyEqual : FuzzyComp);
                    nodeStream_.emplace_back(int(constStream_.size()));
                    constStream_.emplace_back(node.eps_);
                }
                return;
            }

            const auto* arg = Downcast<ExprNode_>(node.arguments_[0]);

            if (arg->isConst_) {
                nodeStream_.emplace_back(op(arg->constVal_) ? True : False);

            } else {
                node.arguments_[0]->Accept(*this);
                nodeStream_.emplace_back(NT);
            }
        }

        void Visit(const NodeEqual_& node) {
            VisitCondition<Equal>(node, [](double x) { return x == 0.0; });
        }

        void Visit(const NodeSup_& node) {
            VisitCondition<Sup>(node, [](double x) { return x > 0.0; });
        }
        void Visit(const NodeSupEqual_& node) {
            //  Fold matches the runtime SupEqual opcode and the tree-walk
            //  evaluator exactly (x >= 0); an EPSILON band here would diverge
            //  for constants in (-EPSILON, 0).
            VisitCondition<SupEqual>(node, [](double x) { return x >= 0.0; });
        }

        //  And/Or/Not

        void Visit(const NodeAnd_& node) {
            node.arguments_[0]->Accept(*this);
            node.arguments_[1]->Accept(*this);
            nodeStream_.emplace_back(fuzzy_ ? FuzzyAnd : And);
        }

        void Visit(const NodeOr_& node) {
            node.arguments_[0]->Accept(*this);
            node.arguments_[1]->Accept(*this);
            nodeStream_.emplace_back(fuzzy_ ? FuzzyOr : Or);
        }

        void Visit(const NodeNot_& node) {
            node.arguments_[0]->Accept(*this);
            nodeStream_.emplace_back(fuzzy_ ? FuzzyNot : Not);
        }

        //  Assign, pays

        void Visit(const NodeAssign_& node) {
            const auto* var = Downcast<NodeVar_>(node.arguments_[0]);
            const auto* rhs = Downcast<ExprNode_>(node.arguments_[1]);

            if (rhs->isConst_) {
                nodeStream_.emplace_back(AssignConst);
                nodeStream_.emplace_back(static_cast<int>(constStream_.size()));
                constStream_.emplace_back(rhs->constVal_);
            } else {
                node.arguments_[1]->Accept(*this);
                nodeStream_.emplace_back(Assign);
            }
            nodeStream_.emplace_back(int(var->index_));
        }

        void Visit(const NodePays_& node) {
            const auto* var = Downcast<NodeVar_>(node.arguments_[0]);
            const auto* rhs = Downcast<ExprNode_>(node.arguments_[1]);

            if (rhs->isConst_) {
                nodeStream_.emplace_back(PaysConst);
                nodeStream_.emplace_back(static_cast<int>(constStream_.size()));
                constStream_.emplace_back(rhs->constVal_);
            } else {
                node.arguments_[1]->Accept(*this);
                nodeStream_.emplace_back(Pays);
            }
            nodeStream_.emplace_back(var->index_);
        }

        //  Leaves

        void Visit(const NodeVar_& node) {
            nodeStream_.emplace_back(Var);
            nodeStream_.emplace_back(node.index_);
        }

        void Visit(const NodeConstVar_& node) {
            nodeStream_.emplace_back(ConstVar);
            nodeStream_.emplace_back(node.index_);
        }

        void Visit(const NodeConst_& node) {
            nodeStream_.emplace_back(Const);
            nodeStream_.emplace_back(static_cast<int>(constStream_.size()));
            constStream_.emplace_back(node.constVal_);
        }

        void Visit(const NodeTrue_&) { nodeStream_.emplace_back(fuzzy_ ? FuzzyTrue : True); }

        void Visit(const NodeFalse_&) { nodeStream_.emplace_back(fuzzy_ ? FuzzyFalse : False); }

        // Scenario related
        void Visit(const NodeSpot_&) { nodeStream_.emplace_back(Spot); }

        // Instructions
        void Visit(const NodeIf_& node) {
            //  Visit condition
            node.arguments_[0]->Accept(*this);

            const auto lastTrue = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;
            const auto n = node.arguments_.size();

            if (fuzzy_) {
                //  Layout: FuzzyIf lastTrue lastFalse nAff aff... [true][false]
                //  lastFalse == lastTrue when there is no else branch, so the
                //  runtime continue point is uniformly lastFalse.
                nodeStream_.emplace_back(FuzzyIf);
                const size_t thisSpace = nodeStream_.size() - 1;
                nodeStream_.emplace_back(0);
                nodeStream_.emplace_back(0);
                nodeStream_.emplace_back(int(node.affectedVars_.size()));
                for (const auto idx : node.affectedVars_)
                    nodeStream_.emplace_back(int(idx));

                for (size_t i = 1; i <= lastTrue; ++i)
                    node.arguments_[i]->Accept(*this);
                nodeStream_[thisSpace + 1] = int(nodeStream_.size());

                if (node.firstElse_ != -1)
                    for (size_t i = node.firstElse_; i < n; ++i)
                        node.arguments_[i]->Accept(*this);
                nodeStream_[thisSpace + 2] = int(nodeStream_.size());
                return;
            }

            //  Mark instruction
            nodeStream_.emplace_back(node.firstElse_ == -1 ? If : IfElse);
            //  Record space
            const size_t thisSpace = nodeStream_.size() - 1;
            //  Make 2 spaces for last if-true and last if-false
            nodeStream_.emplace_back(0);
            if (node.firstElse_ != -1)
                nodeStream_.emplace_back(0);

            //  Visit if-true statements
            for (size_t i = 1; i <= lastTrue; ++i) {
                node.arguments_[i]->Accept(*this);
            }
            //  Record last if-true space
            nodeStream_[thisSpace + 1] = int(nodeStream_.size());

            //  Visit if-false statements
            if (node.firstElse_ != -1) {
                for (size_t i = node.firstElse_; i < n; ++i) {
                    node.arguments_[i]->Accept(*this);
                }
                //  Record last if-false space
                nodeStream_[thisSpace + 2] = int(nodeStream_.size());
            }
        }
    };

    template <class T_>
    inline void EvalCompiled(
        //  Stream to eval
        const Vector_<int>& nodeStream,
        const Vector_<double>& constStream,
        const Vector_<const void*>& dataStream,
        //  Scenario
        const AAD::Sample_<T_>& scenario,
        //  State
        EvalState_<T_>& state,
        //  First (included), last (excluded), reset flag
        size_t first = 0,
        size_t last = 0,
        //  Per-event entry resets the thread_local stacks; the IfElse
        //  true-branch re-entry passes false so the parent's bStack
        //  (still holding the condition) is not wiped.
        bool reset = true) {
        const size_t n = last ? last : nodeStream.size();
        size_t i = first;

        //  Work space
        T_ x, y, z, t;
        size_t idx;

        //  Stacks
        thread_local static StaticStack_<T_> dStack;
        if (reset)
            dStack.Reset();
        thread_local static StaticStack_<bool> bStack;
        if (reset)
            bStack.Reset();

        //  Loop on instructions
        while (i < n) {
            //  Big switch
            switch (nodeStream[i]) {

            case Add:
                dStack[1] += dStack.Top();
                dStack.Pop();
                ++i;
                break;

            case AddConst:
                dStack.Top() += constStream[nodeStream[++i]];
                ++i;
                break;
            case Sub:
                dStack[1] -= dStack.Top();
                dStack.Pop();
                ++i;
                break;

            case SubConst:
                dStack.Top() -= constStream[nodeStream[++i]];
                ++i;
                break;
            case ConstSub:
                dStack.Top() = constStream[nodeStream[++i]] - dStack.Top();
                ++i;
                break;
            case Multi:
                dStack[1] *= dStack.Top();
                dStack.Pop();
                ++i;
                break;
            case MultiConst:
                dStack.Top() *= constStream[nodeStream[++i]];
                ++i;
                break;
            case Div:
                dStack[1] /= dStack.Top();
                dStack.Pop();
                ++i;
                break;
            case DivConst:
                dStack.Top() /= constStream[nodeStream[++i]];
                ++i;
                break;
            case ConstDiv:
                dStack.Top() = constStream[nodeStream[++i]] / dStack.Top();
                ++i;
                break;
            case Pow:
                dStack[1] = pow(dStack[1], dStack.Top());
                dStack.Pop();
                ++i;
                break;
            case PowConst:
                dStack.Top() = pow(dStack.Top(), constStream[nodeStream[++i]]);
                ++i;
                break;
            case ConstPow:
                dStack.Top() = pow(constStream[nodeStream[++i]], dStack.Top());
                ++i;
                break;
            case Max2:
                y = dStack.TopAndPop();
                if (y > dStack[0])
                    dStack[0] = y;
                ++i;
                break;
            case Max2Const:
                y = T_(constStream[nodeStream[++i]]);
                if (y > dStack.Top())
                    dStack.Top() = y;
                ++i;
                break;
            case Min2:
                y = dStack.Top();
                if (y < dStack[1])
                    dStack[1] = y;
                dStack.Pop();
                ++i;
                break;
            case Min2Const:
                y = T_(constStream[nodeStream[++i]]);
                if (y < dStack.Top())
                    dStack.Top() = y;
                ++i;
                break;
            case Spot:
                dStack.Push(scenario.spot_);
                ++i;
                break;
            case Var:
                dStack.Push(state.variables_[nodeStream[++i]]);
                ++i;
                break;
            case ConstVar:
                dStack.Push(state.constVariables_[nodeStream[++i]]);
                ++i;
                break;
            case Const:
                dStack.Push(constStream[nodeStream[++i]]);
                ++i;
                break;
            case Assign:
                idx = nodeStream[++i];
                state.variables_[idx] = dStack.TopAndPop();
                ++i;
                break;
            case AssignConst:
                x = T_(constStream[nodeStream[++i]]);
                idx = nodeStream[++i];
                state.variables_[idx] = x;
                ++i;
                break;
            case Pays:
                ++i;
                idx = nodeStream[i];
                state.variables_[idx] += dStack.TopAndPop() / scenario.numeraire_;
                ++i;
                break;
            case PaysConst:
                x = T_(constStream[nodeStream[++i]]);
                idx = nodeStream[++i];
                state.variables_[idx] += x / scenario.numeraire_;
                ++i;
                break;
            case If:
                if (bStack.Top()) {
                    i += 2;
                } else {
                    i = nodeStream[++i];
                }
                bStack.Pop();
                break;
            case IfElse:
                if (!bStack.Top()) {
                    i = nodeStream[++i];
                } else {
                    //  Re-entrant call over the true branch only. reset=false:
                    //  the parent frame still needs bStack (Pop below) and dStack.
                    EvalCompiled(nodeStream, constStream, dataStream, scenario, state, i + 3, nodeStream[i + 1], false);
                    i = nodeStream[i + 2];
                }
                bStack.Pop();
                break;
            case Equal:
                bStack.Push(dStack.TopAndPop() == 0);
                ++i;
                break;
            case Sup:
                bStack.Push(dStack.TopAndPop() > 0);
                ++i;
                break;
            case SupEqual:
                bStack.Push(dStack.TopAndPop() >= 0);
                ++i;
                break;
            case And:
                if (bStack[1])
                    bStack[1] = bStack.Top();
                bStack.Pop();
                ++i;
                break;
            case Or:
                if (!bStack[1])
                    bStack[1] = bStack.Top();
                bStack.Pop();
                ++i;
                break;
            case Sqrt:
                dStack.Top() = sqrt(dStack.Top());
                ++i;
                break;
            case Log:
                dStack.Top() = log(dStack.Top());
                ++i;
                break;
            case Exp:
                dStack.Top() = exp(dStack.Top());
                ++i;
                break;
            case Not:
                bStack.Top() = !bStack.Top();
                ++i;
                break;
            case UMinus:
                dStack.Top() = -dStack.Top();
                ++i;
                break;
            case True:
                bStack.Push(true);
                ++i;
                break;
            case False:
                bStack.Push(false);
                ++i;
                break;
            case FuzzyEqual: {
                const double eps = constStream[nodeStream[++i]];
                dStack.Top() = BFly(dStack.Top(), eps < 0 ? state.defEps_ : eps);
                ++i;
                break;
            }
            case FuzzyEqualDiscrete: {
                const double lb = constStream[nodeStream[++i]];
                const double rb = constStream[nodeStream[++i]];
                dStack.Top() = BFly(dStack.Top(), lb, rb);
                ++i;
                break;
            }
            case FuzzyComp: {
                const double eps = constStream[nodeStream[++i]];
                dStack.Top() = CSpr(dStack.Top(), eps < 0 ? state.defEps_ : eps);
                ++i;
                break;
            }
            case FuzzyCompDiscrete: {
                const double lb = constStream[nodeStream[++i]];
                const double rb = constStream[nodeStream[++i]];
                dStack.Top() = CSpr(dStack.Top(), lb, rb);
                ++i;
                break;
            }
            case FuzzyAnd:
                //  Probability combinators, same as FuzzyEvaluator_: a * b.
                x = dStack.TopAndPop();
                dStack.Top() *= x;
                ++i;
                break;
            case FuzzyOr:
                //  a + b - a * b
                x = dStack.TopAndPop();
                y = dStack.TopAndPop();
                dStack.Push(x + y - x * y);
                ++i;
                break;
            case FuzzyNot:
                dStack.Top() = 1.0 - dStack.Top();
                ++i;
                break;
            case FuzzyTrue:
                dStack.Push(T_(1.0));
                ++i;
                break;
            case FuzzyFalse:
                dStack.Push(T_(0.0));
                ++i;
                break;
            case FuzzyIf: {
                //  Layout: FuzzyIf lastTrue lastFalse nAff aff... [true][false]
                //  (lastFalse == lastTrue when there is no else). Mirrors
                //  FuzzyEvaluator_::Visit(NodeIf_): hard-branch when dt is
                //  within EPSILON of 0/1, otherwise evaluate both branches
                //  over the affected variables and blend by dt.
                const size_t lastTrue = nodeStream[i + 1];
                const size_t lastFalse = nodeStream[i + 2];
                const int nAff = nodeStream[i + 3];
                const size_t firstAff = i + 4;
                const size_t firstTrue = firstAff + nAff;

                t = dStack.TopAndPop();
                if (t > 1.0 - EPSILON) {
                    //  Absolutely true: run the true branch, skip the else.
                    EvalCompiled(nodeStream, constStream, dataStream, scenario, state, firstTrue, lastTrue, false);
                    i = lastFalse;
                } else if (t < EPSILON) {
                    //  Absolutely false: fall through to the else statements.
                    i = lastTrue;
                } else {
                    const size_t lvl = state.nestedIfLvl_++;
                    for (int k = 0; k < nAff; ++k) {
                        idx = nodeStream[firstAff + k];
                        state.varStore0_[lvl][idx] = state.variables_[idx];
                    }
                    EvalCompiled(nodeStream, constStream, dataStream, scenario, state, firstTrue, lastTrue, false);
                    for (int k = 0; k < nAff; ++k) {
                        idx = nodeStream[firstAff + k];
                        state.varStore1_[lvl][idx] = state.variables_[idx];
                        state.variables_[idx] = state.varStore0_[lvl][idx];
                    }
                    EvalCompiled(nodeStream, constStream, dataStream, scenario, state, lastTrue, lastFalse, false);
                    for (int k = 0; k < nAff; ++k) {
                        idx = nodeStream[firstAff + k];
                        state.variables_[idx] = t * state.varStore1_[lvl][idx] + (1.0 - t) * state.variables_[idx];
                    }
                    --state.nestedIfLvl_;
                    i = lastFalse;
                }
                break;
            }
            }
        }
    }
} // namespace Dal::Script
