//
// Created by wegam on 2022/5/2.
//

#pragma once

#include <dal/math/interp/interp2d.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/math/aad/models/analytics.hpp>

namespace Dal::AAD {
    template <class T_>
    class RiskView_ {
        bool isEmpty_;
        Vector_<> strikes_;
        Vector_<> mats_;
        Matrix_<T_> spreads_;

    public:
        RiskView_() : isEmpty_(true){};
        RiskView_(const Vector_<>& strikes, const Vector_<>& mats)
            : isEmpty_(false), strikes_(strikes), mats_(mats), spreads_(strikes.size(), mats.size()) {
            for (auto& spr : spreads_)
                spr = T_(0.0);
        }

        T_ Spread(double strike, double mat) const {
            return isEmpty_ ? T_(0.0) : Interp2DLinearImplX(strikes_, mats_, spreads_, strike, mat);
        }

        [[nodiscard]] bool IsEmpty() const { return isEmpty_; }
        [[nodiscard]] size_t Rows() const { return strikes_.size(); }
        [[nodiscard]] size_t Cols() const { return mats_.size(); }
        [[nodiscard]] const Vector_<>& Strikes() const { return strikes_; }
        [[nodiscard]] const Vector_<>& Mats() const { return mats_; }
        const Matrix_<T_>& Risks() const { return spreads_; }

        using I_ = typename Matrix_<T_>::I_;
        using CI_ = typename Matrix_<T_>::CI_;

        I_ begin() { return spreads_.begin(); }
        CI_ begin() const { return spreads_.begin(); }
        I_ end() { return spreads_.end(); }
        CI_ end() const { return spreads_.end(); }

        void Bump(int i, int j, double bumpBy) { spreads_(i, j) += bumpBy; }
    };


    class IVS_ {
        double spot_;

    public:
        explicit IVS_(double spot) : spot_(spot) {}
        [[nodiscard]] double Spot() const { return spot_; }
        [[nodiscard]] virtual double ImpliedVol(double strike, double mat) const = 0;

        template <class T_ = double>
        T_ Call(double strike, double mat, const RiskView_<T_>* risk = nullptr) const {
            return BlackScholes<T_>(spot_, strike, ImpliedVol(strike, mat) + (risk ? risk->Spread(strike, mat) : T_(0.0)), mat);
        }

        template <class T_ = double>
        T_ LocalVol(double strike, double mat, const RiskView_<T_>* risk = nullptr) const {
            const T_ c00 = Call(strike, mat, risk);
            const auto dt = 1.e-4 * mat;
            const T_ c01 = Call(strike, mat - dt, risk);
            const T_ c02 = Call(strike, mat + dt, risk);
            const T_ ct = (c02 - c01) * 0.5 / dt;

            const auto ds = 1.0e-04 * strike;
            const T_ c10 = Call(strike - ds, mat, risk);
            const T_ c20 = Call(strike + ds, mat, risk);
            const T_ ckk = (c10 + c20 - 2.0 * c00) / ds / ds;
            return Dal::sqrt(2.0 * ct / ckk) / strike;
        }

        virtual ~IVS_() = default;
    };


    class MertonIVS_ : public IVS_ {
        double vol_;
        double intensity_;
        double averageJmp_;
        double jmpStd;

    public:
        MertonIVS_(double spot, double vol, double intens, double aveJmp, double stdJmp)
            : IVS_(spot), vol_(vol), intensity_(intens), averageJmp_(aveJmp), jmpStd(stdJmp) {}

        [[nodiscard]] double ImpliedVol(double strike, double mat) const override;
    };
} // namespace Dal::AAD