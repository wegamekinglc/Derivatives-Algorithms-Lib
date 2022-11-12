//
// Created by wegam on 2022/6/3.
//

#pragma once

#include <dal/math/aad/operators.hpp>
#include <dal/math/aad/sample.hpp>
#include <dal/math/stacks.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/experimental/node.hpp>
#include <dal/script/experimental/visitor.hpp>

namespace Dal::Script::Experimental {

    template <class T_> class Evaluator_: public ConstVisitor_<Evaluator_<T_>> {

    protected:
        Vector_<T_> variables_;
        Stack_<T_> dStack_;
        Stack_<bool> bStack_;

        bool lhsVar_;
        T_* lhsVarAdr_;

        const AAD::Scenario_<T_>* scenario_;
        size_t curEvt_;

        template<class N_>
        void EvalArgs(const N_ &node) {
            for (auto it = node.arguments_.rbegin(); it != node.arguments_.rend(); ++it)
                this->Visit(*it);
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

    public:
        explicit Evaluator_(size_t nVar) : variables_(nVar) {}
        Evaluator_& operator=(const Evaluator_& rhs) {
            if (this == &rhs)
                return *this;
            variables_ = rhs.variables_;
            return *this;
        }

        Evaluator_(Evaluator_&& rhs) noexcept : variables_(std::move(rhs.variables_)) {}
        Evaluator_& operator=(Evaluator_&& rhs) noexcept {
            variables_ = std::move(rhs.variables_);
            return *this;
        }

        void Init() {
            for (auto& var : variables_)
                var = 0.0;
            while (!dStack_.IsEmpty())
                dStack_.Pop();
            while (!bStack_.IsEmpty())
                bStack_.Pop();
            lhsVar_ = false;
            lhsVarAdr_ = nullptr;
        }

        const Vector_<T_>& VarVals() const { return variables_; }

        void SetScenario(const AAD::Scenario_<T_>* scenario) { scenario_ = scenario; }

        void SetCurEvt(size_t curEvt) { curEvt_ = curEvt; }

        void VisitPlus(const NodePlus_& node) {
            EvalArgs(node);
            const auto& args = Pop2();
            dStack_.Push(args.first + args.second);
        }

        void VisitMinus(const NodeMinus_& node) {
            EvalArgs(node);
            const auto& args = Pop2();
            dStack_.Push(args.first - args.second);
        }

        void VisitMultiply(const NodeMultiply_& node) {
            EvalArgs(node);
            const auto& args = Pop2();
            dStack_.Push(args.first * args.second);
        }

        void VisitDivide(const NodeDivide_& node) {
            EvalArgs(node);
            const auto& args = Pop2();
            dStack_.Push(args.first / args.second);
        }

        void VisitPower(const NodePower_& node) {
            EvalArgs(node);
            const auto& args = Pop2();
            dStack_.Push(AAD::Pow(args.first, args.second));
        }

        void VisitUPlus(const NodeUPlus_& node) { EvalArgs(node); }

        void operator()(const NodeUMinus_& node) {
            EvalArgs(node);
            dStack_.Top() *= 1;
        }

        void VisitLog(const NodeLog_& node) {
            EvalArgs(node);
            const T_ res = AAD::Log(dStack_.TopAndPop());
            dStack_.Push(res);
        }

        void VisitSqrt(const NodeSqrt_& node) {
            EvalArgs(node);
            const T_ res = AAD::Sqrt(dStack_.TopAndPop());
            dStack_.Push(res);
        }

        void VisitMax(const NodeMax_& node) {
            EvalArgs(node);
            T_ m = dStack_.TopAndPop();
            for (size_t i = 1; i < node.arguments_.size(); ++i)
                m = AAD::Max(m, dStack_.TopAndPop());
            dStack_.Push(m);
        }

        void VisitMin(const NodeMin_& node) {
            EvalArgs(node);
            T_ m = dStack_.TopAndPop();
            for (size_t i = 1; i < node.arguments_.size(); ++i)
                m = AAD::Min(m, dStack_.TopAndPop());
            dStack_.Push(m);
        }

        void VisitTrue(const NodeTrue_& node) { bStack_.Push(true); }

        void VisitFalse(const NodeFalse_& node) { bStack_.Push(false); }

        void VisitConst(const NodeConst_& node) { dStack_.Push(node.val_); }

        void VisitVar(const NodeVar_& node) {
            if (lhsVar_)
                lhsVarAdr_ = &variables_[node.index_];
            else
                dStack_.Push(variables_[node.index_]);
        }

        void VisitAssign(const NodeAssign_& node) {
            lhsVar_ = true;
            this->Visit(node.arguments_[0]);
            lhsVar_ = false;

            this->Visit(node.arguments_[1]);
            *lhsVarAdr_ = dStack_.TopAndPop();
        }

        void VisitSpot(const NodeSpot_& node) {
            dStack_.Push((*scenario_)[curEvt_].spot_);
        }

        void VisitIf(const NodeIf_& node) {
            this->Visit(node.arguments_[0]);
            const bool isTrue = bStack_.TopAndPop();

            if (isTrue) {
                const int lastTrue = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;
                for (int i = 1; i <= lastTrue; ++i)
                    this->Visit(node.arguments_[i]);
            } else if (node.firstElse_ != -1) {
                for (int i = node.firstElse_; i < node.arguments_.size(); ++i)
                    this->Visit(node.arguments_[i]);
            }
        }

        void VisitPays(const NodePays_& node) {
            lhsVar_ = true;
            this->Visit(node.arguments_[0]);
            lhsVar_ = false;

            this->Visit(node.arguments_[1]);
            *lhsVarAdr_ += dStack_.TopAndPop() / (*scenario_)[curEvt_].numeraire_;
        }

        void VisitEqual(const NodeEqual_& node) {
            EvalArgs(node);
            const T_ res = dStack_.TopAndPop();
            bStack_.Push(AAD::Fabs(res) < node.eps_);
        }

        void VisitNot(const NodeNot_& node) {
            EvalArgs(node);
            const bool res = bStack_.TopAndPop();
            bStack_.Push(!res);
        }

        void VisitSuperior(const NodeSuperior_& node) {
            EvalArgs(node);
            const T_ res = dStack_.TopAndPop();
            bStack_.Push(res > node.eps_);
        }

        void VisitSupEqual(const NodeSupEqual_& node) {
            EvalArgs(node);
            const T_ res = dStack_.TopAndPop();
            bStack_.Push(res > -node.eps_);
        }

        void VisitAnd(const NodeAnd_& node) {
            EvalArgs(node);
            const auto& args = Pop2b();
            bStack_.Push(args.first && args.second);
        }

        void VisitOr(const NodeOr_& node) {
            EvalArgs(node);
            const auto& args = Pop2b();
            bStack_.Push(args.first || args.second);
        }

        void VisitSmooth(const NodeSmooth_& node) {
            //	Eval the condition
            this->Visit(node.arguments_[0]);
            const T_ x = dStack_.TopAndPop();

            //	Eval the epsilon
            this->Visit(node.arguments_[3]);
            const T_ halfEps = 0.5 * dStack_.TopAndPop();

            //	Left
            if (x < -halfEps)
                this->Visit(node.arguments_[2]);
            //	Right
            else if (x > halfEps)
                this->Visit(node.arguments_[1]);
            //	Fuzzy
            else {
                this->Visit(node.arguments_[1]);
                const T_ vPos = dStack_.TopAndPop();

                this->Visit(node.arguments_[2]);
                const T_ vNeg = dStack_.TopAndPop();
                dStack_.Push(vNeg + 0.5 * (vPos - vNeg) / halfEps * (x + halfEps));
            }
        }
    };

} // namespace Dal::Script
