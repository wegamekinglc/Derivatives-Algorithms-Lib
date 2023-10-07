//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <exception>
#include <stdexcept>
#include <iostream>
#include <string>
#include <sstream>
#include <functional>
#include <array>
#include <algorithm>
#include <memory>
#include <set>
#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/exceptions.hpp>


namespace Dal::Script {

    class Bound_ {
        bool isPlusInf_;
        bool isMinusInf_;
        double real_;

    public:
        struct PlusInfinity {};
        struct MinusInfinity {};
        static const PlusInfinity plusInfinity_;
        static const MinusInfinity minusInfinity_;

        // Real
        explicit Bound_(double val = 0.0) : isPlusInf_(false), isMinusInf_(false), real_(val) {}

        // Infinite
        explicit Bound_(const PlusInfinity&) : isPlusInf_(true), isMinusInf_(false), real_(Dal::INF) {}
        explicit Bound_(const MinusInfinity&) : isPlusInf_(false), isMinusInf_(true), real_(-Dal::INF) {}

        Bound_(const Bound_& rhs) = default;

        Bound_& operator=(double val) {
            isPlusInf_ = isMinusInf_ = false;
            real_ = val;
            return *this;
        }
        Bound_& operator=(const PlusInfinity&) {
            isPlusInf_ = true;
            isMinusInf_ = false;
            real_ = Dal::INF;
            return *this;
        }
        Bound_& operator=(const MinusInfinity&) {
            isPlusInf_ = false;
            isMinusInf_ = true;
            real_ = -Dal::INF;
            return *this;
        }
        Bound_& operator=(const Bound_& rhs) {
            if (this == &rhs)
                return *this;

            isPlusInf_ = rhs.isPlusInf_;
            isMinusInf_ = rhs.isMinusInf_;
            real_ = rhs.real_;
            return *this;
        }

        // Accessors
        [[nodiscard]] bool IsInfinite() const { return isPlusInf_ || isMinusInf_; }
        [[nodiscard]] bool IsPositive(bool strict = false) const { return isPlusInf_ || real_ > (strict ? Dal::EPSILON : -Dal::EPSILON); }
        [[nodiscard]] bool IsNegative(bool strict = false) const { return isMinusInf_ || real_ < (strict ? -Dal::EPSILON : Dal::EPSILON); }
        [[nodiscard]]  bool IsZero() const { return !IsInfinite() && fabs(real_) < Dal::EPSILON; }
        [[nodiscard]] bool IsPlusInf() const { return isPlusInf_; }
        [[nodiscard]] bool IsMinusInf() const { return isMinusInf_; }
        [[nodiscard]] double Val() const { return real_; }

        // Comparison
        bool operator==(const Bound_& rhs) const {
            return isPlusInf_ && rhs.isPlusInf_ || isMinusInf_ && rhs.isMinusInf_ || fabs(real_ - rhs.real_) < Dal::EPSILON;
        }

        bool operator!=(const Bound_& rhs) const { return !operator==(rhs); }

        bool operator<(const Bound_& rhs) const {
            return isMinusInf_ && !rhs.isMinusInf_ || !isPlusInf_ && rhs.isPlusInf_ || real_ < rhs.real_ - Dal::EPSILON;
        }

        bool operator>(const Bound_& rhs) const {
            return !isMinusInf_ && rhs.isMinusInf_ || isPlusInf_ && !rhs.isPlusInf_ || real_ > rhs.real_ + Dal::EPSILON;
        }

        bool operator<=(const Bound_& rhs) const { return !operator>(rhs); }

        bool operator>=(const Bound_& rhs) const { return !operator<(rhs); }

        // Writers
        friend std::ostream& operator<<(std::ostream& ost, const Bound_& bnd) {
            if (bnd.isPlusInf_)
                ost << "+INF";
            else if (bnd.isMinusInf_)
                ost << "-INF";
            else
                ost << bnd.real_;

            return ost;
        }

        [[nodiscard]] std::string Write() const {
            std::ostringstream ost;
            ost << *this;
            return ost.str();
        }

