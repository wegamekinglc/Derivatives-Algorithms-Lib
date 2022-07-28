//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <algorithm>
#include <dal/math/operators.hpp>
#include <dal/string/strings.hpp>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace Dal::Script {

    static constexpr double BIG = 1.0e+12;
    static constexpr double EPS = 1.0e-12;

    class Bound_ {
        bool plusInf_;
        bool minusInf_;
        double real_;

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

        [[nodiscard]] Bound_ Right() const { return right_; }

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
        template <class Func> [[nodiscard]] Interval_ ApplyFunc(Func func, const Interval_& funcDomain) {
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

        // Inclusion
        [[nodiscard]] bool IsInclude(const double x) const { return left_ <= x && right_ >= x; }

        [[nodiscard]] bool IsInclude(const Interval_& rhs) const { return left_ <= rhs.left_ && right_ >= rhs.right_; }

        [[nodiscard]] bool IsIncluded(const Interval_& rhs) const { return left_ >= rhs.left_ && right_ <= rhs.right_; }

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
            } else
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

    class Domain_ {
        std::set<Interval_> intervals_;

    public:
        Domain_() = default;
        Domain_(const Domain_& rhs) = default;
        Domain_(Domain_&& rhs) : intervals_(std::move(rhs.intervals_)) {}

        Domain_& operator=(const Domain_& rhs) {
            if (this == &rhs)
                return *this;
            intervals_ = rhs.intervals_;
            return *this;
        }

        Domain_& operator=(Domain_&& rhs) {
            if (this == &rhs)
                return *this;
            intervals_ = std::move(rhs.intervals_);
            return *this;
        }

        Domain_(const double val) { AddSingleton(val); }
        Domain_(const Interval_& i) { AddInterval(i); }

        void AddInterval(Interval_ interval) {
            static const Interval_ realSpace{Bound_::minusInfinity_, Bound_::plusInfinity_};
            while (true) {
                // Particular case 1: domain is empty, just add the interval
                const auto itb = intervals_.begin();
                const auto ite = intervals_.end();
                if (itb == ite) {
                    intervals_.insert(interval);
                    return;
                }

                // Particular case 2: interval spans real space, then domain becomes the real space
                const Bound_& l = interval.Left();
                const Bound_& r = interval.Right();
                if (l.IsMinusInf() && r.IsPlusInf()) {
                    intervals_.clear();
                    intervals_.insert(realSpace);
                    return;
                }

                // General case: we insert the interval in such a way that the resulting set of intervals are all
                //distinct

                // Find an interval in intervals_ that intersects interval, or intervals_.end() if none

                // STL implementation, nice and elegant, unfortunately poor performance
                // auto it = find_if( intervals_.begin(), intervals_.end(), [&interval] (const Interval_& i) {
                // return intersect( i, interval); });

                // Custom implementation, for performance, much less elegant
                auto it = itb;
                //	First interval is on the strict right of interval, there will be no intersection
                if (itb->Left() > r)
                    it = ite;
                else {
                    //	Last interval in intervals_, we know there is one
                    const Interval_& last = *intervals_.rbegin();

                    //	Last interval is on the strict left of interval, there will be no intersection
                    if (last.Right() < l)
                        it = ite;
                    else {
                        // we may have an intersection, find it
                        it = intervals_.lower_bound(
                            interval); // smallest myInterval >= interval, means it.Left() >= l
                        if (it == ite || it->Left() > r)
                            --it; //	Now it.left() <= l <= r
                        if (it->Right() < l)
                            it = ite; //	it does not intersect
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
                // merge the intersecting interval from intervals_ into interval
                // we don't use the generic merge: too slow
                // merge(interval, *it, &interval);
                // quick merge
                interval.Merge(*it);

                // Remove the merged interval from set
                intervals_.erase(it);
                // Go again until we find no more intersect
            }
        }

        void AddDomain_(const Domain_& rhs) {
            for (auto& interval : rhs.intervals_)
                AddInterval(interval);
        }

        void AddSingleton(const double val) { AddInterval(Interval_(val)); }

        // accessors
        [[nodiscard]] bool IsPositive(const bool strict = false) const {
            for (auto& interval : intervals_)
                if (!interval.IsPositive(strict))
                    return false;
            return true;
        }

        [[nodiscard]] bool IsNegative(const bool strict = false) const {
            for (auto& interval : intervals_)
                if (!interval.IsNegative(strict))
                    return false;
            return true;
        }

        [[nodiscard]] bool IsPosOrNeg(bool strict = false) const {
            return IsPositive(strict) || IsNegative(strict);
        }

        [[nodiscard]] bool IsInf() const {
            for (auto& interval : intervals_)
                if (interval.IsInf())
                    return true;
            return false;
        }

        [[nodiscard]] bool IsDiscrete() const {
            for (auto& interval : intervals_)
                if (!interval.IsSingleton())
                    return false;
            return true;
        }

        // Discrete only is true: return empty if continuous intervals found, false: return all singletons anyway
        [[nodiscard]] Vector_<> GetSingletons(bool discreteOnly = true) const {
            Vector_<> res;
            for (auto& interval : intervals_) {
                double val;
                if (!interval.IsSingleton(&val)) {
                    if (discreteOnly)
                        return {};
                } else
                    res.push_back(val);
            }
            return res;
        }

        // at least one continuous interval
        [[nodiscard]] bool Continuous() const { return !IsDiscrete(); }

        // shortcut for 2 singletons
        [[nodiscard]] bool Boolean(std::pair<double, double>* vals = nullptr) const {
            Vector_<> s = GetSingletons();
            if (s.size() == 2) {
                if (vals) {
                    vals->first = s[0];
                    vals->second = s[1];
                }
                return true;
            } else
                return false;
        }

        // shortcut for 1 singleton
        [[nodiscard]] bool Constant(double* val = nullptr) const {
            Vector_<> s = GetSingletons();
            if (s.size() == 1) {
                if (val)
                    *val = s[0];
                return true;
            } else
                return false;
        }

        // get all continuous intervals, dropping singletons
        [[nodiscard]] Domain_ GetContinuous() const {
            Domain_ res;
            for (auto& interval : intervals_) {
                if (interval.IsContinuous())
                    res.AddInterval(interval);
            }
            return res;
        }

        // get min and max bounds
        [[nodiscard]] Bound_ MinBound() const {
            if (!IsEmpty())
                return intervals_.begin()->Left();
            else
                return Bound_::minusInfinity_;
        }

        [[nodiscard]] Bound_ MaxBound() const {
            if (!IsEmpty())
                return intervals_.rbegin()->Right();
            else
                return Bound_::plusInfinity_;
        }

        [[nodiscard]] bool IsEmpty() const { return intervals_.empty(); }

        [[nodiscard]] size_t Size() const { return intervals_.size(); }

        // writers
        friend std::ostream& operator<<(std::ostream& ost, const Domain_ d) {
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

        [[nodiscard]] String_ Write() const {
            std::ostringstream ost;
            ost << *this;
            return String_(ost.str());
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

        //	Shortcuts for shifting all intervals
        Domain_ operator+=(const double x) {
            if (Fabs(x) < EPS)
                return *this;

            std::set<Interval_> newIntervals;
            for (auto& i : intervals_)
                newIntervals.insert(i + Interval_(x));
            intervals_ = std::move(newIntervals);
            return *this;
        }

        Domain_ operator-=(const double x) {
            if (Fabs(x) < EPS)
                return *this;

            std::set<Interval_> newIntervals;
            for (auto& i : intervals_)
                newIntervals.insert(i - Interval_(x));
            intervals_ = std::move(newIntervals);

            return *this;
        }

        // min/Max
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

        // apply function
        template <class Func> Domain_ ApplyFunc(const Func func, const Interval_& funcDomain_) {
            Domain_ res;
            auto vec = GetSingletons();

            if (vec.empty())
                return funcDomain_;

            //	Singletons, apply func
            for (auto v : vec) {
                try {
                    res.AddSingleton(func(v));
                } catch (const std::domain_error&) {
                    THROW("Domain_ error on function applied to singleton");
                }
            }
            return res;
        }

        // apply function 2 params
        template <class Func>
        Domain_ ApplyFunc2(const Func func, const Domain_& rhs, const Interval_& funcDomain_) {
            Domain_ res;
            auto vec1 = GetSingletons(), vec2 = rhs.GetSingletons();

            if (vec1.empty() || vec2.empty())
                return funcDomain_;

            for (auto v1 : vec1) {
                for (auto v2 : vec2) {
                    try {
                        res.AddSingleton(func(v1, v2));
                    } catch (const std::domain_error&) {
                        THROW("Domain_ error on function applied to singleton");
                    }
                }
            }
            return res;
        }

        // inclusion
        bool IsInclude(double x) const {
            for (auto& interval : intervals_)
                if (interval.IsInclude(x))
                    return true;
            return false;
        }

        bool IsInclude(const Interval_& rhs) const {
            for (auto& interval : intervals_)
                if (interval.IsInclude(rhs))
                    return true;
            return false;
        }

        // useful shortcuts for fuzzying
        [[nodiscard]] bool CanBeZero() const { return IsInclude(0.0); }

        [[nodiscard]] bool CanBeNonZero() const {
            if (IsEmpty() || (intervals_.size() == 1 && intervals_.begin()->IsZero()))
                return false;
            else
                return true;
        }

        [[nodiscard]] bool ZeroIsDiscrete() const {
            for (auto& interval : intervals_)
                if (interval.IsZero())
                    return true;
            return false;
        }

        [[nodiscard]] bool ZeroIsCont() const {
            for (auto& interval : intervals_)
                if (interval.IsContinuous() && interval.IsInclude(0.0))
                    return true;
            return false;
        }

        [[nodiscard]] bool CanBePositive(const bool strict) const {
            if (IsEmpty())
                return false;
            if (intervals_.rbegin()->Right().Value() > (strict ? EPS : -EPS))
                return true;
            return false;
        }

        [[nodiscard]] bool CanBeNegative(const bool strict) const {
            if (IsEmpty())
                return false;
            if (intervals_.begin()->Left().Value() < (strict ? -EPS : EPS))
                return true;
            return false;
        }

        // smallest positive left bound if any
        bool SmallestPosLb(double& res, bool strict = false) const {
            if (intervals_.rbegin()->Left().IsNegative(!strict))
                return false;
            auto it = intervals_.begin();
            while (it->Left().IsNegative(!strict))
                ++it;
            res = it->Left().Value();
            return true;
        }

        // biggest negative right bound if any
        bool BiggestNegRb(double& res, bool strict = false) const {
            if (intervals_.begin()->Right().IsPositive(!strict))
                return false;
            auto it = intervals_.rbegin();
            while (it->Right().IsPositive(!strict))
                ++it;
            res = it->Right().Value();
            return true;
        }
    };
} // namespace Dal::Script