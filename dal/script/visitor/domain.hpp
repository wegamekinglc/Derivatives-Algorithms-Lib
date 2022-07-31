//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <iostream>
#include <set>


namespace Dal::Script {

    static constexpr double BIG = 1.0e+12;
    static constexpr double EPS = 1.0e-12;

    class Bound_ {
        bool plusInf_;
        bool minusInf_;
        double real_;

    public:
        struct PlusInfinity_ {};
        struct MinusInfinity_ {};
        static constexpr PlusInfinity_ plusInfinity_ = {};
        static constexpr MinusInfinity_ minusInfinity_ = {};

        Bound_(double Value = 0.0) : plusInf_(false), minusInf_(false), real_(Value) {}
        Bound_(const PlusInfinity_&) : plusInf_(true), minusInf_(false), real_(BIG) {}
        Bound_(const MinusInfinity_&) : plusInf_(false), minusInf_(true), real_(-BIG) {}
        Bound_(const Bound_& rhs) = default;

        Bound_& operator=(double Value);
        Bound_& operator=(const PlusInfinity_&);
        Bound_& operator=(const MinusInfinity_&);
        Bound_& operator=(const Bound_& rhs);

        // accessors
        [[nodiscard]] bool IsInf() const;
        [[nodiscard]] bool IsPositive(bool strict = false) const;
        [[nodiscard]] bool IsNegative(bool strict = false) const;
        [[nodiscard]] bool IsZero() const;
        [[nodiscard]] bool IsPlusInf() const;
        [[nodiscard]] bool IsMinusInf() const;
        [[nodiscard]] double Value() const;

        // comparison
        bool operator==(const Bound_& rhs) const;
        bool operator!=(const Bound_& rhs) const;
        bool operator<(const Bound_& rhs) const;
        bool operator>(const Bound_& rhs) const;
        bool operator<=(const Bound_& rhs) const;
        bool operator>=(const Bound_& rhs) const;

        // writers
        friend std::ostream& operator<<(std::ostream& ost, const Bound_& bnd);
        [[nodiscard]] String_ Write() const;

        // multiplication
        Bound_ operator*(const Bound_& rhs) const;

        // negation
        Bound_ operator-() const;
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

        // accessors
        [[nodiscard]] Bound_ Left() const;
        [[nodiscard]] Bound_ Right() const;
        [[nodiscard]] bool IsPositive(bool strict = false) const;
        [[nodiscard]] bool IsNegative(bool strict = false) const;
        [[nodiscard]] bool IsPosOrNeg(bool strict = false) const;
        [[nodiscard]] bool IsInf() const;
        [[nodiscard]] bool IsSingleton(double* val = nullptr) const;
        [[nodiscard]] bool IsZero() const;
        [[nodiscard]] bool IsContinuous() const;

        // writers
        friend std::ostream& operator<<(std::ostream& ost, const Interval_& i);
        String_ Write() const;

        // sorting
        bool operator==(const Interval_& rhs) const;
        bool operator<(const Interval_& rhs) const;
        bool operator>(const Interval_& rhs) const;
        bool operator<=(const Interval_& rhs) const;
        bool operator>=(const Interval_& rhs) const;

        // arithmetics
        // addition
        Interval_ operator+(const Interval_& rhs) const;
        Interval_& operator+=(const Interval_& rhs);

        // unary minus
        Interval_ operator-() const;

        // subtraction
        Interval_ operator-(const Interval_& rhs) const;
        Interval_& operator-=(const Interval_& rhs);

        // multiplication
        Interval_ operator*(const Interval_& rhs) const;

        // inverse (1/x)
        [[nodiscard]] Interval_ Inverse() const;

        // division
        Interval_ operator/(const Interval_& rhs) const;

        // min/max
        [[nodiscard]] Interval_ IMin(const Interval_& rhs) const;
        [[nodiscard]] Interval_ IMax(const Interval_& rhs) const;

