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

        // Visit

        // expressions

        // binaries

        void Visit(NodePlus_* node) override {
            VisitArguments(node);
            Domain_ res = domainStack_[1] + domainStack_[0];
            domainStack_.Pop(2);
            domainStack_.Push(std::move(res));
        }
        void Visit(NodeMinus_* node) override {
            VisitArguments(node);
            Domain_ res = domainStack_[1] - domainStack_[0];
            domainStack_.Pop(2);
            domainStack_.Push(std::move(res));
        }
        void Visit(NodeMultiply_* node) override {
            VisitArguments(node);
            Domain_ res = domainStack_[1] * domainStack_[0];
            domainStack_.Pop(2);
            domainStack_.Push(std::move(res));
        }
        void Visit(NodeDivide_* node) override {
            VisitArguments(node);
            Domain_ res = domainStack_[1] / domainStack_[0];
            domainStack_.Pop(2);
            domainStack_.Push(std::move(res));
        }
        void Visit(NodePower_* node) override {
            VisitArguments(node);
            Domain_ res = domainStack_[1].ApplyFunc2<double (*)(double, double)>(
                std::pow, domainStack_[0], Interval_(Bound_::minusInfinity_, Bound_::plusInfinity_));
            domainStack_.Pop(2);
            domainStack_.Push(std::move(res));
        }

        // Unaries
        void Visit(NodeUPlus_* node) override { VisitArguments(node); }
        void Visit(NodeUMinus_* node) override {
            VisitArguments(node);
            domainStack_.Top() = -domainStack_.Top();
        }

        // Functions
        void Visit(NodeLog_* node) override {
            VisitArguments(node);
            Domain_ res = domainStack_.Top().ApplyFunc<double (*)(double)>(
                std::log, Interval_(Bound_::minusInfinity_, Bound_::plusInfinity_));
            domainStack_.Pop();
            domainStack_.Push(std::move(res));
        }
        void Visit(NodeSqrt_* node) override {
            VisitArguments(node);
            Domain_ res =
                domainStack_.Top().ApplyFunc<double (*)(double)>(std::sqrt, Interval_(0.0, Bound_::plusInfinity_));
            domainStack_.Pop();
            domainStack_.Push(std::move(res));
        }
        void Visit(NodeMax_* node) override {
            VisitArguments(node);
            Domain_ res = domainStack_.Top();
            domainStack_.Pop();
            for (size_t i = 1; i < node->arguments_.size(); ++i) {
                res = res.DMax(domainStack_.Top());
                domainStack_.Pop();
            }
            domainStack_.Push(std::move(res));
        }
        void Visit(NodeMin_* node) override {
            VisitArguments(node);
            Domain_ res = domainStack_.Top();
            domainStack_.Pop();
            for (size_t i = 1; i < node->arguments_.size(); ++i) {
                res = res.DMin(domainStack_.Top());
                domainStack_.Pop();
            }
            domainStack_.Push(std::move(res));
        }
        //        void Visit(NodeSmooth_* node) override
        //        {
        //            VisitArguments(node);
        //
        //            // Pop eps
        //            domainStack_.Pop();
        //
        //            // Makes no sense with non-continuous x
        //            if(domainStack_[2].IsDiscrete()) THROW("Smooth called with discrete x");
        //
        //            // Get min and max val if neg and if pos
        //            Bound_ minIfNeg = domainStack_[0].MinBound();
        //            Bound_ maxIfNeg = domainStack_[0].MaxBound();
        //            Bound_ minIfPos = domainStack_[1].MinBound();
        //            Bound_ maxIfPos = domainStack_[1].MaxBound();
        //            Bound_ minB = std::min(minIfNeg, minIfPos), maxB = std::max(maxIfNeg, maxIfPos);
        //
        //            // Pop
        //            domainStack_.Pop(3);
        //
        //            // Result
        //            domainStack_.Push(Interval_(minB, maxB));
        //        }

        // conditions

        void Visit(NodeEqual_* node) override {
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

            // dump domain info to file, comment when not using
#ifdef DUMP
            static int iii = 0;
            ++iii;
            {
                ofstream ofs(string("c:\\temp\\equal") + to_string(iii) + ".txt");
                ofs << "Equality " << iii << endl;
                ofs << "Domain_ = " << dom << endl;
                ofs << "Node discrete = " << node->myDiscrete << endl;
                if (node->myDiscrete)
                    ofs << "Node lB, rB = " << node->lb_ << "," << node->ub_ << endl;
            }
#endif
            // End of dump

            domainStack_.Pop();
        }

        void Visit(NodeNot_* node) override {
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

        // For Visit superior and supEqual
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

            // dump domain info to file, comment when not using
#ifdef DUMP
            static int iii = 0;
            ++iii;
            {
                ofstream ofs(string("c:\\temp\\sup") + (strict ? "" : "equal") + to_string(iii) + ".txt");
                ofs << "Inequality " << iii << endl;
                ofs << "Domain_ = " << dom << endl;
                ofs << "Node discrete = " << node->myDiscrete << endl;
                if (node->myDiscrete)
                    ofs << "Node lB, rB = " << node->lb_ << "," << node->ub_ << endl;
            }
#endif
            // End of dump

            domainStack_.Pop();
        }

        void Visit(NodeSuperior_* node) override { Visit<true>(node); }

        void Visit(NodeSupEqual_* node) override { Visit<false>(node); }

        void Visit(NodeAnd_* node) override {
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
        void Visit(NodeOr_* node) override {
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
        void Visit(NodeIf_* node) override {
            // Last "if true" statement index
            size_t lastTrueStat = node->firstElse_ == -1 ? node->arguments_.size() - 1 : node->firstElse_ - 1;

            // Visit condition
            node->arguments_[0]->AcceptVisitor(this);

            // Always true/false?
            DomainCondProp_ cp = conditionStack_.Top();
            conditionStack_.Pop();

            if (cp == DomainCondProp_("AlwaysTrue")) {
                node->alwaysTrue_ = true;
                node->alwaysFalse_ = false;
                // Visit "if true" statements
                for (size_t i = 1; i <= lastTrueStat; ++i)
                    node->arguments_[i]->AcceptVisitor(this);
            } else if (cp == DomainCondProp_("AlwaysFalse")) {
                node->alwaysTrue_ = false;
                node->alwaysFalse_ = true;
                // Visit "if false" statements, if any
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

        void Visit(NodeAssign_* node) override {
            // Visit the LHS variable
            lhsVar_ = true;
            node->arguments_[0]->AcceptVisitor(this);
            lhsVar_ = false;

            // Visit the RHS expression
            node->arguments_[1]->AcceptVisitor(this);

            // Write RHS domain into variable
            domains_[lhsVarIdx_] = domainStack_.Top();

            // Pop
            domainStack_.Pop();
        }

        void Visit(NodePays_* node) override {
            // Visit the LHS variable
            lhsVar_ = true;
            node->arguments_[0]->AcceptVisitor(this);
            lhsVar_ = false;

            // Visit the RHS expression
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
        void Visit(NodeVar_* node) override {
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

        void Visit(NodeConst_* node) override { domainStack_.Push(node->val_); }

        // Scenario related
        void Visit(NodeSpot_* node) override {
            static const Domain_ realDom(Interval_(Bound_::minusInfinity_, Bound_::plusInfinity_));
            domainStack_.Push(realDom);
        }
    };
} // namespace Dal::Script
