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
    namespace {
        size_t FactorSteps(const std::unique_ptr<Random_>& rsg, size_t nFactors) {
            REQUIRE(rsg && nFactors > 0 && rsg->NDim() > 0 && rsg->NDim() % nFactors == 0,
                    "InvalidBrownianBridge: dimension must be a positive multiple of the factor count");
            return rsg->NDim() / nFactors;
        }
    } // namespace

    BrownianBridgeTransform_::BrownianBridgeTransform_(size_t nDim)
        : ndim_(nDim), bridgeIndex_(ndim_), leftIndex_(ndim_), rightIndex_(ndim_), leftWeight_(ndim_), rightWeight_(ndim_), stdDev_(ndim_), t_(ndim_),
          sqrtdt_(ndim_) {
        REQUIRE(ndim_ > 0, "InvalidBrownianBridge: dimension must be positive");
        for (int i = 0; i < ndim_; ++i)
            t_[i] = static_cast<double>(i + 1);
        Initialize();
    }

    void BrownianBridgeTransform_::Initialize() {
        sqrtdt_[0] = std::sqrt(t_[0]);
        for (int i = 1; i < ndim_; ++i)
            sqrtdt_[i] = std::sqrt(t_[i] - t_[i - 1]);

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
                j = 0; //  wrap around
        }
    }

    void BrownianBridgeTransform_::Apply(const Vector_<>& innerDeviates, Vector_<>* deviates) const {
        REQUIRE(innerDeviates.size() == ndim_, "InvalidBrownianBridge: input dimension mismatch");
        deviates->Resize(ndim_);
        (*deviates)[ndim_ - 1] = stdDev_[0] * innerDeviates[0];
        for (int i = 1; i < ndim_; ++i) {
            int j = leftIndex_[i];
            int k = rightIndex_[i];
            int l = bridgeIndex_[i];
            if (j != 0)
                (*deviates)[l] = leftWeight_[i] * (*deviates)[j - 1] + rightWeight_[i] * (*deviates)[k] + stdDev_[i] * innerDeviates[i];
            else
                (*deviates)[l] = rightWeight_[i] * (*deviates)[k] + stdDev_[i] * innerDeviates[i];
        }
        for (int i = ndim_ - 1; i >= 1; --i) {
            (*deviates)[i] -= (*deviates)[i - 1];
            (*deviates)[i] /= sqrtdt_[i];
        }
        (*deviates)[0] /= sqrtdt_[0];
    }

    BrownianBridge_::BrownianBridge_(std::unique_ptr<Random_>&& rsg) : rsg_(std::move(rsg)), transform_(rsg_->NDim()), innerDeviates_(rsg_->NDim()) {}

    void BrownianBridge_::FillUniform(Vector_<>* deviates) {
        FillNormal(deviates);
        static auto func = [](double x) { return NCDF(x); };
        Transform(*deviates, func, deviates);
    }

    void BrownianBridge_::FillNormal(Vector_<>* deviates) {
        rsg_->FillNormal(&innerDeviates_);
        transform_.Apply(innerDeviates_, deviates);
    }

    FactorBrownianBridge_::FactorBrownianBridge_(std::unique_ptr<Random_>&& rsg, size_t nFactors)
        : rsg_(std::move(rsg)), nFactors_(nFactors), transform_(FactorSteps(rsg_, nFactors_)), innerDeviates_(rsg_->NDim()),
          factorInput_(transform_.NDim()), factorOutput_(transform_.NDim()) {}

    void FactorBrownianBridge_::FillUniform(Vector_<>* deviates) {
        FillNormal(deviates);
        static auto func = [](double x) { return NCDF(x); };
        Transform(*deviates, func, deviates);
    }

    void FactorBrownianBridge_::FillNormal(Vector_<>* deviates) {
        rsg_->FillNormal(&innerDeviates_);
        deviates->Resize(NDim());
        const size_t nSteps = transform_.NDim();
        for (size_t factor = 0; factor < nFactors_; ++factor) {
            for (size_t coordinate = 0; coordinate < nSteps; ++coordinate)
                factorInput_[coordinate] = innerDeviates_[factor * nSteps + coordinate];
            transform_.Apply(factorInput_, &factorOutput_);
            for (size_t step = 0; step < nSteps; ++step)
                (*deviates)[step * nFactors_ + factor] = factorOutput_[step];
        }
    }
} // namespace Dal
