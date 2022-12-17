//
// Created by wegam on 2022/7/31.
//

#include <dal/math/operators.hpp>
#include <algorithm>
#include <dal/script/visitor/domain.hpp>
#include <sstream>
#include <stdexcept>

namespace Dal::Script {

    const typename Bound_::PlusInfinity_ Bound_::plusInfinity_;
    const typename Bound_::MinusInfinity_ Bound_::minusInfinity_;

    Bound_& Bound_::operator=(double Value) {
        plusInf_ = minusInf_ = false;
        real_ = Value;
        return *this;
    }

    Bound_& Bound_::operator=(const PlusInfinity_&) {
        plusInf_ = true;
        minusInf_ = false;
        real_ = BIG;
        return *this;
    }

    Bound_& Bound_::operator=(const MinusInfinity_&) {
        plusInf_ = false;
        minusInf_ = true;
        real_ = -BIG;
        return *this;
    }

    Bound_& Bound_::operator=(const Bound_& rhs) {
        if (this == &rhs)
            return *this;

        plusInf_ = rhs.plusInf_;
        minusInf_ = rhs.minusInf_;
        real_ = rhs.real_;
        return *this;
    }

    // accessors
    bool Bound_::IsInf() const { return plusInf_ || minusInf_; }
    bool Bound_::IsPositive(bool strict) const { return plusInf_ || real_ > (strict ? EPS : -EPS); }
    bool Bound_::IsNegative(bool strict) const { return minusInf_ || real_ < (strict ? -EPS : EPS); }
    bool Bound_::IsZero() const { return !IsInf() && Fabs(real_) < EPS; }
    bool Bound_::IsPlusInf() const { return plusInf_; }
    bool Bound_::IsMinusInf() const { return minusInf_; }
    double Bound_::Value() const { return real_; }

    // comparison
    bool Bound_::operator==(const Bound_& rhs) const {
        return plusInf_ && rhs.plusInf_ || minusInf_ && rhs.minusInf_ || Fabs(real_ - rhs.real_) < EPS;
    }
    bool Bound_::operator!=(const Bound_& rhs) const { return !operator==(rhs); }
    bool Bound_::operator<(const Bound_& rhs) const {
        return minusInf_ && !rhs.minusInf_ || !plusInf_ && rhs.plusInf_ || real_ < rhs.real_ - EPS;
    }
    bool Bound_::operator>(const Bound_& rhs) const {
        return !minusInf_ && rhs.minusInf_ || plusInf_ && !rhs.plusInf_ || real_ > rhs.real_ + EPS;
    }
    bool Bound_::operator<=(const Bound_& rhs) const { return !operator>(rhs); }
    bool Bound_::operator>=(const Bound_& rhs) const { return !operator<(rhs); }

    // writers
    std::ostream& operator<<(std::ostream& ost, const Bound_& bnd) {
        if (bnd.plusInf_)
            ost << "+INF";
        else if (bnd.minusInf_)
            ost << "-INF";
        else
            ost << bnd.real_;
        return ost;
    }

    String_ Bound_::Write() const {
        std::ostringstream ost;
        ost << *this;
        return String_(ost.str());
    }

