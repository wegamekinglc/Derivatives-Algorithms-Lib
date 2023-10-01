//
// Created by wegam on 2023/3/26.
//


#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {
    Vector_<> PiecewiseLinear_::Sofar() const {
        Vector_<> retval(1, 0.0);
        for (int ii = 1; ii < knotDates_.size(); ++ii) {
            const double dt = knotDates_[ii] - knotDates_[ii - 1];
            const double mean = (fLeft_[ii] + fRight_[ii - 1]) / 2;
            retval.push_back(retval.back() + dt * mean);
        }
        return retval;
    }

    double PiecewiseLinear_::IntegralTo(const Date_ &date) const {
        const auto iGE = LowerBound(knotDates_, date) - knotDates_.begin();
        if (iGE <= 0)
            return -fLeft_.front() * (knotDates_.front() - date);
        if (iGE == knotDates_.size())
            return sofar_.back() + fRight_.back() * (date - knotDates_.back());    // extrapolate flat
        if (knotDates_[iGE] == date)
            return sofar_[iGE];
        const auto iLT = iGE - 1;
        const double elapsed = date - knotDates_[iLT];
        const double elapsedFrac = elapsed / (knotDates_[iGE] - knotDates_[iLT]);
        const double fStart = fRight_[iLT];
        const double fStop = fStart + elapsedFrac * (fLeft_[iGE] - fStart);
        return sofar_[iLT] + elapsed * (fStart + fStop) / 2;
    }

    double PiecewiseLinear_::ValueAt(const Date_ &date, bool from_right) const {
        const auto iGE =
                (from_right ? UpperBound(knotDates_, date) : LowerBound(knotDates_, date)) - knotDates_.begin();
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

}