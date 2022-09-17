//
// Created by wegam on 2021/8/8.
//

#pragma once

#include <dal/time/date.hpp>
#include <dal/math/aad/products/base.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/globals.hpp>
#include <sstream>

namespace Dal::AAD {
    template <class T_> class European_ : public Product_<T_> {
    private:
        double strike_;
        Date_ exerciseDate_;
        Date_ settlementDate_;
        Vector_<Time_> timeLine_;
        Vector_<SampleDef_> defLine_;
        Vector_<String_> labels_;

    public:
        European_(double strike, const Date_& exerciseDate, const Date_& settlementDate)
            : strike_(strike), exerciseDate_(exerciseDate), settlementDate_(settlementDate), labels_(1) {
            const auto evaluationDate = Global::Dates_().EvaluationDate();
            timeLine_.push_back((exerciseDate - evaluationDate) / 365.0);
            defLine_.Resize(1);
            SampleDef_& sampleDef = defLine_.front();

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
            labels_[0] = String_(ost.str());
        }

        European_(double strike, const Date_& exerciseDate) : European_(strike, exerciseDate, exerciseDate) {}

        std::unique_ptr<Product_<T_>> Clone() const override { return std::make_unique<European_<T_>>(*this); }

        [[nodiscard]] const Vector_<Time_>& TimeLine() const override { return timeLine_; }

        [[nodiscard]] const Vector_<SampleDef_>& DefLine() const override { return defLine_; }

        [[nodiscard]] const Vector_<String_>& PayoffLabels() const override { return labels_; }

    protected:
        void PayoffsImpl(const Scenario_<T_>& path, Vector_<T_>* payoffs) const override {
            const auto& sample = path.front();
            const auto spot = sample.forwards_.front().front();
            payoffs->front() = Max(spot - strike_, 0.0) * sample.discounts_.front() / sample.numeraire_;
        }

        void PayoffsImpl(const Scenario_<T_>& path, typename Matrix_<T_>::Row_* payoffs) const override {
            const auto& sample = path.front();
            const auto spot = sample.forwards_.front().front();
            (*payoffs)[0] = Max(spot - strike_, 0.0) * sample.discounts_.front() / sample.numeraire_;
        }
    };
} // namespace Dal