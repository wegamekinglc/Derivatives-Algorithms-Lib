//
// Created by wegam on 2022/7/31.
//

#include <dal/script/visitor/domainproc.hpp>

namespace Dal::Script {
#include <dal/auto/MG_DomainCondProp_enum.inc>
    // visit
    // expressions
    // binaries
    void DomainProcessor_::Visit(NodePlus_* node) {
        VisitArguments(node);
        Domain_ res = domainStack_[1] + domainStack_[0];
        domainStack_.Pop(2);
        domainStack_.Push(std::move(res));
    }

    void DomainProcessor_::Visit(NodeMinus_* node) {
        VisitArguments(node);
        Domain_ res = domainStack_[1] - domainStack_[0];
        domainStack_.Pop(2);
        domainStack_.Push(std::move(res));
    }

    void DomainProcessor_::Visit(NodeMultiply_* node) {
        VisitArguments(node);
        Domain_ res = domainStack_[1] * domainStack_[0];
        domainStack_.Pop(2);
        domainStack_.Push(std::move(res));
    }

    void DomainProcessor_::Visit(NodeDivide_* node) {
        VisitArguments(node);
        Domain_ res = domainStack_[1] / domainStack_[0];
        domainStack_.Pop(2);
        domainStack_.Push(std::move(res));
    }

    void DomainProcessor_::Visit(NodePower_* node) {
        VisitArguments(node);
        Domain_ res = domainStack_[1].ApplyFunc2<double (*)(double, double)>(
            std::pow, domainStack_[0], Interval_(Bound_::minusInfinity_, Bound_::plusInfinity_));
        domainStack_.Pop(2);
        domainStack_.Push(std::move(res));
    }

    // unaries
    void DomainProcessor_::Visit(NodeUPlus_* node) { VisitArguments(node); }

    void DomainProcessor_::Visit(NodeUMinus_* node) {
        VisitArguments(node);
        domainStack_.Top() = -domainStack_.Top();
    }

    // functions
    void DomainProcessor_::Visit(NodeLog_* node) {
        VisitArguments(node);
        Domain_ res = domainStack_.Top().ApplyFunc<double (*)(double)>(
            std::log, Interval_(Bound_::minusInfinity_, Bound_::plusInfinity_));
        domainStack_.Pop();
        domainStack_.Push(std::move(res));
    }

    void DomainProcessor_::Visit(NodeSqrt_* node) {
        VisitArguments(node);
        Domain_ res =
            domainStack_.Top().ApplyFunc<double (*)(double)>(std::sqrt, Interval_(0.0, Bound_::plusInfinity_));
        domainStack_.Pop();
        domainStack_.Push(std::move(res));
    }

    void DomainProcessor_::Visit(NodeMax_* node) {
        VisitArguments(node);
        Domain_ res = domainStack_.Top();
        domainStack_.Pop();
        for (size_t i = 1; i < node->arguments_.size(); ++i) {
            res = res.DMax(domainStack_.Top());
            domainStack_.Pop();
        }
        domainStack_.Push(std::move(res));
    }

    void DomainProcessor_::Visit(NodeMin_* node) {
        VisitArguments(node);
        Domain_ res = domainStack_.Top();
        domainStack_.Pop();
        for (size_t i = 1; i < node->arguments_.size(); ++i) {
            res = res.DMin(domainStack_.Top());
            domainStack_.Pop();
        }
        domainStack_.Push(std::move(res));
    }

    void DomainProcessor_::Visit(NodeSmooth_* node) {
        VisitArguments(node);

        // Pop eps
        domainStack_.Pop();

        // Makes no sense with non-continuous x
        if (domainStack_[2].IsDiscrete())
            THROW("Smooth called with discrete x");

        // Get min and max val if neg and if pos
        Bound_ minIfNeg = domainStack_[0].MinBound();
        Bound_ maxIfNeg = domainStack_[0].MaxBound();
        Bound_ minIfPos = domainStack_[1].MinBound();
        Bound_ maxIfPos = domainStack_[1].MaxBound();
        Bound_ minB = std::min(minIfNeg, minIfPos), maxB = std::max(maxIfNeg, maxIfPos);

        // Pop
        domainStack_.Pop(3);

        // Result
        domainStack_.Push(Interval_(minB, maxB));
    }

    // conditions

