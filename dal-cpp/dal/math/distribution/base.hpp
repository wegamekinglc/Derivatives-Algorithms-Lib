//
// Created by wegam on 2022/5/5.
//

#pragma once

#include <dal/math/vectors.hpp>


namespace Dal {
    class OptionType_;
    class String_;

    class Distribution_ {
    public:
        virtual ~Distribution_() = default;
        [[nodiscard]] virtual double Forward() const = 0;
        [[nodiscard]] virtual double OptionPrice(double strike, const OptionType_& type) const = 0;
        // support calibration of whatever vol-like parameter
        virtual double& Vol() = 0; // whatever it means
        [[nodiscard]] virtual const double& Vol() const = 0;
        [[nodiscard]] virtual double VolVega(double strike, const OptionType_& type) const = 0;
        // support hedge computation
        [[nodiscard]] virtual Vector_<String_> ParameterNames() const = 0;
        [[nodiscard]] virtual std::map<String_, double> ParameterDerivatives(double strike,
                                                                             const OptionType_& type,
                                                                             const Vector_<String_>& to_report) const = 0;
    };
}
