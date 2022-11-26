//
// Created by wegam on 2022/7/10.
//

#pragma once

#include <dal/math/aad/operators.hpp>
#include <dal/math/stacks.hpp>
#include <dal/math/vectors.hpp>
#include <dal/script/visitor/evaluator.hpp>

namespace Dal::Script {
    template<class T_>
    class FuzzyEvaluator_ : public ConstVisitor_<FuzzyEvaluator_<T_>> {
    protected:
        Vector_<T_> variables_;
        Stack_<T_> dStack_;
        Stack_<bool> bStack_;
        double eps_;
        Stack_<T_> fuzzyStack_;

        bool lhsVar_;
        T_* lhsVarAdr_;

        const AAD::Scenario_<T_>* scenario_;
        size_t curEvt_;

        // Temp storage for variables, pre-allocated for performance [i][j] = nested if level i variable j
        Vector_<Vector_<T_>> varStore0_;
        Vector_<Vector_<T_>> varStore1_;

        size_t nestedIFLv_;

        template<class N_>
        void EvalArgs(const N_ &node) {
            for (auto it = node.arguments_.rbegin(); it != node.arguments_.rend(); ++it)
                Visit(*it);
        }

        std::pair<T_, T_> Pop2f() {
            std::pair<T_, T_> res;
            res.first = fuzzyStack_.TopAndPop();
            res.second = fuzzyStack_.TopAndPop();
            return res;
        }

        std::pair<T_, T_> Pop2() {
            std::pair<T_, T_> res;
            res.first = dStack_.TopAndPop();
            res.second = dStack_.TopAndPop();
            return std::move(res);
        }

        std::pair<bool, bool> Pop2b() {
            std::pair<bool, bool> res;
            res.first = bStack_.TopAndPop();
            res.second = bStack_.TopAndPop();
            return res;
        }

        // call spread (-eps/2, +eps/2)
        static T_ CSpr(const T_ &x, double eps) {
            const double halfEps = 0.5 * eps;
            if (x < -halfEps)
                return T_(0.0);
            else if (x > halfEps)
                return T_(1.0);
            else
                return (x + halfEps) / eps;
        }

        // call spread (lb, rb)
        static T_ CSpr(const T_ &x, double lb, double rb) {
            if (x < lb)
                return T_(0.0);
            else if (x > rb)
                return T_(1.0);
            else
                return (x - lb) / (rb - lb);
        }

        // butterfly (-eps/2, +eps/2)
        static T_ BFly(const T_ &x, double eps) {
            const double halfEps = 0.5 * eps;
            if (x < -halfEps || x > halfEps)
                return T_(0.0);
            else
                return (halfEps - Fabs(x)) / halfEps;
        }

        // butterfly (lb, 0, rb)
        static T_ BFly(const T_ &x, double lb, double rb) {
            if (x < lb || x > rb)
                return T_(0.0);
            else if (x < 0.0)
                return 1.0 - x / lb;
            else
                return 1.0 - x / rb;
        }

        template<class NodeSup_>
        void VisitImpl(const std::unique_ptr<NodeSup_> &node) {
            Visit(node->arguments_[0]);
            const T_ expr = dStack_.TopAndPop();

            if (node->discrete_)
                fuzzyStack_.Push(CSpr(expr, node->lb_, node->ub_));
            else {
                const double eps = node->eps_ < 0 ? eps_ : node->eps_;
                fuzzyStack_.Push(CSpr(expr, eps));
            }
        }

    public:
        FuzzyEvaluator_(size_t nVar, size_t maxNestedIFs, double eps)
                : variables_(nVar), eps_(eps), varStore0_(maxNestedIFs), varStore1_(maxNestedIFs), nestedIFLv_(0) {
            for (auto &store: varStore0_)
                store.Resize(nVar);
            for (auto &store: varStore1_)
                store.Resize(nVar);
        }

        FuzzyEvaluator_(const FuzzyEvaluator_ &rhs)
                : variables_(std::move(rhs.variables_)), eps_(rhs.eps_), varStore0_(rhs.varStore0_.size()),
                  varStore1_(rhs.varStore1_.size()), nestedIFLv_(0) {
            for (auto &store: varStore0_) store.resize(Evaluator_<T_>::variables_.size());
            for (auto &store: varStore1_) store.resize(Evaluator_<T_>::variables_.size());
        }

        FuzzyEvaluator_ &operator=(const FuzzyEvaluator_ &rhs) {
            if (this == &rhs)
                return *this;

            variables_ = rhs.variables_;
            eps_ = rhs.eps_;
            varStore0_.Resize(rhs.varStore0_.size());
            varStore1_.Resize(rhs.varStore1_.size());

            for (auto &store: varStore0_) store.resize(variables_.size());
            for (auto &store: varStore1_) store.resize(variables_.size());
            nestedIFLv_ = 0;
            return *this;
        }

