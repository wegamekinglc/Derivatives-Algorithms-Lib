//
// Created by wegam on 2021/1/15.
//

#pragma once

#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/math/aad/tape.hpp>
#include <dal/math/specialfunctions.hpp>

namespace Dal::AAD {
    template <class E_> struct Expression_ {
        [[nodiscard]] double value() const { return static_cast<const E_*>(this)->value(); }
        explicit operator double() const { return value(); }
    };

    template <class LHS_, class RHS_, class OP_>
    class BinaryExpression_ : public Expression_<BinaryExpression_<LHS_, RHS_, OP_>> {
        const double value_;
        const LHS_ lhs_;
        const RHS_ rhs_;

    public:
        explicit BinaryExpression_(const Expression_<LHS_>& l, const Expression_<RHS_>& r)
            : value_(OP_::Eval(l.value(), r.value())), lhs_(static_cast<const LHS_&>(l)),
              rhs_(static_cast<const RHS_&>(r)) {}

        [[nodiscard]] FORCE_INLINE double value() const { return value_; }

        enum { numNumbers_ = LHS_::numNumbers_ + RHS_::numNumbers_ };

        template <size_t N_, size_t n_> void PushAdjoint(TapNode_& exprNode, double adjoint) const {
            if constexpr (LHS_::numNumbers_ > 0)
                lhs_.template PushAdjoint<N_, n_>(exprNode, adjoint * OP_::LeftDerivative(lhs_.value(), rhs_.value(), value()));

            if constexpr (RHS_::numNumbers_ > 0)
                rhs_.template PushAdjoint<N_, n_ + LHS_::numNumbers_>(
                    exprNode, adjoint * OP_::RightDerivative(lhs_.value(), rhs_.value(), value()));
        }
    };

    struct OPMult_ {
        FORCE_INLINE static double Eval(double l, double r) { return l * r; }

        FORCE_INLINE static double LeftDerivative(double l, double r, double v) { return r; }

        FORCE_INLINE static double RightDerivative(double l, double r, double v) { return l; }
    };

    struct OPAdd_ {
        FORCE_INLINE static double Eval(double l, double r) { return l + r; }

        FORCE_INLINE static double LeftDerivative(double l, double r, double v) { return 1.; }

        FORCE_INLINE static double RightDerivative(double l, double r, double v) { return 1.; }
    };

    struct OPSub_ {
        FORCE_INLINE static double Eval(double l, double r) { return l - r; }

        FORCE_INLINE static double LeftDerivative(double l, double r, double v) { return 1.; }

        FORCE_INLINE static double RightDerivative(double l, double r, double v) { return -1.; }
    };

    struct OPDiv_ {
        FORCE_INLINE static double Eval(double l, double r) { return l / r; }

        FORCE_INLINE static double LeftDerivative(double l, double r, double v) { return 1. / r; }

        FORCE_INLINE static double RightDerivative(double l, double r, double v) { return -l / r / r; }
    };

    struct OPPow_ {
        FORCE_INLINE static double Eval(double l, double r) { return std::pow(l, r); }

        FORCE_INLINE static double LeftDerivative(double l, double r, double v) { return r * v / l; }

        FORCE_INLINE static double RightDerivative(double l, double r, double v) { return std::log(l) * v; }
    };

    struct OPMax_ {
        FORCE_INLINE static double Eval(double l, double r) { return std::max(l, r); }

        FORCE_INLINE static double LeftDerivative(double l, double r, double v) { return l > r ? 1.0 : 0.0; }

        FORCE_INLINE static double RightDerivative(double l, double r, double v) { return r > l ? 1.0 : 0.0; }
    };

    struct OPMin_ {
        FORCE_INLINE static double Eval(double l, double r) { return std::min(l, r); }

        FORCE_INLINE static double LeftDerivative(double l, double r, double v) { return l < r ? 1.0 : 0.0; }

        FORCE_INLINE static double RightDerivative(double l, double r, double v) { return r < l ? 1.0 : 0.0; }
    };

    // operator overloading for binary expression
    // build the corresponding expressions

    template <class LHS_, class RHS_>
    FORCE_INLINE BinaryExpression_<LHS_, RHS_, OPMult_> operator*(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
        return BinaryExpression_<LHS_, RHS_, OPMult_>(lhs, rhs);
    }

    template <class LHS_, class RHS_>
    FORCE_INLINE BinaryExpression_<LHS_, RHS_, OPAdd_> operator+(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
        return BinaryExpression_<LHS_, RHS_, OPAdd_>(lhs, rhs);
    }

