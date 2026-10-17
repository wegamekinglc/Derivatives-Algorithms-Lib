//
// Created by Codex on 2026/10/9.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include <dal/model/blackscholes.hpp>
#include <dal/model/detail/blackscholesstep.hpp>
#include <dal/platform/platform.hpp>

namespace Dal::AAD {
    template <class T_> struct BlackScholesStepResult_ {
        T_ logSpot_;
        Sample_<T_> sample_;
    };

    class BlackScholesStepPlan_ {
        Vector_<> timeline_;
        Vector_<SampleDef_> definitions_;
        bool today_;

        template <class T_> static void ValidateParameters(const Vector_<T_>& parameters) {
            REQUIRE(parameters.size() == 4, "BlackScholesStepPlan: parameters must contain spot, vol, rate and div");
            Detail::ValidateBlackScholesParameters(parameters[0], parameters[1], parameters[2], parameters[3]);
        }

        template <class T_> Sample_<T_> Sample(size_t sampleId, const T_& spot, const Vector_<T_>& parameters) const {
            Sample_<T_> result;
            const auto& definition = definitions_[sampleId];
            result.Allocate(definition);
            result.Initialize();
            result.spot_ = spot;
            std::fill(result.observations_.begin(), result.observations_.end(), spot);
            if (definition.numeraire_)
                result.numeraire_ = Detail::BlackScholesNumeraire(parameters[2], timeline_[sampleId]);
            for (size_t i = 0; i < definition.discountMats_.size(); ++i)
                result.discounts_[i] = Detail::BlackScholesDiscount(parameters[2], timeline_[sampleId], definition.discountMats_[i]);
            REQUIRE(std::isfinite(Value(spot)), "BlackScholesStepPlan: non-finite emitted spot");
            return result;
        }

    public:
        BlackScholesStepPlan_(Vector_<> timeline, Vector_<SampleDef_> definitions)
            : timeline_(std::move(timeline)), definitions_(std::move(definitions)), today_(!timeline_.empty() && timeline_[0] == 0.0) {
            const BlackScholes_<double> validation(1.0, 0.0);
            validation.ValidateTimeline(timeline_, definitions_);
        }

        [[nodiscard]] size_t Samples() const { return timeline_.size(); }
        [[nodiscard]] size_t SimDim() const { return Samples() - static_cast<size_t>(today_); }

        template <class T_> [[nodiscard]] T_ InitialLogSpot(const Vector_<T_>& parameters) const {
            ValidateParameters(parameters);
            return T_(Dal::log(parameters[0]));
        }

        template <class T_>
        [[nodiscard]] BlackScholesStepResult_<T_>
        Advance(size_t sampleId, T_ logSpot, const Vector_<T_>& parameters, const Vector_<>& gaussian) const {
            REQUIRE(sampleId < Samples(), "BlackScholesStepPlan: sampleId is outside the timeline");
            REQUIRE(gaussian.size() == SimDim(), "BlackScholesStepPlan: Gaussian vector size must equal SimDim");
            ValidateParameters(parameters);
            REQUIRE(std::isfinite(Value(logSpot)), "BlackScholesStepPlan: logSpot must be finite");
            if (sampleId == 0 && today_)
                return {std::move(logSpot), Sample(sampleId, parameters[0], parameters)};
            const size_t draw = sampleId - static_cast<size_t>(today_);
            REQUIRE(std::isfinite(gaussian[draw]), "BlackScholesStepPlan: Gaussian draw must be finite");
            const double previousTime = sampleId == 0 ? 0.0 : timeline_[sampleId - 1];
            const auto coefficients =
                Detail::BlackScholesCoefficients(parameters[1], T_(parameters[2] - parameters[3]), timeline_[sampleId] - previousTime);
            logSpot += coefficients.drift_ + coefficients.std_ * gaussian[draw];
            REQUIRE(std::isfinite(Value(logSpot)), "BlackScholesStepPlan: non-finite logSpot at sampleId=" + String_(std::to_string(sampleId)));
            const T_ spot = Dal::exp(logSpot);
            return {std::move(logSpot), Sample(sampleId, spot, parameters)};
        }
    };
} // namespace Dal::AAD