        // Multiplication
        Bound_ operator*(const Bound_& rhs) const {
            if (IsInfinite() || rhs.IsInfinite()) {
                if (IsPositive(true) && rhs.IsPositive(true) || IsNegative(true) && rhs.IsNegative(true))
                    return Bound_(plusInfinity_);
                else if (IsZero())
                    return rhs; // Here 0 * inf = inf
                else if (rhs.IsZero())
                    return *this; // Same
                else
                    return Bound_(minusInfinity_);
            } else
                return Bound_(real_ * rhs.real_);
        }

        // Negation
        Bound_ operator-() const {
            if (isMinusInf_)
                return Bound_(plusInfinity_);
            else if (isPlusInf_)
                return Bound_(minusInfinity_);
            else
                return Bound_(-real_);
        }
    };

    class Interval_ {
        Bound_ left_;
        Bound_ right_;

    public:
        // Singleton
        explicit Interval_(double val = 0.0) : left_(val), right_(val) {}

        // Interval_
        Interval_(const Bound_& left, const Bound_& right) : left_(left), right_(right) {
            REQUIRE(left != Bound_(Bound_::plusInfinity_) && right != Bound_(Bound_::minusInfinity_) && left <= right, "Inconsistent bounds");
        }

        // Accessors
        [[nodiscard]] Bound_ Left() const { return left_; }

        [[nodiscard]] Bound_ Right() const { return right_; }

        [[nodiscard]] bool IsPositive(bool strict = false) const { return left_.IsPositive(strict); }

        [[nodiscard]] bool IsNegative(bool strict = false) const { return right_.IsNegative(strict); }

        [[nodiscard]] bool IsPosOrNeg(bool strict = false) const { return IsPositive(strict) || IsNegative(strict); }

        [[nodiscard]] bool IsInfinite() const { return left_.IsInfinite() || right_.IsInfinite(); }

        [[nodiscard]] bool IsSingleton(double* val = nullptr) const {
            if (!IsInfinite() && left_ == right_) {
                if (val)
                    *val = left_.Val();
                return true;
            }
            return false;
        }

        [[nodiscard]] bool IsZero() const { return IsSingleton() && left_.IsZero(); }

        [[nodiscard]] bool IsContinuous() const { return !IsSingleton(); }

        // Writers
        friend std::ostream& operator<<(std::ostream& ost, const Interval_ i) {
            double s;
            if (i.IsSingleton(&s))
                ost << "{" << s << "}";
            else
                ost << "(" << i.left_ << "," << i.right_ << ")";
            return ost;
        }