    // multiplication
    Bound_ Bound_::operator*(const Bound_& rhs) const {
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

    // negation
    Bound_ Bound_::operator-() const {
        if (minusInf_)
            return plusInfinity_;
        else if (plusInf_)
            return minusInfinity_;
        else
            return -real_;
    }

    // accessors
    Bound_ Interval_::Left() const { return left_; }
    Bound_ Interval_::Right() const { return right_; }
    bool Interval_::IsPositive(bool strict) const { return left_.IsPositive(strict); }
    bool Interval_::IsNegative(bool strict) const { return right_.IsNegative(strict); }
    bool Interval_::IsPosOrNeg(bool strict) const { return IsPositive(strict) || IsNegative(strict); }
    bool Interval_::IsInf() const { return left_.IsInf() || right_.IsInf(); }
    bool Interval_::IsSingleton(double* val) const {
        if (!IsInf() && left_ == right_) {
            if (val)
                *val = left_.Value();
            return true;
        }
        return false;
    }

    bool Interval_::IsZero() const { return IsSingleton() && left_.IsZero(); }

    bool Interval_::IsContinuous() const { return !IsSingleton(); }

    // writers
    std::ostream& operator<<(std::ostream& ost, const Interval_& i) {
        double s;
        if (i.IsSingleton(&s)) {
            ost << "{" << s << "}";
        } else {
            ost << "(" << i.left_ << "," << i.right_ << ")";
        }
        return ost;
    }

    String_ Interval_::Write() const {
        std::ostringstream ost;
        ost << *this;
        return String_(ost.str());
    }

    // sorting
    bool Interval_::operator==(const Interval_& rhs) const { return left_ == rhs.left_ && right_ == rhs.right_; }

    bool Interval_::operator<(const Interval_& rhs) const {
        return left_ < rhs.left_ || left_ == rhs.left_ && right_ < rhs.right_;
    }

    bool Interval_::operator>(const Interval_& rhs) const {
        return left_ > rhs.left_ || left_ == rhs.left_ && right_ > rhs.right_;
    }

    bool Interval_::operator<=(const Interval_& rhs) const { return !operator>(rhs); }

    bool Interval_::operator>=(const Interval_& rhs) const { return !operator<(rhs); }

    // arithmetics
    // addition
    Interval_ Interval_::operator+(const Interval_& rhs) const {
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

    Interval_& Interval_::operator+=(const Interval_& rhs) {
        *this = *this + rhs;
        return *this;
    }

    // unary minus
    Interval_ Interval_::operator-() const { return {-right_, -left_}; }

    // subtraction
    Interval_ Interval_::operator-(const Interval_& rhs) const { return *this + -rhs; }

    Interval_& Interval_::operator-=(const Interval_& rhs) {
        *this = *this - rhs;
        return *this;
    }

    // multiplication
    Interval_ Interval_::operator*(const Interval_& rhs) const {
        //	If we have a IsZero IsSingleton, the result is a IsZero IsSingleton
        if (IsZero() || rhs.IsZero())
            return Interval_(0.0);

        //	Otherwise we multiply the bounds and go from smallest to largest
        Vector_<Bound_> candidates;
        candidates.push_back(right_ * rhs.right_);
        candidates.push_back(right_ * rhs.left_);
        candidates.push_back(left_ * rhs.right_);
        candidates.push_back(left_ * rhs.left_);

        return {*std::min_element(candidates.begin(), candidates.end()),
                *std::max_element(candidates.begin(), candidates.end())};
    }

    // inverse (1/x)
    Interval_ Interval_::Inverse() const {
        double v;

        // cannot Inverse a IsZero IsSingleton
        if (IsZero())
            THROW("division by {0}");

        // singleton
        else if (IsSingleton(&v))
            return Interval_(1.0 / v);

        // continuous
        else if (IsPosOrNeg(true)) {
            // strict, no 0
            if (IsInf()) {
                if (IsPositive())
                    return {0.0, 1.0 / left_.Value()};
                else
                    return {1.0 / right_.Value(), 0.0};
            }
            return {1.0 / right_.Value(), 1.0 / left_.Value()};
        } else if (left_.IsZero() || right_.IsZero()) {
            // one of the bounds is 0
            if (IsInf()) {
                if (IsPositive())
                    return {0.0, Bound_::plusInfinity_};
                else
                    return {Bound_::minusInfinity_, 0.0};
            } else {
                if (IsPositive())
                    return {1.0 / right_.Value(), Bound_::plusInfinity_};
                else
                    return {Bound_::minusInfinity_, 1.0 / left_.Value()};
            }
        }
        // interval_ contains 0 and 0 is not a bound: Inverse spans real space
        else
            return {Bound_::minusInfinity_, Bound_::plusInfinity_};
    }

    // division
    Interval_ Interval_::operator/(const Interval_& rhs) const { return *this * rhs.Inverse(); }

    // min/max
    Interval_ Interval_::IMin(const Interval_& rhs) const {
        Bound_ lb = left_;
        if (rhs.left_ < lb)
            lb = rhs.left_;

        Bound_ rb = right_;
        if (rhs.right_ < rb)
            rb = rhs.right_;

        return {lb, rb};
    }
    Interval_ Interval_::IMax(const Interval_& rhs) const {
        Bound_ lb = left_;
        if (rhs.left_ > lb)
            lb = rhs.left_;

        Bound_ rb = right_;
        if (rhs.right_ > rb)
            rb = rhs.right_;

        return {lb, rb};
    }

    // inclusion
    bool Interval_::IsInclude(double x) const { return left_ <= x && right_ >= x; }

    bool Interval_::IsInclude(const Interval_& rhs) const { return left_ <= rhs.left_ && right_ >= rhs.right_; }

    bool Interval_::IsIncluded(const Interval_& rhs) const { return left_ >= rhs.left_ && right_ <= rhs.right_; }

    // adjacence
    // 0: is not adjacent
    // 1: *this is adjacent to rhs on the left of rhs
    // 2: *this is adjacent to rhs on the right of rhs
    unsigned Interval_::IsAdjacent(const Interval_& rhs) const {
        if (right_ == rhs.left_)
            return 1;
        else if (left_ == rhs.right_)
            return 2;
        else
            return 0;
    }

    // intersection, returns false if no intersect, true otherwise
    // in which case iSect is set to the intersection unless nullptr
    bool Intersect(const Interval_& lhs, const Interval_& rhs, Interval_* iSect = nullptr) {
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

    // merge, returns false if no intersect, true otherwise
    // in which case iMerge is set to the merged interval unless nullptr
    bool Merge(const Interval_& lhs, const Interval_& rhs, Interval_* iMerge = nullptr) {
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

    // another merge function that merges rhs into this, assuming we already know that they intersect
    void Interval_::Merge(const Interval_& rhs) {
        if (rhs.left_ < left_)
            left_ = rhs.left_;
        if (rhs.right_ > right_)
            right_ = rhs.right_;
    }

        Domain_& Domain_::operator=(const Domain_& rhs) {
            if (this == &rhs)
                return *this;
            intervals_ = rhs.intervals_;
            return *this;
        }

        Domain_& Domain_::operator=(Domain_&& rhs) noexcept {
            if (this == &rhs)
                return *this;
            intervals_ = std::move(rhs.intervals_);
            return *this;
        }

        void Domain_::AddInterval(Interval_ interval) {
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

        void Domain_::AddDomain(const Domain_& rhs) {
            for (auto& interval : rhs.intervals_)
                AddInterval(interval);
        }

        void Domain_::AddSingleton(double val) { AddInterval(Interval_(val)); }

        // accessors
        bool Domain_::IsPositive(bool strict) const {
            for (auto& interval : intervals_)
                if (!interval.IsPositive(strict))
                    return false;
            return true;
        }

        bool Domain_::IsNegative(bool strict) const {
            for (auto& interval : intervals_)
                if (!interval.IsNegative(strict))
                    return false;
            return true;
        }

        bool Domain_::IsPosOrNeg(bool strict) const {
            return IsPositive(strict) || IsNegative(strict);
        }

        bool Domain_::IsInf() const {
            for (auto& interval : intervals_)
                if (interval.IsInf())
                    return true;
            return false;
        }

        bool Domain_::IsDiscrete() const {
            for (auto& interval : intervals_)
                if (!interval.IsSingleton())
                    return false;
            return true;
        }

        // Discrete only is true: return empty if continuous intervals found, false: return all singletons anyway
        Vector_<> Domain_::GetSingletons(bool discreteOnly) const {
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
        bool Domain_::IsContinuous() const { return !IsDiscrete(); }

        // shortcut for 2 singletons
        bool Domain_::IsBoolean(std::pair<double, double>* vals) const {
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
        bool Domain_::IsConstant(double* val) const {
            Vector_<> s = GetSingletons();
            if (s.size() == 1) {
                if (val)
                    *val = s[0];
                return true;
            } else
                return false;
        }

        // get all continuous intervals, dropping singletons
        Domain_ Domain_::GetContinuous() const {
            Domain_ res;
            for (auto& interval : intervals_) {
                if (interval.IsContinuous())
                    res.AddInterval(interval);
            }
            return res;
        }

        // get min and max bounds
        Bound_ Domain_::MinBound() const {
            if (!IsEmpty())
                return intervals_.begin()->Left();
            else
                return Bound_::minusInfinity_;
        }

        Bound_ Domain_::MaxBound() const {
            if (!IsEmpty())
                return intervals_.rbegin()->Right();
            else
                return Bound_::plusInfinity_;
        }

        bool Domain_::IsEmpty() const { return intervals_.empty(); }

        size_t Domain_::Size() const { return intervals_.size(); }

        // writers
        std::ostream& operator<<(std::ostream& ost, const Domain_& d) {
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

        String_ Domain_::Write() const {
            std::ostringstream ost;
            ost << *this;
            return String_(ost.str());
        }

        // Arithmetics
        Domain_ Domain_::operator+(const Domain_& rhs) const {
            Domain_ res;
            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.AddInterval(i + j);
                }
            }
            return res;
        }

        Domain_ Domain_::operator-() const {
            Domain_ res;
            for (auto& i : intervals_) {
                res.AddInterval(-i);
            }
            return res;
        }

        Domain_ Domain_::operator-(const Domain_& rhs) const {
            Domain_ res;
            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.AddInterval(i - j);
                }
            }
            return res;
        }

        Domain_ Domain_::operator*(const Domain_& rhs) const {
            Domain_ res;
            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    size_t s = res.Size();
                    res.AddInterval(i * j);
                }
            }
            return res;
        }

        Domain_ Domain_::Inverse() const {
            Domain_ res;

            for (auto& i : intervals_) {
                res.AddInterval(i.Inverse());
            }
            return res;
        }
        Domain_ Domain_::operator/(const Domain_& rhs) const {
            Domain_ res;
            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.AddInterval(i / j);
                }
            }
            return res;
        }

