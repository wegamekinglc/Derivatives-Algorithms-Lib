//
// Created by wegam on 2023/3/26.
//

#pragma once

#include <algorithm>
#include <cmath>

#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/vectors.hpp>
#include <dal/storage/archive.hpp>

/*IF--------------------------------------------------------------------------
storable LocalVolSurfaceData
    Local volatility by spot and model time, with flat boundary extrapolation
version 1
&members
name is ?string
spots is number[]
times is number[]
vols is number[][]
-IF-------------------------------------------------------------------------*/

namespace Dal {
    struct LocalVolSurfaceData_ : Storable_ {
        Vector_<> spots_;
        Vector_<> times_;
        Matrix_<> vols_;

        LocalVolSurfaceData_(const String_& name, const Vector_<>& spots, const Vector_<>& times, const Matrix_<>& vols);
        void Write(Archive::Store_& dst) const override;
    };

    using LVComponent_ = LocalVolSurfaceData_;

    namespace AAD {
        template <class T_> class LocalVolSurface_ {
            Vector_<> logSpots_;
            Vector_<> times_;
            Matrix_<T_> vols_;
            Vector_<Vector_<T_>> stepVols_;
            Vector_<T_*> parameters_;

            void SetParameterPointers() {
                parameters_.clear();
                for (int i = 0; i < vols_.Rows(); ++i)
                    for (int j = 0; j < vols_.Cols(); ++j)
                        parameters_.push_back(&vols_(i, j));
            }

            [[nodiscard]] T_ TimeInterpolated(size_t row, double time) const {
                if (times_.size() == 1 || time <= times_.front())
                    return vols_(static_cast<int>(row), 0);
                if (time >= times_.back())
                    return vols_(static_cast<int>(row), vols_.Cols() - 1);
                const auto upper = std::upper_bound(times_.begin(), times_.end(), time);
                const auto hi = static_cast<int>(upper - times_.begin());
                const double weight = (time - times_[hi - 1]) / (times_[hi] - times_[hi - 1]);
                return (1.0 - weight) * vols_(static_cast<int>(row), hi - 1) + weight * vols_(static_cast<int>(row), hi);
            }

            template <class F_> [[nodiscard]] T_ SpotInterpolated(const T_& logSpot, const F_& volAtRow) const {
                if (logSpots_.size() == 1 || Value(logSpot) <= logSpots_.front())
                    return volAtRow(0);
                if (Value(logSpot) >= logSpots_.back())
                    return volAtRow(logSpots_.size() - 1);
                const auto upper = std::upper_bound(logSpots_.begin(), logSpots_.end(), Value(logSpot));
                const auto hi = static_cast<size_t>(upper - logSpots_.begin());
                const T_ weight = (logSpot - logSpots_[hi - 1]) / (logSpots_[hi] - logSpots_[hi - 1]);
                return (1.0 - weight) * volAtRow(hi - 1) + weight * volAtRow(hi);
            }

        public:
            explicit LocalVolSurface_(const LocalVolSurfaceData_& data)
                : logSpots_(data.spots_.size()), times_(data.times_), vols_(data.vols_.Rows(), data.vols_.Cols()) {
                for (size_t i = 0; i < logSpots_.size(); ++i)
                    logSpots_[i] = std::log(data.spots_[i]);
                for (int i = 0; i < vols_.Rows(); ++i)
                    for (int j = 0; j < vols_.Cols(); ++j)
                        vols_(i, j) = T_(data.vols_(i, j));
                SetParameterPointers();
            }
            LocalVolSurface_(const LocalVolSurface_& other)
                : logSpots_(other.logSpots_), times_(other.times_), vols_(other.vols_), stepVols_(other.stepVols_) {
                SetParameterPointers();
            }
            LocalVolSurface_& operator=(const LocalVolSurface_& other) {
                if (this != &other) {
                    logSpots_ = other.logSpots_;
                    times_ = other.times_;
                    vols_ = other.vols_;
                    stepVols_ = other.stepVols_;
                    SetParameterPointers();
                }
                return *this;
            }
            [[nodiscard]] T_ Vol(double time, const T_& spot) const {
                REQUIRE(std::isfinite(time) && std::isfinite(Value(spot)) && Value(spot) > 0.0,
                        "InvalidLocalVolSurface: query requires finite time and positive spot");
                const T_ logSpot = Dal::log(spot);
                return SpotInterpolated(logSpot, [&](size_t row) { return TimeInterpolated(row, time); });
            }
            void Prepare(const Vector_<>& stepTimes) {
                for (const double time : stepTimes)
                    REQUIRE(std::isfinite(time), "InvalidLocalVolSurface: preparation requires finite step times");
                stepVols_.Resize(stepTimes.size());
                for (size_t step = 0; step < stepTimes.size(); ++step) {
                    auto& column = stepVols_[step];
                    column.Resize(logSpots_.size());
                    for (size_t row = 0; row < column.size(); ++row)
                        column[row] = TimeInterpolated(row, stepTimes[step]);
                }
            }
            [[nodiscard]] T_ VolAtLogSpot(size_t step, const T_& logSpot) const {
                REQUIRE(step < stepVols_.size(), "InvalidLocalVolSurface: step is outside the prepared times");
                REQUIRE(std::isfinite(Value(logSpot)), "InvalidLocalVolSurface: query requires finite log spot");
                return SpotInterpolated(logSpot, [&](size_t row) { return stepVols_[step][row]; });
            }
            [[nodiscard]] const Vector_<T_*>& Parameters() const { return parameters_; }
        };
    } // namespace AAD
} // namespace Dal