    template <class LHS_, class RHS_>
    FORCE_INLINE BinaryExpression_<LHS_, RHS_, OPSub_> operator-(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
        return BinaryExpression_<LHS_, RHS_, OPSub_>(lhs, rhs);
    }

    template <class LHS_, class RHS_>
    FORCE_INLINE BinaryExpression_<LHS_, RHS_, OPDiv_> operator/(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
        return BinaryExpression_<LHS_, RHS_, OPDiv_>(lhs, rhs);
    }

    template <class LHS_, class RHS_>
    FORCE_INLINE BinaryExpression_<LHS_, RHS_, OPPow_> pow(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
        return BinaryExpression_<LHS_, RHS_, OPPow_>(lhs, rhs);
    }

    template <class LHS_, class RHS_>
    FORCE_INLINE BinaryExpression_<LHS_, RHS_, OPMax_> max(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
        return BinaryExpression_<LHS_, RHS_, OPMax_>(lhs, rhs);
    }

    template <class LHS_, class RHS_>
    FORCE_INLINE BinaryExpression_<LHS_, RHS_, OPMin_> min(const Expression_<LHS_>& lhs, const Expression_<RHS_>& rhs) {
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
            : value_(OP_::Eval(a.value(), 0.0)), arg_(static_cast<const ARG_&>(a)) {}

        explicit UnaryExpression_(const Expression_<ARG_>& a, double b)
            : value_(OP_::Eval(a.value(), b)), arg_(static_cast<const ARG_&>(a)), d_arg_(b) {}

        [[nodiscard]] FORCE_INLINE double value() const { return value_; }

        enum { numNumbers_ = ARG_::numNumbers_ };

        template <size_t N_, size_t n_>
        FORCE_INLINE void PushAdjoint(TapNode_& exprNode, double adjoint) const {
            if constexpr (ARG_::numNumbers_ > 0)
                arg_.template PushAdjoint<N_, n_>(exprNode, adjoint * OP_::Derivative(arg_.value(), value(), d_arg_));
        }
    };

