//
// Created by wegamekinglc on 2021/8/7.
//

#pragma once
#include <dal/math/aad/models/base.hpp>
#include <dal/math/aad/operators.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {

    template <class T_> class BlackScholes_ : public Model_<T_> {
        T_ spot_;
        T_ rate_;
        T_ div_;
        T_ vol_;

        const bool spotMeasure_;
        Vector_<Time_> timeLine_;
        bool todayOnTimeLine_;
        const Vector_<SampleDef_>* defLine_;

        Vector_<T_> stds_;
        Vector_<T_> drifts_;
        Vector_<T_> numeraires_;
        Vector_<Vector_<T_>> discounts_;
        Vector_<Vector_<T_>> forwardFactors_;
        Vector_<Vector_<T_>> libors_;

        Vector_<T_*> parameters_;
        Vector_<String_> parameterLabels_;

    private:
        void SetParamPointers() {
            parameters_[0] = &spot_;
            parameters_[1] = &vol_;
            parameters_[2] = &rate_;
            parameters_[3] = &div_;
        }

        void FillScenario(const size_t& idx, const T_& spot, Sample_<T_>& scenario, const SampleDef_& def) const {
            if (def.numeraire_) {
                scenario.numeraire_ = numeraires_[idx];
                if (spotMeasure_)
                    scenario.numeraire_ *= spot;
            }

            Transform(
                forwardFactors_[idx], [&spot](const T_& ff) { return spot * ff; }, &scenario.forwards_.front());

            Copy(discounts_[idx], &scenario.discounts_);
            Copy(libors_[idx], &scenario.libors_);
        }

    public:
        template <class U_>
        BlackScholes_(const U_& spot,
                      const U_& vol,
                      const bool& spotMeasure = false,
                      const U_& rate = U_(0.0),
                      const U_& div = U_(0.0))
            : spot_(spot), vol_(vol), rate_(rate), div_(div), spotMeasure_(spotMeasure), parameters_(4),
              parameterLabels_(4) {
            parameterLabels_[0] = "spot";
            parameterLabels_[1] = "vol";
            parameterLabels_[2] = "rate";
            parameterLabels_[3] = "div";

            SetParamPointers();
        }

        const T_& Spot() const { return spot_; }

        const T_& Vol() const { return vol_; }

        const T_& Rate() const { return rate_; }

        const T_& Div() const { return div_; }

        const Vector_<T_*>& Parameters() override { return parameters_; }

        const Vector_<String_>& ParameterLabels() const override { return parameterLabels_; }

        std::unique_ptr<Model_<T_>> Clone() const override {
            auto clone = std::make_unique<BlackScholes_<T_>>(*this);
            clone->SetParamPointers();
            return clone;
        }

        void Allocate(const Vector_<Time_>& productTimeLine, const Vector_<SampleDef_>& defLine) override {
            timeLine_.clear();
            timeLine_.push_back(systemTime);

            for (const auto& time : productTimeLine) {
                if (time > systemTime)
                    timeLine_.push_back(time);
            }

            todayOnTimeLine_ = productTimeLine[0] == systemTime;
            defLine_ = &defLine;

            stds_.Resize(timeLine_.size() - 1);
            drifts_.Resize(timeLine_.size() - 1);

            const size_t n = productTimeLine.size();
            numeraires_.Resize(n);

            discounts_.Resize(n);
            forwardFactors_.Resize(n);
            libors_.Resize(n);
            for (size_t j = 0; j < n; ++j) {
                discounts_[j].Resize(defLine[j].discountMats_.size());
                forwardFactors_[j].Resize(defLine[j].forwardMats_.size());
                libors_[j].Resize(defLine[j].liborDefs_.size());
            }
        }

        void Init(const Vector_<Time_>& productTimeline, const Vector_<SampleDef_>& defLine) override {
            const T_ mu = rate_ - div_;
            const size_t n = timeLine_.size() - 1;

            for (size_t i = 0; i < n; ++i) {
                const double dt = timeLine_[i + 1] - timeLine_[i];
                stds_[i] = vol_ * Sqrt(dt);

                if (spotMeasure_)
                    drifts_[i] = (mu + 0.5 * vol_ * vol_) * dt;
                else
                    drifts_[i] = (mu - 0.5 * vol_ * vol_) * dt;
            }

            const size_t m = productTimeline.size();
            for (size_t i = 0; i < m; ++i) {
                if (defLine[i].numeraire_) {
                    if (spotMeasure_)
                        numeraires_[i] = Exp(div_ * productTimeline[i]) / spot_;
                    else
                        numeraires_[i] = Exp(rate_ * productTimeline[i]);
                }

                const size_t pDF = defLine[i].discountMats_.size();
                for (size_t j = 0; j < pDF; ++j)
                    discounts_[i][j] = Exp(-rate_ * (defLine[i].discountMats_[j] - productTimeline[i]));

                const size_t pFF = defLine[i].forwardMats_.front().size();
                for (size_t j = 0; j < pFF; ++j)
                    forwardFactors_[i][j] = Exp(mu * (defLine[i].forwardMats_.front()[j] - productTimeline[i]));

                const size_t pL = defLine[i].liborDefs_.size();
                for (size_t j = 0; j < pL; ++j) {
                    const double dt = defLine[i].liborDefs_[j].end_ - defLine[i].liborDefs_[j].start_;
                    libors_[i][j] = (Exp(rate_ * dt) - 1.0) / dt;
                }
            }
        }

        size_t SimDim() const override { return timeLine_.size() - 1; }

        void GeneratePath(const Vector_<>& gaussVec, Scenario_<T_>* path) const override {
            T_ spot = spot_;
            size_t idx = 0;
            if (todayOnTimeLine_) {
                FillScenario(idx, spot, (*path)[idx], (*defLine_)[idx]);
                ++idx;
            }

            const size_t n = timeLine_.size() - 1;
            for (size_t i = 0; i < n; ++i) {
                spot = spot * Exp(drifts_[i] + stds_[i] * gaussVec[i]);
                FillScenario(idx, spot, (*path)[idx], (*defLine_)[idx]);
                ++idx;
            }
        }
    };
} // namespace Dal