    void DomainProcessor_::Visit(NodeEqual_* node) {
        VisitArguments(node);

        Domain_& dom = domainStack_.Top();

        // Always true / false?
        if (!dom.CanBeZero()) {
            node->alwaysTrue_ = false;
            node->alwaysFalse_ = true;

            conditionStack_.Push(DomainCondProp_("AlwaysFalse"));
        } else if (!dom.CanBeNonZero()) {
            node->alwaysTrue_ = true;
            node->alwaysFalse_ = false;
            conditionStack_.Push(DomainCondProp_("AlwaysTrue"));
        } else {
            node->alwaysTrue_ = node->alwaysFalse_ = false;
            conditionStack_.Push(DomainCondProp_("TrueOrFalse"));

            if (fuzzy_) {
                // Continuous or discrete?
                node->discrete_ = dom.ZeroIsDiscrete();

                // Discrete
                if (node->discrete_) {
                    bool subDomRightOfZero = dom.SmallestPosLb(node->ub_, true);
                    if (!subDomRightOfZero)
                        node->ub_ = 0.5;

                    bool subDomLeftOfZero = dom.BiggestNegRb(node->lb_, true);
                    if (!subDomLeftOfZero)
                        node->lb_ = -0.5;
                }
            }
        }
        domainStack_.Pop();
    }

    void DomainProcessor_::Visit(NodeNot_* node) {
        VisitArguments(node);
        DomainCondProp_ cp = conditionStack_.Top();
        conditionStack_.Pop();

        if (cp == DomainCondProp_("AlwaysTrue")) {
            node->alwaysTrue_ = false;
            node->alwaysFalse_ = true;
            conditionStack_.Push(DomainCondProp_("AlwaysFalse"));
        } else if (cp == DomainCondProp_("AlwaysFalse")) {
            node->alwaysTrue_ = true;
            node->alwaysFalse_ = false;
            conditionStack_.Push(DomainCondProp_("AlwaysTrue"));
        } else {
            node->alwaysTrue_ = node->alwaysFalse_ = false;
            conditionStack_.Push(DomainCondProp_("TrueOrFalse"));
        }
    }

    void DomainProcessor_::Visit(NodeSuperior_* node) { DomainProcessor_::Visit<true>(node); }

    void DomainProcessor_::Visit(NodeSupEqual_* node) { DomainProcessor_::Visit<false>(node); }

    void DomainProcessor_::Visit(NodeAnd_* node) {
        VisitArguments(node);
        DomainCondProp_ cp1 = conditionStack_.Top();
        conditionStack_.Pop();
        DomainCondProp_ cp2 = conditionStack_.Top();
        conditionStack_.Pop();

        if (cp1 == DomainCondProp_("AlwaysTrue") && cp2 == DomainCondProp_("AlwaysTrue")) {
            node->alwaysTrue_ = true;
            node->alwaysFalse_ = false;
            conditionStack_.Push(DomainCondProp_("AlwaysTrue"));
        } else if (cp1 == DomainCondProp_("AlwaysFalse") || cp2 == DomainCondProp_("AlwaysFalse")) {
            node->alwaysTrue_ = false;
            node->alwaysFalse_ = true;
            conditionStack_.Push(DomainCondProp_("AlwaysFalse"));
        } else {
            node->alwaysTrue_ = node->alwaysFalse_ = false;
            conditionStack_.Push(DomainCondProp_("TrueOrFalse"));
        }
    }

    void DomainProcessor_::Visit(NodeOr_* node) {
        VisitArguments(node);
        DomainCondProp_ cp1 = conditionStack_.Top();
        conditionStack_.Pop();
        DomainCondProp_ cp2 = conditionStack_.Top();
        conditionStack_.Pop();

        if (cp1 == DomainCondProp_("AlwaysTrue") || cp2 == DomainCondProp_("AlwaysTrue")) {
            node->alwaysTrue_ = true;
            node->alwaysFalse_ = false;
            conditionStack_.Push(DomainCondProp_("AlwaysTrue"));
        } else if (cp1 == DomainCondProp_("AlwaysFalse") && cp2 == DomainCondProp_("AlwaysFalse")) {
            node->alwaysTrue_ = false;
            node->alwaysFalse_ = true;
            conditionStack_.Push(DomainCondProp_("AlwaysFalse"));
        } else {
            node->alwaysTrue_ = node->alwaysFalse_ = false;
            conditionStack_.Push(DomainCondProp_("TrueOrFalse"));
        }
    }