        [[nodiscard]] std::string Write() const {
            std::ostringstream ost;
            ost << *this;
            return ost.str();
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
                lb = Bound_::minusInfinity_;
            else
                lb = left_.Val() + rhs.left_.Val();

            if (right_.IsPlusInf() || rhs.right_.IsPlusInf())
                rb = Bound_::plusInfinity_;
            else
                rb = right_.Val() + rhs.right_.Val();

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

        // Multiplication
        Interval_ operator*(const Interval_& rhs) const {
            // If we have a IsZero singleton, the result is a IsZero IsSingleton
            if (IsZero() || rhs.IsZero())
                return Interval_(0.0);

            // Otherwise we multiply the bounds and go from smallest to largest
            std::array<Bound_, 4> b;
            b[0] = right_ * rhs.right_;
            b[1] = right_ * rhs.left_;
            b[2] = left_ * rhs.right_;
            b[3] = left_ * rhs.left_;

            return {*std::min_element(b.begin(), b.end()), *std::max_element(b.begin(), b.end())};
        }

        // Inverse (1/x)
        [[nodiscard]] Interval_ Inverse() const {
            // Cannot inverse a IsZero IsSingleton
            REQUIRE(!IsZero(), "Division by {0}");

            double v;

            // Singleton
            if (IsSingleton(&v))
                return Interval_(1.0 / v);
            // Continuous
            else if (IsPosOrNeg(true)) // Strict, no 0
            {
                if (IsInfinite()) {
                    if (IsPositive())
                        return {Bound_(0.0), Bound_(1.0 / left_.Val())};
                    else
                        return {Bound_(1.0 / right_.Val()), Bound_(0.0)};
                }
                return {Bound_(1.0 / right_.Val()), Bound_(1.0 / left_.Val())};
            } else if (left_.IsZero() || right_.IsZero()) // One of the bounds is 0
            {
                if (IsInfinite()) {
                    if (IsPositive())
                        return {Bound_(0.0), Bound_(Bound_::plusInfinity_)};
                    else
                        return {Bound_(Bound_::minusInfinity_), Bound_(0.0)};
                } else {
                    if (IsPositive())
                        return {Bound_(1.0 / right_.Val()), Bound_(Bound_::plusInfinity_)};
                    else
                        return {Bound_(Bound_::minusInfinity_), Bound_(1.0 / left_.Val())};
                }
            }
            // Interval_ contains 0 and 0 is not a bound: inverse spans real space
            else
                return {Bound_(Bound_::minusInfinity_), Bound_(Bound_::plusInfinity_)};
        }

        // Division
        Interval_ operator/(const Interval_& rhs) const { return *this * rhs.Inverse(); }

        // Min/Max
        [[nodiscard]] Interval_ IMin(const Interval_& rhs) const {
            Bound_ lb = left_;
            if (rhs.left_ < lb)
                lb = rhs.left_;

            Bound_ rb = right_;
            if (rhs.right_ < rb)
                rb = rhs.right_;

            return {lb, rb};
        }

        [[nodiscard]] Interval_ IMax(const Interval_& rhs) const {
            Bound_ lb = left_;
            if (rhs.left_ > lb)
                lb = rhs.left_;

            Bound_ rb = right_;
            if (rhs.right_ > rb)
                rb = rhs.right_;

            return {lb, rb};
        }

        // Apply function
        template <class Func> Interval_ ApplyFunc(const Func& func, const Interval_& func_domain) {
            double val;

            // Continuous interval, we know nothing of the function, so we just apply the function domain
            if (!IsSingleton(&val))
                return func_domain;

            // Singleton, we apply the function to find the target IsSingleton
            else {
                try {
                    val = func(val);
                } catch (const std::domain_error& dErr) {
                    THROW("Domain_ error on function applied to IsSingleton");
                }
            }

            return Interval_(val);
        }

        // Apply function 2 params
        template <class Func> Interval_ ApplyFunc2(const Func func, const Interval_& rhs, const Interval_& func_domain) {
            double val, val2;

            // Continuous interval, we know nothing of the function, so we just apply the function domain
            if (!IsSingleton(&val) || !rhs.IsSingleton(&val2))
                return func_domain;

            // Singleton, we apply the function to find the target IsSingleton
            else {
                try {
                    val = func(val, val2);
                } catch (const std::domain_error& dErr) {
                    THROW("Domain_ error on function applied to IsSingleton");
                }
            }

            return Interval_(val);
        }

        // Inclusion
        [[nodiscard]] bool Includes(double x) const { return left_ <= Bound_(x) && right_ >= Bound_(x); }

        [[nodiscard]] bool Includes(const Interval_& rhs) const { return left_ <= rhs.left_ && right_ >= rhs.right_; }

        [[nodiscard]] bool IsIncludedIn(const Interval_& rhs) const { return left_ >= rhs.left_ && right_ <= rhs.right_; }

        // Adjacence
        // 0: is not adjacent
        // 1: *this is adjacent to rhs on the left of rhs
        // 2: *this is adjacent to rhs on the right of rhs
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
        friend bool Intersect(const Interval_& lhs, const Interval_& rhs, Interval_* i_sect = nullptr) {
            Bound_ lb = lhs.left_;
            if (rhs.left_ > lb)
                lb = rhs.left_;

            Bound_ rb = lhs.right_;
            if (rhs.right_ < rb)
                rb = rhs.right_;

            if (rb >= lb) {
                if (i_sect) {
                    i_sect->left_ = lb;
                    i_sect->right_ = rb;
                }
                return true;
            }

            else
                return false;
        }

        // Merge, returns false if no intersect, true otherwise
        // in which case iMerge is set to the merged interval unless nullptr
        friend bool Merge(const Interval_& lhs, const Interval_& rhs, Interval_* i_merge = nullptr) {
            if (!Intersect(lhs, rhs))
                return false;

            if (i_merge) {
                Bound_ lb = lhs.left_;
                if (rhs.left_ < lb)
                    lb = rhs.left_;

                Bound_ rb = lhs.right_;
                if (rhs.right_ > rb)
                    rb = rhs.right_;

                i_merge->left_ = lb;
                i_merge->right_ = rb;
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

    class Domain_ {
        std::set<Interval_> intervals_;

    public:
        Domain_() = default;
        Domain_(const Domain_& rhs) = default;
        Domain_(Domain_&& rhs) noexcept : intervals_(std::move(rhs.intervals_)) {}

        Domain_& operator=(const Domain_& rhs) {
            if (this == &rhs)
                return *this;
            intervals_ = rhs.intervals_;
            return *this;
        }

        Domain_& operator=(Domain_&& rhs) noexcept {
            if (this == &rhs)
                return *this;
            intervals_ = std::move(rhs.intervals_);
            return *this;
        }

        explicit Domain_(double val) { AddSingleton(val); }

        explicit Domain_(const Interval_& i) { AddInterval(i); }

        void AddInterval(Interval_ interval) {
            while (true) {
                // Particular case 1: domain is empty, just add the interval
                const auto itb = intervals_.begin(), ite = intervals_.end();
                if (itb == ite) {
                    intervals_.insert(interval);
                    return;
                }

                // Particular case 2: interval spans real space, then domain becomes the real space
                const Bound_& l = interval.Left();
                const Bound_& r = interval.Right();
                if (l.IsMinusInf() && r.IsPlusInf()) {
                    auto a = Bound_(Bound_::minusInfinity_);
                    auto b = Bound_(Bound_::plusInfinity_);
                    static const Interval_ realSpace(a, b);
                    intervals_.clear();
                    intervals_.insert(realSpace);
                    return;
                }

                // General case: we insert the interval in such a way that the resulting set of intervals are all distinct
                // Find an interval in intervals_ that intersects interval, or interval.end() if none
                // STL implementation, nice and elegant, unfortunately poor performance
                // auto it = find_if( intervals_.begin(), intervals_.end(), [&interval] (const Interval_& i) { return intersect( i, interval); });
                // Custom implementation, for performance, much less elegant
                auto it = itb;
                // First interval is on the strict right of interval, there will be no intersection
                if (itb->Left() > r)
                    it = ite;
                else {
                    // Last interval in intervals_, we know there is one
                    const Interval_& last = *intervals_.rbegin();

                    // Last interval is on the strict left of interval, there will be no intersection
                    if (last.Right() < l)
                        it = ite;

                    else {
                        // We may have an intersection, find it
                        it = intervals_.lower_bound(
                            interval); // Smallest myInterval >= interval, means it.Left() >= l
                        if (it == ite || it->Left() > r)
                            --it; // Now it.Left() <= l <= r
                        if (it->Right() < l)
                            it = ite; // it does not intersect
                    }
                }

                // End of find an interval in intervals_ that intersects interval
                // it points to an interval in intervals_ that intersects interval, or ite if none
                // No intersection, just add the interval
                if (it == ite) {
                    intervals_.insert(interval);
                    return;
                }

                // We have an intersection

                // Merge the intersecting interval from intervals_ into interval

                // We don't use the generic merge: too slow
                // merge( interval, *it, &interval);
                // Quick merge
                interval.Merge(*it);

                // Remove the merged interval from set
                intervals_.erase(it);

                // Go again until we find no more intersect
            }
        }

        void AddDomain(const Domain_& rhs) {
            for (auto& interval : rhs.intervals_)
                AddInterval(interval);
        }

        void AddSingleton(double val) { AddInterval(Interval_(val)); }

        // Accessors
        [[nodiscard]] bool IsPositive(bool strict = false) const {
            for (auto& interval : intervals_)
                if (!interval.IsPositive(strict))
                    return false;
            return true;
        }

        [[nodiscard]] bool IsNegative(bool strict = false) const {
            for (auto& interval : intervals_)
                if (!interval.IsNegative(strict))
                    return false;
            return true;
        }

        [[nodiscard]] bool IsPosOrNeg(bool strict = false) const { return IsPositive(strict) || IsNegative(strict); }

        [[nodiscard]] bool IsInfinite() const {
            for (auto& interval : intervals_)
                if (interval.IsInfinite())
                    return true;
            return false;
        }

        [[nodiscard]] bool IsDiscrete() const {
            for (auto& interval : intervals_)
                if (!interval.IsSingleton())
                    return false;
            return true;
        }

        // Discrete only is true: return empty if IsContinuous intervals found, false: return all singletons anyway
        [[nodiscard]] Vector_<double> GetSingletons(bool discrete_only = true) const {
            Vector_<double> res;
            for (auto& interval : intervals_) {
                double val;
                if (!interval.IsSingleton(&val)) {
                    if (discrete_only)
                        return {};
                } else
                    res.push_back(val);
            }
            return res;
        }

        // At least one IsContinuous interval
        [[nodiscard]] bool IsContinuous() const { return !IsDiscrete(); }

        // Shortcut for 2 singletons
        bool IsBoolean(pair<double, double>* values = nullptr) const {
            Vector_<double> s = GetSingletons();
            if (s.size() == 2) {
                if (values) {
                    values->first = s[0];
                    values->second = s[1];
                }
                return true;
            } else
                return false;
        }

        // Shortcut for 1 IsSingleton
        bool IsConstant(double* val = nullptr) const {
            Vector_<double> s = GetSingletons();
            if (s.size() == 1) {
                if (val)
                    *val = s[0];
                return true;
            } else
                return false;
        }

        // Get all IsContinuous intervals, dropping singletons
        [[nodiscard]] Domain_ GetContinuous() const {
            Domain_ res;
            for (auto& interval : intervals_) {
                if (interval.IsContinuous())
                    res.AddInterval(interval);
            }
            return res;
        }

        // Get min and max bounds
        [[nodiscard]] Bound_ MinBound() const {
            if (!IsEmpty())
                return intervals_.begin()->Left();
            else
                return Bound_(Bound_::minusInfinity_);
        }

        [[nodiscard]] Bound_ MaxBound() const {
            if (!IsEmpty())
                return intervals_.rbegin()->Right();
            else
                return Bound_(Bound_::plusInfinity_);
        }

        [[nodiscard]] bool IsEmpty() const { return intervals_.empty(); }

        [[nodiscard]] size_t Size() const { return intervals_.size(); }

        // Writers

        friend std::ostream& operator<<(std::ostream& ost, const Domain_& d) {
            ost << "{";
            auto i = d.intervals_.begin();
            while (i != d.intervals_.end()) {
                ost << *i;
                ++i;
                if (i != d.intervals_.end())
                    ost << ";";
            }
            ost << "}";

            return ost;
        }

        [[nodiscard]] std::string Write() const {
            std::ostringstream ost;
            ost << *this;
            return ost.str();
        }

        // Arithmetics
        Domain_ operator+(const Domain_& rhs) const {
            Domain_ res;

            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.AddInterval(i + j);
                }
            }

            return res;
        }

        Domain_ operator-() const {
            Domain_ res;

            for (auto& i : intervals_) {
                res.AddInterval(-i);
            }

            return res;
        }

        Domain_ operator-(const Domain_& rhs) const {
            Domain_ res;

            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.AddInterval(i - j);
                }
            }

            return res;
        }
        Domain_ operator*(const Domain_& rhs) const {
            Domain_ res;

            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    size_t s = res.Size();
                    res.AddInterval(i * j);
                }
            }

