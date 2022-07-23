//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <dal/math/operators.hpp>
#include <dal/string//strings.hpp>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace Dal::Script {

    class Bound_ {
        bool plusInf_;
        bool minusInf_;
        double real_;

        static constexpr double BIG = 1.0e+12;
        static constexpr double EPS = 1.0e-12;

    public:
        struct PlusInfinity_ {};
        static constexpr PlusInfinity_ plusInfinity_;
        struct MinusInfinity_ {};
        static constexpr MinusInfinity_ minusInfinity_;

        Bound_(double Value = 0.0) : plusInf_(false), minusInf_(false), real_(Value) {}
        Bound_(const PlusInfinity_&) : plusInf_(true), minusInf_(false), real_(BIG) {}
        Bound_(const MinusInfinity_&) : plusInf_(false), minusInf_(true), real_(-BIG) {}
        Bound_(const Bound_& rhs) = default;

        Bound_& operator=(double Value) {
            plusInf_ = minusInf_ = false;
            real_ = Value;
            return *this;
        }

        Bound_& operator=(const PlusInfinity_&) {
            plusInf_ = true;
            minusInf_ = false;
            real_ = BIG;
            return *this;
        }

        Bound_& operator=(const MinusInfinity_&) {
            plusInf_ = false;
            minusInf_ = true;
            real_ = -BIG;
            return *this;
        }

        Bound_& operator=(const Bound_& rhs) {
            if (this == &rhs)
                return *this;

            plusInf_ = rhs.plusInf_;
            minusInf_ = rhs.minusInf_;
            real_ = rhs.real_;
            return *this;
        }

        //	Accessors
        [[nodiscard]] bool IsInf() const { return plusInf_ || minusInf_; }

        [[nodiscard]] bool IsPositive(bool strict = false) const { return plusInf_ || real_ > (strict ? EPS : -EPS); }

        [[nodiscard]] bool IsNegative(bool strict = false) const { return minusInf_ || real_ < (strict ? -EPS : EPS); }

        [[nodiscard]] bool IsZero() const { return !IsInf() && Fabs(real_) < EPS; }

        [[nodiscard]] bool IsPlusInf() const { return plusInf_; }

        [[nodiscard]] bool IsMinusInf() const { return minusInf_; }

        [[nodiscard]] double Value() const { return real_; }

        //	Comparison
        bool operator==(const Bound_& rhs) const {
            return plusInf_ && rhs.plusInf_ || minusInf_ && rhs.minusInf_ || Fabs(real_ - rhs.real_) < EPS;
        }

        bool operator!=(const Bound_& rhs) const { return !operator==(rhs); }

        bool operator<(const Bound_& rhs) const {
            return minusInf_ && !rhs.minusInf_ || !plusInf_ && rhs.plusInf_ || real_ < rhs.real_ - EPS;
        }

        bool operator>(const Bound_& rhs) const {
            return !minusInf_ && rhs.minusInf_ || plusInf_ && !rhs.plusInf_ || real_ > rhs.real_ + EPS;
        }

        bool operator<=(const Bound_& rhs) const { return !operator>(rhs); }

        bool operator>=(const Bound_& rhs) const { return !operator<(rhs); }

        //	Writers
        friend std::ostream& operator<<(std::ostream& ost, const Bound_& bnd) {
            if (bnd.plusInf_)
                ost << "+INF";
            else if (bnd.minusInf_)
                ost << "-INF";
            else
                ost << bnd.real_;
            return ost;
        }

        [[nodiscard]] String_ Write() const {
            std::ostringstream ost;
            ost << *this;
            return String_(ost.str());
        }

        // Multiplication
        Bound_ operator*(const Bound_& rhs) const {
            if (IsInf() || rhs.IsInf()) {
                if (IsPositive(true) && rhs.IsPositive(true) || IsNegative(true) && rhs.IsNegative(true))
                    return plusInfinity_;
                else if (IsZero())
                    return rhs;
                else if (rhs.IsZero())
                    return *this;
                else
                    return minusInfinity_;
            } else
                return real_ * rhs.real_;
        }

        // Negation
        Bound_ operator-() const {
            if (minusInf_)
                return plusInfinity_;
            else if (plusInf_)
                return minusInfinity_;
            else
                return -real_;
        }
    };

    class Interval_ {
        Bound_ left_;
        Bound_ right_;

    public:
        // Singleton
        explicit Interval_(double Value = 0.0) : left_(Value), right_(Value) {}

        // Interval_
        Interval_(const Bound_& left, const Bound_& right) : left_(left), right_(right) {
            if (left == Bound_(Bound_::plusInfinity_) || right == Bound_(Bound_::minusInfinity_) || left > right)
                THROW("inconsistent bounds");
        }

        // Accessors
        [[nodiscard]] Bound_ Left() const { return left_; }

        [[nodiscard]] Bound_ right() const { return right_; }

        [[nodiscard]] bool IsPositive(bool strict = false) const { return left_.IsPositive(strict); }

        [[nodiscard]] bool IsNegative(bool strict = false) const { return right_.IsNegative(strict); }

        [[nodiscard]] bool IsPosOrNeg(bool strict = false) const { return IsPositive(strict) || IsNegative(strict); }

        [[nodiscard]] bool IsInf() const { return left_.IsInf() || right_.IsInf(); }

        [[nodiscard]] bool IsSingleton(double* val = nullptr) const {
            if (!IsInf() && left_ == right_) {
                if (val)
                    *val = left_.Value();
                return true;
            }
            return false;
        }

        [[nodiscard]] bool IsZero() const { return IsSingleton() && left_.IsZero(); }

        [[nodiscard]] bool IsContinuous() const { return !IsSingleton(); }

        // Writers
        friend std::ostream& operator<<(std::ostream& ost, const Interval_ i) {
            double s;
            if (i.IsSingleton(&s)) {
                ost << "{" << s << "}";
            } else {
                ost << "(" << i.left_ << "," << i.right_ << ")";
            }
            return ost;
        }

        [[nodiscard]] String_ Write() const {
            std::ostringstream ost;
            ost << *this;
            return String_(ost.str());
        }

        // Sorting
        bool operator==(const Interval_& rhs) const { return left_ == rhs.left_ && right_ == rhs.right_; }

        bool operator<(const Interval_& rhs) const {
            return left_ < rhs.left_ || left_ == rhs.left_ && right_ < rhs.right_;
        }

        bool operator>(const Interval_& rhs) const {
            return left_ > rhs.left_ || left_ == rhs.left_ && right_ > rhs.right_;
        }

        bool operator<=(const Interval_& rhs) const { return !operator>(rhs); }

        bool operator>=(const Interval_& rhs) const { return !operator<(rhs); }

        // Arithmetics
        // Addition
        Interval_ operator+(const Interval_& rhs) const {
            Bound_ lb, rb;

            if (left_.IsMinusInf() || rhs.left_.IsMinusInf())
                lb = Bound_(Bound_::minusInfinity_);
            else
                lb = left_.Value() + rhs.left_.Value();

            if (right_.IsPlusInf() || rhs.right_.IsPlusInf())
                rb = Bound_(Bound_::plusInfinity_);
            else
                rb = right_.Value() + rhs.right_.Value();

            return {lb, rb};
        }

        Interval_& operator+=(const Interval_& rhs) {
            *this = *this + rhs;
            return *this;
        }

        // Unary minus
        Interval_ operator-() const { return {-right_, -left_}; }

        // Subtraction
        Interval_ operator-(const Interval_& rhs) const { return *this + -rhs; }

        Interval_& operator-=(const Interval_& rhs) {
            *this = *this - rhs;
            return *this;
        }

        //	Multiplication
        Interval_ operator*(const Interval_& rhs) const {
            //	If we have a IsZero IsSingleton, the result is a IsZero IsSingleton
            if (IsZero() || rhs.IsZero())
                return Interval_(0.0);

            //	Otherwise we multiply the bounds and go from smallest to largest
            Vector_<Bound_> candidates;
            candidates.push_back(right_ * rhs.right_);
            candidates.push_back(right_ * rhs.left_);
            candidates.push_back(left_ * rhs.right_);
            candidates.push_back(left_ * rhs.left_);

            return Interval_(*std::min_element(candidates.begin(), candidates.end()),
                             *std::max_element(candidates.begin(), candidates.end()));
        }

        //	Inverse (1/x)
        [[nodiscard]] Interval_ Inverse() const {
            double v;

            //	Cannot Inverse a IsZero IsSingleton
            if (IsZero())
                THROW("division by {0}");

            //	Singleton
            else if (IsSingleton(&v))
                return Interval_(1.0 / v);

            //	Continuous
            else if (IsPosOrNeg(true)) //	Strict, no 0
            {
                if (IsInf()) {
                    if (IsPositive())
                        return Interval_(0.0, 1.0 / left_.Value());
                    else
                        return Interval_(1.0 / right_.Value(), 0.0);
                }
                return Interval_(1.0 / right_.Value(), 1.0 / left_.Value());
            } else if (left_.IsZero() || right_.IsZero()) //	One of the bounds is 0
            {
                if (IsInf()) {
                    if (IsPositive())
                        return Interval_(0.0, Bound_::plusInfinity_);
                    else
                        return Interval_(Bound_::minusInfinity_, 0.0);
                } else {
                    if (IsPositive())
                        return Interval_(1.0 / right_.Value(), Bound_::plusInfinity_);
                    else
                        return Interval_(Bound_::minusInfinity_, 1.0 / left_.Value());
                }
            }
            //	Interval_ contains 0 and 0 is not a bound: Inverse spans real space
            else
                return Interval_(Bound_::minusInfinity_, Bound_::plusInfinity_);
        }

        //	Division
        Interval_ operator/(const Interval_& rhs) const { return *this * rhs.Inverse(); }

        //	Min/Max
        [[nodiscard]] Interval_ IMin(const Interval_& rhs) const {
            Bound_ lb = left_;
            if (rhs.left_ < lb)
                lb = rhs.left_;

            Bound_ rb = right_;
            if (rhs.right_ < rb)
                rb = rhs.right_;

            return Interval_(lb, rb);
        }
        [[nodiscard]] Interval_ IMax(const Interval_& rhs) const {
            Bound_ lb = left_;
            if (rhs.left_ > lb)
                lb = rhs.left_;

            Bound_ rb = right_;
            if (rhs.right_ > rb)
                rb = rhs.right_;

            return Interval_(lb, rb);
        }

        // Apply function
        template <class Func>
        [[nodiscard]] Interval_ ApplyFunc(Func func, const Interval_& funcDomain) {
            double value;

            //	Continuous interval, we know nothing of the function, so we just apply the function domain
            if (!IsSingleton(&value))
                return funcDomain;
            //	Singleton, we apply the function to find the target IsSingleton
            else {
                try {
                    value = func(value);
                } catch (const std::domain_error&) {
                    THROW("domain error on function applied to IsSingleton");
                }
            }
            return Interval_(value);
        }

        // Apply function 2 params
        template <class Func>
        [[nodiscard]] Interval_ ApplyFunc2(Func func, const Interval_& rhs, const Interval_& funcDomain) {
            double value, val2;

            //	Continuous interval, we know nothing of the function, so we just apply the function domain
            if (!IsSingleton(&value) || !rhs.IsSingleton(&val2))
                return funcDomain;

            //	Singleton, we apply the function to find the target IsSingleton
            else {
                try {
                    value = func(value, val2);
                } catch (const std::domain_error& dErr) {
                    THROW("domain error on function applied to IsSingleton");
                }
            }
            return Interval_(value);
        }

        //	Inclusion
        [[nodiscard]] bool IsInclude(const double x) const { return left_ <= x && right_ >= x; }

        [[nodiscard]] bool IsInclude(const Interval_& rhs) const { return left_ <= rhs.left_ && right_ >= rhs.right_; }

        [[nodiscard]] bool IsIncluded(const Interval_& rhs) const { return left_ >= rhs.left_ && right_ <= rhs.right_; }

        //	Adjacence
        //	0: is not adjacent
        //	1: *this is adjacent to rhs on the left of rhs
        //	2: *this is adjacent to rhs on the right of rhs
        [[nodiscard]] unsigned IsAdjacent(const Interval_& rhs) const {
            if (right_ == rhs.left_)
                return 1;
            else if (left_ == rhs.right_)
                return 2;
            else
                return 0;
        }

        // Intersection, returns false if no intersect, true otherwise
        // in which case iSect is set to the intersection unless nullptr
        friend bool Intersect(const Interval_& lhs, const Interval_& rhs, Interval_* iSect = nullptr) {
            Bound_ lb = lhs.left_;
            if (rhs.left_ > lb)
                lb = rhs.left_;

            Bound_ rb = lhs.right_;
            if (rhs.right_ < rb)
                rb = rhs.right_;

            if (rb >= lb) {
                if (iSect) {
                    iSect->left_ = lb;
                    iSect->right_ = rb;
                }
                return true;
            }
            else
                return false;
        }

        // Merge, returns false if no intersect, true otherwise
        // in which case iMerge is set to the merged interval unless nullptr
        friend bool Merge(const Interval_& lhs, const Interval_& rhs, Interval_* iMerge = nullptr) {
            if (!Intersect(lhs, rhs))
                return false;

            if (iMerge) {
                Bound_ lb = lhs.left_;
                if (rhs.left_ < lb)
                    lb = rhs.left_;

                Bound_ rb = lhs.right_;
                if (rhs.right_ > rb)
                    rb = rhs.right_;

                iMerge->left_ = lb;
                iMerge->right_ = rb;
            }
            return true;
        }

        // Another merge function that merges rhs into this, assuming we already know that they intersect
        void Merge(const Interval_& rhs) {
            if (rhs.left_ < left_)
                left_ = rhs.left_;
            if (rhs.right_ > right_)
                right_ = rhs.right_;
        }
    };

    class Domain_ {};
} // namespace Dal::Script