        // apply function
        template <class Func> Interval_ ApplyFunc(Func func, const Interval_& funcDomain) {
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

        // apply function 2 params
        template <class Func>
        Interval_ ApplyFunc2(Func func, const Interval_& rhs, const Interval_& funcDomain) {
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

        // inclusion
        [[nodiscard]] bool IsInclude(double x) const;
        [[nodiscard]] bool IsInclude(const Interval_& rhs) const;
        [[nodiscard]] bool IsIncluded(const Interval_& rhs) const;

        // adjacence
        // 0: is not adjacent
        // 1: *this is adjacent to rhs on the left of rhs
        // 2: *this is adjacent to rhs on the right of rhs
        [[nodiscard]] unsigned IsAdjacent(const Interval_& rhs) const;

        // intersection, returns false if no intersect, true otherwise
        // in which case iSect is set to the intersection unless nullptr
        friend bool Intersect(const Interval_& lhs, const Interval_& rhs, Interval_* iSect);

        // merge, returns false if no intersect, true otherwise
        // in which case iMerge is set to the merged interval unless nullptr
        friend bool Merge(const Interval_& lhs, const Interval_& rhs, Interval_* iMerge);

        // another merge function that merges rhs into this, assuming we already know that they intersect
        void Merge(const Interval_& rhs);
    };

    class Domain_ {
        std::set<Interval_> intervals_;

    public:
        Domain_() = default;
        Domain_(const Domain_& rhs) = default;
        Domain_(Domain_&& rhs) noexcept: intervals_(std::move(rhs.intervals_)) {}

        Domain_& operator=(const Domain_& rhs);
        Domain_& operator=(Domain_&& rhs) noexcept;

        Domain_(double val) { AddSingleton(val); }
        Domain_(const Interval_& i) { AddInterval(i); }

        void AddInterval(Interval_ interval);
        void AddDomain(const Domain_& rhs);
        void AddSingleton(double val);

        // accessors
        [[nodiscard]] bool IsPositive(bool strict = false) const;
        [[nodiscard]] bool IsNegative(bool strict = false) const;
        [[nodiscard]] bool IsPosOrNeg(bool strict = false) const;
        [[nodiscard]] bool IsInf() const;
        [[nodiscard]] bool IsDiscrete() const;

        // discrete only is true: return empty if continuous intervals found, false: return all singletons anyway
        [[nodiscard]] Vector_<> GetSingletons(bool discreteOnly = true) const;

        // at least one continuous interval
        [[nodiscard]] bool IsContinuous() const;

        // shortcut for 2 singletons
        [[nodiscard]] bool IsBoolean(std::pair<double, double>* vals = nullptr) const;

        // shortcut for 1 singleton
        [[nodiscard]] bool IsConstant(double* val = nullptr) const;

        // get all continuous intervals, dropping singletons
        [[nodiscard]] Domain_ GetContinuous() const;

        // get min and max bounds
        [[nodiscard]] Bound_ MinBound() const;
        [[nodiscard]] Bound_ MaxBound() const;
        [[nodiscard]] bool IsEmpty() const;
        [[nodiscard]] size_t Size() const;

        // writers
        friend std::ostream& operator<<(std::ostream& ost, const Domain_& d);
        [[nodiscard]] String_ Write() const;

        // arithmetics
        Domain_ operator+(const Domain_& rhs) const;
        Domain_ operator-() const;
        Domain_ operator-(const Domain_& rhs) const;
        Domain_ operator*(const Domain_& rhs) const;
        [[nodiscard]] Domain_ Inverse() const;
        Domain_ operator/(const Domain_& rhs) const;

        // shortcuts for shifting all intervals
        Domain_ operator+=(double x);
        Domain_ operator-=(double x);

        // min/max
        [[nodiscard]] Domain_ DMin(const Domain_& rhs) const;
        [[nodiscard]] Domain_ DMax(const Domain_& rhs) const;

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
        [[nodiscard]] bool IsInclude(double x) const;
        [[nodiscard]] bool IsInclude(const Interval_& rhs) const;

        // useful shortcuts for fuzzying
        [[nodiscard]] bool CanBeZero() const;
        [[nodiscard]] bool CanBeNonZero() const;
        [[nodiscard]] bool ZeroIsDiscrete() const;
        [[nodiscard]] bool ZeroIsCont() const;
        [[nodiscard]] bool CanBePositive(bool strict) const;
        [[nodiscard]] bool CanBeNegative(bool strict) const;

        // smallest positive left bound if any
        bool SmallestPosLb(double& res, bool strict = false) const;

        // biggest negative right bound if any
        bool BiggestNegRb(double& res, bool strict = false) const;
    };
} // namespace Dal::Script