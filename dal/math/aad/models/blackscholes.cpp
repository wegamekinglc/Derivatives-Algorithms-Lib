//
// Created by wegamekinglc on 2021/8/7.
//

#include <cmath>
#include <dal/platform/strict.hpp>
#include <dal/math/aad/models/blackscholes.hpp>

namespace Dal {

    const Time_ systemTime = 0.0;

    template <class T_>
    void BlackScholes_<T_>::Allocate(const Vector_<Time_>& productTimeLine,
                                     const Vector_<SampleDef_>& defLine) {
        timeLine_.clear();
        timeLine_.push_back(systemTime);

        for (const auto& time : productTimeLine) {
            if (time > systemTime)
                timeLine_.push_back(time);
        }

        todayOnTimeLine_ = productTimeLine[0] == systemTime;
        defLine_ = &defLine;

        stds_.Resize(timeLine_.size() - 1);
        drifts_.Resize(timeLine_.size() - 1);

        const size_t n = productTimeLine.size();
        numeraires_.Resize(n);

        discounts_.Resize(n);
        forwardFactors_.Resize(n);
        libors_.Resize(n);
        for (size_t j = 0; j < n; ++j) {
            discounts_[j].Resize(defLine[j].discountMats_.size());
            forwardFactors_[j].Resize(defLine[j].forwardMats_.size());
            libors_[j].Resize(defLine[j].liborDefs_.size());
        }
    }

    template <class T_>
    void BlackScholes_<T_>::Init(const Vector_<Time_>& productTimeline,
                                 const Vector_<SampleDef_>& defLine) {
        const T_ mu = rate_ - div_;
        const size_t n = timeLine_.size() - 1;

        for(size_t i = 0; i < n; ++i) {
            const double dt = timeLine_[i + 1] - timeLine_[i];
            stds_[i] = vol_ * std::sqrt(dt);
        }
    }
}
