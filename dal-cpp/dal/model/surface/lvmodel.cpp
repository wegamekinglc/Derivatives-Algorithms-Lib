//
// Created by Codex on 2026/9/30.
//

#include <dal/model/surface/lvmodel.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
    namespace {
        void ValidateDimensions(const LocalVolSurfaceData_& data) {
            REQUIRE(!data.spots_.empty() && !data.times_.empty() && data.vols_.Rows() == static_cast<int>(data.spots_.size()) &&
                        data.vols_.Cols() == static_cast<int>(data.times_.size()),
                    "InvalidLocalVolSurface: grid dimensions must match nonempty spot and time axes");
        }

        void ValidateSpotAxis(const Vector_<>& spots) {
            for (size_t i = 0; i < spots.size(); ++i)
                REQUIRE(std::isfinite(spots[i]) && spots[i] > 0.0 && (i == 0 || spots[i] > spots[i - 1]),
                        "InvalidLocalVolSurface: spots must be finite, positive, and strictly increasing");
        }

        void ValidateTimeAxis(const Vector_<>& times) {
            for (size_t i = 0; i < times.size(); ++i)
                REQUIRE(std::isfinite(times[i]) && times[i] >= 0.0 && (i == 0 || times[i] > times[i - 1]),
                        "InvalidLocalVolSurface: times must be finite, nonnegative, and strictly increasing");
        }

        void ValidateVolatilities(const Matrix_<>& vols) {
            for (int i = 0; i < vols.Rows(); ++i)
                for (int j = 0; j < vols.Cols(); ++j)
                    REQUIRE(std::isfinite(vols(i, j)) && vols(i, j) >= 0.0, "InvalidLocalVolSurface: volatilities must be finite and nonnegative");
        }
    } // namespace

#include <dal/auto/MG_LocalVolSurfaceData_v1_Read.inc>
#include <dal/auto/MG_LocalVolSurfaceData_v1_Write.inc>

    LocalVolSurfaceData_::LocalVolSurfaceData_(const String_& name, const Vector_<>& spots, const Vector_<>& times, const Matrix_<>& vols)
        : Storable_("LocalVolSurfaceData", name), spots_(spots), times_(times), vols_(vols) {
        ValidateDimensions(*this);
        ValidateSpotAxis(spots_);
        ValidateTimeAxis(times_);
        ValidateVolatilities(vols_);
    }

    void LocalVolSurfaceData_::Write(Archive::Store_& dst) const { LocalVolSurfaceData_v1::XWrite(dst, name_, spots_, times_, vols_); }
} // namespace Dal
