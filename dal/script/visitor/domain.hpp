//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <math.h>
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

namespace Dal::Script {

    class Bound {
        bool isPlusInf_;
        bool isMinusInf_;
        double real_;

    public:
        struct PlusInfinity {};
        struct MinusInfinity {};
        static const PlusInfinity plusInfinity_;
        static const MinusInfinity minusInfinity_;

        //	Real
        Bound(const double val = 0.0) : isPlusInf_(false), isMinusInf_(false), real_(val) {}

        //	Infinite
        Bound(const PlusInfinity) : isPlusInf_(true), isMinusInf_(false), real_(Dal::INF) {}
        Bound(const MinusInfinity) : isPlusInf_(false), isMinusInf_(true), real_(-Dal::INF) {}

        Bound(const Bound& rhs) : isPlusInf_(rhs.isPlusInf_), isMinusInf_(rhs.isMinusInf_), real_(rhs.real_) {}

        Bound& operator=(const double val) {
            isPlusInf_ = isMinusInf_ = false;
            real_ = val;

            return *this;
        }
        Bound& operator=(const PlusInfinity) {
            isPlusInf_ = true;
            isMinusInf_ = false;
            real_ = Dal::INF;

            return *this;
        }
        Bound& operator=(const MinusInfinity) {
            isPlusInf_ = false;
            isMinusInf_ = true;
            real_ = -Dal::INF;

            return *this;
        }
        Bound& operator=(const Bound& rhs) {
            if (this == &rhs)
                return *this;

            isPlusInf_ = rhs.isPlusInf_;
            isMinusInf_ = rhs.isMinusInf_;
            real_ = rhs.real_;

            return *this;
        }

        //	Accessors

        bool infinite() const { return isPlusInf_ || isMinusInf_; }

        bool positive(const bool strict = false) const { return isPlusInf_ || real_ > (strict ? Dal::EPSILON : -Dal::EPSILON); }

        bool negative(const bool strict = false) const { return isMinusInf_ || real_ < (strict ? -Dal::EPSILON : Dal::EPSILON); }

        bool zero() const { return !infinite() && fabs(real_) < Dal::EPSILON; }

        bool plusInf() const { return isPlusInf_; }

        bool minusInf() const { return isMinusInf_; }

        double val() const { return real_; }

        //	Comparison

        bool operator==(const Bound& rhs) const {
            return isPlusInf_ && rhs.isPlusInf_ || isMinusInf_ && rhs.isMinusInf_ || fabs(real_ - rhs.real_) < Dal::EPSILON;
        }

        bool operator!=(const Bound& rhs) const { return !operator==(rhs); }

        bool operator<(const Bound& rhs) const {
            return isMinusInf_ && !rhs.isMinusInf_ || !isPlusInf_ && rhs.isPlusInf_ || real_ < rhs.real_ - Dal::EPSILON;
        }

        bool operator>(const Bound& rhs) const {
            return !isMinusInf_ && rhs.isMinusInf_ || isPlusInf_ && !rhs.isPlusInf_ || real_ > rhs.real_ + Dal::EPSILON;
        }

        bool operator<=(const Bound& rhs) const { return !operator>(rhs); }

        bool operator>=(const Bound& rhs) const { return !operator<(rhs); }

        //	Writers

        friend std::ostream& operator<<(std::ostream& ost, const Bound bnd) {
            if (bnd.isPlusInf_)
                ost << "+INF";
            else if (bnd.isMinusInf_)
                ost << "-INF";
            else
                ost << bnd.real_;

            return ost;
        }

        std::string write() const {
            std::ostringstream ost;
            ost << *this;
            return ost.str();
        }

        //	Multiplication
        Bound operator*(const Bound& rhs) const {
            if (infinite() || rhs.infinite()) {
                if (positive(true) && rhs.positive(true) || negative(true) && rhs.negative(true))
                    return plusInfinity_;
                else if (zero())
                    return rhs; //	Here 0 * inf = inf
                else if (rhs.zero())
                    return *this; //	Same
                else
                    return minusInfinity_;
            } else
                return real_ * rhs.real_;
        }

        //	Negation
        Bound operator-() const {
            if (isMinusInf_)
                return plusInfinity_;
            else if (isPlusInf_)
                return minusInfinity_;
            else
                return -real_;
        }
    };

    class Interval {
        Bound left_;
        Bound right_;

    public:
        //	Singleton
        Interval(const double val = 0.0) : left_(val), right_(val) {}

        //	Interval
        Interval(const Bound& left, const Bound& right) : left_(left), right_(right) {
            // #ifdef _DEBUG
            if (left == Bound::plusInfinity_ || right == Bound::minusInfinity_ || left > right)
                throw std::runtime_error("Inconsistent bounds");
            // #endif
        }

