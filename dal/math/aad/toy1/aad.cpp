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


#include <dal/math/aad/toy1/aad.hpp>
#include <iostream>
#include <cmath>

namespace Dal {
    namespace AAD_Toy1 {
        double Node_::Result() const { return result_; }

        void Node_::ResetProcessed() {
            for (const auto& arg : arguments_)
                arg->ResetProcessed();
            processed_ = false;
        }

        void Node_::SetOrder(unsigned int order) { order_ = order; }

        unsigned Node_::Order() const { return order_; }

        double& Node_::Adjoint() { return adjoint_; }

        void Node_::ResetAdjoints() {
            for (const auto& arg : arguments_)
                arg->ResetAdjoints();
            adjoint_ = 0.;
        }

        PlusNode_::PlusNode_(const shared_ptr<Node_>& lhs, const shared_ptr<Node_>& rhs) {
            arguments_.Resize(2);
            arguments_[0] = lhs;
            arguments_[1] = rhs;
        }

        /*
     * Plus nodes stuff
         */

        void PlusNode_::Evaluate() { result_ = arguments_[0]->Result() + arguments_[1]->Result(); }

        void PlusNode_::LogInstruction() {
            std::cout << "y" << order_ << " = y" << arguments_[0]->Order() << " + y" << arguments_[1]->Order()
                      << std::endl;
        }

        void PlusNode_::PropagateAdjoint() {
            arguments_[0]->Adjoint() += adjoint_;
            arguments_[1]->Adjoint() += adjoint_;
            adjoint_ = 0.;
        }

        /*
     * Times nodes stuff
         */

        TimesNode_::TimesNode_(const shared_ptr<Node_>& lhs, const shared_ptr<Node_>& rhs) {
            arguments_.Resize(2);
            arguments_[0] = lhs;
            arguments_[1] = rhs;
        }

        void TimesNode_::Evaluate() { result_ = arguments_[0]->Result() * arguments_[1]->Result(); }

        void TimesNode_::LogInstruction() {
            std::cout << "y" << order_ << " = y" << arguments_[0]->Order() << " * y" << arguments_[1]->Order()
                      << std::endl;
        }

        void TimesNode_::PropagateAdjoint() {
            arguments_[0]->Adjoint() += arguments_[1]->Result() * adjoint_;
            arguments_[1]->Adjoint() += arguments_[0]->Result() * adjoint_;
            adjoint_ = 0.;
        }

        /*
     * Log nodes stuff
         */

        LogNode_::LogNode_(const shared_ptr<Node_>& arg) {
            arguments_.Resize(1);
            arguments_[0] = arg;
        }

        void LogNode_::Evaluate() { result_ = std::log(arguments_[0]->Result()); }

        void LogNode_::LogInstruction() {
            std::cout << "y" << order_ << " = log(y" << arguments_[0]->Order() << ")" << std::endl;
        }

        void LogNode_::PropagateAdjoint() {
            arguments_[0]->Adjoint() += adjoint_ / arguments_[0]->Result();
            adjoint_ = 0.;
        }

        /*
     * Leaf nodes stuff
         */

        Leaf_::Leaf_(double val) : value_(val) {}

        double Leaf_::GetVal() const { return value_; }

        void Leaf_::SetVal(double val) { value_ = val; }

        void Leaf_::Evaluate() { result_ = value_; }

        void Leaf_::LogInstruction() { std::cout << "y" << order_ << " = " << value_ << std::endl; }

        void Leaf_::PropagateAdjoint() {}

        /*
     * Number class stuff
         */

        Number_::Number_(double val) : node_(new Leaf_(val)) {}

        Number_::Number_(const shared_ptr<Node_>& node) : node_(node) {}

        shared_ptr<Node_> Number_::Node() const { return node_; }

        void Number_::SetVal(double val) { std::dynamic_pointer_cast<Leaf_>(node_)->SetVal(val); }

        double Number_::GetVal() const { return std::dynamic_pointer_cast<Leaf_>(node_)->GetVal(); }

        double Number_::Evaluate() {
            node_->ResetProcessed();
            auto func = [](Node_& n) { n.Evaluate(); };
            node_->PostOrder(func);
            return node_->Result();
        }

        void Number_::SetOrder() {
            node_->ResetProcessed();
            auto func = [order = 0](Node_& n) mutable { n.SetOrder(++order); };
            node_->PostOrder(func);
        }

        void Number_::LogResult() const {
            node_->ResetProcessed();
            auto func = [](Node_& n) {
                std::cout << "Processed node " << n.Order() << " result " << n.Result() << std::endl;
            };
            node_->PostOrder(func);
        }

        void Number_::LogProgram() {
            node_->ResetProcessed();
            auto func = [](Node_& n) { n.LogInstruction(); };
            node_->PostOrder(func);
        }

        double& Number_::Adjoint() { return node_->Adjoint(); }

        void Number_::PropagateAdjoints() {
            node_->ResetAdjoints();
            node_->Adjoint() = 1.0;
            auto func = [](Node_& n) { n.PropagateAdjoint(); };
            node_->PreOrder(func);
        }

        /*
     * Operators stuff
         */

        shared_ptr<Node_> operator+(const Number_& lhs, const Number_& rhs) {
            return shared_ptr<Node_>(new PlusNode_(lhs.Node(), rhs.Node()));
        }

        shared_ptr<Node_> operator*(const Number_& lhs, const Number_& rhs) {
            return shared_ptr<Node_>(new TimesNode_(lhs.Node(), rhs.Node()));
        }

        shared_ptr<Node_> Log(const Number_& arg) { return shared_ptr<Node_>(new LogNode_(arg.Node())); }
    }
}
