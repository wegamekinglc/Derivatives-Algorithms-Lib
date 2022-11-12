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

    template <class T_> class Evaluator_ {

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

        void Visit(const std::unique_ptr<ScriptNode_>& node) {
            if (auto item = std::get_if<NodePlus_>(&*node)) {
                EvalArgs(*item);
                const auto& args = Pop2();
                dStack_.Push(args.first + args.second);
            } else if (auto item = std::get_if<NodeConst_>(&*node)) {
                dStack_.Push(item->val_);
            } else if (auto item = std::get_if<NodeMinus_>(&*node)) {
                EvalArgs(*item);
                const auto& args = Pop2();
                dStack_.Push(args.first - args.second);
            }
        }

//        using ConstVisitor_::operator();
//
//        void operator()(const std::unique_ptr<NodePlus_>& node) {
//            EvalArgs(node);
//            const auto& args = Pop2();
//            dStack_.Push(args.first + args.second);
//        }
//
//        void operator()(const std::unique_ptr<NodeMinus_>& node) {
//            EvalArgs(node);
//            const auto& args = Pop2();
//            dStack_.Push(args.first - args.second);
//        }
//
//        void operator()(const std::unique_ptr<NodeMultiply_>& node) {
//            EvalArgs(node);
//            const auto& args = Pop2();
//            dStack_.Push(args.first * args.second);
//        }
//
//        void operator()(const std::unique_ptr<NodeDivide_>& node) {
//            EvalArgs(node);
//            const auto& args = Pop2();
//            dStack_.Push(args.first / args.second);
//        }
//
//        void operator()(const std::unique_ptr<NodePower_>& node) {
//            EvalArgs(node);
//            const auto& args = Pop2();
//            dStack_.Push(AAD::Pow(args.first, args.second));
//        }
//
//        void operator()(const std::unique_ptr<NodeUPlus_>& node) { EvalArgs(node); }
//
//        void operator()(const std::unique_ptr<NodeUMinus_>& node) {
//            EvalArgs(node);
//            dStack_.Top() *= 1;
//        }
//
//        void operator()(const std::unique_ptr<NodeLog_>& node) {
//            EvalArgs(node);
//            const T_ res = AAD::Log(dStack_.TopAndPop());
//            dStack_.Push(res);
//        }
//
//        void operator()(const std::unique_ptr<NodeSqrt_>& node) {
//            EvalArgs(node);
//            const T_ res = AAD::Sqrt(dStack_.TopAndPop());
//            dStack_.Push(res);
//        }
//
//        void operator()(const std::unique_ptr<NodeMax_>& node) {
//            EvalArgs(node);
//            T_ m = dStack_.TopAndPop();
//            for (size_t i = 1; i < node->arguments_.size(); ++i)
//                m = AAD::Max(m, dStack_.TopAndPop());
//            dStack_.Push(m);
//        }
//
//        void operator()(const std::unique_ptr<NodeMin_>& node) {
//            EvalArgs(node);
//            T_ m = dStack_.TopAndPop();
//            for (size_t i = 1; i < node->arguments_.size(); ++i)
//                m = AAD::Min(m, dStack_.TopAndPop());
//            dStack_.Push(m);
//        }
//
//        void operator()(const std::unique_ptr<NodeTrue_>& node) { bStack_.Push(true); }
//
//        void operator()(const std::unique_ptr<NodeFalse_>& node) { bStack_.Push(false); }
//
//        void operator()(const std::unique_ptr<NodeConst_>& node) { dStack_.Push(node->val_); }
//
//        void operator()(const std::unique_ptr<NodeVar_>& node) {
//            if (lhsVar_)
//                lhsVarAdr_ = &variables_[node->index_];
//            else
//                dStack_.Push(variables_[node->index_]);
//        }
//
//        void operator()(const std::unique_ptr<NodeAssign_>& node) {
//            lhsVar_ = true;
//            std::visit(*this, *node->arguments_[0]);
//            lhsVar_ = false;
//
//            std::visit(*this, *node->arguments_[1]);
//            *lhsVarAdr_ = dStack_.TopAndPop();
//        }
//
//        void operator()(const std::unique_ptr<NodeSpot_>& node) {
//            dStack_.Push((*scenario_)[curEvt_].spot_);
//        }
//
//        void operator()(const std::unique_ptr<NodeIf_>& node) {
//            std::visit(*this, *node->arguments_[0]);
//            const bool isTrue = bStack_.TopAndPop();
//
//            if (isTrue) {
//                const int lastTrue = node->firstElse_ == -1 ? node->arguments_.size() - 1 : node->firstElse_ - 1;
//                for (int i = 1; i <= lastTrue; ++i)
//                    std::visit(*this, *node->arguments_[i]);
//            } else if (node->firstElse_ != -1) {
//                for (int i = node->firstElse_; i < node->arguments_.size(); ++i)
//                    std::visit(*this, *node->arguments_[i]);
//            }
//        }
//
//        void operator()(const std::unique_ptr<NodePays_>& node) {
//            lhsVar_ = true;
//            std::visit(*this, *node->arguments_[0]);
//            lhsVar_ = false;
//
//            std::visit(*this, *node->arguments_[1]);
//            *lhsVarAdr_ += dStack_.TopAndPop() / (*scenario_)[curEvt_].numeraire_;
//        }
//
//        void operator()(const std::unique_ptr<NodeEqual_>& node) {
//            EvalArgs(node);
//            const T_ res = dStack_.TopAndPop();
//            bStack_.Push(AAD::Fabs(res) < node->eps_);
//        }
//
//        void operator()(const std::unique_ptr<NodeNot_>& node) {
//            EvalArgs(node);
//            const bool res = bStack_.TopAndPop();
//            bStack_.Push(!res);
//        }
//
//        void operator()(const std::unique_ptr<NodeSuperior_>& node) {
//            EvalArgs(node);
//            const T_ res = dStack_.TopAndPop();
//            bStack_.Push(res > node->eps_);
//        }
//
//        void operator()(const std::unique_ptr<NodeSupEqual_>& node) {
//            EvalArgs(node);
//            const T_ res = dStack_.TopAndPop();
//            bStack_.Push(res > -node->eps_);
//        }
//
//        void operator()(const std::unique_ptr<NodeAnd_>& node) {
//            EvalArgs(node);
//            const auto& args = Pop2b();
//            bStack_.Push(args.first && args.second);
//        }
//
//        void operator()(const std::unique_ptr<NodeOr_>& node) {
//            EvalArgs(node);
//            const auto& args = Pop2b();
//            bStack_.Push(args.first || args.second);
//        }
//
//        void operator()(const std::unique_ptr<NodeSmooth_>& node) {
//            //	Eval the condition
//            std::visit(*this, *node->arguments_[0]);
//            const T_ x = dStack_.TopAndPop();
//
//            //	Eval the epsilon
//            std::visit(*this, *node->arguments_[3]);
//            const T_ halfEps = 0.5 * dStack_.TopAndPop();
//
//            //	Left
//            if (x < -halfEps)
//                std::visit(*this, *node->arguments_[2]);
//            //	Right
//            else if (x > halfEps)
//                std::visit(*this, *node->arguments_[1]);
//            //	Fuzzy
//            else {
//                std::visit(*this, *node->arguments_[1]);
//                const T_ vPos = dStack_.TopAndPop();
//
//                std::visit(*this, *node->arguments_[2]);
//                const T_ vNeg = dStack_.TopAndPop();
//                dStack_.Push(vNeg + 0.5 * (vPos - vNeg) / halfEps * (x + halfEps));
//            }
//        }
    };

} // namespace Dal::Script
