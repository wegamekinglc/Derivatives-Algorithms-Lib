//
// Created by wegam on 2020/12/5.
//

#pragma once

#include <memory>
#include <dal/math/vectors.hpp>

namespace Dal {
    namespace AAD_Toy2 {
        class Node_ {
        protected:
            Vector_<Node_*> arguments_;
            double result_;
            double adjoint_;

        public:
            double Result() const;
            double& Adjoint();
            void ResetAdjoints();
            virtual void PropagateAdjoint() = 0;
        };

        class PlusNode_ : public Node_ {
        public:
            PlusNode_(Node_* lhs, Node_* rhs);
            void PropagateAdjoint() override;
        };

        class TimesNode_ : public Node_ {
        public:
            TimesNode_(Node_* lhs, Node_* rhs);
            void PropagateAdjoint() override;
        };

        class LogNode_ : public Node_ {
        public:
            LogNode_(Node_* arg);
            void PropagateAdjoint() override;
        };

        class Leaf_ : public Node_ {
        public:
            Leaf_(double val);
            double GetVal() const;
            void SetVal(double val);
            void PropagateAdjoint() override;
        };

        class Number_ {
            Node_* node_;

        public:
            static Vector_<std::unique_ptr<Node_>> tape_;

            Number_(double val);
            Number_(Node_* node);

            Node_* Node() const;
            void SetVal(double val);
            double GetVal() const;
            double& Adjoint();
            void PropagateAdjoints();
        };

        Number_ operator+(const Number_& lhs, const Number_& rhs);
        Number_ operator*(const Number_& lhs, const Number_& rhs);
        Number_ Log(const Number_& arg);
    }
}