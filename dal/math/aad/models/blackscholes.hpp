//
// Created by wegamekinglc on 2021/8/7.
//

#pragma once
#include <dal/math/aad/models/base.hpp>

namespace Dal {
    template <class T_>
    class BlackScholes_: public Model_<T_> {
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

    public:
        template <class U_>
            BlackScholes_(const U_& spot,
                          const U_& vol,
                          const bool& spotMeasure = false,
                          const U_& rate = U_(0.0),
                          const U_& div = U_(0.0))
                          : spot_(spot),
                            vol_(vol),
                            rate_(rate),
                            div_(div),
                            spotMeasure_(spotMeasure),
                            parameters_(4),
                            parameterLabels_(4) {
            parameterLabels_[0] = "spot";
            parameterLabels_[1] = "vol";
            parameterLabels_[2] = "rate";
            parameterLabels_[3] = "div";

            SetParamPointers();
        }

        const T_& Spot() const {
            return spot_;
        }

        const T_& Vol() const {
            return vol_;
        }

        const T_& Rate() const {
            return rate_;
        }

        const T_& Div() const {
            return div_;
        }

        const Vector_<T_*>& Parameters() override {
            return parameters_;
        }

        const Vector_<String_>& ParameterLabels() const override {
            return parameterLabels_;
        }

        std::unique_ptr<Model_<T_>> Clone() const override {
            auto clone = std::make_unique<BlackScholes_<T_>>(*this);
            clone->SetParamPointers();
            return clone;
        }

        void Allocate(const Vector_<Time_>& productTimeLine,
                      const Vector_<SampleDef_>& defLine) override;

        void Init(const Vector_<Time_>& productTimeline,
                  const Vector_<SampleDef_>& defLine) override;
    };
}