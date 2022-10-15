//
// Created by wegam on 2021/1/15.
//

#pragma once

#include <cmath>
#include <dal/math/aad/tape.hpp>
#include <dal/math/specialfunctions.hpp>

namespace Dal::AAD {
    template <class E_> struct Expression_ {
        [[nodiscard]] virtual double Value() const { return static_cast<const E_*>(this)->Value(); }

        explicit operator double() const { return Value(); }
    };

    template <class LHS_, class RHS_, class OP_>
    class BinaryExpression_ : public Expression_<BinaryExpression_<LHS_, RHS_, OP_>> {
        const double value_;
        const LHS_ lhs_;
        const RHS_ rhs_;

    public:
        explicit BinaryExpression_(const Expression_<LHS_>& l, const Expression_<RHS_>& r)
            : value_(OP_::Eval(l.Value(), r.Value())), lhs_(static_cast<const LHS_&>(l)),
              rhs_(static_cast<const RHS_&>(r)) {}

        [[nodiscard]] double Value() const { return value_; }

        enum { numNumbers_ = LHS_::numNumbers_ + RHS_::numNumbers_ };

        template <size_t N_, size_t n_> void PushAdjoint(TapNode_& exprNode, double adjoint) const {
            if (LHS_::numNumbers_ > 0)
                lhs_.PushAdjoint<N_, n_>(exprNode, adjoint * OP_::LeftDerivative(lhs_.Value(), rhs_.Value(), Value()));

            if (RHS_::numNumbers_ > 0)
                rhs_.PushAdjoint<N_, n_ + LHS_::numNumbers_>(
                    exprNode, adjoint * OP_::RightDerivative(lhs_.Value(), rhs_.Value(), Value()));
        }
    };

    struct OPMult_ {
        static double Eval(double l, double r) { return l * r; }

        static double LeftDerivative(double l, double r, double v) { return r; }

        static double RightDerivative(double l, double r, double v) { return l; }
    };

    struct OPAdd_ {
        static double Eval(double l, double r) { return l + r; }

        static double LeftDerivative(double l, double r, double v) { return 1.; }

        static double RightDerivative(double l, double r, double v) { return 1.; }
    };

    struct OPSub_ {
        static double Eval(double l, double r) { return l - r; }

        static double LeftDerivative(double l, double r, double v) { return 1.; }

        static double RightDerivative(double l, double r, double v) { return -1.; }
    };

    struct OPDiv_ {
        static double Eval(double l, double r) { return l / r; }

        static double LeftDerivative(double l, double r, double v) { return 1. / r; }

        static double RightDerivative(double l, double r, double v) { return -l / r / r; }
    };

    struct OPPow_ {
        static double Eval(double l, double r) { return std::pow(l, r); }

        static double LeftDerivative(double l, double r, double v) { return r * v / l; }

        static double RightDerivative(double l, double r, double v) { return std::log(l) * v; }
    };

    struct OPMax_ {
        static double Eval(double l, double r) { return std::max(l, r); }

        static double LeftDerivative(double l, double r, double v) { return l > r ? 1.0 : 0.0; }

        static double RightDerivative(double l, double r, double v) { return r > l ? 1.0 : 0.0; }
    };

    struct OPMin_ {
        static double Eval(double l, double r) { return std::min(l, r); }

        static double LeftDerivative(double l, double r, double v) { return l < r ? 1.0 : 0.0; }

        static double RightDerivative(double l, double r, double v) { return r < l ? 1.0 : 0.0; }
    };

    // operator overloading for binary expression
    // build the corresponding expressions

    template <class LHS_, class RHS_>
    BinaryExpression_<LHS_, RHS_, OPMult_> operator*(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
        return BinaryExpression_<LHS_, RHS_, OPMult_>(lhs, rhs);
    }

    template <class LHS_, class RHS_>
    BinaryExpression_<LHS_, RHS_, OPAdd_> operator+(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
        return BinaryExpression_<LHS_, RHS_, OPAdd_>(lhs, rhs);
    }

    template <class LHS_, class RHS_>
    BinaryExpression_<LHS_, RHS_, OPSub_> operator-(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
        return BinaryExpression_<LHS_, RHS_, OPSub_>(lhs, rhs);
    }

