//
// Created by wegam on 2022/5/2.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <iterator>

#include <dal/math/matrix/matrixutils.hpp>
#include <dal/model/ivs.hpp>
#include <dal/model/surface/lvmodel.hpp>
#include <dal/model/utilities.hpp>
#include <dal/platform/platform.hpp>

namespace Dal::AAD {
    // Calibrate a local-volatility grid from an implied-volatility surface.
    // This file owns calibration only; simulation uses HybridLocalVolEquity_.
    template <class IT_, class OT_, class T_ = double>
    void DupireCalibMaturity(
        const IVS_& ivs, double maturity, IT_ spotsBegin, IT_ spotsEnd, OT_ lVolsBegin, const RiskView_<T_>& riskView = RiskView_<T_>()) {
        const size_t nSpots = static_cast<size_t>(std::distance(spotsBegin, spotsEnd));
        REQUIRE(nSpots > 0, "DupireCalib: spot grid must be nonempty");

        const double atmCall = static_cast<double>(ivs.Call(ivs.Spot(), maturity));
        const double width = 2.5 * atmCall * M_SQRT_2_PI;
        size_t low = 0;
        while (low < nSpots && spotsBegin[low] < ivs.Spot() - width)
            ++low;
        size_t high = nSpots;
        while (high > 0 && spotsBegin[high - 1] > ivs.Spot() + width)
            --high;
        REQUIRE(low < high, "DupireCalib: no spot nodes inside stable calibration band");

        for (size_t i = low; i < high; ++i)
            lVolsBegin[i] = ivs.LocalVol(spotsBegin[i], maturity, &riskView);
        for (size_t i = 0; i < low; ++i)
            lVolsBegin[i] = lVolsBegin[low];
        for (size_t i = high; i < nSpots; ++i)
            lVolsBegin[i] = lVolsBegin[high - 1];
    }

    template <class T_ = double>
    auto DupireCalib(const IVS_& ivs,
                     const Vector_<>& inclSpots,
                     double maxDs,
                     const Vector_<>& inclTimes,
                     double maxDt,
                     const RiskView_<T_>& riskView = RiskView_<T_>()) {
        struct {
            Vector_<> spots_;
            Vector_<> times_;
            Matrix_<T_> lVols_;
        } results;

        REQUIRE(!inclSpots.empty() && std::is_sorted(inclSpots.begin(), inclSpots.end()), "DupireCalib: inclusion spots must be nonempty and sorted");
        REQUIRE(!inclTimes.empty() && std::is_sorted(inclTimes.begin(), inclTimes.end()), "DupireCalib: inclusion times must be nonempty and sorted");

        // The one-hour floor avoids a degenerate maturity grid near zero.
        constexpr double ONE_HOUR_YF = 0.000114469;
        results.spots_ = FillData(inclSpots, maxDs, 0.01);
        results.times_ = FillData(inclTimes, maxDt, ONE_HOUR_YF, &maxDt, &maxDt + 1);
        Matrix_<T_> timeMajor(results.times_.size(), results.spots_.size());
        for (size_t j = 0; j < results.times_.size(); ++j)
            DupireCalibMaturity(ivs, results.times_[j], results.spots_.begin(), results.spots_.end(), timeMajor[j], riskView);
        results.lVols_ = Matrix::MakeTranspose(timeMajor);
        return results;
    }
} // namespace Dal::AAD

namespace Dal {
    // Snapshot a calibrated surface; it can be archived and attached to Hybrid.
    Handle_<LocalVolSurfaceData_> CalibrateDupireLocalVolSurface(const String_& name,
                                                                 const AAD::IVS_& ivs,
                                                                 const Vector_<>& inclusionSpots,
                                                                 double maxSpotSpacing,
                                                                 const Vector_<>& inclusionTimes,
                                                                 double maxTimeSpacing);
} // namespace Dal
