//
// Created by wegam on 2022/2/14.
//

#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>

namespace Dal::Script {
    void NodeCollect_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeTrue_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeFalse_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeUPlus_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeUMinus_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodePlus_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeMinus_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeMultiply_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeDivide_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodePower_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeSqrt_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeLog_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeMax_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeMin_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeConst_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeVar_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeAssign_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeIf_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeEqual_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeSuperior_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeSupEqual_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeAnd_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeOr_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodeNot_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    void NodePays_::AcceptVisitor(Visitor_* visitor) {
        visitor->Visit(this);
    }

    /*
     *
     */

    void NodeCollect_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeTrue_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeFalse_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeUPlus_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeUMinus_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodePlus_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeMinus_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeMultiply_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeDivide_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodePower_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeSqrt_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeLog_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeMax_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeMin_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeConst_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeVar_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeAssign_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeIf_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeEqual_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeSuperior_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeSupEqual_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeAnd_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeOr_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodeNot_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }

    void NodePays_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->Visit(this);
    }
}
