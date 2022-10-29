//
// Created by wegam on 2022/10/29.
//

#pragma once

#include <dal/math/random/base.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {

    class BrownianBridge_ : public Random_ {
        std::unique_ptr<Random_> rsg_;

    public:
        BrownianBridge_(std::unique_ptr<Random_>&& rsg): rsg_(std::move(rsg)) {}
        void FillUniform(Vector_<>* deviates) {
            rsg_->FillUniform(deviates);
        }

        void FillNormal(Vector_<>* deviates) {
            rsg_->FillNormal(deviates);
        }

        void SkipTo(size_t n_points) {
            rsg_->SkipTo(n_points);
        }

        Random_* Clone() const {
            Random_* rsg = rsg_->Clone();
            return new BrownianBridge_(std::unique_ptr<Random_>(rsg));
        }

        size_t NDim() const {
            return rsg_->NDim();
        }
    };
}