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
#include <dal/utilities/exceptions.hpp>

namespace Dal::Script {
    template <class T_> struct EvalState_ {
        Vector_<T_> variables_;
        Vector_<> variablesInit_;
        Vector_<T_> constVariables_;

        //  Fuzzy If blend state.
        double defEps_ = 0.0;
        size_t nestedIfLvl_ = 0;
        Vector_<Vector_<T_>> varStore0_;
        Vector_<Vector_<T_>> varStore1_;

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

    //  Hand-written because opcodes are NTTPs and serialized stream integers.
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
        //  31 is intentionally unused.
        Sqrt = 32,
        Log = 33,
        Exp = 34,
        Not = 35,
        UMinus = 36,
        True = 37,
        False = 38,
        ConstVar = 39,
        //  Fuzzy opcodes.
        FuzzyEqual = 40,
        FuzzyEqualDiscrete = 41,
        FuzzyComp = 42,
        FuzzyCompDiscrete = 43,
        FuzzyAnd = 44,
        FuzzyOr = 45,
        FuzzyNot = 46,
        FuzzyTrue = 47,
        FuzzyFalse = 48,
        FuzzyIf = 49             //  operands: lastTrue, lastFalse, nAff, aff...
    };

    class Compiler_ : public ConstVisitor_<Compiler_> {
        Vector_<int> nodeStream_;
        Vector_<double> constStream_;
        const bool fuzzy_;

    public:
        explicit Compiler_(bool fuzzy = false) : fuzzy_(fuzzy) {}

        using ConstVisitor_<Compiler_>::Visit;
        [[nodiscard]] const Vector_<int>& NodeStream() const { return nodeStream_; }
        [[nodiscard]] const Vector_<double>& ConstStream() const { return constStream_; }

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

        template <NodeType_ NT, typename OP> void VisitCondition(const CompNode_& node, OP op) {
            if (fuzzy_) {
                //  Keep fuzzy conditions as smoothed runtime opcodes.
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
            VisitCondition<SupEqual>(node, [](double x) { return x >= 0.0; });
        }

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

        void Visit(const NodeSpot_&) { nodeStream_.emplace_back(Spot); }

        void Visit(const NodeCollect_& node) { VisitArguments(node); }

        void CompileHardIf(const NodeIf_& node, size_t lastTrue, size_t n) {
            nodeStream_.emplace_back(node.firstElse_ == -1 ? If : IfElse);
            const size_t thisSpace = nodeStream_.size() - 1;
            nodeStream_.emplace_back(0);
            if (node.firstElse_ != -1)
                nodeStream_.emplace_back(0);

            for (size_t i = 1; i <= lastTrue; ++i) {
                node.arguments_[i]->Accept(*this);
            }
            nodeStream_[thisSpace + 1] = int(nodeStream_.size());

            if (node.firstElse_ != -1) {
                for (size_t i = node.firstElse_; i < n; ++i) {
                    node.arguments_[i]->Accept(*this);
                }
                nodeStream_[thisSpace + 2] = int(nodeStream_.size());
            }
        }

        void CompileFuzzyIf(const NodeIf_& node, size_t lastTrue, size_t n) {
            //  Layout: FuzzyIf lastTrue lastFalse nAff aff... [true][false]
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
        }

        void Visit(const NodeIf_& node) {
            node.arguments_[0]->Accept(*this);

            const auto lastTrue = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;
            const auto n = node.arguments_.size();

            if (fuzzy_)
                CompileFuzzyIf(node, lastTrue, n);
            else
                CompileHardIf(node, lastTrue, n);
        }
    };

    template <class T_>
    inline void EvalCompiled(const Vector_<int>& nodeStream,
                             const Vector_<double>& constStream,
                             const AAD::Sample_<T_>& scenario,
                             EvalState_<T_>& state,
                             size_t first = 0,
                             size_t last = 0,
                             bool reset = true);

