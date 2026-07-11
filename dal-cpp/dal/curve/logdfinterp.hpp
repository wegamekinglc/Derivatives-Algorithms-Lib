//
// Created by dal-implementer on 2026/7/12.
//

#pragma once

#include <memory>

#include <dal/curve/logdfscheme.hpp>
#include <dal/math/interp/interpweights.hpp>

namespace Dal {
    class LogDfInterpolation_ {
        Vector_<> yf_;
        LogDfScheme_ scheme_;
        Interp::LinearWeightGeometry_ linear_;
        std::unique_ptr<Interp::NaturalCubicWeightGeometry_> cubic_;
        int cubicStorageOffset_ = 0;
        int mixedCutoffIndex_ = -1;

        [[nodiscard]] Interp::InterpWeights_ SecantExtrapolation(double yf) const;

    public:
        LogDfInterpolation_(const Vector_<>& yf, LogDfScheme_ scheme);
        [[nodiscard]] Interp::InterpWeights_ WeightsAt(double yf) const;

        template <class T_> T_ Evaluate(const Vector_<T_>& values, double yf) const {
            REQUIRE(values.size() == yf_.size(), "LogDfInterpolation_: ordinate count must equal year-fraction count");
            return Interp::ApplyInterpWeights(values, WeightsAt(yf));
        }
    };
} // namespace Dal
