//
// Created by wegam on 2021/1/15.
//

#pragma once

#include <cmath>
#include <mutex>
#include <type_traits>
#include <dal/platform/host.hpp>
#include <dal/math/specialfunctions.hpp>

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
#include <dal/math/aad/tape.hpp>

namespace Dal::AAD {

    FORCE_INLINE Tape_* Tape() {
        thread_local Tape_ tape;
        return &tape;
    }

    template <class E_> struct Expression_ {
        template <class EE_>
        friend double Value(const Expression_<EE_>&);

        explicit operator double() const { return Value(*this); }
    };

    template <class LHS_, class RHS_, class OP_>
    class BinaryExpression_ : public Expression_<BinaryExpression_<LHS_, RHS_, OP_>> {
        const double value_;
        const LHS_ lhs_;
        const RHS_ rhs_;

    public:
        explicit BinaryExpression_(const Expression_<LHS_>& l, const Expression_<RHS_>& r)
            : value_(OP_::Eval(Value(l), Value(r))), lhs_(static_cast<const LHS_&>(l)),
              rhs_(static_cast<const RHS_&>(r)) {}

        template <class L_, class R_, class O_>
        friend double Value(const BinaryExpression_<L_, R_, O_>&);

        static constexpr int numNumbers_ = static_cast<int>(LHS_::numNumbers_) + static_cast<int>(RHS_::numNumbers_);