        // shortcuts for shifting all intervals
        Domain_ Domain_::operator+=(const double x) {
            if (Fabs(x) < EPS)
                return *this;

            std::set<Interval_> newIntervals;
            for (auto& i : intervals_)
                newIntervals.insert(i + Interval_(x));
            intervals_ = std::move(newIntervals);
            return *this;
        }

        Domain_ Domain_::operator-=(const double x) {
            if (Fabs(x) < EPS)
                return *this;

            std::set<Interval_> newIntervals;
            for (auto& i : intervals_)
                newIntervals.insert(i - Interval_(x));
            intervals_ = std::move(newIntervals);

            return *this;
        }

        // min/max
        Domain_ Domain_::DMin(const Domain_& rhs) const {
            Domain_ res;
            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.AddInterval(i.IMin(j));
                }
            }
            return res;
        }

        Domain_ Domain_::DMax(const Domain_& rhs) const {
            Domain_ res;
            for (auto& i : intervals_) {
                for (auto& j : rhs.intervals_) {
                    res.AddInterval(i.IMax(j));
                }
            }
            return res;
        }

        // inclusion
        bool Domain_::IsInclude(double x) const {
            for (auto& interval : intervals_)
                if (interval.IsInclude(x))
                    return true;
            return false;
        }

        bool Domain_::IsInclude(const Interval_& rhs) const {
            for (auto& interval : intervals_)
                if (interval.IsInclude(rhs))
                    return true;
            return false;
        }

        // useful shortcuts for fuzzying
        bool Domain_::CanBeZero() const { return IsInclude(0.0); }

        bool Domain_::CanBeNonZero() const {
            if (IsEmpty() || (intervals_.size() == 1 && intervals_.begin()->IsZero()))
                return false;
            else
                return true;
        }

        bool Domain_::ZeroIsDiscrete() const {
            for (auto& interval : intervals_)
                if (interval.IsZero())
                    return true;
            return false;
        }

        bool Domain_::ZeroIsCont() const {
            for (auto& interval : intervals_)
                if (interval.IsContinuous() && interval.IsInclude(0.0))
                    return true;
            return false;
        }

        bool Domain_::CanBePositive(const bool strict) const {
            if (IsEmpty())
                return false;
            if (intervals_.rbegin()->Right().Value() > (strict ? EPS : -EPS))
                return true;
            return false;
        }

        bool Domain_::CanBeNegative(const bool strict) const {
            if (IsEmpty())
                return false;
            if (intervals_.begin()->Left().Value() < (strict ? -EPS : EPS))
                return true;
            return false;
        }

        // smallest positive left bound if any
        bool Domain_::SmallestPosLb(double& res, bool strict) const {
            if (intervals_.rbegin()->Left().IsNegative(!strict))
                return false;
            auto it = intervals_.begin();
            while (it->Left().IsNegative(!strict))
                ++it;
            res = it->Left().Value();
            return true;
        }

        // biggest negative right bound if any
        bool Domain_::BiggestNegRb(double& res, bool strict) const {
            if (intervals_.begin()->Right().IsPositive(!strict))
                return false;
            auto it = intervals_.rbegin();
            while (it->Right().IsPositive(!strict))
                ++it;
            res = it->Right().Value();
            return true;
        }
} // namespace Dal::Script
