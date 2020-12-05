//
// Created by wegam on 2020/12/2.
//

#pragma once

#include <memory>
#include <queue>
#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>

namespace Dal {
    using std::shared_ptr;
    class Node_ {
    protected:
        Vector_<shared_ptr<Node_>> arguments_;
        bool processed_ = false;
        unsigned order_ = 0;
        double result_;
        double adjoint_ = 0.;

    public:
        virtual ~Node_() = default;

        template <class V_>
        void                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               PostOrder(V_& visitFunc) {
            if (!processed_) {
                for (const auto& arg : arguments_)
                    arg->PostOrder(visitFunc);
                visitFunc(*this);
                processed_ = true;
            }
        }

        template <class V_>
        void PreOrder(V_& visitFunc) {
            visitFunc(*this);
            for (const auto& arg : arguments_)
                arg->PreOrder(visitFunc);
        }

        template <class V_>
        void BreadthFirst(V_& visitFunc) {
            std::queue<std::shared_ptr<Node_>> q;
            BreadthFirst(visitFunc, q);
        }

        template <class V_>
        void BreadthFirst(V_& visitFunc, std::queue<std::shared_ptr<Node_>>& q) {
            for (const auto& arg : arguments_)
                q.push(arg);

            visitFunc(*this);
            while (!q.empty()) {
                auto n = q.front();
                q.pop();
                n->BreadthFirst(visitFunc, q);
            }
        }

        double Result() const;
        void ResetProcessed();
        void SetOrder(unsigned order);
        unsigned Order() const;
        double& Adjoint();
        void ResetAdjoints();

        virtual void Evaluate() = 0;
        virtual void LogInstruction() = 0;
        virtual void PropagateAdjoint() = 0;
    };

    class PlusNode_ : public Node_ {
    public:
        PlusNode_(const shared_ptr<Node_>& lhs, const shared_ptr<Node_>& rhs);
        void LogInstruction() override;
        void PropagateAdjoint() override;
        void Evaluate() override;
    };

    class TimesNode_ : public Node_ {
    public:
        TimesNode_(const shared_ptr<Node_>& lhs, const shared_ptr<Node_>& rhs);
        void LogInstruction() override;
        void PropagateAdjoint() override;
        void Evaluate() override;
    };

    class LogNode_ : public Node_ {
    public:
        LogNode_(const shared_ptr<Node_>& arg);
        void LogInstruction() override;
        void PropagateAdjoint() override;
        void Evaluate() override;
    };
    
    class Leaf_ : public Node_ {
        double value_;
    public:
        Leaf_(double val);
        double GetVal() const;
        void SetVal(double val);
        void LogInstruction() override;
        void PropagateAdjoint() override;
        void Evaluate() override;
    };

    class Number_ {
        shared_ptr<Node_> node_;
    public:
        Number_(double val);
        Number_(const shared_ptr<Node_>& node);
        shared_ptr<Node_> Node() const;
        void SetVal(double val);
        double GetVal() const;

        double Evaluate();
        void SetOrder();
        void LogResult() const;
        void LogProgram();
        double& Adjoint();
        void PropagateAdjoints();
    };

    shared_ptr<Node_> operator+(const Number_& lhs, const Number_& rhs);
    shared_ptr<Node_> operator*(const Number_& lhs, const Number_& rhs);
    shared_ptr<Node_> Log(const Number_& arg);
}