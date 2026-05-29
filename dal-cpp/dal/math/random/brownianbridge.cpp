//
// Created by wegam on 2022/10/29.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/random/brownianbridge.hpp>
#include <dal/math/operators.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {

    BrownianBridge_::BrownianBridge_(std::unique_ptr<Random_>&& rsg)
            : rsg_(std::move(rsg)), ndim_(rsg_->NDim()),
              bridgeIndex_(ndim_), leftIndex_(ndim_), rightIndex_(ndim_),
              leftWeight_(ndim_), rightWeight_(ndim_), stdDev_(ndim_), t_(ndim_), sqrtdt_(ndim_), innerDeviates_(ndim_) {
        for (int i = 0; i < ndim_; ++i)
            t_[i] = static_cast<double>(i + 1);
        Initialize();
    }

    void BrownianBridge_::Initialize() {
        sqrtdt_[0] = std::sqrt(t_[0]);
        for (int i = 1; i < ndim_; ++i)
            sqrtdt_[i] = std::sqrt(t_[i] - t_[i-1]);

        Vector_<int> map(ndim_, 0);
        map[ndim_ - 1] = 1;
        bridgeIndex_[0] = ndim_ - 1;
        stdDev_[0] = std::sqrt(t_[ndim_ - 1]);
        leftWeight_[0] = rightWeight_[0] = 0.0;
        for (int j = 0, i = 1; i < ndim_; ++i) {
            while (map[j] != 0U)
                ++j;
            int k = j;
            while (map[k] == 0U)
                ++k;
            int l = j + ((k - 1 - j) >> 1);
            map[l] = i;
            bridgeIndex_[i] = l;
            leftIndex_[i] = j;
            rightIndex_[i] = k;
            if (j != 0) {
                leftWeight_[i] = (t_[k] - t_[l]) / (t_[k] - t_[j - 1]);
                rightWeight_[i] = (t_[l] - t_[j - 1]) / (t_[k] - t_[j - 1]);
                stdDev_[i] = std::sqrt(((t_[l] - t_[j - 1]) * (t_[k] - t_[l])) / (t_[k] - t_[j - 1]));
            } else {
                leftWeight_[i] = (t_[k] - t_[l]) / t_[k];
                rightWeight_[i] = t_[l] / t_[k];
                stdDev_[i] = std::sqrt(t_[l] * (t_[k] - t_[l]) / t_[k]);
            }
            j = k + 1;
            if (j >= ndim_)
                j = 0;    //  wrap around
        }
    }

    void BrownianBridge_::FillUniform(Vector_<> *deviates) {
        FillNormal(deviates);
        static auto func = [](double x) { return NCDF(x); };
        Transform(*deviates, func, deviates);
    }

    void BrownianBridge_::FillNormal(Vector_<> *deviates) {
        deviates->Resize(NDim());
        rsg_->FillNormal(&innerDeviates_);
        (*deviates)[ndim_ - 1] = stdDev_[0] * innerDeviates_[0];
        for (int i = 1; i< ndim_; ++i) {
            int j = leftIndex_[i];
            int k = rightIndex_[i];
            int l = bridgeIndex_[i];
            if (j != 0)
                (*deviates)[l] = leftWeight_[i] * (*deviates)[j-1] + rightWeight_[i] * (*deviates)[k] + stdDev_[i] * innerDeviates_[i];
            else
                (*deviates)[l] = rightWeight_[i] * (*deviates)[k] + stdDev_[i] * innerDeviates_[i];
        }
        // ...after which, we calculate the variations and
        // normalize to unit times
        for (int i = ndim_ - 1; i >= 1; --i) {
            (*deviates)[i] -= (*deviates)[i-1];
            (*deviates)[i] /= sqrtdt_[i];
        }
        (*deviates)[0] /= sqrtdt_[0];
    }
}