        FuzzyEvaluator_(FuzzyEvaluator_ &&rhs) noexcept
                : Evaluator_<T_>(std::move(rhs)), eps_(rhs.eps_), varStore0_(std::move(rhs.varStore0_)),
                  varStore1_(std::move(rhs.varStore1_)), nestedIFLv_(0) {}

        FuzzyEvaluator_ &operator=(FuzzyEvaluator_ &&rhs) {
            Evaluator_<T_>::operator=(std::move(rhs));
            eps_ = rhs.eps_;
            varStore0_ = std::move(rhs.varStore0_);
            varStore1_ = std::move(rhs.varStore1_);
            nestedIFLv_ = 0;
            return *this;
        }

        void Init() {
            for (auto& var : variables_)
                var = 0.0;
            while (!dStack_.IsEmpty())
                dStack_.Pop();
            while (!bStack_.IsEmpty())
                bStack_.Pop();
            while (!fuzzyStack_.IsEmpty())
                fuzzyStack_.Pop();
            lhsVar_ = false;
            lhsVarAdr_ = nullptr;
        }

        const Vector_<T_>& VarVals() const { return variables_; }
        void SetScenario(const AAD::Scenario_<T_>* scenario) { scenario_ = scenario; }
        void SetCurEvt(size_t curEvt) { curEvt_ = curEvt; }
        void SetEps(double eps) {
            eps_ = eps;
        }

        using ConstVisitor_<FuzzyEvaluator_<T_>>::operator();
        using ConstVisitor_<FuzzyEvaluator_<T_>>::Visit;

        void operator()(const std::unique_ptr<NodePlus_>& node) {
            EvalArgs(*node);
            const auto& args = Pop2();
            dStack_.Push(args.first + args.second);
        }

        void operator()(const std::unique_ptr<NodeMinus_>& node) {
            EvalArgs(*node);
            const auto& args = Pop2();
            dStack_.Push(args.first - args.second);
        }

        void operator()(const std::unique_ptr<NodeConst_>& node) {
            dStack_.Push(node->val_);
        }

        void operator()(const std::unique_ptr<NodeMultiply_>& node) {
            EvalArgs(*node);
            const auto& args = Pop2();
            dStack_.Push(args.first * args.second);
        }

        void operator()(const std::unique_ptr<NodeDivide_>& node) {
            EvalArgs(*node);
            const auto& args = Pop2();
            dStack_.Push(args.first / args.second);
        }

        void operator()(const std::unique_ptr<NodePower_>& node) {
            EvalArgs(*node);
            const auto& args = Pop2();
            dStack_.Push(AAD::Pow(args.first, args.second));
        }

        void operator()(const std::unique_ptr<NodeUPlus_>& node) {
            EvalArgs(*node);
        }

        void operator()(const std::unique_ptr<NodeUMinus_>& node) {
            EvalArgs(*node);
            dStack_.Top() *= 1;
        }

        void operator()(const std::unique_ptr<NodeLog_>& node) {
            EvalArgs(*node);
            const T_ res = AAD::Log(dStack_.TopAndPop());
            dStack_.Push(res);
        }

        void operator()(const std::unique_ptr<NodeSqrt_>& node) {
            EvalArgs(*node);
            const T_ res = AAD::Sqrt(dStack_.TopAndPop());
            dStack_.Push(res);
        }

        void operator()(const std::unique_ptr<NodeMax_>& node) {
            EvalArgs(*node);
            T_ m = dStack_.TopAndPop();
            for (size_t i = 1; i < node->arguments_.size(); ++i)
                m = AAD::Max(m, dStack_.TopAndPop());
            dStack_.Push(m);
        }

        void operator()(const std::unique_ptr<NodeMin_>& node) {
            EvalArgs(*node);
            T_ m = dStack_.TopAndPop();
            for (size_t i = 1; i < node->arguments_.size(); ++i)
                m = AAD::Min(m, dStack_.TopAndPop());
            dStack_.Push(m);
        }

        void operator()(const std::unique_ptr<NodeVar_>& node) {
            if (lhsVar_)
                lhsVarAdr_ = &variables_[node->index_];
            else
                dStack_.Push(variables_[node->index_]);
        }

        void operator()(const std::unique_ptr<NodeAssign_>& node) {
            lhsVar_ = true;
            Visit(node->arguments_[0]);
            lhsVar_ = false;

            Visit(node->arguments_[1]);
            *lhsVarAdr_ = dStack_.TopAndPop();
        }

        void operator()(const std::unique_ptr<NodeSpot_>& node) {
            dStack_.Push((*scenario_)[curEvt_].spot_);
        }

