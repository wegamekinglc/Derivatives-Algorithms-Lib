//
// Created by wegam on 2021/8/8.
//

#pragma once

#include <dal/time/date.hpp>
#include <dal/math/aad/products/base.hpp>
#include <dal/math/aad/operators.hpp>
#include <dal/storage/globals.hpp>
#include <sstream>

namespace Dal::AAD {
    template <class T_> class European_ : public Product_<T_> {
    private:
        double strike_;
        Date_ exerciseDate_;
        Date_ settlementDate_;

    public:
        European_(double strike, const Date_& exerciseDate, const Date_& settlementDate)
            : strike_(strike), exerciseDate_(exerciseDate), settlementDate_(settlementDate){
            const auto evaluationDate = Global::Dates_().EvaluationDate();
            Product_<T_>::labels_.Resize(1);
            Product_<T_>::timeLine_.push_back((exerciseDate - evaluationDate) / 365.0);
            Product_<T_>::defLine_.Resize(1);
            SampleDef_& sampleDef = Product_<T_>::defLine_.front();

            sampleDef.numeraire_ = true;
            sampleDef.forwardMats_.push_back({(settlementDate - evaluationDate) / 365.0});
            sampleDef.discountMats_.push_back((settlementDate - evaluationDate) / 365.0);

            //  Identify the product
            std::ostringstream ost;
            ost.precision(2);
            ost << std::fixed;
            if (settlementDate == exerciseDate) {
                ost << "call " << strike << " " << Date::ToString(exerciseDate);
            } else {
                ost << "call " << strike << " " << Date::ToString(exerciseDate) << " " << Date::ToString(settlementDate);
            }
            Product_<T_>::labels_[0] = String_(ost.str());
        }

        European_(double strike, const Date_& exerciseDate) : European_(strike, exerciseDate, exerciseDate) {}

        std::unique_ptr<Product_<T_>> Clone() const override { return std::make_unique<European_<T_>>(*this); }

        template <class C_>
        void PayoffsImplX(const Scenario_<T_>& path, C_ payoffs) const {
            const auto& sample = path[0];
            const auto spot = sample.forwards_[0][0];
            (*payoffs)[0] = Max(spot - strike_, 0.0) * sample.discounts_[0]/ sample.numeraire_;
        }

        inline void PayoffsImpl(const Scenario_<T_>& path, Vector_<T_>* payoffs) const override {
            PayoffsImplX<Vector_<T_>*>(path, payoffs);
        }

        inline void PayoffsImpl(const Scenario_<T_>& path, typename Matrix_<T_>::Row_* payoffs) const override {
            PayoffsImplX<typename Matrix_<T_>::Row_*>(path, payoffs);
        }
    };
} // namespace Dal