        //	Accessors

        Bound left() const { return left_; }

        Bound right() const { return right_; }

        bool positive(const bool strict = false) const { return left_.positive(strict); }

        bool negative(const bool strict = false) const { return right_.negative(strict); }

        bool posOrNeg(const bool strict = false) const { return positive(strict) || negative(strict); }

        bool infinite() const { return left_.infinite() || right_.infinite(); }

        bool singleton(double* val = nullptr) const {
            if (!infinite() && left_ == right_) {
                if (val)
                    *val = left_.val();
                return true;
            }
            return false;
        }

        bool zero() const { return singleton() && left_.zero(); }

        bool continuous() const { return !singleton(); }

        //	Writers

        friend std::ostream& operator<<(std::ostream& ost, const Interval i) {
            double s;
            if (i.singleton(&s)) {
                ost << "{" << s << "}";
            } else {
                ost << "(" << i.left_ << "," << i.right_ << ")";
            }

            return ost;
        }

        std::string write() const {
            std::ostringstream ost;
            ost << *this;
            return ost.str();
        }

        //	Sorting

        bool operator==(const Interval& rhs) const { return left_ == rhs.left_ && right_ == rhs.right_; }

        bool operator<(const Interval& rhs) const {
            return left_ < rhs.left_ || left_ == rhs.left_ && right_ < rhs.right_;
        }

        bool operator>(const Interval& rhs) const {
            return left_ > rhs.left_ || left_ == rhs.left_ && right_ > rhs.right_;
        }

        bool operator<=(const Interval& rhs) const { return !operator>(rhs); }

        bool operator>=(const Interval& rhs) const { return !operator<(rhs); }

        //	Arithmetics

        //	Addition
        Interval operator+(const Interval& rhs) const {
            Bound lb, rb;

            if (left_.minusInf() || rhs.left_.minusInf())
                lb = Bound::minusInfinity_;
            else
                lb = left_.val() + rhs.left_.val();

            if (right_.plusInf() || rhs.right_.plusInf())
                rb = Bound::plusInfinity_;
            else
                rb = right_.val() + rhs.right_.val();

            return Interval(lb, rb);
        }

        Interval& operator+=(const Interval& rhs) {
            *this = *this + rhs;
            return *this;
        }

        //	Unary minus
        Interval operator-() const { return Interval(-right_, -left_); }

        //	Subtraction
        Interval operator-(const Interval& rhs) const { return *this + -rhs; }

        Interval& operator-=(const Interval& rhs) {
            *this = *this - rhs;
            return *this;
        }

        //	Multiplication

        Interval operator*(const Interval& rhs) const {
            //	If we have a zero singleton, the result is a zero singleton
            if (zero() || rhs.zero())
                return 0.0;

            //	Otherwise we multiply the bounds and go from smallest to largest
            std::array<Bound, 4> b;
            b[0] = right_ * rhs.right_;
            b[1] = right_ * rhs.left_;
            b[2] = left_ * rhs.right_;
            b[3] = left_ * rhs.left_;

            return Interval(*std::min_element(b.begin(), b.end()), *std::max_element(b.begin(), b.end()));
        }

        //	Inverse (1/x)
        Interval inverse() const {
            double v;

            //	Cannot inverse a zero singleton
            if (zero())
                throw std::runtime_error("Division by {0}");

            //	Singleton
            else if (singleton(&v))
                return 1.0 / v;

            //	Continuous
            else if (posOrNeg(true)) //	Strict, no 0
            {
                if (infinite()) {
                    if (positive())
                        return Interval(0.0, 1.0 / left_.val());
                    else
                        return Interval(1.0 / right_.val(), 0.0);
                }
                return Interval(1.0 / right_.val(), 1.0 / left_.val());
            } else if (left_.zero() || right_.zero()) //	One of the bounds is 0
            {
                if (infinite()) {
                    if (positive())
                        return Interval(0.0, Bound::plusInfinity_);
                    else
                        return Interval(Bound::minusInfinity_, 0.0);
                } else {
                    if (positive())
                        return Interval(1.0 / right_.val(), Bound::plusInfinity_);
                    else
                        return Interval(Bound::minusInfinity_, 1.0 / left_.val());
                }
            }
            //	Interval contains 0 and 0 is not a bound: inverse spans real space
            else
                return Interval(Bound::minusInfinity_, Bound::plusInfinity_);
        }

        //	Division
        Interval operator/(const Interval& rhs) const { return *this * rhs.inverse(); }

