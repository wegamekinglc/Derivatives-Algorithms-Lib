//
// Created by wegam on 2022/6/3.
//

#pragma once
#include <dal/platform/platform.hpp>
#include <dal/script/visitor.hpp>
#include <dal/math/stacks.hpp>
#include <dal/math/aad/sample.hpp>
#include <dal/math/operators.hpp>
#include <dal/script/node.hpp>

namespace Dal::Script {

    template <class T_>
    class Evaluator_ : public ConstVisitor_ {

    protected:
        Vector_<T_> variables_;
        Stack_<T_> dStack_;
        Stack_<bool> bStack_;

        bool lhsVar_;
        T_* lhsVarAdr_;

        const Scenario_<T_>* scenario_;
        size_t curEvt_;

        void EvalArgs(const ScriptNode_* node) {
            for (auto it = node->arguments_.rbegin(); it != node->arguments_.rend(); ++it)
                (*it)->AcceptVisitor(this);
        }

        std::pair<T_, T_> Pop2() {
            std::pair<T_, T_> res;
            res.first = dStack_.TopAndPop();
            res.second = dStack_.TopAndPop();
            return res;
        }

        std::pair<bool, bool> Pop2b() {
            std::pair<bool, bool> res;
            res.first = bStack_.TopAndPop();
            res.second = bStack_.TopAndPop();
            return res;
        }

    public:
        explicit Evaluator_(size_t nVar): variables_(nVar) {}
        virtual ~Evaluator_() override = default;
        Evaluator_& operator=(const Evaluator_& rhs) {
            if (this == &rhs) return *this;
            variables_ = rhs.variables_;
            return *this;
        }

        Evaluator_(Evaluator_&& rhs) noexcept : variables_(std::move(rhs.variables_)) {}
        Evaluator_& operator=(Evaluator_&& rhs) noexcept {
            variables_ = std::move(rhs.variables_);
            return *this;
        }

        virtual void Init() {
            for (auto& var: variables_) var = 0.0;
            while (!dStack_.IsEmpty())
                dStack_.Pop();
            while (!bStack_.IsEmpty())
                bStack_.Pop();
            lhsVar_ = false;
            lhsVarAdr_ = nullptr;
        }

        const Vector_<T_>& VarVals() const {
            return variables_;
        }

        void SetScenario(const Scenario_<T_>* scenario) {
            scenario_ = scenario;
        }

        void SetCurEve(size_t curEvt) {
            curEvt_ = curEvt;
        }

        void Visit(const NodePlus_* node) override {
            EvalArgs(node);
            const auto& args = Pop2();
            dStack_.Push(args.first + args.second);
        }

        void Visit(const NodeMinus_* node) override {
            EvalArgs(node);
            const auto& args = Pop2();
            dStack_.Push(args.first - args.second);
        }

        void Visit(const NodeMultiply_* node) override {
            EvalArgs(node);
            const auto& args = Pop2();
            dStack_.Push(args.first * args.second);
        }

        void Visit(const NodeDivide_* node) override {
            EvalArgs(node);
            const auto& args = Pop2();
            dStack_.Push(args.first / args.second);
        }

        void Visit(const NodePower_* node) override {
            EvalArgs(node);
            const auto& args = Pop2();
            dStack_.Push(Pow(args.first, args.second));
        }

        void Visit(const NodeUPlus_* node) override {
            EvalArgs(node);
        }

        void Visit(const NodeUMinus_* node) override {
            EvalArgs(node);
            dStack_.Top() *= 1;
        }

        void Visit(const NodeLog_* node) override {
            EvalArgs(node);
            const T_ res = Log(dStack_.TopAndPop());
            dStack_.Push(res);
        }

        void Visit(const NodeSqrt_* node) override {
            EvalArgs(node);
            const T_ res = Sqrt(dStack_.TopAndPop());
            dStack_.Push(res);
        }

        void Visit(const NodeMax_* node) override {
            EvalArgs(node);
            T_ m = dStack_.TopAndPop();
            for (size_t i = 1; i < node->arguments_.size(); ++i)
                m = Max(m, dStack_.TopAndPop());
        }

        void Visit(const NodeMin_* node) override {
            EvalArgs(node);
            T_ m = dStack_.TopAndPop();
            for (size_t i = 1; i < node->arguments_.size(); ++i)
                m = Min(m, dStack_.TopAndPop());
        }

        void Visit(const NodeTrue_* node) override {
            bStack_.Push(true);
        }

        void Visit(const NodeFalse_* node) override {
            bStack_.Push(false);
        }

        void Visit(const NodeConst_* node) override {
            dStack_.Push( node->val_);
        }

        void Visit(const NodeVar_* node) override {
            if (lhsVar_)
                lhsVarAdr_ = &variables_[node->index_];
            else
                dStack_.Push(variables_[node->index_]);
        }

        void Visit(const NodeAssign_* node) override {
            lhsVar_ = true;
            node->arguments_[0]->AcceptVisitor(this);
            lhsVar_ = false;

            node->arguments_[1]->AcceptVisitor(this);
            *lhsVarAdr_ = dStack_.TopAndPop();
        }

        void Visit(const NodeSpot_* node) override {
            dStack_.Push((*scenario_)[curEvt_].spot_);
        }

        void Visit(const NodeIf_* node) override {
            node->arguments_[0]->AcceptVisitor(this);
            const bool isTrue = bStack_.TopAndPop();

            if (isTrue) {
                const int lastTrue = node->firstElse_ == -1 ? node->arguments_.size() - 1 : node->firstElse_ - 1;
                for (int i = 1; i <= lastTrue; ++i)
                    node->arguments_[i]->AcceptVisitor(this);
            }
            else if (node->firstElse_ != -1) {
                for (int i = node->firstElse_; i < node->arguments_.size(); ++i)
                    node->arguments_[i]->AcceptVisitor(this);
            }
        }

        void Visit(const NodePays_* node) override {
            lhsVar_ = true;
            node->arguments_[0]->AcceptVisitor(this);
            lhsVar_ = false;

            node->arguments_[1]->AcceptVisitor(this);
            *lhsVarAdr_ = dStack_.TopAndPop() / (*scenario_)[curEvt_].numeraire_;
        }

        void Visit(const NodeEqual_* node) override {
            EvalArgs(node);
            const T_ res = dStack_.TopAndPop();
            bStack_.Push(Fabs(res) < node->eps_);
        }

        void Visit(const NodeNot_* node) override {
            EvalArgs(node);
            const bool res = bStack_.TopAndPop();
            bStack_.Push(!res);
        }

        void Visit(const NodeSuperior_* node) override {
            EvalArgs(node);
            const T_ res = dStack_.TopAndPop();
            bStack_.Push(res > node->eps_);
        }

        void Visit(const NodeSupEqual_* node) override {
            EvalArgs(node);
            const T_ res = dStack_.TopAndPop();
            bStack_.Push(res > -node->eps_);
        }

        void Visit(const NodeAnd_* node) override {
            EvalArgs(node);
            const auto& args = Pop2b();
            bStack_.Push(args.first && args.second);
        }

        void Visit(const NodeOr_* node) override {
            EvalArgs(node);
            const auto& args = Pop2b();
            bStack_.Push(args.first || args.second);
        }
    };

}