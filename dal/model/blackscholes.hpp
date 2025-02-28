//
// Created by wegamekinglc on 2021/8/7.
//

#pragma once

#include <dal/math/operators.hpp>
#include <dal/model/base.hpp>
#include <dal/storage/archive.hpp>
#include <dal/utilities/algorithms.hpp>

/*IF--------------------------------------------------------------------------
storable BSModelData
    Black - Scholes model data
version 1
&members
name is ?string
spot is number
vol is number
rate is number
div is number
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace AAD {
        template <class T_ = double> class BlackScholes_ : public Model_<T_> {
            T_ spot_;
            T_ rate_;
            T_ div_;
            T_ vol_;

            Vector_<> timeLine_;
            bool todayOnTimeLine_;
            const Vector_<SampleDef_>* defLine_;

            Vector_<T_> stds_;
            Vector_<T_> drifts_;
            Vector_<T_> numeraires_;

            Vector_<T_*> parameters_;
            Vector_<String_> parameterLabels_;

            void SetParamPointers() {
                parameters_[0] = &spot_;
                parameters_[1] = &vol_;
                parameters_[2] = &rate_;
                parameters_[3] = &div_;
            }

            void FillScenario(const size_t& idx, const T_& spot, Sample_<T_>& scenario, const SampleDef_& def) const {
                if (def.numeraire_)
                    scenario.numeraire_ = numeraires_[idx];
                scenario.spot_ = spot;
            }

        public:
            template <class U_>
            BlackScholes_(const U_& spot,
                          const U_& vol,
                          const U_& rate = U_(0.0),
                          const U_& div = U_(0.0))
                : spot_(spot), vol_(vol), rate_(rate), div_(div), parameters_(4), parameterLabels_(4) {
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

            const Vector_<T_*>& Parameters() const override { return parameters_; }

            const Vector_<String_>& ParameterLabels() const override { return parameterLabels_; }

            std::unique_ptr<Model_<T_>> Clone() const override {
                auto clone = std::make_unique<BlackScholes_<T_>>(*this);
                clone->SetParamPointers();
                return clone;
            }

            void Allocate(const Vector_<>& productTimeLine, const Vector_<SampleDef_>& defLine) override {
                timeLine_.clear();
                timeLine_.push_back(0);

                for (const auto& time : productTimeLine) {
                    if (time > 0)
                        timeLine_.push_back(time);
                }

                todayOnTimeLine_ = productTimeLine[0] == 0;
                defLine_ = &defLine;

                stds_.Resize(timeLine_.size() - 1);
                drifts_.Resize(timeLine_.size() - 1);

                const size_t n = productTimeLine.size();
                numeraires_.Resize(n);
            }

            void Init(const Vector_<>& productTimeline, const Vector_<SampleDef_>& defLine) override {
                const T_ mu = rate_ - div_;
                const size_t n = timeLine_.size() - 1;

                for (size_t i = 0; i < n; ++i) {
                    const double dt = timeLine_[i + 1] - timeLine_[i];
                    stds_[i] = vol_ * Dal::sqrt(dt);

                    drifts_[i] = (mu - 0.5 * vol_ * vol_) * dt;
                }

                const size_t m = productTimeline.size();
                for (size_t i = 0; i < m; ++i)
                    if (defLine[i].numeraire_)
                        numeraires_[i] = Dal::exp(rate_ * productTimeline[i]);
            }

            [[nodiscard]] size_t SimDim() const override { return timeLine_.size() - 1; }

            void GeneratePath(const Vector_<>& gaussVec, Scenario_<T_>* path) const override {
                T_ spot = spot_;
                size_t idx = 0;
                if (todayOnTimeLine_) {
                    FillScenario(idx, spot, (*path)[idx], (*defLine_)[idx]);
                    ++idx;
                }

                T_ logSpot = Dal::log(spot);
                const size_t n = timeLine_.size() - 1;
                for (size_t i = 0; i < n; ++i) {
                    logSpot += drifts_[i] + stds_[i] * gaussVec[i];
                    FillScenario(idx, Dal::exp(logSpot), (*path)[idx], (*defLine_)[idx]);
                    ++idx;
                }
            }
        };
    }

    struct BSModelData_: ModelData_ {
        double spot_;
        double vol_;
        double rate_;
        double div_;

        BSModelData_(const String_& name,
                     double spot,
                     double vol,
                     double rate = 0.0,
                     double div = 0.0)
                     : ModelData_("BSModelData_", name), spot_(spot), vol_(vol), rate_(rate), div_(div) {
            parameterLabels_.Resize(4);
            parameterLabels_[0] = "spot";
            parameterLabels_[1] = "vol";
            parameterLabels_[2] = "rate";
            parameterLabels_[3] = "div";
        }

        void Write(Archive::Store_& dst) const override;

    private:
        BSModelData_* MutantModel(const String_* new_name, const Slide_* slide) const override;
    };

} // namespace Dal