        template <size_t N_, size_t n_> void PushAdjoint(TapNode_& exprNode, double adjoint, Tape_* tape) const {
            if constexpr (LHS_::numNumbers_ > 0)
                lhs_.template PushAdjoint<N_, n_>(exprNode, adjoint * OP_::LeftDerivative(Value(lhs_), Value(rhs_), Value(*this)), tape);

            if constexpr (RHS_::numNumbers_ > 0)
                rhs_.template PushAdjoint<N_, n_ + LHS_::numNumbers_>(
                    exprNode, adjoint * OP_::RightDerivative(Value(lhs_), Value(rhs_), Value(*this)), tape);
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

    template <class ARG_, class OP_> class UnaryExpression_ : public Expression_<UnaryExpression_<ARG_, OP_>> {
        const double value_;
        const ARG_ arg_;
        const double d_arg_ = 0.0;

    public:
        explicit UnaryExpression_(const Expression_<ARG_>& a)
            : value_(OP_::Eval(Value(a), 0.0)), arg_(static_cast<const ARG_&>(a)) {}

        explicit UnaryExpression_(const Expression_<ARG_>& a, double b)
            : value_(OP_::Eval(Value(a), b)), arg_(static_cast<const ARG_&>(a)), d_arg_(b) {}

        template <class A_, class O_>
        friend double Value(const UnaryExpression_<A_, O_>&);

        static constexpr int numNumbers_ = ARG_::numNumbers_;

        template <size_t N_, size_t n_>
        FORCE_INLINE void PushAdjoint(TapNode_& exprNode, double adjoint, Tape_* tape) const {
            if constexpr (ARG_::numNumbers_ > 0)
                arg_.template PushAdjoint<N_, n_>(exprNode, adjoint * OP_::Derivative(Value(arg_), value_, d_arg_), tape);
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

    struct OPAbs_ {
        FORCE_INLINE static double Eval(double r, double d) { return std::abs(r); }

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
    FORCE_INLINE UnaryExpression_<ARG_, OPAbs_> abs(const Expression_<ARG_>& arg) {
        return UnaryExpression_<ARG_, OPAbs_>(arg);
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

    template <class E_, class F_>
    FORCE_INLINE bool operator==(const Expression_<E_>& lhs, const Expression_<F_>& rhs) { return Value(lhs) == Value(rhs); }

    template <class E_>
    FORCE_INLINE bool operator==(const Expression_<E_>& lhs, double rhs) { return Value(lhs) == rhs; }

    template <class E_>
    FORCE_INLINE bool operator==(double lhs, const Expression_<E_>& rhs) { return lhs == Value(rhs); }

    template <class E_, class F_>
    FORCE_INLINE bool operator!=(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return Value(lhs) != Value(rhs);
    }

    template <class E_>
    FORCE_INLINE bool operator!=(const Expression_<E_>& lhs, double rhs) { return Value(lhs) != rhs; }

    template <class E_>
    FORCE_INLINE bool operator!=(double lhs, const Expression_<E_>& rhs) { return lhs != Value(rhs); }

    template <class E_, class F_>
    FORCE_INLINE bool operator<(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return Value(lhs) < Value(rhs);
    }

    template <class E_>
    FORCE_INLINE bool operator<(const Expression_<E_>& lhs, double rhs) { return Value(lhs) < rhs; }

    template <class E_>
    FORCE_INLINE bool operator<(double lhs, const Expression_<E_>& rhs) { return lhs < Value(rhs); }

    template <class E_, class F_>
    FORCE_INLINE bool operator>(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return Value(lhs) > Value(rhs);
    }

    template <class E_>
    FORCE_INLINE bool operator>(const Expression_<E_>& lhs, double rhs) { return Value(lhs) > rhs; }

    template <class E_>
    FORCE_INLINE bool operator>(double lhs, const Expression_<E_>& rhs) { return lhs > Value(rhs); }

    template <class E_, class F_>
    FORCE_INLINE bool operator<=(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return Value(lhs) <= Value(rhs);
    }

    template <class E_>
    FORCE_INLINE bool operator<=(const Expression_<E_>& lhs, double rhs) { return Value(lhs) <= rhs; }

    template <class E_>
    FORCE_INLINE bool operator<=(double lhs, const Expression_<E_>& rhs) { return lhs <= Value(rhs); }

    template <class E_, class F_>
    FORCE_INLINE bool operator>=(const Expression_<E_>& lhs, const Expression_<F_>& rhs) {
        return Value(lhs) >= Value(rhs);
    }

    template <class E_>
    FORCE_INLINE bool operator>=(const Expression_<E_>& lhs, double rhs) { return Value(lhs) >= rhs; }

    template <class E_>
    FORCE_INLINE bool operator>=(double lhs, const Expression_<E_>& rhs) { return lhs >= Value(rhs); }

    template <class RHS_>
    FORCE_INLINE UnaryExpression_<RHS_, OPSubDL_> operator-(const Expression_<RHS_>& rhs) {
        return 0.0 - rhs;
    }

    template <class RHS_>
    FORCE_INLINE UnaryExpression_<RHS_, OPAddD_> operator+(const Expression_<RHS_>& rhs) {
        return rhs + 0.0;
    }

    class Number_ : public Expression_<Number_> {
        double value_;
        TapNode_* node_;

        template <size_t N_>
        FORCE_INLINE TapNode_* CreateMultiNode() { return Tape()->RecordNode<N_>(); }

        template <class E_> void FromExpr(const Expression_<E_>& e) {
            Tape_* tape = Tape();
            auto* node = tape->RecordNode<E_::numNumbers_>();
            static_cast<const E_&>(e).template PushAdjoint<E_::numNumbers_, 0>(*node, 1.0, tape);
            node_ = node;
        }

    public:
        static constexpr int numNumbers_ = 1;

        template <size_t N_, size_t n_>
        FORCE_INLINE void PushAdjoint(TapNode_& exprNode, double adjoint, Tape_* tape) const {
            exprNode.pAdjPtrs_[n_] = tape->multi_ ? node_->pAdjoints_ : &node_->adjoint_;
            exprNode.pDerivatives_[n_] = adjoint;
        }

        Number_(): value_(0.0), node_(nullptr) {}

        Number_(double val) : value_(val) { node_ = CreateMultiNode<0>(); }

        FORCE_INLINE Number_& operator=(double val) {
            value_ = val;
            node_ = CreateMultiNode<0>();
            return *this;
        }

        template <class E_>
        FORCE_INLINE Number_(const Expression_<E_>& e) : value_(Value(e)) {
            FromExpr<E_>(static_cast<const E_&>(e));
        }

        template <class E_>
        FORCE_INLINE Number_& operator=(const Expression_<E_>& e) {
            value_ = Value(e);
            FromExpr<E_>(static_cast<const E_&>(e));
            return *this;
        }

        friend void PutOnTape(Number_&);

        friend double Value(const Number_&);
        friend double& Adjoint(const Number_&);

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

    FORCE_INLINE double Value(const double& num) { return num; }

    template<class E_>
    FORCE_INLINE double Value(const Expression_<E_>& e) { return Value(*static_cast<const E_*>(&e)); }

    template <class LHS_, class RHS_, class OP_>
    FORCE_INLINE double Value(const BinaryExpression_<LHS_, RHS_, OP_>& e) { return e.value_; }

    template <class ARG_, class OP_>
    FORCE_INLINE double Value(const UnaryExpression_<ARG_, OP_>& e) { return e.value_; }

    FORCE_INLINE double Value(const Number_& num) { return num.value_; }
    FORCE_INLINE double& Adjoint(const Number_& num) {
        REQUIRE(num.node_ != nullptr, "Adjoint: Number_ has no tape node");
        return num.node_->Adjoint();
    }

    FORCE_INLINE void PutOnTape(Number_& n) { n.node_ = n.CreateMultiNode<0>(); }
} // namespace Dal::AAD
#elif defined(DAL_USE_ADEPT_AAD)
#include <dal/math/aad/tape.hpp>

namespace Dal::AAD {
    using Number_ = adept::adouble;

    FORCE_INLINE Tape_* Tape() {
        thread_local Tape_ tape;
        return &tape;
    }

    using adept::operator*;
    using adept::operator+;
    using adept::operator-;
    using adept::operator/;
    using adept::operator==;
    using adept::operator!=;
    using adept::operator<;
    using adept::operator<=;
    using adept::operator>;
    using adept::operator>=;

    using adept::abs;
    using adept::erfc;
    using adept::exp;
    using adept::log;
    using adept::max;
    using adept::min;
    using adept::pow;
    using adept::sqrt;

    FORCE_INLINE double Value(const Number_& num) {
        return adept::value(num);
    }

    FORCE_INLINE double Value(double num) {
        return num;
    }

    class Adjoint_ {
        Number_& num_;

    public:
        explicit Adjoint_(Number_& num) : num_(num) {}

        FORCE_INLINE Adjoint_& operator=(double adjoint) {
            num_.set_gradient(adjoint);
            return *this;
        }

        FORCE_INLINE operator double() const {
            return num_.get_gradient();
        }
    };

    FORCE_INLINE double Adjoint(const Number_& num) {
        return num.get_gradient();
    }

    FORCE_INLINE Adjoint_ Adjoint(Number_& num) {
        return Adjoint_(num);
    }

    FORCE_INLINE void PutOnTape(Number_&) {}
} // namespace Dal::AAD
#elif defined(DAL_USE_XAD_AAD)
#include <dal/math/aad/tape.hpp>

namespace Dal::AAD {
    using Number_ = xad::adj<double>::active_type;

    FORCE_INLINE Tape_* Tape() {
        thread_local Tape_ tape;
        return &tape;
    }

    using xad::operator*;
    using xad::operator+;
    using xad::operator-;
    using xad::operator/;
    using xad::operator==;
    using xad::operator!=;
    using xad::operator<;
    using xad::operator<=;
    using xad::operator>;
    using xad::operator>=;

    using xad::abs;
    using xad::erfc;
    using xad::exp;
    using xad::log;
    using xad::max;
    using xad::min;
    using xad::pow;
    using xad::sqrt;

    FORCE_INLINE double Value(const Number_& num) {
        return xad::value(num);
    }

    FORCE_INLINE double Value(double num) {
        return num;
    }

    FORCE_INLINE double Adjoint(const Number_& num) {
        return xad::derivative(num);
    }

    FORCE_INLINE Number_::derivative_type& Adjoint(Number_& num) {
        return xad::derivative(num);
    }

    FORCE_INLINE void PutOnTape(Number_& n) {
        Tape()->tape_.registerInput(n);
    }
} // namespace Dal::AAD
#elif defined(DAL_USE_CODIPACK_AAD)
#include <dal/math/aad/tape.hpp>

namespace Dal::AAD {
    using Number_ = Tape_::active_type;

    FORCE_INLINE Tape_* Tape() {
        thread_local Tape_ tape;
        return &tape;
    }

    using codi::operator*;
    using codi::operator+;
    using codi::operator-;
    using codi::operator/;
    using codi::operator==;
    using codi::operator!=;
    using codi::operator<;
    using codi::operator<=;
    using codi::operator>;
    using codi::operator>=;

    using codi::abs;
    using codi::erfc;
    using codi::exp;
    using codi::log;
    using codi::max;
    using codi::min;
    using codi::pow;
    using codi::sqrt;

    FORCE_INLINE double Value(const Number_& num) {
        return num.getValue();
    }

    FORCE_INLINE double Value(double num) {
        return num;
    }

    FORCE_INLINE double Adjoint(const Number_& num) {
        return num.getGradient();
    }

    FORCE_INLINE Number_::Gradient& Adjoint(Number_& num) {
        return num.gradient();
    }

    FORCE_INLINE void PutOnTape(Number_& n) {
        Tape()->tape_.registerInput(n);
    }
} // namespace Dal::AAD
#endif

namespace Dal::AAD {
    // Read an adjoint as a passive scalar without selecting a backend's mutable-adjoint
    // proxy overload. In particular, Adept's proxy is assignable and convertible, so
    // passing it through Value(...) would be ambiguous between Number_ and double.
    FORCE_INLINE double AdjointValue(const Number_& num) { return Adjoint(num); }
} // namespace Dal::AAD

#if defined(DAL_USE_ADEPT_AAD) || defined(DAL_USE_XAD_AAD) || defined(DAL_USE_CODIPACK_AAD)
namespace Dal::AAD {
    constexpr double INV_SQRT_2PI = 1.0 / M_SQRT_2_PI;
    constexpr double SQRT_2 = M_SQRT_2;

    FORCE_INLINE Number_ NPDF(const Number_& z) {
        return INV_SQRT_2PI * exp(-0.5 * z * z);
    }

    FORCE_INLINE Number_ NCDF(const Number_& z) {
        return 0.5 * erfc(-z / SQRT_2);
    }
} // namespace Dal::AAD
#endif