    template <class LHS_, class RHS_>
    BinaryExpression_<LHS_, RHS_, OPDiv_> operator/(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
        return BinaryExpression_<LHS_, RHS_, OPDiv_>(lhs, rhs);
    }

    template <class LHS_, class RHS_>
    BinaryExpression_<LHS_, RHS_, OPPow_> Pow(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
        return BinaryExpression_<LHS_, RHS_, OPPow_>(lhs, rhs);
    }

    template <class LHS_, class RHS_>
    BinaryExpression_<LHS_, RHS_, OPMax_> Max(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
        return BinaryExpression_<LHS_, RHS_, OPMax_>(lhs, rhs);
    }

    template <class LHS_, class RHS_>
    BinaryExpression_<LHS_, RHS_, OPMin_> Min(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
        return BinaryExpression_<LHS_, RHS_, OPMin_>(lhs, rhs);
    }

    // Unary expression: same logic with one argument

    // the CRTP class

    template <class ARG_, class OP_> class UnaryExpression_ : public Expression_<UnaryExpression_<ARG_, OP_>> {
        const double value_;
        const ARG_ arg_;
        const double d_arg_ = 0.0;

    public:
        explicit UnaryExpression_(const Expression_<ARG_>& a)
            : value_(OP_::Eval(a.Value(), 0.0)), arg_(static_cast<const ARG_&>(a)) {}

        explicit UnaryExpression_(const Expression_<ARG_>& a, double b)
            : value_(OP_::Eval(a.Value(), b)), arg_(static_cast<const ARG_&>(a)), d_arg_(b) {}

        [[nodiscard]] double Value() const override { return value_; }

        enum { numNumbers_ = ARG_::numNumbers_ };

        template <size_t N_, size_t n_> void PushAdjoint(TapNode_& exprNode, double adjoint) const {
            if (ARG_::numNumbers_ > 0)
                arg_.PushAdjoint<N_, n_>(exprNode, adjoint * OP_::Derivative(arg_.Value(), Value(), d_arg_));
        }
    };

    struct OPExp_ {
        static double Eval(double r, double d) { return std::exp(r); }

        static double Derivative(double r, double v, double d) { return v; }
    };

    struct OPLog_ {
        static double Eval(double r, double d) { return std::log(r); }

        static double Derivative(double r, double v, double d) { return 1.0 / r; }
    };

    struct OPSqrt_ {
        static double Eval(double r, double d) { return std::sqrt(r); }

        static double Derivative(double r, double v, double d) { return 0.5 / v; }
    };

    struct OPFabs_ {
        static double Eval(double r, double d) { return std::fabs(r); }

        static double Derivative(double r, double v, double d) { return r > 0.0 ? 1.0 : -1.0; }
    };

    struct OPNormalDens_ {
        static double Eval(double r, double d) { return Dal::NPDF(r); }

        static double Derivative(double r, double v, double d) { return -r * v; }
    };

    struct OPNormalCdf_ {
        static double Eval(double r, double d) { return Dal::NCDF(r); }

        static double Derivative(double r, double v, double d) { return Dal::NPDF(r); }
    };

    // binary operators with a double on one side

    struct OPMultD_ {
        static double Eval(double r, double d) { return r * d; }

        static double Derivative(double r, double v, double d) { return d; }
    };

    struct OPAddD_ {
        static double Eval(double r, double d) { return r + d; }

        static double Derivative(double r, double v, double d) { return 1.0; }
    };

    struct OPSubDL_ {
        static double Eval(double r, double d) { return d - r; }

        static double Derivative(double r, double v, double d) { return -1.0; }
    };

    struct OPSubDR_ {
        static double Eval(double r, double d) { return r - d; }

        static double Derivative(double r, double v, double d) { return 1.0; }
    };

    struct OPDivDL_ {
        static double Eval(double r, double d) { return d / r; }

        static double Derivative(double r, double v, double d) { return -d / r / r; }
    };

    struct OPDivDR_ {
        static double Eval(double r, double d) { return r / d; }

        static double Derivative(double r, double v, double d) { return 1.0 / d; }
    };

    struct OPPowDL_ {
        static double Eval(double r, double d) { return std::pow(d, r); }

        static double Derivative(double r, double v, double d) { return std::log(d) * v; }
    };

