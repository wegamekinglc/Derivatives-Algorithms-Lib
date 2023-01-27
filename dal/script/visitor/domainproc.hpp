//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <cmath>
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
    /*
            Domain processor

            Variable indexer and if processor must have been run prior

            Determines the domains of all variables and expressions. Note that the goal is to identify singletons from
       continuous intervals, not necessarily to compute all intervals accurately. For example, is x's domain is {0} and
       y's domain is (-inf, inf) then xy's domain is {0}, but if x and y have domain (-inf, inf), then xy's domain is
       (-inf, inf), even if it turns out that y=x.

            This processor sets the "always true" and "always false" flags
            on the if, equal, superior, not, and and or nodes according to the condition's domain

            In addition, when fuzzy processing is requested, the processor sets the continuous/isDiscrete_ flag on the
       equal and superior nodes and if isDiscrete_, the left and right interpolation bounds

    */

    class DomainProcessor_ : public Visitor_<DomainProcessor_> {
        //	Fuzzy?
        const bool fuzzy_;

        //	Domains for all variables
        Vector_<Domain> varDomains_;

        //	Stack of domains for expressions
        StaticStack_<Domain> domStack_;

        //	Stack of always true/false properties for conditions
        enum CondProp { alwaysTrue_, alwaysFalse_, trueOrFalse };
        StaticStack_<CondProp> condStack_;

        //	LHS variable being visited?
        bool isLhsVar_;
        size_t lhsVarIdx_;

    public:
        using Visitor_<DomainProcessor_>::Visit;

        //	Domains start with the singleton 0
        DomainProcessor_(const size_t nVar, const bool fuzzy)
            : fuzzy_(fuzzy), varDomains_(nVar, 0.0), isLhsVar_(false) {}

        //	Visitors

        //	Expressions

        //	Binaries

        void Visit(NodeAdd& node) {
            VisitArguments(node);
            Domain res = domStack_[1] + domStack_[0];
            domStack_.pop(2);
            domStack_.push(std::move(res));
        }
        void Visit(NodeSub& node) {
            VisitArguments(node);
            Domain res = domStack_[1] - domStack_[0];
            domStack_.pop(2);
            domStack_.push(std::move(res));
        }
        void Visit(NodeMult& node) {
            VisitArguments(node);
            Domain res = domStack_[1] * domStack_[0];
            domStack_.pop(2);
            domStack_.push(std::move(res));
        }
        void Visit(NodeDiv& node) {
            VisitArguments(node);
            Domain res = domStack_[1] / domStack_[0];
            domStack_.pop(2);
            domStack_.push(std::move(res));
        }
        void Visit(NodePow& node) {
            VisitArguments(node);
            Domain res = domStack_[1].applyFunc2<double (*)(const double, const double)>(
                pow, domStack_[0], Interval(Bound::minusInfinity_, Bound::plusInfinity_));
            domStack_.pop(2);
            domStack_.push(std::move(res));
        }

        //	Unaries
        void Visit(NodeUplus& node) { VisitArguments(node); }
        void Visit(NodeUminus& node) {
            VisitArguments(node);
            domStack_.top() = -domStack_.top();
        }

        //	Functions
        void Visit(NodeLog& node) {
            VisitArguments(node);
            Domain res = domStack_.top().applyFunc<double (*)(const double)>(
                log, Interval(Bound::minusInfinity_, Bound::plusInfinity_));
            domStack_.pop();
            domStack_.push(std::move(res));
        }
        void Visit(NodeSqrt& node) {
            VisitArguments(node);
            Domain res = domStack_.top().applyFunc<double (*)(const double)>(sqrt, Interval(0.0, Bound::plusInfinity_));
            domStack_.pop();
            domStack_.push(std::move(res));
        }
        void Visit(NodeMax& node) {
            VisitArguments(node);
            Domain res = domStack_.top();
            domStack_.pop();
            for (size_t i = 1; i < node.arguments_.size(); ++i) {
                res = res.dmax(domStack_.top());
                domStack_.pop();
            }
            domStack_.push(std::move(res));
        }
        void Visit(NodeMin& node) {
            VisitArguments(node);
            Domain res = domStack_.top();
            domStack_.pop();
            for (size_t i = 1; i < node.arguments_.size(); ++i) {
                res = res.dmin(domStack_.top());
                domStack_.pop();
            }
            domStack_.push(std::move(res));
        }
        void Visit(NodeSmooth& node) {
            VisitArguments(node);

            //	Pop eps_
            domStack_.pop();

            //	Makes no sense with non-continuous x
            if (domStack_[2].IsDiscrete()) {
                throw std::runtime_error("Smooth called with isDiscrete_ x");
            }

            //	Get min and max val if neg and if pos
            Bound minIfNeg = domStack_[0].minBound();
            Bound maxIfNeg = domStack_[0].maxBound();
            Bound minIfPos = domStack_[1].minBound();
            Bound maxIfPos = domStack_[1].maxBound();
            Bound minB = min(minIfNeg, minIfPos), maxB = max(maxIfNeg, maxIfPos);

            //	Pop
            domStack_.pop(3);

            //	Result
            domStack_.push(Interval(minB, maxB));
        }

        //	Conditions

        void Visit(NodeEqual& node) {
            VisitArguments(node);

            Domain& dom = domStack_.top();

            //	Always true / false?
            if (!dom.canBeZero()) {
                node.alwaysTrue_ = false;
                node.alwaysFalse_ = true;

                condStack_.push(alwaysFalse_);
            } else if (!dom.canBeNonZero()) {
                node.alwaysTrue_ = true;
                node.alwaysFalse_ = false;
                condStack_.push(alwaysTrue_);
            } else {
                node.alwaysTrue_ = node.alwaysFalse_ = false;
                condStack_.push(trueOrFalse);

                if (fuzzy_) {
                    //	Continuous or isDiscrete_?
                    node.isDiscrete_ = dom.zeroIsDiscrete();

                    //	Discrete
                    if (node.isDiscrete_) {
                        bool subDomRightOfZero = dom.smallestPosLb(node.rb_, true);
                        if (!subDomRightOfZero)
                            node.rb_ = 0.5;

                        bool subDomLeftOfZero = dom.biggestNegRb(node.lb_, true);
                        if (!subDomLeftOfZero)
                            node.lb_ = -0.5;
                    }
                }
            }

            //	Dump domain info to file, comment when not using
#ifdef DUMP
            static int iii = 0;
            ++iii;
            {
                ofstream ofs(string("c:\\temp\\equal") + to_string(iii) + ".txt");
                ofs << "Equality " << iii << endl;
                ofs << "Domain = " << dom << endl;
                ofs << "Node isDiscrete_ = " << node.isDiscrete_ << endl;
                if (node.isDiscrete_)
                    ofs << "Node lB, rB = " << node.lb_ << "," << node.rb_ << endl;
            }
#endif
            //	End of dump

            domStack_.pop();
        }

        void Visit(NodeNot& node) {
            VisitArguments(node);
            CondProp cp = condStack_.top();
            condStack_.pop();

            if (cp == alwaysTrue_) {
                node.alwaysTrue_ = false;
                node.alwaysFalse_ = true;
                condStack_.push(alwaysFalse_);
            } else if (cp == alwaysFalse_) {
                node.alwaysTrue_ = true;
                node.alwaysFalse_ = false;
                condStack_.push(alwaysTrue_);
            } else {
                node.alwaysTrue_ = node.alwaysFalse_ = false;
                condStack_.push(trueOrFalse);
            }
        }

        //	For visiting superior and supEqual
        template <bool strict, class NodeSup> inline void visitSupT(NodeSup& node) {
            VisitArguments(node);

            Domain& dom = domStack_.top();

            //	Always true / false?
            if (!dom.canBePositive(strict)) {
                node.alwaysTrue_ = false;
                node.alwaysFalse_ = true;
                condStack_.push(alwaysFalse_);
            } else if (!dom.canBeNegative(!strict)) {
                node.alwaysTrue_ = true;
                node.alwaysFalse_ = false;
                condStack_.push(alwaysTrue_);
            }
            //	Can be true or false
            else {
                node.alwaysTrue_ = node.alwaysFalse_ = false;
                condStack_.push(trueOrFalse);

                if (fuzzy_) {
                    //	Continuous or isDiscrete_?
                    node.isDiscrete_ = !dom.canBeZero() || dom.zeroIsDiscrete();

                    //	Fuzzy logic processing
                    if (node.isDiscrete_) {
                        //	Case 1: expr cannot be zero
                        if (!dom.canBeZero()) {
                            //		we know we have subdomains on the left and on the right of 0
                            dom.smallestPosLb(node.rb_, true);
                            dom.biggestNegRb(node.lb_, true);
                        }
                        //	Case 2: {0} is a singleton
                        else {
                            if (strict) {
                                node.lb_ = 0.0;
                                dom.smallestPosLb(node.rb_, true);
                            } else {
                                node.rb_ = 0.0;
                                dom.biggestNegRb(node.lb_, true);
                            }
                        }
                    }
                }
            }

            //	Dump domain info to file, comment when not using
#ifdef DUMP
            static int iii = 0;
            ++iii;
            {
                ofstream ofs(string("c:\\temp\\sup") + (strict ? "" : "equal") + to_string(iii) + ".txt");
                ofs << "Inequality " << iii << endl;
                ofs << "Domain = " << dom << endl;
                ofs << "Node isDiscrete_ = " << node.isDiscrete_ << endl;
                if (node.isDiscrete_)
                    ofs << "Node lB, rB = " << node.lb_ << "," << node.rb_ << endl;
            }
#endif
            //	End of dump

            domStack_.pop();
        }

        void Visit(NodeSup& node) { visitSupT<true>(node); }

        void Visit(NodeSupEqual& node) { visitSupT<false>(node); }

        void Visit(NodeAnd& node) {
            VisitArguments(node);
            CondProp cp1 = condStack_.top();
            condStack_.pop();
            CondProp cp2 = condStack_.top();
            condStack_.pop();

            if (cp1 == alwaysTrue_ && cp2 == alwaysTrue_) {
                node.alwaysTrue_ = true;
                node.alwaysFalse_ = false;
                condStack_.push(alwaysTrue_);
            } else if (cp1 == alwaysFalse_ || cp2 == alwaysFalse_) {
                node.alwaysTrue_ = false;
                node.alwaysFalse_ = true;
                condStack_.push(alwaysFalse_);
            } else {
                node.alwaysTrue_ = node.alwaysFalse_ = false;
                condStack_.push(trueOrFalse);
            }
        }
        void Visit(NodeOr& node) {
            VisitArguments(node);
            CondProp cp1 = condStack_.top();
            condStack_.pop();
            CondProp cp2 = condStack_.top();
            condStack_.pop();

            if (cp1 == alwaysTrue_ || cp2 == alwaysTrue_) {
                node.alwaysTrue_ = true;
                node.alwaysFalse_ = false;
                condStack_.push(alwaysTrue_);
            } else if (cp1 == alwaysFalse_ && cp2 == alwaysFalse_) {
                node.alwaysTrue_ = false;
                node.alwaysFalse_ = true;
                condStack_.push(alwaysFalse_);
            } else {
                node.alwaysTrue_ = node.alwaysFalse_ = false;
                condStack_.push(trueOrFalse);
            }
        }

        //	Instructions
        void Visit(NodeIf& node) {
            //	Last "if true" statement index
            size_t lastTrueStat = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;

            //	Visit condition
            node.arguments_[0]->Accept(*this);

            //	Always true/false?
            CondProp cp = condStack_.top();
            condStack_.pop();

            if (cp == alwaysTrue_) {
                node.alwaysTrue_ = true;
                node.alwaysFalse_ = false;
                //	Visit "if true" statements
                for (size_t i = 1; i <= lastTrueStat; ++i)
                    node.arguments_[i]->Accept(*this);
            } else if (cp == alwaysFalse_) {
                node.alwaysTrue_ = false;
                node.alwaysFalse_ = true;
                //	Visit "if false" statements, if any
                if (node.firstElse_ != -1)
                    for (size_t i = node.firstElse_; i < node.arguments_.size(); ++i)
                        node.arguments_[i]->Accept(*this);
            } else {
                node.alwaysTrue_ = node.alwaysFalse_ = false;

                //	Record variable domain before if statements are executed
                Vector_<Domain> domStore0(node.affectedVars_.size());
                for (size_t i = 0; i < node.affectedVars_.size(); ++i)
                    domStore0[i] = varDomains_[node.affectedVars_[i]];

                //	Execute if statements
                for (size_t i = 1; i <= lastTrueStat; ++i)
                    node.arguments_[i]->Accept(*this);

                //	Record variable domain after if statements are executed
                Vector_<Domain> domStore1(node.affectedVars_.size());
                for (size_t i = 0; i < node.affectedVars_.size(); ++i)
                    domStore1[i] = std::move(varDomains_[node.affectedVars_[i]]);

                //	Reset variable domains
                for (size_t i = 0; i < node.affectedVars_.size(); ++i)
                    varDomains_[node.affectedVars_[i]] = std::move(domStore0[i]);

                //	Execute else statements if any
                if (node.firstElse_ != -1)
                    for (size_t i = node.firstElse_; i < node.arguments_.size(); ++i)
                        node.arguments_[i]->Accept(*this);

                //	Merge domains
                for (size_t i = 0; i < node.affectedVars_.size(); ++i)
                    varDomains_[node.affectedVars_[i]].addDomain(domStore1[i]);
            }
        }

        void Visit(NodeAssign& node) {
            //	Visit the LHS variable
            isLhsVar_ = true;
            node.arguments_[0]->Accept(*this);
            isLhsVar_ = false;

            //	Visit the RHS expression
            node.arguments_[1]->Accept(*this);

            //	Write RHS domain into variable
            varDomains_[lhsVarIdx_] = domStack_.top();

            //	Pop
            domStack_.pop();
        }

        void Visit(NodePays& node) {
            //	Visit the LHS variable
            isLhsVar_ = true;
            node.arguments_[0]->Accept(*this);
            isLhsVar_ = false;

            //	Visit the RHS expression
            node.arguments_[1]->Accept(*this);

            //	Write RHS domain into variable

            //	Numeraire domain = (0,+inf)
            static const Domain numDomain(Interval(0.0, Bound::plusInfinity_));

            //	Payment domain
            Domain payDomain = domStack_.top() / numDomain;

            //	Write
            varDomains_[lhsVarIdx_] = varDomains_[lhsVarIdx_] + payDomain;

            //	Pop
            domStack_.pop();
        }

        //	Variables and constants
        void Visit(NodeVar& node) {
            //	LHS?
            if (isLhsVar_) //	Write
            {
                //	Record address in myLhsVarAdr
                lhsVarIdx_ = node.index_;
            } else //	Read
            {
                //	Push domain onto the stack
                domStack_.push(varDomains_[node.index_]);
            }
        }

        void Visit(NodeConst& node) { domStack_.push(node.constVal_); }

        //	Scenario related
        void Visit(NodeSpot& node) {
            static const Domain realDom(Interval(Bound::minusInfinity_, Bound::plusInfinity_));
            domStack_.push(realDom);
        }
    };
} // namespace Dal::Script
