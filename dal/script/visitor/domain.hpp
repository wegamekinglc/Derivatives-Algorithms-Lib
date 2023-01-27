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
        bool myPlusInf;
        bool myMinusInf;
        double myReal;

    public:
        struct PlusInfinity {};
        struct MinusInfinity {};
        static const PlusInfinity plusInfinity;
        static const MinusInfinity minusInfinity;

        //	Real
        Bound(const double val = 0.0) : myPlusInf(false), myMinusInf(false), myReal(val) {}

        //	Infinite
        Bound(const PlusInfinity) : myPlusInf(true), myMinusInf(false), myReal(Dal::INF) {}
        Bound(const MinusInfinity) : myPlusInf(false), myMinusInf(true), myReal(-Dal::INF) {}

        Bound(const Bound& rhs) : myPlusInf(rhs.myPlusInf), myMinusInf(rhs.myMinusInf), myReal(rhs.myReal) {}

        Bound& operator=(const double val) {
            myPlusInf = myMinusInf = false;
            myReal = val;

            return *this;
        }
        Bound& operator=(const PlusInfinity) {
            myPlusInf = true;
            myMinusInf = false;
            myReal = Dal::INF;

            return *this;
        }
        Bound& operator=(const MinusInfinity) {
            myPlusInf = false;
            myMinusInf = true;
            myReal = -Dal::INF;

            return *this;
        }
        Bound& operator=(const Bound& rhs) {
            if (this == &rhs)
                return *this;

            myPlusInf = rhs.myPlusInf;
            myMinusInf = rhs.myMinusInf;
            myReal = rhs.myReal;

            return *this;
        }

        //	Accessors

        bool infinite() const { return myPlusInf || myMinusInf; }

        bool positive(const bool strict = false) const { return myPlusInf || myReal > (strict ? Dal::EPSILON : -Dal::EPSILON); }

        bool negative(const bool strict = false) const { return myMinusInf || myReal < (strict ? -Dal::EPSILON : Dal::EPSILON); }

        bool zero() const { return !infinite() && fabs(myReal) < Dal::EPSILON; }

        bool plusInf() const { return myPlusInf; }

        bool minusInf() const { return myMinusInf; }

        double val() const { return myReal; }

        //	Comparison

        bool operator==(const Bound& rhs) const {
            return myPlusInf && rhs.myPlusInf || myMinusInf && rhs.myMinusInf || fabs(myReal - rhs.myReal) < Dal::EPSILON;
        }

        bool operator!=(const Bound& rhs) const { return !operator==(rhs); }

        bool operator<(const Bound& rhs) const {
            return myMinusInf && !rhs.myMinusInf || !myPlusInf && rhs.myPlusInf || myReal < rhs.myReal - Dal::EPSILON;
        }

        bool operator>(const Bound& rhs) const {
            return !myMinusInf && rhs.myMinusInf || myPlusInf && !rhs.myPlusInf || myReal > rhs.myReal + Dal::EPSILON;
        }

        bool operator<=(const Bound& rhs) const { return !operator>(rhs); }

        bool operator>=(const Bound& rhs) const { return !operator<(rhs); }

        //	Writers

        friend std::ostream& operator<<(std::ostream& ost, const Bound bnd) {
            if (bnd.myPlusInf)
                ost << "+INF";
            else if (bnd.myMinusInf)
                ost << "-INF";
            else
                ost << bnd.myReal;

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
                    return plusInfinity;
                else if (zero())
                    return rhs; //	Here 0 * inf = inf
                else if (rhs.zero())
                    return *this; //	Same
                else
                    return minusInfinity;
            } else
                return myReal * rhs.myReal;
        }

        //	Negation
        Bound operator-() const {
            if (myMinusInf)
                return plusInfinity;
            else if (myPlusInf)
                return minusInfinity;
            else
                return -myReal;
        }
    };

    class Interval {
        Bound myLeft;
        Bound myRight;

    public:
        //	Singleton
        Interval(const double val = 0.0) : myLeft(val), myRight(val) {}

        //	Interval
        Interval(const Bound& left, const Bound& right) : myLeft(left), myRight(right) {
            // #ifdef _DEBUG
            if (left == Bound::plusInfinity || right == Bound::minusInfinity || left > right)
                throw std::runtime_error("Inconsistent bounds");
            // #endif
        }

        //	Accessors

        Bound left() const { return myLeft; }

        Bound right() const { return myRight; }

        bool positive(const bool strict = false) const { return myLeft.positive(strict); }

        bool negative(const bool strict = false) const { return myRight.negative(strict); }

        bool posOrNeg(const bool strict = false) const { return positive(strict) || negative(strict); }

        bool infinite() const { return myLeft.infinite() || myRight.infinite(); }

        bool singleton(double* val = nullptr) const {
            if (!infinite() && myLeft == myRight) {
                if (val)
                    *val = myLeft.val();
                return true;
            }
            return false;
        }

        bool zero() const { return singleton() && myLeft.zero(); }

        bool continuous() const { return !singleton(); }

        //	Writers

        friend std::ostream& operator<<(std::ostream& ost, const Interval i) {
            double s;
            if (i.singleton(&s)) {
                ost << "{" << s << "}";
            } else {
                ost << "(" << i.myLeft << "," << i.myRight << ")";
            }

            return ost;
        }

        std::string write() const {
            std::ostringstream ost;
            ost << *this;
            return ost.str();
        }

        //	Sorting

        bool operator==(const Interval& rhs) const { return myLeft == rhs.myLeft && myRight == rhs.myRight; }

        bool operator<(const Interval& rhs) const {
            return myLeft < rhs.myLeft || myLeft == rhs.myLeft && myRight < rhs.myRight;
        }

        bool operator>(const Interval& rhs) const {
            return myLeft > rhs.myLeft || myLeft == rhs.myLeft && myRight > rhs.myRight;
        }

        bool operator<=(const Interval& rhs) const { return !operator>(rhs); }

        bool operator>=(const Interval& rhs) const { return !operator<(rhs); }

        //	Arithmetics

        //	Addition
        Interval operator+(const Interval& rhs) const {
            Bound lb, rb;

            if (myLeft.minusInf() || rhs.myLeft.minusInf())
                lb = Bound::minusInfinity;
            else
                lb = myLeft.val() + rhs.myLeft.val();

            if (myRight.plusInf() || rhs.myRight.plusInf())
                rb = Bound::plusInfinity;
            else
                rb = myRight.val() + rhs.myRight.val();

            return Interval(lb, rb);
        }

        Interval& operator+=(const Interval& rhs) {
            *this = *this + rhs;
            return *this;
        }

        //	Unary minus
        Interval operator-() const { return Interval(-myRight, -myLeft); }

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
            b[0] = myRight * rhs.myRight;
            b[1] = myRight * rhs.myLeft;
            b[2] = myLeft * rhs.myRight;
            b[3] = myLeft * rhs.myLeft;

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
                        return Interval(0.0, 1.0 / myLeft.val());
                    else
                        return Interval(1.0 / myRight.val(), 0.0);
                }
                return Interval(1.0 / myRight.val(), 1.0 / myLeft.val());
            } else if (myLeft.zero() || myRight.zero()) //	One of the bounds is 0
            {
                if (infinite()) {
                    if (positive())
                        return Interval(0.0, Bound::plusInfinity);
                    else
                        return Interval(Bound::minusInfinity, 0.0);
                } else {
                    if (positive())
                        return Interval(1.0 / myRight.val(), Bound::plusInfinity);
                    else
                        return Interval(Bound::minusInfinity, 1.0 / myLeft.val());
                }
            }
            //	Interval contains 0 and 0 is not a bound: inverse spans real space
            else
                return Interval(Bound::minusInfinity, Bound::plusInfinity);
        }

        //	Division
        Interval operator/(const Interval& rhs) const { return *this * rhs.inverse(); }

        //	Min/Max
        Interval imin(const Interval& rhs) const {
            Bound lb = myLeft;
            if (rhs.myLeft < lb)
                lb = rhs.myLeft;

            Bound rb = myRight;
            if (rhs.myRight < rb)
                rb = rhs.myRight;

            return Interval(lb, rb);
        }
        Interval imax(const Interval& rhs) const {
            Bound lb = myLeft;
            if (rhs.myLeft > lb)
                lb = rhs.myLeft;

            Bound rb = myRight;
            if (rhs.myRight > rb)
                rb = rhs.myRight;

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
                    throw std::runtime_error("Domain error on function applied to singleton");
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
                    throw std::runtime_error("Domain error on function applied to singleton");
                }
            }

            return val;
        }

        //	Inclusion
        bool includes(const double x) const { return myLeft <= x && myRight >= x; }

        bool includes(const Interval& rhs) const { return myLeft <= rhs.myLeft && myRight >= rhs.myRight; }

        bool isIncludedIn(const Interval& rhs) const { return myLeft >= rhs.myLeft && myRight <= rhs.myRight; }

        //	Adjacence
        //	0: is not adjacent
        //	1: *this is adjacent to rhs on the left of rhs
        //	2: *this is adjacent to rhs on the right of rhs
        unsigned isAdjacent(const Interval& rhs) const {
            if (myRight == rhs.myLeft)
                return 1;
            else if (myLeft == rhs.myRight)
                return 2;
            else
                return 0;
        }

        //	Intersection, returns false if no intersect, true otherwise
        //		in which case iSect is set to the intersection unless nullptr
        friend bool intersect(const Interval& lhs, const Interval& rhs, Interval* iSect = nullptr) {
            Bound lb = lhs.myLeft;
            if (rhs.myLeft > lb)
                lb = rhs.myLeft;

            Bound rb = lhs.myRight;
            if (rhs.myRight < rb)
                rb = rhs.myRight;

            if (rb >= lb) {
                if (iSect) {
                    iSect->myLeft = lb;
                    iSect->myRight = rb;
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
                Bound lb = lhs.myLeft;
                if (rhs.myLeft < lb)
                    lb = rhs.myLeft;

                Bound rb = lhs.myRight;
                if (rhs.myRight > rb)
                    rb = rhs.myRight;

                iMerge->myLeft = lb;
                iMerge->myRight = rb;
            }

            return true;
        }

        //	Another merge function that merges rhs into this, assuming we already know that they intersect
        void merge(const Interval& rhs) {
            if (rhs.myLeft < myLeft)
                myLeft = rhs.myLeft;
            if (rhs.myRight > myRight)
                myRight = rhs.myRight;
        }
    };

    class Domain {
        std::set<Interval> myIntervals;

    public:
        Domain() {}

        Domain(const Domain& rhs) : myIntervals(rhs.myIntervals) {}

        Domain(Domain&& rhs) : myIntervals(move(rhs.myIntervals)) {}

        Domain& operator=(const Domain& rhs) {
            if (this == &rhs)
                return *this;
            myIntervals = rhs.myIntervals;
            return *this;
        }

        Domain& operator=(Domain&& rhs) {
            if (this == &rhs)
                return *this;
            myIntervals = move(rhs.myIntervals);
            return *this;
        }

        Domain(const double val) { addSingleton(val); }

        Domain(const Interval& i) { addInterval(i); }

        void addInterval(Interval interval) {
            while (true) {
                //	Particular case 1: domain is empty, just add the interval
                const auto itb = myIntervals.begin(), ite = myIntervals.end();
                if (itb == ite) {
                    myIntervals.insert(interval);
                    return;
                }

                //	Particular case 2: interval spans real space, then domain becomes the real space
                const Bound& l = interval.left();
                const Bound& r = interval.right();
                if (l.minusInf() && r.plusInf()) {
                    static const Interval realSpace(Bound::minusInfinity, Bound::plusInfinity);
                    myIntervals.clear();
                    myIntervals.insert(realSpace);
                    return;
                }

                //	General case: we insert the interval in such a way that the resulting set of intervals are all distinct

                //	Find an interval in myIntervals that intersects interval, or myinterval.end() if none

                //	STL implementation, nice and elegant, unfortunately poor performance
                //	auto it = find_if( myIntervals.begin(), myIntervals.end(), [&interval] (const Interval& i) { return intersect( i, interval); });

                //	Custom implementation, for performance, much less elegant
                auto it = itb;
                //	First interval is on the strict right of interval, there will be no intersection
                if (itb->left() > r)
                    it = ite;
                else {
                    //	Last interval in myIntervals, we know there is one
                    const Interval& last = *myIntervals.rbegin();

                    //	Last interval is on the strict left of interval, there will be no intersection
                    if (last.right() < l)
                        it = ite;

                    else {
                        //	We may have an intersection, find it
                        it = myIntervals.lower_bound(
                            interval); //	Smallest myInterval >= interval, means it.left() >= l
                        if (it == ite || it->left() > r)
                            --it; //	Now it.left() <= l <= r
                        if (it->right() < l)
                            it = ite; //	it does not intersect
                    }
                }

                //	End of find an interval in myIntervals that intersects interval
                //		it points to an interval in myIntervals that intersects interval, or ite if none

                //	No intersection, just add the interval
                if (it == ite) {
                    myIntervals.insert(interval);
                    return;
                }

                //	We have an intersection

                //	Merge the intersecting interval from myIntervals into interval

                //	We don't use the generic merge: too slow
                //	merge( interval, *it, &interval);
                //	Quick merge
                interval.merge(*it);

                //	Remove the merged interval from set
                myIntervals.erase(it);

                //	Go again until we find no more intersect
            }
        }

        void addDomain(const Domain& rhs) {
            for (auto& interval : rhs.myIntervals)
                addInterval(interval);
        }

        void addSingleton(const double val) { addInterval(val); }

        //	Accessors

        bool positive(const bool strict = false) const {
            for (auto& interval : myIntervals)
                if (!interval.positive(strict))
                    return false;
            return true;
        }

        bool negative(const bool strict = false) const {
            for (auto& interval : myIntervals)
                if (!interval.negative(strict))
                    return false;
            return true;
        }

        bool posOrNeg(const bool strict = false) const { return positive(strict) || negative(strict); }

        bool infinite() const {
            for (auto& interval : myIntervals)
                if (interval.infinite())
                    return true;
            return false;
        }

        bool IsDiscrete() const {
            for (auto& interval : myIntervals)
                if (!interval.singleton())
                    return false;
            return true;
        }

        //	Discrete only is true: return empty if continuous intervals found, false: return all singletons anyway
        Vector_<double> getSingletons(const bool discreteOnly = true) const {
            Vector_<double> res;
            for (auto& interval : myIntervals) {
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
        Domain getContinuous() const {
            Domain res;
            for (auto& interval : myIntervals) {
                if (interval.continuous())
                    res.addInterval(interval);
            }
            return res;
        }

        //	Get min and max bounds
        Bound minBound() const {
            if (!empty())
                return myIntervals.begin()->left();
            else
                return Bound::minusInfinity;
        }
        Bound maxBound() const {
            if (!empty())
                return myIntervals.rbegin()->right();
            else
                return Bound::plusInfinity;
        }

        bool empty() const { return myIntervals.empty(); }

        size_t size() const { return myIntervals.size(); }

        //	Writers

        friend std::ostream& operator<<(std::ostream& ost, const Domain d) {
            ost << "{";
            auto i = d.myIntervals.begin();
            while (i != d.myIntervals.end()) {
                ost << *i;
                ++i;
                if (i != d.myIntervals.end())
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
        Domain operator+(const Domain& rhs) const {
            Domain res;

            for (auto& i : myIntervals) {
                for (auto& j : rhs.myIntervals) {
                    res.addInterval(i + j);
                }
            }

            return res;
        }
        Domain operator-() const {
            Domain res;

            for (auto& i : myIntervals) {
                res.addInterval(-i);
            }

            return res;
        }
        Domain operator-(const Domain& rhs) const {
            Domain res;

            for (auto& i : myIntervals) {
                for (auto& j : rhs.myIntervals) {
                    res.addInterval(i - j);
                }
            }

            return res;
        }
        Domain operator*(const Domain& rhs) const {
            Domain res;

            for (auto& i : myIntervals) {
                for (auto& j : rhs.myIntervals) {
                    size_t s = res.size();
                    res.addInterval(i * j);
                }
            }

            return res;
        }
        Domain inverse() const {
            Domain res;

            for (auto& i : myIntervals) {
                res.addInterval(i.inverse());
            }

            return res;
        }
        Domain operator/(const Domain& rhs) const {
            Domain res;

            for (auto& i : myIntervals) {
                for (auto& j : rhs.myIntervals) {
                    res.addInterval(i / j);
                }
            }

            return res;
        }

        //	Shortcuts for shifting all intervals
        Domain operator+=(const double x) {
            if (fabs(x) < Dal::EPSILON)
                return *this;

            std::set<Interval> newIntervals;
            for (auto& i : myIntervals)
                newIntervals.insert(i + x);
            myIntervals = move(newIntervals);

            return *this;
        }

        Domain operator-=(const double x) {
            if (fabs(x) < Dal::EPSILON)
                return *this;

            std::set<Interval> newIntervals;
            for (auto& i : myIntervals)
                newIntervals.insert(i - x);
            myIntervals = move(newIntervals);

            return *this;
        }

        //	Min/Max
        Domain dmin(const Domain& rhs) const {
            Domain res;

            for (auto& i : myIntervals) {
                for (auto& j : rhs.myIntervals) {
                    res.addInterval(i.imin(j));
                }
            }

            return res;
        }
        Domain dmax(const Domain& rhs) const {
            Domain res;

            for (auto& i : myIntervals) {
                for (auto& j : rhs.myIntervals) {
                    res.addInterval(i.imax(j));
                }
            }

            return res;
        }

        //	Apply function
        template <class Func> Domain applyFunc(const Func func, const Interval& funcDomain) {
            Domain res;

            auto vec = getSingletons();

            if (vec.empty())
                return funcDomain;

            //	Singletons, apply func
            for (auto v : vec) {
                try {
                    res.addSingleton(func(v));
                } catch (const std::domain_error&) {
                    throw std::runtime_error("Domain error on function applied to singleton");
                }
            }

            return res;
        }

        //	Apply function 2 params
        template <class Func> Domain applyFunc2(const Func func, const Domain& rhs, const Interval& funcDomain) {
            Domain res;

            auto vec1 = getSingletons(), vec2 = rhs.getSingletons();

            if (vec1.empty() || vec2.empty())
                return funcDomain;

            for (auto v1 : vec1) {
                for (auto v2 : vec2) {
                    try {
                        res.addSingleton(func(v1, v2));
                    } catch (const std::domain_error&) {
                        throw std::runtime_error("Domain error on function applied to singleton");
                    }
                }
            }

            return res;
        }

        //	Inclusion
        bool includes(const double x) const {
            for (auto& interval : myIntervals)
                if (interval.includes(x))
                    return true;
            return false;
        }

        bool includes(const Interval& rhs) const {
            for (auto& interval : myIntervals)
                if (interval.includes(rhs))
                    return true;
            return false;
        }

        //	Useful shortcuts for fuzzying

        bool canBeZero() const { return includes(0.0); }

        bool canBeNonZero() const {
            if (empty())
                return false;
            else if (myIntervals.size() == 1 && myIntervals.begin()->zero())
                return false;
            else
                return true;
        }

        bool zeroIsDiscrete() const {
            for (auto& interval : myIntervals)
                if (interval.zero())
                    return true;
            return false;
        }

        bool zeroIsCont() const {
            for (auto& interval : myIntervals)
                if (interval.continuous() && interval.includes(0.0))
                    return true;
            return false;
        }

        bool canBePositive(const bool strict) const {
            if (empty())
                return false;
            if (myIntervals.rbegin()->right().val() > (strict ? Dal::EPSILON : -Dal::EPSILON))
                return true;
            return false;
        }

        bool canBeNegative(const bool strict) const {
            if (empty())
                return false;
            if (myIntervals.begin()->left().val() < (strict ? -Dal::EPSILON : Dal::EPSILON))
                return true;

            return false;
        }

        //	Smallest positive left bound if any
        bool smallestPosLb(double& res, const bool strict = false) const {
            if (myIntervals.rbegin()->left().negative(!strict))
                return false;

            auto it = myIntervals.begin();
            while (it->left().negative(!strict))
                ++it;
            res = it->left().val();
            return true;
        }

        //	Biggest negative right bound if any
        bool biggestNegRb(double& res, const bool strict = false) const {
            if (myIntervals.begin()->right().positive(!strict))
                return false;

            auto it = myIntervals.rbegin();
            while (it->right().positive(!strict))
                ++it;
            res = it->right().val();
            return true;
        }
    };
}