//
// Created by wegam on 2022/10/29.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/random/base.hpp>
#include <dal/math/vectors.hpp>

namespace Dal {
    class BrownianBridgeTransform_ {
        size_t ndim_;
        Vector_<int> bridgeIndex_;
        Vector_<int> leftIndex_;
        Vector_<int> rightIndex_;
        Vector_<> leftWeight_;
        Vector_<> rightWeight_;
        Vector_<> stdDev_;
        Vector_<> t_;
        Vector_<> sqrtdt_;

        void Initialize();

    public:
        explicit BrownianBridgeTransform_(size_t nDim);
        void Apply(const Vector_<>& innerDeviates, Vector_<>* deviates) const;
        [[nodiscard]] size_t NDim() const { return ndim_; }
    };

    class BrownianBridge_ : public Random_ {
        std::unique_ptr<Random_> rsg_;
        BrownianBridgeTransform_ transform_;
        Vector_<> innerDeviates_;

    public:
        explicit BrownianBridge_(std::unique_ptr<Random_>&& rsg);

        void FillUniform(Vector_<>* deviates) override;
        void FillNormal(Vector_<>* deviates) override;

        void SkipTo(size_t nPoints) override { rsg_->SkipTo(nPoints); }

        [[nodiscard]] std::unique_ptr<Random_> Clone() const override { return std::make_unique<BrownianBridge_>(rsg_->Clone()); }

        [[nodiscard]] size_t NDim() const override { return transform_.NDim(); }
    };

    // Sobol coordinate factor * nSteps + bridgeCoordinate maps to output step * nFactors + factor.
    class FactorBrownianBridge_ : public Random_ {
        std::unique_ptr<Random_> rsg_;
        size_t nFactors_;
        BrownianBridgeTransform_ transform_;
        Vector_<> innerDeviates_;
        Vector_<> factorInput_;
        Vector_<> factorOutput_;

    public:
        FactorBrownianBridge_(std::unique_ptr<Random_>&& rsg, size_t nFactors);
        void FillUniform(Vector_<>* deviates) override;
        void FillNormal(Vector_<>* deviates) override;
        void SkipTo(size_t nPoints) override { rsg_->SkipTo(nPoints); }
        [[nodiscard]] std::unique_ptr<Random_> Clone() const override { return std::make_unique<FactorBrownianBridge_>(rsg_->Clone(), nFactors_); }
        [[nodiscard]] size_t NDim() const override { return rsg_->NDim(); }
    };
} // namespace Dal