    struct OPExp_ {
        FORCE_INLINE static double Eval(double r, double d) { return std::exp(r); }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return v; }
    };

    struct OPLog_ {
        FORCE_INLINE static double Eval(double r, double d) { return std::log(r); }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return 1.0 / r; }
    };

    struct OPSqrt_ {
        FORCE_INLINE static double Eval(double r, double d) { return std::sqrt(r); }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return 0.5 / v; }
    };

    struct OPFabs_ {
        FORCE_INLINE static double Eval(double r, double d) { return std::fabs(r); }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return r > 0.0 ? 1.0 : -1.0; }
    };

    struct OPNormalDens_ {
        FORCE_INLINE static double Eval(double r, double d) { return Dal::NPDF(r); }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return -r * v; }
    };

    struct OPNormalCdf_ {
        FORCE_INLINE static double Eval(double r, double d) { return Dal::NCDF(r); }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return Dal::NPDF(r); }
    };

    struct OPErfc_ {
        FORCE_INLINE static double Eval(double r, double d) { return std::erfc(r); }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return -1.12837916709551 * std::exp(-r*r); }
    };

    // binary operators with a double on one side

    struct OPMultD_ {
        FORCE_INLINE static double Eval(double r, double d) { return r * d; }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return d; }
    };

    struct OPAddD_ {
        FORCE_INLINE static double Eval(double r, double d) { return r + d; }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return 1.0; }
    };

    struct OPSubDL_ {
        FORCE_INLINE static double Eval(double r, double d) { return d - r; }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return -1.0; }
    };

    struct OPSubDR_ {
        FORCE_INLINE static double Eval(double r, double d) { return r - d; }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return 1.0; }
    };

    struct OPDivDL_ {
        FORCE_INLINE static double Eval(double r, double d) { return d / r; }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return -d / r / r; }
    };

    struct OPDivDR_ {
        FORCE_INLINE static double Eval(double r, double d) { return r / d; }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return 1.0 / d; }
    };

    struct OPPowDL_ {
        FORCE_INLINE static double Eval(double r, double d) { return std::pow(d, r); }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return std::log(d) * v; }
    };

    struct OPPowDR_ {
        FORCE_INLINE static double Eval(double r, double d) { return std::pow(r, d); }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return d * v / r; }
    };

    struct OPMaxD_ {
        FORCE_INLINE static double Eval(double r, double d) { return std::max(r, d); }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return r > d ? 1.0 : 0.0; }
    };

    struct OPMinD_ {
        FORCE_INLINE static double Eval(double r, double d) { return std::min(r, d); }

        FORCE_INLINE static double Derivative(double r, double v, double d) { return r < d ? 1.0 : 0.0; }
    };

    // overloading

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPExp_> exp(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPExp_>(arg);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPLog_> log(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPLog_>(arg);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPSqrt_> sqrt(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPSqrt_>(arg);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPFabs_> fabs(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPFabs_>(arg);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPNormalDens_> NPDF(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPNormalDens_>(arg);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPNormalCdf_> NCDF(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPNormalCdf_>(arg);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPErfc_> erfc(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPErfc_>(arg);
    }

    // binary operators with a double on one side

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPMultD_> operator*(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPMultD_>(rhs, d);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPMultD_> operator*(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPMultD_>(lhs, d);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPAddD_> operator+(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPAddD_>(rhs, d);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPAddD_> operator+(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPAddD_>(lhs, d);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPSubDL_> operator-(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPSubDL_>(rhs, d);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPSubDR_> operator-(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPSubDR_>(lhs, d);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPDivDL_> operator/(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPDivDL_>(rhs, d);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPDivDR_> operator/(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPDivDR_>(lhs, d);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPPowDL_> pow(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPPowDL_>(rhs, d);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPPowDR_> pow(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPPowDR_>(lhs, d);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPMaxD_> max(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPMaxD_>(rhs, d);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPMaxD_> max(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPMaxD_>(lhs, d);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPMinD_> min(double d, const Expression_<ARG_>& rhs) {
        return UnaryExpression_<ARG_, OPMinD_>(rhs, d);
    }

    template <class ARG_>
    FORCE_INLINE UnaryExpression_<ARG_, OPMinD_> min(const Expression_<ARG_>& lhs, double d) {
        return UnaryExpression_<ARG_, OPMinD_>(lhs, d);
    }

    // comparison same as traditional

    template <class E_, class F_>
    FORCE_INLINE bool operator==(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return lhs.value() == rhs.value();
    }

    template <class E_>
    FORCE_INLINE bool operator==(const Expression_<E_>& lhs, double rhs) { return lhs.value() == rhs; }

    template <class E_>
    FORCE_INLINE bool operator==(double lhs, const Expression_<E_>& rhs) { return lhs == rhs.value(); }

    template <class E_, class F_>
    FORCE_INLINE bool operator!=(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return lhs.value() != rhs.value();
    }

    template <class E_>
    FORCE_INLINE bool operator!=(const Expression_<E_>& lhs, double rhs) { return lhs.value() != rhs; }

    template <class E_>
    FORCE_INLINE bool operator!=(double lhs, const Expression_<E_>& rhs) { return lhs != rhs.value(); }

    template <class E_, class F_>
    FORCE_INLINE bool operator<(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return lhs.value() < rhs.value();
    }

    template <class E_>
    FORCE_INLINE bool operator<(const Expression_<E_>& lhs, double rhs) { return lhs.value() < rhs; }

    template <class E_>
    FORCE_INLINE bool operator<(double lhs, const Expression_<E_>& rhs) { return lhs < rhs.value(); }

    template <class E_, class F_>
    FORCE_INLINE bool operator>(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return lhs.value() > rhs.value();
    }

    template <class E_>
    FORCE_INLINE bool operator>(const Expression_<E_>& lhs, double rhs) { return lhs.value() > rhs; }

    template <class E_>
    FORCE_INLINE bool operator>(double lhs, const Expression_<E_>& rhs) { return lhs > rhs.value(); }

    template <class E_, class F_>
    FORCE_INLINE bool operator<=(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return lhs.value() <= rhs.value();
    }

    template <class E_>
    FORCE_INLINE bool operator<=(const Expression_<E_>& lhs, double rhs) { return lhs.value() <= rhs; }

    template <class E_>
    FORCE_INLINE bool operator<=(double lhs, const Expression_<E_>& rhs) { return lhs <= rhs.value(); }

    template <class E_, class F_>
    FORCE_INLINE bool operator>=(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return lhs.value() >= rhs.value();
    }

    template <class E_>
    FORCE_INLINE bool operator>=(const Expression_<E_>& lhs, double rhs) { return lhs.value() >= rhs; }

    template <class E_>
    FORCE_INLINE bool operator>=(double lhs, const Expression_<E_>& rhs) { return lhs >= rhs.value(); }

    // unary operators +/-

    template <class RHS_>
    FORCE_INLINE UnaryExpression_<RHS_, OPSubDL_> operator-(const Expression_<RHS_>& rhs) {
        return 0.0 - rhs;
    }

    template <class RHS_>
    FORCE_INLINE UnaryExpression_<RHS_, OPAddD_> operator+(const Expression_<RHS_>& rhs) {
        return rhs + 0.0;
    }

    // the Number type, also an expression

    class Number_ : public Expression_<Number_> {
        double value_;
        TapNode_* node_;

        template <size_t N_>
        FORCE_INLINE TapNode_* CreateMultiNode() { return tape_.RecordNode<N_>(); }

        template <class E_> void FromExpr(const Expression_<E_>& e) {
            auto* node = this->CreateMultiNode<E_::numNumbers_>();
            static_cast<const E_&>(e).template PushAdjoint<E_::numNumbers_, 0>(*node, 1.0);
            node_ = node;
        }

    public:
        static thread_local Tape_ tape_;

        FORCE_INLINE static Tape_& getTape() { return tape_;}

        enum { numNumbers_ = 1 };

        template <size_t N_, size_t n_>
        FORCE_INLINE void PushAdjoint(TapNode_& exprNode, double adjoint) const {
            exprNode.p_adj_ptrs_[n_] = Tape_::multi_ ? node_->p_adjoints_ : &node_->adjoint_;
            exprNode.p_derivatives_[n_] = adjoint;
        }

        Number_() = default;

        explicit Number_(double val) : value_(val) { node_ = CreateMultiNode<0>(); }

        FORCE_INLINE Number_& operator=(double val) {
            value_ = val;
            node_ = CreateMultiNode<0>();
            return *this;
        }

        template <class E_>
        FORCE_INLINE Number_(const Expression_<E_>& e) : value_(e.value()) {
            FromExpr<E_>(static_cast<const E_&>(e));
        }

        template <class E_>
        FORCE_INLINE Number_& operator=(const Expression_<E_>& e) {
            value_ = e.value();
            FromExpr<E_>(static_cast<const E_&>(e));
            return *this;
        }

        FORCE_INLINE void PutOnTape() { node_ = CreateMultiNode<0>(); }

        double& value() { return value_; }
        [[nodiscard]] FORCE_INLINE double value() const { return value_; }
        [[nodiscard]] FORCE_INLINE double getGradient() const { return node_->Adjoint(); }

        FORCE_INLINE void setGradient(double adjoint) {
            node_->Adjoint() = adjoint;
        }

        FORCE_INLINE void ResetAdjoints() { tape_.ResetAdjoints(); }

        // propagation

        static void PropagateAdjoints(Tape_::Iterator_ propagateFrom, Tape_::Iterator_ propagateTo) {
            auto it = propagateFrom;
            while (it != propagateTo) {
                it->PropagateOne();
                --it;
            }
            it->PropagateOne();
        }

        static void evaluate(const Tape_::Position_& from, const Tape_::Position_& to) {
            auto it = from.nodes_pos_;
            auto to_it = to.nodes_pos_;
            while (it != to_it) {
                --it;
                it->PropagateOne();
            }
        }

        static void PropagateAdjointsMulti(Tape_::Iterator_ propagate_from, Tape_::Iterator_ propagate_to) {
            auto it = propagate_from;
            while (it != propagate_to) {
                it->PropagateAll();
                --it;
            }
            it->PropagateAll();
        }

        // unary operators

        template <class E_>
        FORCE_INLINE Number_& operator+=(const Expression_<E_>& e) {
            *this = *this + e;
            return *this;
        }

        template <class E_>
        FORCE_INLINE Number_& operator*=(const Expression_<E_>& e) {
            *this = *this * e;
            return *this;
        }

        template <class E_>
        FORCE_INLINE Number_& operator-=(const Expression_<E_>& e) {
            *this = *this - e;
            return *this;
        }

        template <class E_>
        FORCE_INLINE Number_& operator/=(const Expression_<E_>& e) {
            *this = *this / e;
            return *this;
        }

        FORCE_INLINE Number_& operator+=(const double& e) {
            *this = *this + e;
            return *this;
        }

        FORCE_INLINE Number_& operator*=(const double& e) {
            *this = *this * e;
            return *this;
        }

        FORCE_INLINE Number_& operator-=(const double& e) {
            *this = *this - e;
            return *this;
        }

        FORCE_INLINE Number_& operator/=(const double& e) {
            *this = *this / e;
            return *this;
        }
    };
} // namespace Dal
