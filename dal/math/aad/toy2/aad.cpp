//
// Created by wegam on 2020/12/5.
//

#include <cmath>
#include <dal/math/aad/toy2/aad.hpp>

namespace Dal {
    namespace AAD_Toy2 {
        double Node_::Result() const { return result_; }

        double& Node_::Adjoint() { return adjoint_; }

        void Node_::ResetAdjoints() {
            for (auto arg : arguments_)
                arg->ResetAdjoints();
            adjoint_ = 0.0;
        }

        /*
     * Plus nodes stuff
         */
        PlusNode_::PlusNode_(Node_* lhs, Node_* rhs) {
            arguments_.Resize(2);
            arguments_[0] = lhs;
            arguments_[1] = rhs;
            result_ = arguments_[0]->Result() + arguments_[1]->Result();
        }

        void PlusNode_::PropagateAdjoint() {
            arguments_[0]->Adjoint() += adjoint_;
            arguments_[1]->Adjoint() += adjoint_;
        }

        /*
     * Times nodes stuff
         */
        TimesNode_::TimesNode_(Node_* lhs, Node_* rhs) {
            arguments_.Resize(2);
            arguments_[0] = lhs;
            arguments_[1] = rhs;
            result_ = arguments_[0]->Result() * arguments_[1]->Result();
        }

        void TimesNode_::PropagateAdjoint() {
            arguments_[0]->Adjoint() += arguments_[1]->Result() * adjoint_;
            arguments_[1]->Adjoint() += arguments_[0]->Result() * adjoint_;
        }

        /*
     * Log nodes stuff
         */
        LogNode_::LogNode_(Node_* arg) {
            arguments_.Resize(1);
            arguments_[0] = arg;
            result_ = std::log(arguments_[0]->Result());
        }

        void LogNode_::PropagateAdjoint() { arguments_[0]->Adjoint() += adjoint_ / arguments_[0]->Result(); }

        /*
     * Leaf nodes stuff
         */
        Leaf_::Leaf_(double val) { result_ = val; }

        double Leaf_::GetVal() const { return result_; }

        void Leaf_::SetVal(double val) { result_ = val; }

        void Leaf_::PropagateAdjoint() {}

        /*
     * Number stuff
         */

        Vector_<std::unique_ptr<Node_>> Number_::tape_;

        Number_::Number_(double val) : node_(new Leaf_(val)) { tape_.push_back(std::unique_ptr<Node_>(node_)); }

        Number_::Number_(Node_* node) : node_(node) {}

        Node_* Number_::Node() const { return node_; }

        void Number_::SetVal(double val) { dynamic_cast<Leaf_*>(node_)->SetVal(val); }

        double Number_::GetVal() const { return dynamic_cast<Leaf_*>(node_)->GetVal(); }

        double& Number_::Adjoint() { return node_->Adjoint(); }

        void Number_::PropagateAdjoints() {
            node_->ResetAdjoints();
            node_->Adjoint() = 1.0;
            auto it = tape_.rbegin();
            while (it->get() != node_)
                ++it;
            while (it != tape_.rend()) {
                (*it)->PropagateAdjoint();
                ++it;
            }
        }

        Number_ operator+(const Number_& lhs, const Number_& rhs) {
            Node_* n = new PlusNode_(lhs.Node(), rhs.Node());
            Number_::tape_.push_back(std::unique_ptr<Node_>(n));
            return n;
        }

        Number_ operator*(const Number_& lhs, const Number_& rhs) {
            Node_* n = new TimesNode_(lhs.Node(), rhs.Node());
            Number_::tape_.push_back(std::unique_ptr<Node_>(n));
            return n;
        }

        Number_ Log(const Number_& arg) {
            Node_* n = new LogNode_(arg.Node());
            Number_::tape_.push_back(std::unique_ptr<Node_>(n));
            return n;
        }
    }
}