    template <class T_>
    inline bool EvalBasicArithmeticOp(int op, size_t& i, StaticStack_<T_>& dStack) {
        switch (op) {
        case Add:
            dStack[1] += dStack.Top();
            dStack.Pop();
            ++i;
            return true;
        case Sub:
            dStack[1] -= dStack.Top();
            dStack.Pop();
            ++i;
            return true;
        case Multi:
            dStack[1] *= dStack.Top();
            dStack.Pop();
            ++i;
            return true;
        case Div:
            dStack[1] /= dStack.Top();
            dStack.Pop();
            ++i;
            return true;
        case Pow:
            dStack[1] = pow(dStack[1], dStack.Top());
            dStack.Pop();
            ++i;
            return true;
        default:
            return false;
        }
    }

    template <class T_>
    inline bool EvalExtremaOp(int op, size_t& i, StaticStack_<T_>& dStack) {
        switch (op) {
        case Max2: {
            const T_ y = dStack.TopAndPop();
            if (y > dStack[0])
                dStack[0] = y;
            ++i;
            return true;
        }
        case Min2: {
            const T_ y = dStack.TopAndPop();
            if (y < dStack[0])
                dStack[0] = y;
            ++i;
            return true;
        }
        default:
            return false;
        }
    }

    template <class T_>
    inline bool EvalConstArithmeticOp(int op,
                                      const Vector_<int>& nodeStream,
                                      const Vector_<double>& constStream,
                                      size_t& i,
                                      StaticStack_<T_>& dStack) {
        switch (op) {
        case AddConst:
            dStack.Top() += constStream[nodeStream[++i]];
            ++i;
            return true;
        case SubConst:
            dStack.Top() -= constStream[nodeStream[++i]];
            ++i;
            return true;
        case ConstSub:
            dStack.Top() = constStream[nodeStream[++i]] - dStack.Top();
            ++i;
            return true;
        case MultiConst:
            dStack.Top() *= constStream[nodeStream[++i]];
            ++i;
            return true;
        case DivConst:
            dStack.Top() /= constStream[nodeStream[++i]];
            ++i;
            return true;
        case ConstDiv:
            dStack.Top() = constStream[nodeStream[++i]] / dStack.Top();
            ++i;
            return true;
        default:
            return false;
        }
    }

    template <class T_>
    inline bool EvalConstPowOp(int op,
                               const Vector_<int>& nodeStream,
                               const Vector_<double>& constStream,
                               size_t& i,
                               StaticStack_<T_>& dStack) {
        switch (op) {
        case PowConst:
            dStack.Top() = pow(dStack.Top(), constStream[nodeStream[++i]]);
            ++i;
            return true;
        case ConstPow:
            dStack.Top() = pow(constStream[nodeStream[++i]], dStack.Top());
            ++i;
            return true;
        default:
            return false;
        }
    }

    template <class T_>
    inline bool EvalConstExtremaOp(int op,
                                   const Vector_<int>& nodeStream,
                                   const Vector_<double>& constStream,
                                   size_t& i,
                                   StaticStack_<T_>& dStack) {
        switch (op) {
        case Max2Const: {
            const T_ y(constStream[nodeStream[++i]]);
            if (y > dStack.Top())
                dStack.Top() = y;
            ++i;
            return true;
        }
        case Min2Const: {
            const T_ y(constStream[nodeStream[++i]]);
            if (y < dStack.Top())
                dStack.Top() = y;
            ++i;
            return true;
        }
        default:
            return false;
        }
    }

    template <class T_>
    inline bool EvalUnaryMathOp(int op, size_t& i, StaticStack_<T_>& dStack) {
        switch (op) {
        case Sqrt:
            dStack.Top() = sqrt(dStack.Top());
            ++i;
            return true;
        case Log:
            dStack.Top() = log(dStack.Top());
            ++i;
            return true;
        case Exp:
            dStack.Top() = exp(dStack.Top());
            ++i;
            return true;
        case UMinus:
            dStack.Top() = -dStack.Top();
            ++i;
            return true;
        default:
            return false;
        }
    }

