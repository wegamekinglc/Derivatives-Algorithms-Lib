//
// Created by wegam on 2022/2/14.
//

#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>

namespace Dal::Script {
    void NodeCollect_::AcceptVisitor(Visitor_* visitor) {
        visitor->VisitCollect(this);
    }

    void NodeTrue_::AcceptVisitor(Visitor_* visitor) {
        visitor->VisitTrue(this);
    }

    void NodeFalse_::AcceptVisitor(Visitor_* visitor) {
        visitor->VisitFalse(this);
    }

    void NodeCollect_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->VisitCollect(this);
    }

    void NodeFalse_::AcceptVisitor(ConstVisitor_* visitor) const {
        visitor->VisitFalse(this);
    }

}