        //	Min/Max
        Interval imin(const Interval& rhs) const {
            Bound lb = left_;
            if (rhs.left_ < lb)
                lb = rhs.left_;

            Bound rb = right_;
            if (rhs.right_ < rb)
                rb = rhs.right_;

            return Interval(lb, rb);
        }
        Interval imax(const Interval& rhs) const {
            Bound lb = left_;
            if (rhs.left_ > lb)
                lb = rhs.left_;

            Bound rb = right_;
            if (rhs.right_ > rb)
                rb = rhs.right_;

            return Interval(lb, rb);
        }

        //	Apply function
        template <class Func> Interval applyFunc(const Func func, const Interval& funcDomain) {
            double val;

            //	Continuous interval, we know nothing of the function, so we just apply the function domain
            if (!singleton(&val))
                return funcDomain;

            //	Singleton, we apply the function to find the target singleton
            else {
                try {
                    val = func(val);
                } catch (const std::domain_error& dErr) {
                    throw std::runtime_error("Domain_ error on function applied to singleton");
                }
            }

            return val;
        }

        //	Apply function 2 params
        template <class Func> Interval applyFunc2(const Func func, const Interval& rhs, const Interval& funcDomain) {
            double val, val2;

            //	Continuous interval, we know nothing of the function, so we just apply the function domain
            if (!singleton(&val) || !rhs.singleton(&val2))
                return funcDomain;

            //	Singleton, we apply the function to find the target singleton
            else {
                try {
                    val = func(val, val2);
                } catch (const std::domain_error& dErr) {
                    throw std::runtime_error("Domain_ error on function applied to singleton");
                }
            }

            return val;
        }

        //	Inclusion
        bool includes(const double x) const { return left_ <= x && right_ >= x; }

        bool includes(const Interval& rhs) const { return left_ <= rhs.left_ && right_ >= rhs.right_; }

        bool isIncludedIn(const Interval& rhs) const { return left_ >= rhs.left_ && right_ <= rhs.right_; }

        //	Adjacence
        //	0: is not adjacent
        //	1: *this is adjacent to rhs on the left of rhs
        //	2: *this is adjacent to rhs on the right of rhs
        unsigned isAdjacent(const Interval& rhs) const {
            if (right_ == rhs.left_)
                return 1;
            else if (left_ == rhs.right_)
                return 2;
            else
                return 0;
        }

        //	Intersection, returns false if no intersect, true otherwise
        //		in which case iSect is set to the intersection unless nullptr
        friend bool intersect(const Interval& lhs, const Interval& rhs, Interval* iSect = nullptr) {
            Bound lb = lhs.left_;
            if (rhs.left_ > lb)
                lb = rhs.left_;

            Bound rb = lhs.right_;
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

        //	Merge, returns false if no intersect, true otherwise
        //		in which case iMerge is set to the merged interval unless nullptr
        friend bool merge(const Interval& lhs, const Interval& rhs, Interval* iMerge = nullptr) {
            if (!intersect(lhs, rhs))
                return false;

            if (iMerge) {
                Bound lb = lhs.left_;
                if (rhs.left_ < lb)
                    lb = rhs.left_;

                Bound rb = lhs.right_;
                if (rhs.right_ > rb)
                    rb = rhs.right_;

                iMerge->left_ = lb;
                iMerge->right_ = rb;
            }

            return true;
        }

        //	Another merge function that merges rhs into this, assuming we already know that they intersect
        void merge(const Interval& rhs) {
            if (rhs.left_ < left_)
                left_ = rhs.left_;
            if (rhs.right_ > right_)
                right_ = rhs.right_;
        }
    };

    class Domain_ {
        std::set<Interval> intervals_;

    public:
        Domain_() {}

        Domain_(const Domain_& rhs) : intervals_(rhs.intervals_) {}

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

        Domain_(const double val) { addSingleton(val); }

        Domain_(const Interval& i) { addInterval(i); }

