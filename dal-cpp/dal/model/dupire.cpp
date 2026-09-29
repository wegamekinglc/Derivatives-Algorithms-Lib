//
// Created by wegam on 2022/12/4.
//

#include <dal/model/dupire.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {
    Handle_<LocalVolSurfaceData_> CalibrateDupireLocalVolSurface(const String_& name,
                                                                 const AAD::IVS_& ivs,
                                                                 const Vector_<>& inclusionSpots,
                                                                 double maxSpotSpacing,
                                                                 const Vector_<>& inclusionTimes,
                                                                 double maxTimeSpacing) {
        const auto calibrated = AAD::DupireCalib<double>(ivs, inclusionSpots, maxSpotSpacing, inclusionTimes, maxTimeSpacing);
        return Handle_<LocalVolSurfaceData_>(new LocalVolSurfaceData_(name, calibrated.spots_, calibrated.times_, calibrated.lVols_));
    }
} // namespace Dal