    template <class T_>
    inline bool EvalLoadOp(int op,
                           const Vector_<int>& nodeStream,
                           const Vector_<double>& constStream,
                           const AAD::Sample_<T_>& scenario,
                           EvalState_<T_>& state,
                           size_t& i,
                           StaticStack_<T_>& dStack) {
        switch (op) {
        case Spot:
            dStack.Push(scenario.spot_);
            ++i;
            return true;
        case Var:
            dStack.Push(state.variables_[nodeStream[++i]]);
            ++i;
            return true;
        case ConstVar:
            dStack.Push(state.constVariables_[nodeStream[++i]]);
            ++i;
            return true;
        case Const:
            dStack.Push(constStream[nodeStream[++i]]);
            ++i;
            return true;
        default:
            return false;
        }
    }

    template <class T_>
    inline bool EvalStoreOp(int op,
                            const Vector_<int>& nodeStream,
                            const Vector_<double>& constStream,
                            const AAD::Sample_<T_>& scenario,
                            EvalState_<T_>& state,
                            size_t& i,
                            StaticStack_<T_>& dStack) {
        switch (op) {
        case Assign: {
            const size_t idx = nodeStream[++i];
            state.variables_[idx] = dStack.TopAndPop();
            ++i;
            return true;
        }
        case AssignConst: {
            const double val = constStream[nodeStream[++i]];
            const size_t idx = nodeStream[++i];
            state.variables_[idx] = T_(val);
            ++i;
            return true;
        }
        case Pays: {
            const size_t idx = nodeStream[++i];
            state.variables_[idx] += dStack.TopAndPop() / scenario.numeraire_;
            ++i;
            return true;
        }
        case PaysConst: {
            const double val = constStream[nodeStream[++i]];
            const size_t idx = nodeStream[++i];
            state.variables_[idx] += T_(val) / scenario.numeraire_;
            ++i;
            return true;
        }
        default:
            return false;
        }
    }

    template <class T_>
    inline bool EvalBranchOp(int op,
                             const Vector_<int>& nodeStream,
                             const Vector_<double>& constStream,
                             const AAD::Sample_<T_>& scenario,
                             EvalState_<T_>& state,
                             size_t& i,
                             StaticStack_<bool>& bStack) {
        switch (op) {
        case If:
            if (bStack.Top()) {
                i += 2;
            } else {
                i = nodeStream[++i];
            }
            bStack.Pop();
            return true;
        case IfElse:
            if (!bStack.Top()) {
                i = nodeStream[++i];
            } else {
                //  Preserve parent stacks while running the true branch.
                EvalCompiled(nodeStream, constStream, scenario, state, i + 3, nodeStream[i + 1], false);
                i = nodeStream[i + 2];
            }
            bStack.Pop();
            return true;
        default:
            return false;
        }
    }

    template <class T_>
    inline bool EvalComparisonOp(int op, size_t& i, StaticStack_<T_>& dStack, StaticStack_<bool>& bStack) {
        switch (op) {
        case Equal:
            bStack.Push(dStack.TopAndPop() == 0);
            ++i;
            return true;
        case Sup:
            bStack.Push(dStack.TopAndPop() > 0);
            ++i;
            return true;
        case SupEqual:
            bStack.Push(dStack.TopAndPop() >= 0);
            ++i;
            return true;
        default:
            return false;
        }
    }

