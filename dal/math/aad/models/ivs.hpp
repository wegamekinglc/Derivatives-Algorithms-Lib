//
// Created by wegam on 2022/5/2.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/interp/interp2d.hpp>


namespace Dal::AAD {
    template <class T_>
    class RiskView_ {
        bool isEmpty_;
        Vector_<> strikes_;
        Vector_<Time_> mats_;
        Matrix_<T_> spreads_;

    public:
        RiskView_(): isEmpty_(true) {};
        RiskView_(const Vector_<>& strikes, const Vector_<Time_>& mats)
        : isEmpty_(false), strikes_(strikes), mats_(mats), spreads_(strikes.size(), mats.size()) {
            for (auto& spr: spreads_) spr = T_(0.0);
        }

        T_ Spread(double strike, Time_ mat) const {
            return isEmpty_ ? T_(0.0) : Interp2DLinearImplX(strikes_, mats_, spreads_, strike, mat);
        }

        bool IsEmpty() const { return isEmpty_; }
        size_t Rows() const { return strikes_.size(); }
        size_t Cols() const { return mats_.size(); }
        const Vector_<>& Strikes() const { return strikes_; }
        const Vector_<Time_>& Mats() const { return mats_; }
        const Matrix_<T_>& Risks() const { return spreads_; }

        using I_ = typename Matrix_<T_>::I_;
        using CI_ = typename Matrix_<T_>::CI_;

        I_ begin() { return spreads_.begin(); }
        CI_ begin() const { return spreads_.begin(); }
        I_ end() { return spreads_.end(); }
        CI_ end() const { return spreads_.end(); }

        void Bump(int i, int j, double bumpBy) {
            spreads_(i, j) += bumpBy;
        }
    };
}