            return res;
        }

        [[nodiscard]] Domain_ Inverse() const {
            Domain_ res;

            for (auto& i : intervals_) {
                res.AddInterval(i.Inverse());
            }

            return res;
        }

        Domain_ operator/(const Domain_& rhs) const {
            Domain_ res;

            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.AddInterval(i / j);
                }
            }

            return res;
        }

        // Shortcuts for shifting all intervals
        Domain_ operator+=(double x) {
            if (fabs(x) < Dal::EPSILON)
                return *this;

            std::set<Interval_> newIntervals;
            for (auto& i : intervals_)
                newIntervals.insert(i + Interval_(x));
            intervals_ = std::move(newIntervals);

            return *this;
        }

        Domain_ operator-=(double x) {
            if (fabs(x) < Dal::EPSILON)
                return *this;

            std::set<Interval_> newIntervals;
            for (auto& i : intervals_)
                newIntervals.insert(i - Interval_(x));
            intervals_ = std::move(newIntervals);

            return *this;
        }

        // Min/Max
        [[nodiscard]] Domain_ DMin(const Domain_& rhs) const {
            Domain_ res;

            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.AddInterval(i.IMin(j));
                }
            }

            return res;
        }

        [[nodiscard]] Domain_ DMax(const Domain_& rhs) const {
            Domain_ res;

            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.AddInterval(i.IMax(j));
                }
            }

            return res;
        }

        // Apply function
        template <class Func_> Domain_ ApplyFunc(const Func_ func, const Interval_& func_domain) {
            Domain_ res;

            auto vec = GetSingletons();

            if (vec.empty())
                return Domain_(func_domain);

            // Singletons, apply func
            for (auto v : vec) {
                try {
                    res.AddSingleton(func(v));
                } catch (const std::domain_error&) {
                    THROW("Domain_ error on function applied to IsSingleton");
                }
            }

            return res;
        }

        // Apply function 2 params
        template <class Func_> Domain_ ApplyFunc2(const Func_& func, const Domain_& rhs, const Interval_& func_domain) {
            Domain_ res;

            auto vec1 = GetSingletons(), vec2 = rhs.GetSingletons();

            if (vec1.empty() || vec2.empty())
                return Domain_(func_domain);

            for (auto v1 : vec1) {
                for (auto v2 : vec2) {
                    try {
                        res.AddSingleton(func(v1, v2));
                    } catch (const std::domain_error&) {
                        THROW("Domain_ error on function applied to IsSingleton");
                    }
                }
            }

            return res;
        }

        // Inclusion
        [[nodiscard]] bool Includes(const double x) const {
            return std::any_of(intervals_.begin(),
                               intervals_.end(),
                               [&x](const Interval_& i) { return i.Includes(x);});
        }

        [[nodiscard]] bool Includes(const Interval_& rhs) const {
            return std::any_of(intervals_.begin(),
                               intervals_.end(),
                               [&rhs](const Interval_& i) { return i.Includes(rhs);});
        }

        // Useful shortcuts for fuzzy
        [[nodiscard]] bool CanBeZero() const { return Includes(0.0); }

        [[nodiscard]] bool CanBeNonZero() const {
            if (IsEmpty() || (intervals_.size() == 1 && intervals_.begin()->IsZero()))
                return false;
            else
                return true;
        }

        [[nodiscard]] bool ZeroIsDiscrete() const {
            return std::any_of(intervals_.begin(),
                               intervals_.end(),
                               [](const Interval_& i) { return i.IsZero();});
        }

        [[nodiscard]] bool ZeroIsCont() const {
            return std::any_of(intervals_.begin(),
                               intervals_.end(),
                               [](const Interval_& i) { return i.IsContinuous() && i.Includes(0.0);});
        }

        [[nodiscard]] bool CanBePositive(bool strict) const {
            if (IsEmpty())
                return false;
            if (intervals_.rbegin()->Right().Val() > (strict ? Dal::EPSILON : -Dal::EPSILON))
                return true;
            return false;
        }

        [[nodiscard]] bool CanBeNegative(bool strict) const {
            if (IsEmpty())
                return false;
            if (intervals_.begin()->Left().Val() < (strict ? -Dal::EPSILON : Dal::EPSILON))
                return true;

            return false;
        }

        // Smallest IsPositive left bound if any
        [[nodiscard]] bool SmallestPosLb(double& res, bool strict = false) const {
            if (intervals_.rbegin()->Left().IsNegative(!strict))
                return false;

            auto it = intervals_.begin();
            while (it->Left().IsNegative(!strict))
                ++it;
            res = it->Left().Val();
            return true;
        }

        // Biggest IsNegative right bound if any
        [[nodiscard]] bool BiggestNegRb(double& res, bool strict = false) const {
            if (intervals_.begin()->Right().IsPositive(!strict))
                return false;

            auto it = intervals_.rbegin();
            while (it->Right().IsPositive(!strict))
                ++it;
            res = it->Right().Val();
            return true;
        }
    };
}