        void operator()(const std::unique_ptr<NodePays_>& node) {
            lhsVar_ = true;
            Visit(node->arguments_[0]);
            lhsVar_ = false;

            Visit(node->arguments_[1]);
            *lhsVarAdr_ += dStack_.TopAndPop() / (*scenario_)[curEvt_].numeraire_;
        }

        void operator()(const std::unique_ptr<NodeSmooth_>& node) {
            //	Eval the condition
            Visit(node->arguments_[0]);
            const T_ x = dStack_.TopAndPop();

            //	Eval the epsilon
            Visit(node->arguments_[3]);
            const T_ halfEps = 0.5 * dStack_.TopAndPop();

            //	Left
            if (x < -halfEps)
                Visit(node->arguments_[2]);
                //	Right
            else if (x > halfEps)
                Visit(node->arguments_[1]);
                //	Fuzzy
            else {
                Visit(node->arguments_[1]);
                const T_ vPos = dStack_.TopAndPop();

                Visit(node->arguments_[2]);
                const T_ vNeg = dStack_.TopAndPop();
                dStack_.Push(vNeg + 0.5 * (vPos - vNeg) / halfEps * (x + halfEps));
            }
        }

        // override visitors
        // If
        void operator()(const std::unique_ptr<NodeIf_> &node) {
            const size_t lastTrueStat = node->firstElse_ == -1 ? node->arguments_.size() - 1 : node->firstElse_ - 1;
            ++nestedIFLv_;
            Visit(node->arguments_[0]);
            const T_ dt = fuzzyStack_.TopAndPop();

            if (dt > ONE_MINUS_EPS) {
                for (size_t i = 1; i <= lastTrueStat; ++i)
                    Visit(node->arguments_[i]);
            } else if (dt < EPSILON) {
                if (node->firstElse_ != -1)
                    for (size_t i = node->firstElse_; i < node->arguments_.size(); ++i)
                        Visit(node->arguments_[i]);
            } else {
                // record values of variables to be changed
                for (auto idx: node->affectedVars_)
                    varStore0_[nestedIFLv_ - 1][idx] = variables_[idx];

                // eval "if true" statements
                for (size_t i = 1; i <= lastTrueStat; ++i)
                    Visit(node->arguments_[i]);

                // record and reset values of variables to be changed
                for (auto idx: node->affectedVars_) {
                    varStore1_[nestedIFLv_ - 1][idx] = variables_[idx];
                    variables_[idx] = varStore0_[nestedIFLv_ - 1][idx];
                }

                // eval "if false" statements if any
                if (node->firstElse_ != -1)
                    for (size_t i = node->firstElse_; i < node->arguments_.size(); ++i)
                        Visit(node->arguments_[i]);
                // set values of variables to fuzzy values
                for (auto idx: node->affectedVars_)
                    variables_[idx] = dt * varStore1_[nestedIFLv_ - 1][idx] + (1.0 - dt) * variables_[idx];
            }
            --nestedIFLv_;
        }

        void operator()(const std::unique_ptr<NodeTrue_> &node) {
            fuzzyStack_.Push(T_(1.0));
        }

        void operator()(const std::unique_ptr<NodeFalse_> &node) {
            fuzzyStack_.Push(T_(0.0));
        }

        void operator()(const std::unique_ptr<NodeEqual_> &node) {
            Visit(node->arguments_[0]);
            const T_ expr = dStack_.TopAndPop();

            if (node->discrete_)
                fuzzyStack_.Push(BFly(expr, node->lb_, node->ub_));
            else {
                double eps = node->eps_ < 0 ? eps_ : node->eps_;
                fuzzyStack_.Push(BFly(expr, eps));
            }
        }

        // inequality
        // for visiting superior and supEqual
        void operator()(const std::unique_ptr<NodeSuperior_> &node) {
            VisitImpl(node);
        }

        void operator()(const std::unique_ptr<NodeSupEqual_> &node) {
            VisitImpl(node);
        }

        // negation
        void operator()(const std::unique_ptr<NodeNot_> &node) {
            EvalArgs(*node);
            fuzzyStack_.Top() = 1.0 - fuzzyStack_.Top();
        }

        // combinators
        // hard coded proba stlye and->dt(lhs)*dt(rhs), or->dt(lhs)+dt(rhs)-dt(lhs)*dt(rhs)
        void operator()(const std::unique_ptr<NodeAnd_> &node) {
            EvalArgs(*node);
            const auto &args = Pop2f();
            fuzzyStack_.Push(args.first * args.second);
        }

        void operator()(const std::unique_ptr<NodeOr_> &node) {
            EvalArgs(*node);
            const auto &args = Pop2f();
            fuzzyStack_.Push(args.first + args.second - args.first * args.second);
        }
    };
}