    inline bool EvalLogicalOp(int op, size_t& i, StaticStack_<bool>& bStack) {
        switch (op) {
        case And:
            if (bStack[1])
                bStack[1] = bStack.Top();
            bStack.Pop();
            ++i;
            return true;
        case Or:
            if (!bStack[1])
                bStack[1] = bStack.Top();
            bStack.Pop();
            ++i;
            return true;
        case Not:
            bStack.Top() = !bStack.Top();
            ++i;
            return true;
        case True:
            bStack.Push(true);
            ++i;
            return true;
        case False:
            bStack.Push(false);
            ++i;
            return true;
        default:
            return false;
        }
    }

    template <class T_>
    inline bool EvalFuzzyConditionOp(int op,
                                     const Vector_<int>& nodeStream,
                                     const Vector_<double>& constStream,
                                     EvalState_<T_>& state,
                                     size_t& i,
                                     StaticStack_<T_>& dStack) {
        switch (op) {
        case FuzzyEqual: {
            const double eps = constStream[nodeStream[++i]];
            dStack.Top() = BFly(dStack.Top(), eps < 0 ? state.defEps_ : eps);
            ++i;
            return true;
        }
        case FuzzyEqualDiscrete: {
            const double lb = constStream[nodeStream[++i]];
            const double rb = constStream[nodeStream[++i]];
            dStack.Top() = BFly(dStack.Top(), lb, rb);
            ++i;
            return true;
        }
        case FuzzyComp: {
            const double eps = constStream[nodeStream[++i]];
            dStack.Top() = CSpr(dStack.Top(), eps < 0 ? state.defEps_ : eps);
            ++i;
            return true;
        }
        case FuzzyCompDiscrete: {
            const double lb = constStream[nodeStream[++i]];
            const double rb = constStream[nodeStream[++i]];
            dStack.Top() = CSpr(dStack.Top(), lb, rb);
            ++i;
            return true;
        }
        default:
            return false;
        }
    }

    template <class T_>
    inline bool EvalFuzzyLogicOp(int op, size_t& i, StaticStack_<T_>& dStack) {
        switch (op) {
        case FuzzyAnd: {
            const T_ x = dStack.TopAndPop();
            dStack.Top() *= x;
            ++i;
            return true;
        }
        case FuzzyOr: {
            const T_ x = dStack.TopAndPop();
            const T_ y = dStack.TopAndPop();
            dStack.Push(x + y - x * y);
            ++i;
            return true;
        }
        case FuzzyNot:
            dStack.Top() = 1.0 - dStack.Top();
            ++i;
            return true;
        case FuzzyTrue:
            dStack.Push(T_(1.0));
            ++i;
            return true;
        case FuzzyFalse:
            dStack.Push(T_(0.0));
            ++i;
            return true;
        default:
            return false;
        }
    }

    template <class T_>
    inline bool EvalFuzzyIfOp(int op,
                              const Vector_<int>& nodeStream,
                              const Vector_<double>& constStream,
                              const AAD::Sample_<T_>& scenario,
                              EvalState_<T_>& state,
                              size_t& i,
                              StaticStack_<T_>& dStack) {
        if (op != FuzzyIf)
            return false;

        //  Layout: FuzzyIf lastTrue lastFalse nAff aff... [true][false]
        const size_t lastTrue = nodeStream[i + 1];
        const size_t lastFalse = nodeStream[i + 2];
        const int nAff = nodeStream[i + 3];
        const size_t firstAff = i + 4;
        const size_t firstTrue = firstAff + nAff;

        const T_ t = dStack.TopAndPop();
        if (t > 1.0 - EPSILON) {
            EvalCompiled(nodeStream, constStream, scenario, state, firstTrue, lastTrue, false);
            i = lastFalse;
        } else if (t < EPSILON) {
            i = lastTrue;
        } else {
            const size_t lvl = state.nestedIfLvl_++;
            for (int k = 0; k < nAff; ++k) {
                const size_t idx = nodeStream[firstAff + k];
                state.varStore0_[lvl][idx] = state.variables_[idx];
            }
            EvalCompiled(nodeStream, constStream, scenario, state, firstTrue, lastTrue, false);
            for (int k = 0; k < nAff; ++k) {
                const size_t idx = nodeStream[firstAff + k];
                state.varStore1_[lvl][idx] = state.variables_[idx];
                state.variables_[idx] = state.varStore0_[lvl][idx];
            }
            EvalCompiled(nodeStream, constStream, scenario, state, lastTrue, lastFalse, false);
            for (int k = 0; k < nAff; ++k) {
                const size_t idx = nodeStream[firstAff + k];
                state.variables_[idx] = t * state.varStore1_[lvl][idx] + (1.0 - t) * state.variables_[idx];
            }
            --state.nestedIfLvl_;
            i = lastFalse;
        }
        return true;
    }