    struct OPPowDR_ {
        static double Eval(double r, double d) { return std::pow(r, d); }

        static double Derivative(double r, double v, double d) { return d * v / r; }
    };

    struct OPMaxD_ {
        static double Eval(double r, double d) { return std::max(r, d); }

        static double Derivative(double r, double v, double d) { return r > d ? 1.0 : 0.0; }
    };

    struct OPMinD_ {
        static double Eval(double r, double d) { return std::min(r, d); }

        static double Derivative(double r, double v, double d) { return r < d ? 1.0 : 0.0; }
    };

    // overloading

    template <class ARG_> UnaryExpression_<ARG_, OPExp_> Exp(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPExp_>(arg);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPLog_> Log(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPLog_>(arg);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPSqrt_> Sqrt(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPSqrt_>(arg);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPFabs_> Fabs(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPFabs_>(arg);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPNormalDens_> NPDF(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPNormalDens_>(arg);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPNormalCdf_> NCDF(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPNormalCdf_>(arg);
    }

    // binary operators with a double on one side

    template <class ARG_> UnaryExpression_<ARG_, OPMultD_> operator*(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPMultD_>(rhs, d);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPMultD_> operator*(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPMultD_>(lhs, d);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPAddD_> operator+(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPAddD_>(rhs, d);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPAddD_> operator+(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPAddD_>(lhs, d);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPSubDL_> operator-(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPSubDL_>(rhs, d);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPSubDR_> operator-(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPSubDR_>(lhs, d);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPDivDL_> operator/(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPDivDL_>(rhs, d);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPDivDR_> operator/(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPDivDR_>(lhs, d);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPPowDL_> Pow(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPPowDL_>(rhs, d);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPPowDR_> Pow(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPPowDR_>(lhs, d);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPMaxD_> Max(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPMaxD_>(rhs, d);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPMaxD_> Max(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPMaxD_>(lhs, d);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPMinD_> Min(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPMinD_>(rhs, d);
    }

    template <class ARG_> UnaryExpression_<ARG_, OPMinD_> Min(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPMinD_>(lhs, d);
    }

    // comparison same as traditional

    template <class E_, class F_> bool operator==(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return lhs.Value() == rhs.Value();
    }

    template <class E_> bool operator==(const Expression_<E_>& lhs, double rhs) { return lhs.Value() == rhs; }

    template <class E_> bool operator==(double lhs, const Expression_<E_>& rhs) { return lhs == rhs.Value(); }

    template <class E_, class F_> bool operator!=(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return lhs.Value() != rhs.Value();
    }

    template <class E_> bool operator!=(const Expression_<E_>& lhs, double rhs) { return lhs.Value() != rhs; }

    template <class E_> bool operator!=(double lhs, const Expression_<E_>& rhs) { return lhs != rhs.Value(); }

    template <class E_, class F_> bool operator<(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return lhs.Value() < rhs.Value();
    }

    template <class E_> bool operator<(const Expression_<E_>& lhs, double rhs) { return lhs.Value() < rhs; }

    template <class E_> bool operator<(double lhs, const Expression_<E_>& rhs) { return lhs < rhs.Value(); }

    template <class E_, class F_> bool operator>(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return lhs.Value() > rhs.Value();
    }

    template <class E_> bool operator>(const Expression_<E_>& lhs, double rhs) { return lhs.Value() > rhs; }

    template <class E_> bool operator>(double lhs, const Expression_<E_>& rhs) { return lhs > rhs.Value(); }

    template <class E_, class F_> bool operator<=(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return lhs.Value() <= rhs.Value();
    }

    template <class E_> bool operator<=(const Expression_<E_>& lhs, double rhs) { return lhs.Value() <= rhs; }

    template <class E_> bool operator<=(double lhs, const Expression_<E_>& rhs) { return lhs <= rhs.Value(); }

    template <class E_, class F_> bool operator>=(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return lhs.Value() >= rhs.Value();
    }

    template <class E_> bool operator>=(const Expression_<E_>& lhs, double rhs) { return lhs.Value() >= rhs; }

    template <class E_> bool operator>=(double lhs, const Expression_<E_>& rhs) { return lhs >= rhs.Value(); }

    // unary operators +/-

    template <class RHS_> UnaryExpression_<RHS_, OPSubDL_> operator-(const Expression_<RHS_>& rhs) { return 0.0 - rhs; }

    template <class RHS_> UnaryExpression_<RHS_, OPAddD_> operator+(const Expression_<RHS_>& rhs) { return rhs + 0.0; }

    // the Number type, also an expression

    class Number_ : public Expression_<Number_> {
        double value_;
        TapNode_* node_;

        template <size_t N_> TapNode_* CreateMultiNode() { return tape_->template RecordNode<N_>(); }

        template <class E_> void FromExpr(const Expression_<E_>& e) {
            auto* node = this->CreateMultiNode<E_::numNumbers_>();
            static_cast<const E_&>(e).PushAdjoint<E_::numNumbers_, 0>(*node, 1.0);
            node_ = node;
        }

    public:
        static thread_local Tape_* tape_;

        enum { numNumbers_ = 1 };

        template <size_t N_, size_t n_> void PushAdjoint(TapNode_& exprNode, double adjoint) const {
            exprNode.p_adj_ptrs_[n_] = Tape_::multi_ ? node_->p_adjoints_ : &node_->adjoint_;
            exprNode.p_derivatives_[n_] = adjoint;
        }

        Number_() = default;

        explicit Number_(double val) : value_(val) {}

        Number_& operator=(double val) {
            value_ = val;
            CreateMultiNode<0>();
            return *this;
        }

        template <class E_> Number_(const Expression_<E_>& e) : value_(e.Value()) { FromExpr<E_>(e); }

        template <class E_> Number_& operator=(const Expression_<E_>& e) {
            value_ = e.Value();
            FromExpr<E_>(e);
            return *this;
        }

        explicit operator double() const { return Value(); }

        explicit operator double&() { return Value(); }

        void PutOnTape() { node_ = CreateMultiNode<0>(); }

        double& Value() { return value_; }
        [[nodiscard]] double Value() const override { return value_; }

        double& Adjoint() { return node_->Adjoint(); }

        [[nodiscard]] double Adjoint() const { return node_->Adjoint(); }

        double& Adjoint(size_t n) { return node_->Adjoint(n); }

        double Adjoint(size_t n) const { return node_->Adjoint(n); }

        void ResetAdjoints() { tape_->ResetAdjoints(); }

        // propagation

        static void PropagateAdjoints(Tape_::Iterator_ propagateFrom, Tape_::Iterator_ propagateTo) {
            auto it = propagateFrom;
            while (it != propagateTo) {
                it->PropagateOne();
                --it;
            }
            it->PropagateOne();
        }

        void PropagateAdjoints(Tape_::Iterator_ propagate_to) {
            Adjoint() = 1.0;
            auto propagate_from = tape_->Find(node_);
            PropagateAdjoints(propagate_from, propagate_to);
        }

        void PropagateToStart() { PropagateAdjoints(tape_->Begin()); }

        void PropagateToMark() { PropagateAdjoints(tape_->MarkIt()); }

        static void PropagateMarkToStart() { PropagateAdjoints(std::prev(tape_->MarkIt()), tape_->Begin()); }

        static void PropagateAdjointsMulti(Tape_::Iterator_ propagate_from, Tape_::Iterator_ propagate_to) {
            auto it = propagate_from;
            while (it != propagate_to) {
                it->PropagateAll();
                --it;
            }
            it->PropagateAll();
        }

        // unary operators

        template <class E_> Number_& operator+=(const Expression_<E_>& e) {
            *this = *this + e;
            return *this;
        }

        template <class E_> Number_& operator*=(const Expression_<E_>& e) {
            *this = *this * e;
            return *this;
        }

        template <class E_> Number_& operator-=(const Expression_<E_>& e) {
            *this = *this - e;
            return *this;
        }

        template <class E_> Number_& operator/=(const Expression_<E_>& e) {
            *this = *this / e;
            return *this;
        }

        Number_& operator+=(const double& e) {
            *this = *this + e;
            return *this;
        }

        Number_& operator*=(const double& e) {
            *this = *this * e;
            return *this;
        }

        Number_& operator-=(const double& e) {
            *this = *this - e;
            return *this;
        }

        Number_& operator/=(const double& e) {
            *this = *this / e;
            return *this;
        }
    };
} // namespace Dal
