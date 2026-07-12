//
// Created by dal-implementer on 2026/7/12.
//

#include <algorithm>

#include <dal/platform/strict.hpp>

#include <dal/curve/logdfinterp.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {
    LogDfInterpolation_::LogDfInterpolation_(const Vector_<>& yf, LogDfScheme_ scheme) : yf_(yf), scheme_(scheme), linear_(yf) {
        REQUIRE(yf_.size() >= 2, "LogDfInterpolation_: need at least 2 abscissae");
        REQUIRE(IsMonotonic(yf_), "LogDfInterpolation_: abscissae must be strictly increasing");

        switch (scheme_.Switch()) {
        case LogDfScheme_::Value_::LOG_LINEAR:
            break;
        case LogDfScheme_::Value_::LOG_CUBIC_NATURAL:
            REQUIRE(yf_.size() >= 3, "LogDfInterpolation_: natural cubic requires at least 3 abscissae");
            cubic_ = std::make_unique<Interp::NaturalCubicWeightGeometry_>(yf_);
            break;
        case LogDfScheme_::Value_::MIXED: {
            REQUIRE(yf_.size() >= 4, "LogDfInterpolation_: mixed interpolation requires at least 4 abscissae");
            mixedCutoffIndex_ = std::max(1, static_cast<int>(yf_.size()) - 5);
            cubicStorageOffset_ = mixedCutoffIndex_;
            const Vector_<> tailYf(yf_.begin() + cubicStorageOffset_, yf_.end());
            REQUIRE(tailYf.size() >= 3, "LogDfInterpolation_: mixed cubic tail requires at least 3 abscissae");
            cubic_ = std::make_unique<Interp::NaturalCubicWeightGeometry_>(tailYf);
            break;
        }
        default:
            THROW(String_("LogDfInterpolation_: unknown scheme: ") + scheme_.String());
        }
    }

    Interp::InterpWeights_ LogDfInterpolation_::SecantExtrapolation(double yf) const {
        const int n = static_cast<int>(yf_.size());
        const double excess = (yf - yf_[n - 1]) / (yf_[n - 1] - yf_[n - 2]);
        return {{n - 2, -excess}, {n - 1, 1.0 + excess}};
    }

    Interp::InterpWeights_ LogDfInterpolation_::WeightsAt(double yf) const {
        if (yf > yf_.back())
            return SecantExtrapolation(yf);

        switch (scheme_.Switch()) {
        case LogDfScheme_::Value_::LOG_LINEAR:
            return linear_.At(yf);
        case LogDfScheme_::Value_::LOG_CUBIC_NATURAL:
            return cubic_->At(yf);
        case LogDfScheme_::Value_::MIXED:
            if (yf <= yf_[mixedCutoffIndex_])
                return linear_.At(yf);
            else {
                auto result = cubic_->At(yf);
                for (auto& [index, weight] : result) {
                    static_cast<void>(weight);
                    index += cubicStorageOffset_;
                }
                return result;
            }
        default:
            THROW(String_("LogDfInterpolation_::WeightsAt: unknown scheme: ") + scheme_.String());
        }
    }
} // namespace Dal