    template <class T_>
    inline bool EvalLowOpcode(int op,
                              const Vector_<int>& nodeStream,
                              const Vector_<double>& constStream,
                              size_t& i,
                              StaticStack_<T_>& dStack) {
        if (EvalBasicArithmeticOp(op, i, dStack))
            return true;
        if (EvalExtremaOp(op, i, dStack))
            return true;
        if (EvalConstArithmeticOp(op, nodeStream, constStream, i, dStack))
            return true;
        if (EvalConstPowOp(op, nodeStream, constStream, i, dStack))
            return true;
        return EvalConstExtremaOp(op, nodeStream, constStream, i, dStack);
    }

    template <class T_>
    inline bool EvalMidOpcode(int op,
                              const Vector_<int>& nodeStream,
                              const Vector_<double>& constStream,
                              const AAD::Sample_<T_>& scenario,
                              EvalState_<T_>& state,
                              size_t& i,
                              StaticStack_<T_>& dStack,
                              StaticStack_<bool>& bStack) {
        if (EvalUnaryMathOp(op, i, dStack))
            return true;
        if (EvalLoadOp(op, nodeStream, constStream, scenario, state, i, dStack))
            return true;
        if (EvalStoreOp(op, nodeStream, constStream, scenario, state, i, dStack))
            return true;
        if (EvalBranchOp(op, nodeStream, constStream, scenario, state, i, bStack))
            return true;
        if (EvalComparisonOp(op, i, dStack, bStack))
            return true;
        return EvalLogicalOp(op, i, bStack);
    }

    template <class T_>
    inline bool EvalFuzzyOpcode(int op,
                                const Vector_<int>& nodeStream,
                                const Vector_<double>& constStream,
                                const AAD::Sample_<T_>& scenario,
                                EvalState_<T_>& state,
                                size_t& i,
                                StaticStack_<T_>& dStack) {
        if (EvalFuzzyConditionOp(op, nodeStream, constStream, state, i, dStack))
            return true;
        if (EvalFuzzyLogicOp(op, i, dStack))
            return true;
        return EvalFuzzyIfOp(op, nodeStream, constStream, scenario, state, i, dStack);
    }

    template <class T_>
    inline void EvalCompiled(const Vector_<int>& nodeStream,
                             const Vector_<double>& constStream,
                             const AAD::Sample_<T_>& scenario,
                             EvalState_<T_>& state,
                             size_t first,
                             size_t last,
                             bool reset) {
        const size_t n = last ? last : nodeStream.size();
        size_t i = first;

        thread_local static StaticStack_<T_> dStack;
        if (reset)
            dStack.Reset();
        thread_local static StaticStack_<bool> bStack;
        if (reset)
            bStack.Reset();

        while (i < n) {
            const int op = nodeStream[i];
            if (EvalLowOpcode(op, nodeStream, constStream, i, dStack))
                continue;
            if (EvalMidOpcode(op, nodeStream, constStream, scenario, state, i, dStack, bStack))
                continue;
            if (EvalFuzzyOpcode(op, nodeStream, constStream, scenario, state, i, dStack))
                continue;
            THROW("unknown compiled script opcode: " + std::to_string(op));
        }
    }
} // namespace Dal::Script
