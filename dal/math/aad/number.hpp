/*
 * Modified by wegamekinglc on 2020/12/13.
 * Written by Antoine Savine in 2018
 * This code is the strict IP of Antoine Savine
 * License to use and alter this code for personal and commercial applications
 * is freely granted to any person or company who purchased a copy of the book
 * Modern Computational Finance: AAD and Parallel Simulations
 * Antoine Savine
 * Wiley, 2018
 * As long as this comment is preserved at the top of the file
 */

#pragma once

#include <cmath>
#include <dal/math/aad/tape.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::AAD {
    class Number_ {
    private:
        double value_;
        TapNode_* node_;

        template <size_t N_> void CreateNode() { node_ = tape_->RecordNode<N_>(); }

        TapNode_& Node() const {
#ifndef NDEBUG
            auto it = tape_->Find(node_);
            if (it == tape_->End())
                THROW("Put a breakpoint here");
#endif
            return const_cast<TapNode_&>(*node_);
        }

        double& Derivative() { return node_->p_derivatives_[0]; }

        double& LeftDer() { return node_->p_derivatives_[0]; }

        double& RightDer() { return node_->p_derivatives_[1]; }

        double*& AdjPtr() { return node_->p_adj_ptrs_[0]; }

        double*& LeftAdjPtr() { return node_->p_adj_ptrs_[0]; }

        double*& RightAdjPtr() { return node_->p_adj_ptrs_[1]; }

        Number_(TapNode_& arg, double val) : value_(val) {
            CreateNode<1>();
            node_->p_adj_ptrs_[0] = Tape_::multi_ ? arg.p_adjoints_ : &arg.adjoint_;
        }

        Number_(TapNode_& lhs, TapNode_& rhs, double val) : value_(val) {
            CreateNode<2>();
            if (Tape_::multi_) {
                node_->p_adj_ptrs_[0] = lhs.p_adjoints_;
                node_->p_adj_ptrs_[1] = rhs.p_adjoints_;
            } else {
                node_->p_adj_ptrs_[0] = &lhs.adjoint_;
                node_->p_adj_ptrs_[1] = &rhs.adjoint_;
            }
        }

    public:
        static thread_local Tape_* tape_;

        Number_() {}

        explicit Number_(double val) : value_(val) { CreateNode<0>(); }

        Number_& operator=(double val) {
            value_ = val;
            CreateNode<0>();
            return *this;
        }

        void PutOnTape() { CreateNode<0>(); }

        explicit operator double&() { return value_; }

        explicit operator double() { return value_; }

        double& Value() { return value_; }

        double Value() const { return value_; }

        double& Adjoint() { return node_->Adjoint(); }

        void ResetAdjoints() { tape_->ResetAdjoints(); }

        static void PropagateAdjoints(Tape_::Iterator_ propagate_from, Tape_::Iterator_ propagate_to) {
            auto it = propagate_from;
            while (it != propagate_to) {
                it->PropagateOne();
                --it;
            }
            it->PropagateOne(); // INCLUSIVE
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

        inline friend Number_ operator+(const Number_& lhs, const Number_& rhs) {
            const double e = lhs.Value() + rhs.Value();
            Number_ result(lhs.Node(), rhs.Node(), e);
            result.LeftDer() = 1.0;
            result.RightDer() = 1.0;
            return result;
        }

        inline friend Number_ operator+(const Number_& lhs, double rhs) {
            const double e = lhs.Value() + rhs;
            Number_ result(lhs.Node(), e);
            result.Derivative() = 1.0;
            return result;
        }

        inline friend Number_ operator+(double lhs, const Number_& rhs) { return rhs + lhs; }

        inline friend Number_ operator-(const Number_& lhs, const Number_& rhs) {
            const double e = lhs.Value() - rhs.Value();
            Number_ result(lhs.Node(), rhs.Node(), e);
            result.LeftDer() = 1.0;
            result.RightDer() = -1.0;
            return result;
        }

        inline friend Number_ operator-(const Number_& lhs, double rhs) {
            const double e = lhs.Value() - rhs;
            Number_ result(lhs.Node(), e);
            result.Derivative() = 1.0;
            return result;
        }

        inline friend Number_ operator-(double lhs, const Number_& rhs) {
            const double e = lhs - rhs.Value();
            Number_ result(rhs.Node(), e);
            result.Derivative() = -1.0;
            return result;
        }

        inline friend Number_ operator*(const Number_& lhs, const Number_& rhs) {
            const double e = lhs.Value() * rhs.Value();
            Number_ result(lhs.Node(), rhs.Node(), e);
            result.LeftDer() = rhs.Value();
            result.RightDer() = lhs.Value();
            return result;
        }

        inline friend Number_ operator*(const Number_& lhs, double rhs) {
            const double e = lhs.Value() * rhs;
            Number_ result(lhs.Node(), e);
            result.Derivative() = rhs;
            return result;
        }

        inline friend Number_ operator*(double lhs, const Number_& rhs) { return rhs * lhs; }

        inline friend Number_ operator/(const Number_& lhs, const Number_& rhs) {
            const double e = lhs.Value() / rhs.Value();
            Number_ result(lhs.Node(), rhs.Node(), e);
            const double inv_rhs = 1.0 / rhs.Value();
            result.LeftDer() = inv_rhs;
            result.RightDer() = -lhs.Value() * inv_rhs * inv_rhs;
            return result;
        }

        inline friend Number_ operator/(const Number_& lhs, double rhs) {
            const double e = lhs.Value() / rhs;
            Number_ result(lhs.Node(), e);
            result.Derivative() = 1.0 / rhs;
            return result;
        }

        inline friend Number_ operator/(double lhs, const Number_& rhs) {
            const double e = lhs / rhs.Value();
            Number_ result(rhs.Node(), e);
            result.Derivative() = -lhs / rhs.Value() / rhs.Value();
            return result;
        }

        inline friend Number_ Pow(const Number_& lhs, const Number_& rhs) {
            const double e = std::pow(lhs.Value(), rhs.Value());
            Number_ result(lhs.Node(), rhs.Node(), e);
            result.LeftDer() = rhs.Value() * e / lhs.Value();
            result.RightDer() = std::log(lhs.Value()) * e;

            return result;
        }

        inline friend Number_ Pow(const Number_& lhs, double rhs) {
            const double e = std::pow(lhs.Value(), rhs);
            Number_ result(lhs.Node(), e);
            result.Derivative() = rhs * e / lhs.Value();
            return result;
        }

        inline friend Number_ Pow(double lhs, const Number_& rhs) {
            const double e = std::pow(lhs, rhs.Value());
            Number_ result(rhs.Node(), e);
            result.Derivative() = std::log(lhs) * e;
            return result;
        }

        inline friend Number_ Max(const Number_& lhs, const Number_& rhs) {
            const bool l_max = lhs.Value() > rhs.Value();
            Number_ result(lhs.Node(), rhs.Node(), l_max ? lhs.Value() : rhs.Value());
            if (l_max) {
                result.LeftDer() = 1.0;
                result.RightDer() = 0.0;
            } else {
                result.LeftDer() = 0.0;
                result.RightDer() = 1.0;
            }
            return result;
        }

        inline friend Number_ Max(const Number_& lhs, double rhs) {
            const bool l_max = lhs.Value() > rhs;
            Number_ result(lhs.Node(), l_max ? lhs.Value() : rhs);
            result.Derivative() = l_max ? 1.0 : 0.0;
            return result;
        }

        inline friend Number_ Max(double lhs, const Number_& rhs) {
            const bool r_max = rhs.Value() > lhs;
            Number_ result(rhs.Node(), r_max ? rhs.Value() : lhs);
            result.Derivative() = r_max ? 1.0 : 0.0;
            return result;
        }

        inline friend Number_ Min(const Number_& lhs, const Number_& rhs) {
            const bool l_min = lhs.Value() < rhs.Value();
            Number_ result(lhs.Node(), rhs.Node(), l_min ? lhs.Value() : rhs.Value());
            if (l_min) {
                result.LeftDer() = 1.0;
                result.RightDer() = 0.0;
            } else {
                result.LeftDer() = 0.0;
                result.RightDer() = 1.0;
            }
            return result;
        }

        inline friend Number_ Min(const Number_& lhs, double rhs) {
            const bool l_min = lhs.Value() < rhs;
            Number_ result(lhs.Node(), l_min ? lhs.Value() : rhs);
            result.Derivative() = l_min ? 1.0 : 0.0;
            return result;
        }

        inline friend Number_ Min(double lhs, const Number_& rhs) {
            const bool r_min = rhs.Value() < lhs;
            Number_ result(rhs.Node(), r_min ? rhs.Value() : lhs);
            result.Derivative() = r_min ? 1.0 : 0.0;
            return result;
        }

        Number_& operator+=(const Number_& arg) {
            *this = *this + arg;
            return *this;
        }

        Number_& operator+=(double arg) {
            *this = *this + arg;
            return *this;
        }

        Number_& operator-=(const Number_& arg) {
            *this = *this - arg;
            return *this;
        }

        Number_& operator-=(double arg) {
            *this = *this - arg;
            return *this;
        }

        Number_& operator*=(const Number_& arg) {
            *this = *this * arg;
            return *this;
        }

        Number_& operator*=(double arg) {
            *this = *this * arg;
            return *this;
        }

        Number_& operator/=(const Number_& arg) {
            *this = *this / arg;
            return *this;
        }

        Number_& operator/=(double arg) {
            *this = *this / arg;
            return *this;
        }

        Number_ operator-() const { return 0. - *this; }

        Number_ operator+() const { return *this; }

        inline friend Number_ Exp(const Number_& arg) {
            const double e = std::exp(arg.Value());
            Number_ result(arg.Node(), e);
            result.Derivative() = e;
            return result;
        }

        inline friend Number_ Log(const Number_& arg) {
            const double e = std::log(arg.Value());
            Number_ result(arg.Node(), e);
            result.Derivative() = 1.0 / arg.Value();
            return result;
        }

        inline friend Number_ Sqrt(const Number_& arg) {
            const double e = std::sqrt(arg.Value());
            Number_ result(arg.Node(), e);
            result.Derivative() = 0.5 / e;
            return result;
        }

        inline friend Number_ Square(const Number_& arg) {
            const double e = arg.Value() * arg.Value();
            Number_ result(arg.Node(), e);
            result.Derivative() = 2.0 * arg.Value();
            return result;
        }

        inline friend Number_ Fabs(const Number_& arg) {
            const double e = std::fabs(arg.Value());
            Number_ result(arg.Node(), e);
            result.Derivative() = arg.Value() > 0.0 ? 1.0 : -1.0;
            return result;
        }

        inline friend bool operator==(const Number_& lhs, const Number_& rhs) { return lhs.Value() == rhs.Value(); }

        inline friend bool operator==(const Number_& lhs, double rhs) { return lhs.Value() == rhs; }

        inline friend bool operator==(double lhs, const Number_& rhs) { return lhs == rhs.Value(); }

        inline friend bool operator!=(const Number_& lhs, const Number_& rhs) { return lhs.Value() != rhs.Value(); }

        inline friend bool operator!=(const Number_& lhs, double rhs) { return lhs.Value() != rhs; }

        inline friend bool operator!=(double lhs, const Number_& rhs) { return lhs != rhs.Value(); }

        inline friend bool operator<(const Number_& lhs, const Number_& rhs) { return lhs.Value() < rhs.Value(); }

        inline friend bool operator<(const Number_& lhs, double rhs) { return lhs.Value() < rhs; }

        inline friend bool operator<(double lhs, const Number_& rhs) { return lhs < rhs.Value(); }

        inline friend bool operator>(const Number_& lhs, const Number_& rhs) { return lhs.Value() > rhs.Value(); }

        inline friend bool operator>(const Number_& lhs, double rhs) { return lhs.Value() > rhs; }

        inline friend bool operator>(double lhs, const Number_& rhs) { return lhs > rhs.Value(); }

        inline friend bool operator<=(const Number_& lhs, const Number_& rhs) { return lhs.Value() <= rhs.Value(); }

        inline friend bool operator<=(const Number_& lhs, double rhs) { return lhs.Value() <= rhs; }

        inline friend bool operator<=(double lhs, const Number_& rhs) { return lhs <= rhs.Value(); }

        inline friend bool operator>=(const Number_& lhs, const Number_& rhs) { return lhs.Value() >= rhs.Value(); }

        inline friend bool operator>=(const Number_& lhs, double rhs) { return lhs.Value() >= rhs; }

        inline friend bool operator>=(double lhs, const Number_& rhs) { return lhs >= rhs.Value(); }
    };
} // namespace Dal
