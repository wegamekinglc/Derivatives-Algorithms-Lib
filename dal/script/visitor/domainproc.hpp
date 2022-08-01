//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <cmath>
#include <dal/math/stacks.hpp>
#include <dal/math/vectors.hpp>
#include <dal/script/visitor.hpp>
#include <dal/script/visitor/domain.hpp>

/*IF--------------------------------------------------------------------------
enumeration DomainCondProp
    flag for domain condition property
switchable
alternative AlwaysTrue
alternative AlwaysFalse
alternative TrueOrFalse
-IF-------------------------------------------------------------------------*/

namespace Dal::Script {
#include <dal/auto/MG_DomainCondProp_enum.hpp>
    class DomainProcessor_ : public Visitor_ {
        const bool fuzzy_;
        Vector_<Domain_> domains_;
        Stack_<Domain_> domainStack_;
        Stack_<DomainCondProp_> conditionStack_;
        bool lhsVar_;
        size_t lhsVarIdx_;

    public:
        // domains start with the singleton 0
        DomainProcessor_(size_t nVar, bool fuzzy) : fuzzy_(fuzzy), domains_(nVar, 0.0), lhsVar_(false) {}

        // visit
        // expressions
        // binaries
        void Visit(NodePlus_* node) override;
        void Visit(NodeMinus_* node) override;
        void Visit(NodeMultiply_* node) override;
        void Visit(NodeDivide_* node) override;
        void Visit(NodePower_* node) override;

        // unaries
        void Visit(NodeUPlus_* node) override;
        void Visit(NodeUMinus_* node) override;

        // functions
        void Visit(NodeLog_* node) override;
        void Visit(NodeSqrt_* node) override;
        void Visit(NodeMax_* node) override;
        void Visit(NodeMin_* node) override;
        void Visit(NodeSmooth_* node) override;

        // conditions
        void Visit(NodeEqual_* node) override;
        void Visit(NodeNot_* node) override;

        // for visit superior and supEqual
        template <bool strict, class NodeSup_> inline void Visit(NodeSup_* node) {
            VisitArguments(node);

            Domain_& dom = domainStack_.Top();

            // Always true / false?
            if (!dom.CanBePositive(strict)) {
                node->alwaysTrue_ = false;
                node->alwaysFalse_ = true;
                conditionStack_.Push(DomainCondProp_("AlwaysFalse"));
            } else if (!dom.CanBeNegative(!strict)) {
                node->alwaysTrue_ = true;
                node->alwaysFalse_ = false;
                conditionStack_.Push(DomainCondProp_("AlwaysTrue"));
            }
            // Can be true or false
            else {
                node->alwaysTrue_ = node->alwaysFalse_ = false;
                conditionStack_.Push(DomainCondProp_("TrueOrFalse"));

                if (fuzzy_) {
                    // Continuous or discrete?
                    node->discrete_ = !dom.CanBeZero() || dom.ZeroIsDiscrete();

                    // Fuzzy logic processing
                    if (node->discrete_) {
                        // Case 1: expr cannot be zero
                        if (!dom.CanBeZero()) {
                            // 	we know we have subdomains on the left and on the right of 0
                            dom.SmallestPosLb(node->ub_, true);
                            dom.BiggestNegRb(node->lb_, true);
                        }
                        // Case 2: {0} is a singleton
                        else {
                            if (strict) {
                                node->lb_ = 0.0;
                                dom.SmallestPosLb(node->ub_, true);
                            } else {
                                node->ub_ = 0.0;
                                dom.BiggestNegRb(node->lb_, true);
                            }
                        }
                    }
                }
            }
            domainStack_.Pop();
        }

        void Visit(NodeSuperior_* node) override;
        void Visit(NodeSupEqual_* node) override;
        void Visit(NodeAnd_* node) override;
        void Visit(NodeOr_* node) override;

        // instructions
        void Visit(NodeIf_* node) override;
        void Visit(NodeAssign_* node) override;
        void Visit(NodePays_* node) override;

        // variables and constants
        void Visit(NodeVar_* node) override;
        void Visit(NodeConst_* node) override;

        // scenario related
        void Visit(NodeSpot_* node) override;
    };
} // namespace Dal::Script
