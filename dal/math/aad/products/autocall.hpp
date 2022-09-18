//
// Created by wegam on 2022/9/18.
//

#pragma once

#include <dal/math/aad/operators.hpp>
#include <dal/math/aad/products/base.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
#include <sstream>

namespace Dal::AAD {
    template <class T_> class AutoCall_ : public Product_<T_> {
        size_t numAssets_;
        Vector_<String_> assetNames_;
        Date_ maturity_;
        Time_ maturityTime_;
        int numPeriods_;
        Vector_<> refs_;

        double ko_;
        double strike_;
        double cpn_;
        double smooth_;

    public:
        //  Constructor: store data and build timeline
        AutoCall_(const Vector_<String_>& assets,
                  const Vector_<> refs,
                  const Date_& maturity,
                  int periods,
                  double ko,
                  double strike,
                  double cpn,
                  double smooth)
            : numAssets_(assets.size()), assetNames_(assets), refs_(refs), maturity_(maturity), numPeriods_(periods),
              ko_(ko), strike_(strike), cpn_(cpn), smooth_(Max(smooth, EPSILON)) {
            Product_<T_>::timeLine_.Resize(periods);
            Product_<T_>::defLine_.Resize(periods);
            Product_<T_>::labels_.Resize(1);

            Time_ time = 0.0;
            const auto evaluationDate = Global::Dates_().EvaluationDate();
            const Time_ maturity_time = (maturity_ - evaluationDate) / 365.0;
            maturityTime_ = maturity_time;
            const double dt = maturity_time / periods;

            for (size_t step = 0; step < periods; ++step) {
                time += dt;
                Product_<T_>::timeLine_[step] = time;
                Product_<T_>::defLine_[step].numeraire_ = true;
                Product_<T_>::defLine_[step].forwardMats_ =
                    Vector_<Vector_<Time_>>(numAssets_, Vector_<Time_>(1, time));
            }

            Product_<T_>::labels_[0] = "auto call strike " + ToString(int(100 * strike_ + EPSILON)) + " KO " +
                                       ToString(int(100 * ko_ + EPSILON)) + " CPN " +
                                       ToString(int(100 * cpn_ + EPSILON)) + " " + ToString(periods) + " periods of " +
                                       ToString(int(12 * maturity_time / periods + EPSILON)) + "m";
        }

        [[nodiscard]] size_t NumAssets() const override { return numAssets_; }
        [[nodiscard]] const Vector_<String_>& AssetNames() const override { return assetNames_; }
        [[nodiscard]] const Vector_<>& Refs() const { return refs_; }

        std::unique_ptr<Product_<T_>> Clone() const override { return std::make_unique<AutoCall_<T_>>(*this); }

        template <class C_>
        void PayoffsImplX(const Scenario_<T_>& path, Vector_<T_>* payoffs) const override {
            static thread_local Vector_<T_> perfs;
            perfs.Resize(numAssets_);

            const double dt = maturityTime_ / numPeriods_;
            double notionalAlive = 1.0;
            (*payoffs)[0] = 0.0;
            for (int step = 0; step < numPeriods_ - 1; ++step) {
                auto& state = path[step];
                Transform(
                    state.forwards,
                    refs_,
                    [](const Vector_<T_>& fwds, double ref) { return fwds[0] / ref; },
                    &perfs);
                T_ worst = *MinElement(perfs);
                (*payoffs)[0] += notionalAlive * cpn_ * dt / state.numeraire_;
                T_ notionalSurviving = notionalAlive * Min(1.0, Max(0.0, (ko_ + smooth_ - worst) / 2 / smooth_));
                T_ notionalDead = notionalAlive - notionalSurviving;
                (*payoffs)[0] += notionalDead / state.numeraire_;
                notionalAlive = notionalSurviving;
            }

            int step = numPeriods_ - 1;
            auto& state = path[step];
            Transform(
                state.forwards,
                refs_,
                [](const Vector_<T_>& fwds, double ref) { return fwds[0] / ref; },
                &perfs);
            T_ worst = *MinElement(perfs);
            (*payoffs)[0] += notionalAlive * cpn_ * dt / state.numeraire_;
            (*payoffs)[0] += notionalAlive / state.numeraire_;
            (*payoffs)[0] -= notionalAlive * Max(strike_ - worst, 0.0) / strike_ / state.numeraire;
        }

        inline void PayoffsImpl(const Scenario_<T_>& path, Vector_<T_>* payoffs) const override {
            PayoffsImplX<Vector_<T_>*>(path, payoffs);
        }

        inline void PayoffsImpl(const Scenario_<T_>& path, typename Matrix_<T_>::Row_* payoffs) const override {
            PayoffsImplX<typename Matrix_<T_>::Row_*>(path, payoffs);
        }
    };
} // namespace Dal::AAD