    // instructions
    void DomainProcessor_::Visit(NodeIf_* node) {
        // Last "if true" statement index
        size_t lastTrueStat = node->firstElse_ == -1 ? node->arguments_.size() - 1 : node->firstElse_ - 1;

        // DomainProcessor_::Visit condition
        node->arguments_[0]->AcceptVisitor(this);

        // Always true/false?
        DomainCondProp_ cp = conditionStack_.Top();
        conditionStack_.Pop();

        if (cp == DomainCondProp_("AlwaysTrue")) {
            node->alwaysTrue_ = true;
            node->alwaysFalse_ = false;
            // DomainProcessor_::Visit "if true" statements
            for (size_t i = 1; i <= lastTrueStat; ++i)
                node->arguments_[i]->AcceptVisitor(this);
        } else if (cp == DomainCondProp_("AlwaysFalse")) {
            node->alwaysTrue_ = false;
            node->alwaysFalse_ = true;
            // DomainProcessor_::Visit "if false" statements, if any
            if (node->firstElse_ != -1)
                for (size_t i = node->firstElse_; i < node->arguments_.size(); ++i)
                    node->arguments_[i]->AcceptVisitor(this);
        } else {
            node->alwaysTrue_ = node->alwaysFalse_ = false;

            // Record variable domain before if statements are executed
            Vector_<Domain_> domStore0(node->affectedVars_.size());
            for (size_t i = 0; i < node->affectedVars_.size(); ++i)
                domStore0[i] = domains_[node->affectedVars_[i]];

            // Execute if statements
            for (size_t i = 1; i <= lastTrueStat; ++i)
                node->arguments_[i]->AcceptVisitor(this);

            // Record variable domain after if statements are executed
            Vector_<Domain_> domStore1(node->affectedVars_.size());
            for (size_t i = 0; i < node->affectedVars_.size(); ++i)
                domStore1[i] = std::move(domains_[node->affectedVars_[i]]);

            // Reset variable domains
            for (size_t i = 0; i < node->affectedVars_.size(); ++i)
                domains_[node->affectedVars_[i]] = std::move(domStore0[i]);

            // Execute else statements if any
            if (node->firstElse_ != -1)
                for (size_t i = node->firstElse_; i < node->arguments_.size(); ++i)
                    node->arguments_[i]->AcceptVisitor(this);

            // Merge domains
            for (size_t i = 0; i < node->affectedVars_.size(); ++i)
                domains_[node->affectedVars_[i]].AddDomain(domStore1[i]);
        }
    }

    void DomainProcessor_::Visit(NodeAssign_* node) {
        // DomainProcessor_::Visit the LHS variable
        lhsVar_ = true;
        node->arguments_[0]->AcceptVisitor(this);
        lhsVar_ = false;

        // DomainProcessor_::Visit the RHS expression
        node->arguments_[1]->AcceptVisitor(this);

        // Write RHS domain into variable
        domains_[lhsVarIdx_] = domainStack_.Top();

        // Pop
        domainStack_.Pop();
    }

    void DomainProcessor_::Visit(NodePays_* node) {
        // DomainProcessor_::Visit the LHS variable
        lhsVar_ = true;
        node->arguments_[0]->AcceptVisitor(this);
        lhsVar_ = false;

        // DomainProcessor_::Visit the RHS expression
        node->arguments_[1]->AcceptVisitor(this);

        // Write RHS domain into variable

        // Numeraire domain = (0,+inf)
        static const Domain_ numDomain(Interval_(0.0, Bound_::plusInfinity_));

        // Payment domain
        Domain_ payDomain = domainStack_.Top() / numDomain;

        // Write
        domains_[lhsVarIdx_] = domains_[lhsVarIdx_] + payDomain;

        // Pop
        domainStack_.Pop();
    }

    // Variables and constants
    void DomainProcessor_::Visit(NodeVar_* node) {
        // LHS?
        if (lhsVar_) // Write
        {
            // Record address in myLhsVarAdr
            lhsVarIdx_ = node->index_;
        } else // Read
        {
            // Push domain onto the stack
            domainStack_.Push(domains_[node->index_]);
        }
    }

    void DomainProcessor_::Visit(NodeConst_* node) { domainStack_.Push(node->val_); }

    // Scenario related
    void DomainProcessor_::Visit(NodeSpot_* node) {
        static const Domain_ realDom(Interval_(Bound_::minusInfinity_, Bound_::plusInfinity_));
        domainStack_.Push(realDom);
    }
} // namespace Dal::Script
