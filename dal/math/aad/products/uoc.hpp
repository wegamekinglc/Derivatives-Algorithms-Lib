//
// Created by wegam on 2022/9/18.
//

#pragma once

#include <dal/time/date.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/aad/products/base.hpp>
#include <dal/math/aad/operators.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/schedules.hpp>
#include <sstream>


namespace Dal::AAD {
    template <class T_ = double> class UOC_ : public Product_<T_> {
        bool callPut_;  // false = call, true = put
        double strike_;
        double barrier_;  // note = always up and out for now
        Date_ maturity_;
        double smooth_;

    public:
        UOC_(double strike, double barrier, const Schedule_& events, double smooth, bool callPut = false)
        : callPut_(callPut), strike_(strike), barrier_(barrier), maturity_(events.back()), smooth_(smooth) {
            const auto evaluationDate = Global::Dates_().EvaluationDate();
            Product_<T_>::labels_.Resize(1);
            Product_<T_>::timeLine_.push_back(0.0);
            for(const auto& date: events) {
                if (date > evaluationDate)
                    Product_<T_>::timeLine_.push_back((date - evaluationDate) / 365.0);
            }
            const size_t n = Product_<T_>::timeLine_.size();
            Product_<T_>::defLine_.Resize(n);
            for (size_t i = 0; i < n; ++i) {
                Product_<T_>::defLine_[i].numeraire_ = false;
                Product_<T_>::defLine_[i].forwardMats_.push_back({Product_<T_>::timeLine_[i]});
            }
            Product_<T_>::defLine_.back().numeraire_ = true;

            std::ostringstream ost;
            ost.precision(2);
            ost << std::fixed;
            ost << " up and out " << barrier_ << " smooth " << smooth_;
            Product_<T_>::labels_[0] = String_(ost.str());
        }

        std::unique_ptr<Product_<T_>> Clone() const override { return std::make_unique<UOC_<T_>>(*this); }

        template <class C_>
        void PayoffsImplX(const Scenario_<T_>& path, C_ payoffs) const {
            const double smooth = barrier_ * smooth_;
            const double twoSmooth = 2.0 * smooth;

            T_ alive(1.0);
            if (smooth >= Dal::EPSILON) {
                for (const auto& sample : path) {
                    const auto spot = sample.forwards_[0][0];
                    const auto dis_barrier = spot - barrier_;
                    alive *= Min(1.0, Max(0.0, 0.5 - dis_barrier / twoSmooth));
                }
            } else {
                for (const auto& sample : path) {
                    const auto spot = sample.forwards_[0][0];
                    if (spot >= barrier_) {
                        alive = 0.0;
                        break;
                    }
                }
            }
            const auto finalSpot = path.back().forwards_[0][0];
            if (callPut_)
                (*payoffs)[0] = alive * Max(strike_ - finalSpot, 0.0) / path.back().numeraire_;
            else
                (*payoffs)[0] = alive * Max(finalSpot - strike_, 0.0) / path.back().numeraire_;
        }

        inline void PayoffsImpl(const Scenario_<T_>& path, Vector_<T_>* payoffs) const override {
            PayoffsImplX<Vector_<T_>*>(path, payoffs);
        }

        inline void PayoffsImpl(const Scenario_<T_>& path, typename Matrix_<T_>::Row_* payoffs) const override {
            PayoffsImplX<typename Matrix_<T_>::Row_*>(path, payoffs);
        }
    };
} // namespace Dal::AAD