        void addInterval(Interval interval) {
            while (true) {
                //	Particular case 1: domain is empty, just add the interval
                const auto itb = intervals_.begin(), ite = intervals_.end();
                if (itb == ite) {
                    intervals_.insert(interval);
                    return;
                }

                //	Particular case 2: interval spans real space, then domain becomes the real space
                const Bound& l = interval.left();
                const Bound& r = interval.right();
                if (l.minusInf() && r.plusInf()) {
                    static const Interval realSpace(Bound::minusInfinity_, Bound::plusInfinity_);
                    intervals_.clear();
                    intervals_.insert(realSpace);
                    return;
                }

                //	General case: we insert the interval in such a way that the resulting set of intervals are all distinct

                //	Find an interval in intervals_ that intersects interval, or myinterval.end() if none

                //	STL implementation, nice and elegant, unfortunately poor performance
                //	auto it = find_if( intervals_.begin(), intervals_.end(), [&interval] (const Interval& i) { return intersect( i, interval); });

                //	Custom implementation, for performance, much less elegant
                auto it = itb;
                //	First interval is on the strict right of interval, there will be no intersection
                if (itb->left() > r)
                    it = ite;
                else {
                    //	Last interval in intervals_, we know there is one
                    const Interval& last = *intervals_.rbegin();

                    //	Last interval is on the strict left of interval, there will be no intersection
                    if (last.right() < l)
                        it = ite;

                    else {
                        //	We may have an intersection, find it
                        it = intervals_.lower_bound(
                            interval); //	Smallest myInterval >= interval, means it.left() >= l
                        if (it == ite || it->left() > r)
                            --it; //	Now it.left() <= l <= r
                        if (it->right() < l)
                            it = ite; //	it does not intersect
                    }
                }

                //	End of find an interval in intervals_ that intersects interval
                //		it points to an interval in intervals_ that intersects interval, or ite if none

                //	No intersection, just add the interval
                if (it == ite) {
                    intervals_.insert(interval);
                    return;
                }

                //	We have an intersection

                //	Merge the intersecting interval from intervals_ into interval

                //	We don't use the generic merge: too slow
                //	merge( interval, *it, &interval);
                //	Quick merge
                interval.merge(*it);

                //	Remove the merged interval from set
                intervals_.erase(it);

                //	Go again until we find no more intersect
            }
        }

        void addDomain(const Domain_& rhs) {
            for (auto& interval : rhs.intervals_)
                addInterval(interval);
        }

        void addSingleton(const double val) { addInterval(val); }

        //	Accessors

        bool positive(const bool strict = false) const {
            for (auto& interval : intervals_)
                if (!interval.positive(strict))
                    return false;
            return true;
        }

        bool negative(const bool strict = false) const {
            for (auto& interval : intervals_)
                if (!interval.negative(strict))
                    return false;
            return true;
        }

        bool posOrNeg(const bool strict = false) const { return positive(strict) || negative(strict); }

        bool infinite() const {
            for (auto& interval : intervals_)
                if (interval.infinite())
                    return true;
            return false;
        }

        bool IsDiscrete() const {
            for (auto& interval : intervals_)
                if (!interval.singleton())
                    return false;
            return true;
        }

        //	Discrete only is true: return empty if continuous intervals found, false: return all singletons anyway
        Vector_<double> getSingletons(const bool discreteOnly = true) const {
            Vector_<double> res;
            for (auto& interval : intervals_) {
                double val;
                if (!interval.singleton(&val)) {
                    if (discreteOnly)
                        return Vector_<double>();
                } else
                    res.push_back(val);
            }
            return res;
        }

        //	At least one continuous interval
        bool continuous() const { return !IsDiscrete(); }

        //	Shortcut for 2 singletons
        bool boolean(pair<double, double>* vals = nullptr) const {
            Vector_<double> s = getSingletons();
            if (s.size() == 2) {
                if (vals) {
                    vals->first = s[0];
                    vals->second = s[1];
                }
                return true;
            } else
                return false;
        }

        //	Shortcut for 1 singleton
        bool constant(double* val = nullptr) const {
            Vector_<double> s = getSingletons();
            if (s.size() == 1) {
                if (val)
                    *val = s[0];
                return true;
            } else
                return false;
        }

        //	Get all continuous intervals, dropping singletons
        Domain_ getContinuous() const {
            Domain_ res;
            for (auto& interval : intervals_) {
                if (interval.continuous())
                    res.addInterval(interval);
            }
            return res;
        }

        //	Get min and max bounds
        Bound minBound() const {
            if (!empty())
                return intervals_.begin()->left();
            else
                return Bound::minusInfinity_;
        }
        Bound maxBound() const {
            if (!empty())
                return intervals_.rbegin()->right();
            else
                return Bound::plusInfinity_;
        }

        bool empty() const { return intervals_.empty(); }

        size_t size() const { return intervals_.size(); }

        //	Writers

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

        std::string write() const {
            std::ostringstream ost;
            ost << *this;
            return ost.str();
        }

        //	Arithmetics
        Domain_ operator+(const Domain_& rhs) const {
            Domain_ res;

            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.addInterval(i + j);
                }
            }

