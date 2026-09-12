//
// Created by wegam on 2023/3/26.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/piecewiselinear_internal.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {
    Vector_<> PiecewiseLinearInternal::Sofar(const Vector_<Date_>& knots, const Vector_<>& left, const Vector_<>& right) {
        Vector_<> retval(1, 0.0);
        for (int ii = 1; ii < knots.size(); ++ii) {
            const double dt = knots[ii] - knots[ii - 1];
            const double mean = (left[ii] + right[ii - 1]) / 2;
            retval.push_back(retval.back() + dt * mean);
        }
        return retval;
    }

    double PiecewiseLinearInternal::IntegralTo(
        const Vector_<Date_>& knots, const Vector_<>& left, const Vector_<>& right, const Vector_<>& sofar, const Date_& date) {
        const auto iGE = LowerBound(knots, date) - knots.begin();
        if (iGE <= 0)
            return -left.front() * (knots.front() - date);
        if (iGE == knots.size())
            return sofar.back() + right.back() * (date - knots.back()); // extrapolate flat
        if (knots[iGE] == date)
            return sofar[iGE];
        const auto iLT = iGE - 1;
        const double elapsed = date - knots[iLT];
        const double elapsedFrac = elapsed / (knots[iGE] - knots[iLT]);
        const double fStart = right[iLT];
        const double fStop = fStart + elapsedFrac * (left[iGE] - fStart);
        return sofar[iLT] + elapsed * (fStart + fStop) / 2;
    }

    Vector_<> PiecewiseLinear_::Sofar() const { return PiecewiseLinearInternal::Sofar(knotDates_, fLeft_, fRight_); }

    double PiecewiseLinear_::IntegralTo(const Date_& date) const {
        return PiecewiseLinearInternal::IntegralTo(knotDates_, fLeft_, fRight_, sofar_, date);
    }

    double PiecewiseLinear_::ValueAt(const Date_& date, bool from_right) const {
        const auto iGE = (from_right ? UpperBound(knotDates_, date) : LowerBound(knotDates_, date)) - knotDates_.begin();
        if (iGE <= 0)
            return fLeft_[0];
        if (iGE == knotDates_.size())
            return fRight_.back();
        const auto iLT = iGE - 1;
        const double elapsed = date - knotDates_[iLT];
        const double elapsedFrac = elapsed / (knotDates_[iGE] - knotDates_[iLT]);
        const double fStart = fRight_[iLT];
        return fStart + elapsedFrac * (fLeft_[iGE] - fStart);
    }

} // namespace Dal
