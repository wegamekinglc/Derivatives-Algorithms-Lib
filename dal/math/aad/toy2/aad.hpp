/*
 * Modified by wegamekinglc on 2020/12/13.
 * Written by Antoine Savine in 2018
 * This code is the strict IP of Antoine Savine
 * License to use and alter this code for personal and commercial applications
 * is freely granted to any person or company who purchased a copy of the book
 * Modern Computational Finance: AAD and Parallel Simulations
 * Antoine Savine
 * Wiley, 2018
 * As long as this comment is preserved at the top of the file
*/


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