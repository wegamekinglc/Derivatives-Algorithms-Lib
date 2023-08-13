//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/math/stacks.hpp>
#include <dal/math/vectors.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>
#include <dal/script/visitor/domain.hpp>

// #define DUMP

#ifdef DUMP
#include <fstream>
#endif

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
    /*
            Domain_ processor

            Variable indexer and if processor must have been run prior

            Determines the domains of all variables and expressions. Note that the goal is to identify singletons from
       IsContinuous intervals, not necessarily to compute all intervals accurately. For example, is x's domain is {0} and
       y's domain is (-inf, inf) then xy's domain is {0}, but if x and y have domain (-inf, inf), then xy's domain is
       (-inf, inf), even if it turns out that y=x.

            This processor sets the "always true" and "always false" flags
            on the if, equal, superior, not, and and or nodes according to the condition's domain

            In addition, when fuzzy processing is requested, the processor sets the IsContinuous/isDiscrete_ flag on the
       equal and superior nodes and if isDiscrete_, the left and right interpolation bounds

    */

    class DomainProcessor_ : public Visitor_<DomainProcessor_> {
        // Fuzzy?
        const bool fuzzy_;

        // Domains for all variables
        Vector_<Domain_> varDomains_;

        // Stack of domains for expressions
        StaticStack_<Domain_> domStack_;
        using Value_ = typename DomainCondProp_::Value_;
        StaticStack_<Value_> condStack_;

        // LHS variable being visited?
        bool isLhsVar_;
        size_t lhsVarIdx_;

    public:
        using Visitor_<DomainProcessor_>::Visit;

        // Domains start with the IsSingleton 0
        DomainProcessor_(const size_t n_vars, bool fuzzy)
            : fuzzy_(fuzzy), varDomains_(n_vars, Domain_(0.0)), isLhsVar_(false), lhsVarIdx_(-1) {}

        [[nodiscard]] const Vector_<Domain_>& VarDomains() const {
            return varDomains_;
        }

        // Visitors

        // Expressions

        // Binaries

        void Visit(NodeAdd_& node) {
            VisitArguments(node);
            Domain_ res = domStack_[1] + domStack_[0];
            domStack_.Pop(2);
            domStack_.Push(std::move(res));
        }
        void Visit(NodeSub_& node) {
            VisitArguments(node);
            Domain_ res = domStack_[1] - domStack_[0];
            domStack_.Pop(2);
            domStack_.Push(std::move(res));
        }
        void Visit(NodeMulti_& node) {
            VisitArguments(node);
            Domain_ res = domStack_[1] * domStack_[0];
            domStack_.Pop(2);
            domStack_.Push(std::move(res));
        }
        void Visit(NodeDiv_& node) {
            VisitArguments(node);
            Domain_ res = domStack_[1] / domStack_[0];
            domStack_.Pop(2);
            domStack_.Push(std::move(res));
        }
        void Visit(NodePow_& node) {
            VisitArguments(node);
            Domain_ res = domStack_[1].ApplyFunc2<double (*)(double, double)>(
                pow, domStack_[0], Interval_(Bound_(Bound_::minusInfinity_), Bound_(Bound_::plusInfinity_)));
            domStack_.Pop(2);
            domStack_.Push(std::move(res));
        }

        // Unaries
        void Visit(NodeUPlus_& node) { VisitArguments(node); }
        void Visit(NodeUMinus_& node) {
            VisitArguments(node);
            domStack_.Top() = -domStack_.Top();
        }

        // Functions
        void Visit(NodeLog_& node) {
            VisitArguments(node);
            Domain_ res = domStack_.Top().ApplyFunc<double (*)(double)>(
                log, Interval_(Bound_(0.0), Bound_(Bound_::plusInfinity_)));
            domStack_.Pop();
            domStack_.Push(std::move(res));
        }

        void Visit(NodeSqrt_& node) {
            VisitArguments(node);
            Domain_ res = domStack_.Top().ApplyFunc<double (*)(double)>(sqrt, Interval_(Bound_(0.0), Bound_(Bound_::plusInfinity_)));
            domStack_.Pop();
            domStack_.Push(std::move(res));
        }

        void Visit(NodeExp_& node) {
            VisitArguments(node);
            Domain_ res = domStack_.Top().ApplyFunc<double (*)(double)>(
                    exp, Interval_(Bound_(Bound_::minusInfinity_), Bound_(Bound_::plusInfinity_)));
            domStack_.Pop();
            domStack_.Push(std::move(res));
        }

        void Visit(NodeMax_& node) {
            VisitArguments(node);
            Domain_ res = domStack_.Top();
            domStack_.Pop();
            for (size_t i = 1; i < node.arguments_.size(); ++i) {
                res = res.DMax(domStack_.Top());
                domStack_.Pop();
            }
            domStack_.Push(std::move(res));
        }

        void Visit(NodeMin_& node) {
            VisitArguments(node);
            Domain_ res = domStack_.Top();
            domStack_.Pop();
            for (size_t i = 1; i < node.arguments_.size(); ++i) {
                res = res.DMin(domStack_.Top());
                domStack_.Pop();
            }
            domStack_.Push(std::move(res));
        }

        // Conditions

        void Visit(NodeEqual_& node) {
            VisitArguments(node);

            Domain_& dom = domStack_.Top();

            // Always true / false?
            if (!dom.CanBeZero()) {
                node.alwaysTrue_ = false;
                node.alwaysFalse_ = true;

                condStack_.Push(Value_::AlwaysFalse);
            } else if (!dom.CanBeNonZero()) {
                node.alwaysTrue_ = true;
                node.alwaysFalse_ = false;
                condStack_.Push(Value_::AlwaysTrue);
            } else {
                node.alwaysTrue_ = node.alwaysFalse_ = false;
                condStack_.Push(Value_::TrueOrFalse);

                if (fuzzy_) {
                    // Continuous or isDiscrete_?
                    node.isDiscrete_ = dom.ZeroIsDiscrete();

                    // Discrete
                    if (node.isDiscrete_) {
                        bool subDomRightOfZero = dom.SmallestPosLb(node.rb_, true);
                        if (!subDomRightOfZero)
                            node.rb_ = 0.5;

                        bool subDomLeftOfZero = dom.BiggestNegRb(node.lb_, true);
                        if (!subDomLeftOfZero)
                            node.lb_ = -0.5;
                    }
                }
            }

            // Dump domain info to file, comment when not using
#ifdef DUMP
            static int iii = 0;
            ++iii;
            {
                ofstream ofs(string("c:\\temp\\equal") + to_string(iii) + ".txt");
                ofs << "Equality " << iii << endl;
                ofs << "Domain_ = " << dom << endl;
                ofs << "Node isDiscrete_ = " << node.isDiscrete_ << endl;
                if (node.isDiscrete_)
                    ofs << "Node lB, rB = " << node.lb_ << "," << node.rb_ << endl;
            }
#endif
            // End of dump

            domStack_.Pop();
        }

        void Visit(NodeNot_& node) {
            VisitArguments(node);
            DomainCondProp_ cp = condStack_.Top();
            condStack_.Pop();

            if (cp == Value_::AlwaysTrue) {
                node.alwaysTrue_ = false;
                node.alwaysFalse_ = true;
                condStack_.Push(Value_::AlwaysFalse);
            } else if (cp == Value_::AlwaysFalse) {
                node.alwaysTrue_ = true;
                node.alwaysFalse_ = false;
                condStack_.Push(Value_::AlwaysTrue);
            } else {
                node.alwaysTrue_ = node.alwaysFalse_ = false;
                condStack_.Push(Value_::TrueOrFalse);
            }
        }

        // For visiting superior and supEqual
        template <bool Strict_, class NSup_> inline void visitSupT(NSup_& node) {
            VisitArguments(node);

            Domain_& dom = domStack_.Top();

            // Always true / false?
            if (!dom.CanBePositive(Strict_)) {
                node.alwaysTrue_ = false;
                node.alwaysFalse_ = true;
                condStack_.Push(Value_::AlwaysFalse);
            } else if (!dom.CanBeNegative(!Strict_)) {
                node.alwaysTrue_ = true;
                node.alwaysFalse_ = false;
                condStack_.Push(Value_::AlwaysTrue);
            }
            // Can be true or false
            else {
                node.alwaysTrue_ = node.alwaysFalse_ = false;
                condStack_.Push(Value_::TrueOrFalse);

                if (fuzzy_) {
                    // Continuous or isDiscrete_?
                    node.isDiscrete_ = !dom.CanBeZero() || dom.ZeroIsDiscrete();

                    // Fuzzy logic processing
                    if (node.isDiscrete_) {
                        // Case 1: expr cannot be IsZero
                        if (!dom.CanBeZero()) {
                            //  we know we have subdomains on the left and on the right of 0
                            std::ignore = dom.SmallestPosLb(node.rb_, true);
                            std::ignore = dom.BiggestNegRb(node.lb_, true);
                        }
                        // Case 2: {0} is a IsSingleton
                        else {
                            if (Strict_) {
                                node.lb_ = 0.0;
                                std::ignore = dom.SmallestPosLb(node.rb_, true);
                            } else {
                                node.rb_ = 0.0;
                                std::ignore = dom.BiggestNegRb(node.lb_, true);
                            }
                        }
                    }
                }
            }

            // Dump domain info to file, comment when not using
#ifdef DUMP
            static int iii = 0;
            ++iii;
            {
                ofstream ofs(string("c:\\temp\\sup") + (strict ? "" : "equal") + to_string(iii) + ".txt");
                ofs << "Inequality " << iii << endl;
                ofs << "Domain_ = " << dom << endl;
                ofs << "Node isDiscrete_ = " << node.isDiscrete_ << endl;
                if (node.isDiscrete_)
                    ofs << "Node lB, rB = " << node.lb_ << "," << node.rb_ << endl;
            }
#endif
            // End of dump

            domStack_.Pop();
        }

        void Visit(NodeSup_& node) { visitSupT<true>(node); }

        void Visit(NodeSupEqual_& node) { visitSupT<false>(node); }

        void Visit(NodeAnd_& node) {
            VisitArguments(node);
            DomainCondProp_ cp1 = condStack_.Top();
            condStack_.Pop();
            DomainCondProp_ cp2 = condStack_.Top();
            condStack_.Pop();

            if (cp1 == Value_::AlwaysTrue && cp2 == Value_::AlwaysTrue) {
                node.alwaysTrue_ = true;
                node.alwaysFalse_ = false;
                condStack_.Push(Value_::AlwaysTrue);
            } else if (cp1 == Value_::AlwaysFalse || cp2 == Value_::AlwaysFalse) {
                node.alwaysTrue_ = false;
                node.alwaysFalse_ = true;
                condStack_.Push(Value_::AlwaysFalse);
            } else {
                node.alwaysTrue_ = node.alwaysFalse_ = false;
                condStack_.Push(Value_::TrueOrFalse);
            }
        }
        void Visit(NodeOr_& node) {
            VisitArguments(node);
            DomainCondProp_ cp1 = condStack_.Top();
            condStack_.Pop();
            DomainCondProp_ cp2 = condStack_.Top();
            condStack_.Pop();

            if (cp1 == Value_::AlwaysTrue || cp2 == Value_::AlwaysTrue) {
                node.alwaysTrue_ = true;
                node.alwaysFalse_ = false;
                condStack_.Push(Value_::AlwaysTrue);
            } else if (cp1 == Value_::AlwaysFalse && cp2 == Value_::AlwaysFalse) {
                node.alwaysTrue_ = false;
                node.alwaysFalse_ = true;
                condStack_.Push(Value_::AlwaysFalse);
            } else {
                node.alwaysTrue_ = node.alwaysFalse_ = false;
                condStack_.Push(Value_::TrueOrFalse);
            }
        }

        // Instructions
        void Visit(NodeIf_& node) {
            // Last "if true" statement index
            size_t lastTrueStat = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;

            // Visit condition
            node.arguments_[0]->Accept(*this);

            // Always true/false?
            DomainCondProp_ cp = condStack_.Top();
            condStack_.Pop();

            if (cp == Value_::AlwaysTrue) {
                node.alwaysTrue_ = true;
                node.alwaysFalse_ = false;
                // Visit "if true" statements
                for (size_t i = 1; i <= lastTrueStat; ++i)
                    node.arguments_[i]->Accept(*this);
            } else if (cp == Value_::AlwaysFalse) {
                node.alwaysTrue_ = false;
                node.alwaysFalse_ = true;
                // Visit "if false" statements, if any
                if (node.firstElse_ != -1)
                    for (size_t i = node.firstElse_; i < node.arguments_.size(); ++i)
                        node.arguments_[i]->Accept(*this);
            } else {
                node.alwaysTrue_ = node.alwaysFalse_ = false;

                // Record variable domain before if statements are executed
                Vector_<Domain_> domStore0(node.affectedVars_.size());
                for (size_t i = 0; i < node.affectedVars_.size(); ++i)
                    domStore0[i] = varDomains_[node.affectedVars_[i]];

                // Execute if statements
                for (size_t i = 1; i <= lastTrueStat; ++i)
                    node.arguments_[i]->Accept(*this);

                // Record variable domain after if statements are executed
                Vector_<Domain_> domStore1(node.affectedVars_.size());
                for (size_t i = 0; i < node.affectedVars_.size(); ++i)
                    domStore1[i] = std::move(varDomains_[node.affectedVars_[i]]);

                // Reset variable domains
                for (size_t i = 0; i < node.affectedVars_.size(); ++i)
                    varDomains_[node.affectedVars_[i]] = std::move(domStore0[i]);

                // Execute else statements if any
                if (node.firstElse_ != -1)
                    for (size_t i = node.firstElse_; i < node.arguments_.size(); ++i)
                        node.arguments_[i]->Accept(*this);

                // Merge domains
                for (size_t i = 0; i < node.affectedVars_.size(); ++i)
                    varDomains_[node.affectedVars_[i]].AddDomain(domStore1[i]);
            }
        }

        void Visit(NodeAssign_& node) {
            // Visit the LHS variable
            isLhsVar_ = true;
            node.arguments_[0]->Accept(*this);
            isLhsVar_ = false;

            // Visit the RHS expression
            node.arguments_[1]->Accept(*this);

            // Write RHS domain into variable
            varDomains_[lhsVarIdx_] = domStack_.Top();

            // Pop
            domStack_.Pop();
        }

        void Visit(NodePays_& node) {
            // Visit the LHS variable
            isLhsVar_ = true;
            node.arguments_[0]->Accept(*this);
            isLhsVar_ = false;

            // Visit the RHS expression
            node.arguments_[1]->Accept(*this);

            // Write RHS domain into variable

            // Numeraire domain = (0,+inf)
            static const Domain_ numDomain(Interval_(Bound_(0.0), Bound_(Bound_::plusInfinity_)));

            // Payment domain
            Domain_ payDomain = domStack_.Top() / numDomain;

            // Write
            varDomains_[lhsVarIdx_] = varDomains_[lhsVarIdx_] + payDomain;

            // Pop
            domStack_.Pop();
        }

        // Variables and constants
        void Visit(NodeVar_& node) {
            // LHS?
            if (isLhsVar_) // Write
            {
                // Record address in myLhsVarAdr
                lhsVarIdx_ = node.index_;
            } else // Read
            {
                // Push domain onto the stack
                domStack_.Push(varDomains_[node.index_]);
            }
        }

        void Visit(NodeConst_& node) { domStack_.Push(node.constVal_); }
        void Visit(NodeConstVar_& node) { domStack_.Push(node.constVal_); }

        // Scenario related
        void Visit(NodeSpot_&) {
            static auto interval = Interval_(Bound_(Bound_::minusInfinity_), Bound_(Bound_::plusInfinity_));
            static const Domain_ realDom(interval);
            domStack_.Push(realDom);
        }
    };
} // namespace Dal::Script
