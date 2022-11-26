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
    class DomainProcessor_ : public Visitor_<DomainProcessor_> {
        const bool fuzzy_;
        Vector_<Domain_> domains_;
        Stack_<Domain_> domainStack_;
        Stack_<DomainCondProp_> conditionStack_;
        bool lhsVar_;
        size_t lhsVarIdx_;

    public:
        // domains start with the singleton 0
        DomainProcessor_(size_t nVar, bool fuzzy) : fuzzy_(fuzzy), domains_(nVar, 0.0), lhsVar_(false) {}

        using Visitor_<DomainProcessor_>::operator();
        using Visitor_<DomainProcessor_>::Visit;
        // visit
        // expressions
        // binaries
        void operator()(std::unique_ptr<NodePlus_>& node);
        void operator()(std::unique_ptr<NodeMinus_>& node);
        void operator()(std::unique_ptr<NodeMultiply_>& node);
        void operator()(std::unique_ptr<NodeDivide_>& node);
        void operator()(std::unique_ptr<NodePower_>& node);

        // unaries
        void operator()(std::unique_ptr<NodeUPlus_>& node);
        void operator()(std::unique_ptr<NodeUMinus_>& node);

        // functions
        void operator()(std::unique_ptr<NodeLog_>& node);
        void operator()(std::unique_ptr<NodeSqrt_>& node);
        void operator()(std::unique_ptr<NodeMax_>& node);
        void operator()(std::unique_ptr<NodeMin_>& node);
        void operator()(std::unique_ptr<NodeSmooth_>& node);

        // conditions
        void operator()(std::unique_ptr<NodeEqual_>& node);
        void operator()(std::unique_ptr<NodeNot_>& node);

        // for visit superior and supEqual
        template <bool strict, class NodeSup_> void operator()(std::unique_ptr<NodeSup_>& node) {
            VisitArguments(*node);

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

        void operator()(std::unique_ptr<NodeSuperior_>& node);
        void operator()(std::unique_ptr<NodeSupEqual_>& node);
        void operator()(std::unique_ptr<NodeAnd_>& node);
        void operator()(std::unique_ptr<NodeOr_>& node);

        // instructions
        void operator()(std::unique_ptr<NodeIf_>& node);
        void operator()(std::unique_ptr<NodeAssign_>& node);
        void operator()(std::unique_ptr<NodePays_>& node);

        // variables and constants
        void operator()(std::unique_ptr<NodeVar_>& node);
        void operator()(std::unique_ptr<NodeConst_>& node);

        // scenario related
        void operator()(std::unique_ptr<NodeSpot_>& node);
    };
} // namespace Dal::Script
