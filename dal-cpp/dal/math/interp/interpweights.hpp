//
// Created by dal-implementer on 2026/7/12.
//

#pragma once

#include <utility>

#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace Interp {
        using InterpWeights_ = Vector_<std::pair<int, double>>;

        template <class T_> T_ ApplyInterpWeights(const Vector_<T_>& values, const InterpWeights_& weights) {
            T_ retval(0.0);
            for (const auto& [index, weight] : weights) {
                REQUIRE(index >= 0 && index < static_cast<int>(values.size()), "ApplyInterpWeights: weight index is outside the ordinate vector");
                retval += weight * values[index];
            }
            return retval;
        }

        class LinearWeightGeometry_ {
            Vector_<> x_;

        public:
            explicit LinearWeightGeometry_(const Vector_<>& x);
            [[nodiscard]] InterpWeights_ At(double x) const;
        };

        class NaturalCubicWeightGeometry_ {
            Vector_<> x_;
            Vector_<Vector_<>> secondDerivativeWeights_;

        public:
            explicit NaturalCubicWeightGeometry_(const Vector_<>& x);
            [[nodiscard]] InterpWeights_ At(double x) const;
        };
    } // namespace Interp
} // namespace Dal
