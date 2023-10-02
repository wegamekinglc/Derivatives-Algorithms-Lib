//
// Created by wegam on 2022/10/29.
//

#pragma once

#include <dal/math/random/base.hpp>
#include <dal/math/vectors.hpp>

namespace Dal {

    class BrownianBridge_ : public Random_ {
        std::unique_ptr<Random_> rsg_;
        size_t ndim_;
        Vector_<int> bridgeIndex_;
        Vector_<int> leftIndex_;
        Vector_<int> rightIndex_;
        Vector_<> leftWeight_;
        Vector_<> rightWeight_;
        Vector_<> stdDev_;
        Vector_<> t_;
        Vector_<> sqrtdt_;
        Vector_<> innerDeviates_;

        void Initialize();

    public:
        explicit BrownianBridge_(std::unique_ptr<Random_>&& rsg);

        void FillUniform(Vector_<>* deviates) override;
        void FillNormal(Vector_<>* deviates) override;

        void SkipTo(size_t n_points) override {
            rsg_->SkipTo(n_points);
        }

        [[nodiscard]] Random_* Clone() const override {
            Random_* rsg = rsg_->Clone();
            return new BrownianBridge_(std::move(std::unique_ptr<Random_>(rsg)));
        }

        [[nodiscard]] size_t NDim() const override {
            return ndim_;
        }
    };
}