            return res;
        }
        Domain_ operator-() const {
            Domain_ res;

            for (auto& i : intervals_) {
                res.addInterval(-i);
            }

            return res;
        }
        Domain_ operator-(const Domain_& rhs) const {
            Domain_ res;

            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.addInterval(i - j);
                }
            }

            return res;
        }
        Domain_ operator*(const Domain_& rhs) const {
            Domain_ res;

            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    size_t s = res.size();
                    res.addInterval(i * j);
                }
            }

            return res;
        }
        Domain_ inverse() const {
            Domain_ res;

            for (auto& i : intervals_) {
                res.addInterval(i.inverse());
            }

            return res;
        }
        Domain_ operator/(const Domain_& rhs) const {
            Domain_ res;

            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.addInterval(i / j);
                }
            }

            return res;
        }

        //	Shortcuts for shifting all intervals
        Domain_ operator+=(const double x) {
            if (fabs(x) < Dal::EPSILON)
                return *this;

            std::set<Interval> newIntervals;
            for (auto& i : intervals_)
                newIntervals.insert(i + x);
            intervals_ = std::move(newIntervals);

            return *this;
        }

        Domain_ operator-=(const double x) {
            if (fabs(x) < Dal::EPSILON)
                return *this;

            std::set<Interval> newIntervals;
            for (auto& i : intervals_)
                newIntervals.insert(i - x);
            intervals_ = std::move(newIntervals);

            return *this;
        }

        //	Min/Max
        Domain_ dmin(const Domain_& rhs) const {
            Domain_ res;

            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.addInterval(i.imin(j));
                }
            }

            return res;
        }
        Domain_ dmax(const Domain_& rhs) const {
            Domain_ res;

            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.addInterval(i.imax(j));
                }
            }

            return res;
        }

        //	Apply function
        template <class Func> Domain_ applyFunc(const Func func, const Interval& funcDomain) {
            Domain_ res;

            auto vec = getSingletons();

            if (vec.empty())
                return funcDomain;

            //	Singletons, apply func
            for (auto v : vec) {
                try {
                    res.addSingleton(func(v));
                } catch (const std::domain_error&) {
                    throw std::runtime_error("Domain_ error on function applied to singleton");
                }
            }

            return res;
        }

        //	Apply function 2 params
        template <class Func> Domain_ applyFunc2(const Func func, const Domain_& rhs, const Interval& funcDomain) {
            Domain_ res;

            auto vec1 = getSingletons(), vec2 = rhs.getSingletons();

            if (vec1.empty() || vec2.empty())
                return funcDomain;

            for (auto v1 : vec1) {
                for (auto v2 : vec2) {
                    try {
                        res.addSingleton(func(v1, v2));
                    } catch (const std::domain_error&) {
                        throw std::runtime_error("Domain_ error on function applied to singleton");
                    }
                }
            }

            return res;
        }

        //	Inclusion
        bool includes(const double x) const {
            for (auto& interval : intervals_)
                if (interval.includes(x))
                    return true;
            return false;
        }

        bool includes(const Interval& rhs) const {
            for (auto& interval : intervals_)
                if (interval.includes(rhs))
                    return true;
            return false;
        }

        //	Useful shortcuts for fuzzying

        bool canBeZero() const { return includes(0.0); }

        bool canBeNonZero() const {
            if (empty())
                return false;
            else if (intervals_.size() == 1 && intervals_.begin()->zero())
                return false;
            else
                return true;
        }

        bool zeroIsDiscrete() const {
            for (auto& interval : intervals_)
                if (interval.zero())
                    return true;
            return false;
        }

        bool zeroIsCont() const {
            for (auto& interval : intervals_)
                if (interval.continuous() && interval.includes(0.0))
                    return true;
            return false;
        }

        bool canBePositive(const bool strict) const {
            if (empty())
                return false;
            if (intervals_.rbegin()->right().val() > (strict ? Dal::EPSILON : -Dal::EPSILON))
                return true;
            return false;
        }

        bool canBeNegative(const bool strict) const {
            if (empty())
                return false;
            if (intervals_.begin()->left().val() < (strict ? -Dal::EPSILON : Dal::EPSILON))
                return true;

            return false;
        }

        //	Smallest positive left bound if any
        bool smallestPosLb(double& res, const bool strict = false) const {
            if (intervals_.rbegin()->left().negative(!strict))
                return false;

            auto it = intervals_.begin();
            while (it->left().negative(!strict))
                ++it;
            res = it->left().val();
            return true;
        }

        //	Biggest negative right bound if any
        bool biggestNegRb(double& res, const bool strict = false) const {
            if (intervals_.begin()->right().positive(!strict))
                return false;

            auto it = intervals_.rbegin();
            while (it->right().positive(!strict))
                ++it;
            res = it->right().val();
            return true;
        }
    };
}