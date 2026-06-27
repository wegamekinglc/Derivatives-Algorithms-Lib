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
    // Determines domains of all variables and expressions. Sets alwaysTrue_/alwaysFalse_ flags
    // on condition and if nodes. When fuzzy is enabled, also sets isDiscrete_ and interpolation bounds.

    class DomainProcessor_ : public Visitor_<DomainProcessor_> {
        const bool fuzzy_;
        Vector_<Domain_> varDomains_;
        StaticStack_<Domain_> domStack_;

        using Value_ = typename DomainCondProp_::Value_;
        StaticStack_<Value_> condStack_;

        bool isLhsVar_;
        size_t lhsVarIdx_;

    public:
        using Visitor_<DomainProcessor_>::Visit;

        // All variable domains start as the singleton {0}
        DomainProcessor_(const size_t nVars, bool fuzzy)
            : fuzzy_(fuzzy), varDomains_(nVars, Domain_(0.0)), isLhsVar_(false), lhsVarIdx_(-1), condStack_() {}

        [[nodiscard]] const Vector_<Domain_>& VarDomains() const {
            return varDomains_;
        }

        // Expressions — binaries

        template <class OP_> void VisitBinary(Node_& node, const OP_& op) {
            VisitArguments(node);
            Domain_ res = op(domStack_[1], domStack_[0]);
            domStack_.Pop(2);
            domStack_.Push(std::move(res));
        }

        void Visit(NodeAdd_& node) {
            VisitBinary(node, [](const Domain_& x, const Domain_& y) { return x + y; });
        }
        void Visit(NodeSub_& node) {
            VisitBinary(node, [](const Domain_& x, const Domain_& y) { return x - y; });
        }
        void Visit(NodeMulti_& node) {
            VisitBinary(node, [](const Domain_& x, const Domain_& y) { return x * y; });
        }
        void Visit(NodeDiv_& node) {
            VisitBinary(node, [](const Domain_& x, const Domain_& y) { return x / y; });
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
                    node.isDiscrete_ = dom.ZeroIsDiscrete();

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
        template <bool Strict_, class NSup_> inline void VisitSupT(NSup_& node) {
            VisitArguments(node);

            const Domain_& dom = domStack_.Top();

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
                    node.isDiscrete_ = !dom.CanBeZero() || dom.ZeroIsDiscrete();

                    if (node.isDiscrete_) {
                        // Case 1: expr cannot be zero — subdomains on both sides of 0 exist
                        if (!dom.CanBeZero()) {
                            std::ignore = dom.SmallestPosLb(node.rb_, true);
                            std::ignore = dom.BiggestNegRb(node.lb_, true);
                        }
                        // Case 2: {0} is a singleton
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

            domStack_.Pop();
        }

        void Visit(NodeSup_& node) { VisitSupT<true>(node); }

        void Visit(NodeSupEqual_& node) { VisitSupT<false>(node); }

        void Visit(NodeAnd_& node) {
            VisitArguments(node);
            const DomainCondProp_ cp1 = condStack_.Top();
            condStack_.Pop();
            const DomainCondProp_ cp2 = condStack_.Top();
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
            const DomainCondProp_ cp1 = condStack_.Top();
            condStack_.Pop();
            const DomainCondProp_ cp2 = condStack_.Top();
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
            const size_t lastTrueStat = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;

            node.arguments_[0]->Accept(*this);

            const DomainCondProp_ cp = condStack_.Top();
            condStack_.Pop();

            if (cp == Value_::AlwaysTrue) {
                node.alwaysTrue_ = true;
                node.alwaysFalse_ = false;
                for (size_t i = 1; i <= lastTrueStat; ++i)
                    node.arguments_[i]->Accept(*this);
            } else if (cp == Value_::AlwaysFalse) {
                node.alwaysTrue_ = false;
                node.alwaysFalse_ = true;
                if (node.firstElse_ != -1)
                    for (size_t i = node.firstElse_; i < node.arguments_.size(); ++i)
                        node.arguments_[i]->Accept(*this);
            } else {
                node.alwaysTrue_ = node.alwaysFalse_ = false;

                Vector_<Domain_> domStore0(node.affectedVars_.size());
                for (size_t i = 0; i < node.affectedVars_.size(); ++i)
                    domStore0[i] = varDomains_[node.affectedVars_[i]];

                for (size_t i = 1; i <= lastTrueStat; ++i)
                    node.arguments_[i]->Accept(*this);

                Vector_<Domain_> domStore1(node.affectedVars_.size());
                for (size_t i = 0; i < node.affectedVars_.size(); ++i)
                    domStore1[i] = std::move(varDomains_[node.affectedVars_[i]]);

                for (size_t i = 0; i < node.affectedVars_.size(); ++i)
                    varDomains_[node.affectedVars_[i]] = std::move(domStore0[i]);

                if (node.firstElse_ != -1)
                    for (size_t i = node.firstElse_; i < node.arguments_.size(); ++i)
                        node.arguments_[i]->Accept(*this);

                for (size_t i = 0; i < node.affectedVars_.size(); ++i)
                    varDomains_[node.affectedVars_[i]].AddDomain(domStore1[i]);
            }
        }

        void Visit(NodeAssign_& node) {
            isLhsVar_ = true;
            node.arguments_[0]->Accept(*this);
            isLhsVar_ = false;

            node.arguments_[1]->Accept(*this);

            varDomains_[lhsVarIdx_] = domStack_.Top();
            domStack_.Pop();
        }

        void Visit(NodePays_& node) {
            isLhsVar_ = true;
            node.arguments_[0]->Accept(*this);
            isLhsVar_ = false;

            node.arguments_[1]->Accept(*this);

            // Payment is divided by numeraire: (0,+inf)
            static const Domain_ numDomain(Interval_(Bound_(0.0), Bound_(Bound_::plusInfinity_)));
            Domain_ payDomain = domStack_.Top() / numDomain;

            varDomains_[lhsVarIdx_] = varDomains_[lhsVarIdx_] + payDomain;
            domStack_.Pop();
        }

        // Variables and constants
        void Visit(NodeVar_& node) {
            if (isLhsVar_)
                lhsVarIdx_ = node.index_;
            else
                domStack_.Push(varDomains_[node.index_]);
        }

        void Visit(NodeConst_& node) { domStack_.Push(node.constVal_); }
        void Visit(NodeConstVar_& node) { domStack_.Push(node.constVal_); }

        // Scenario
        void Visit(NodeSpot_&) {
            static auto interval = Interval_(Bound_(Bound_::minusInfinity_), Bound_(Bound_::plusInfinity_));
            static const Domain_ realDom(interval);
            domStack_.Push(realDom);
        }
    };
} // namespace Dal::Script
