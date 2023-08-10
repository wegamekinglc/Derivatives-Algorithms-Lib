//
// Created by wegam on 2022/11/6.
//

#pragma once

#include <dal/script/event.hpp>
#include <dal/math/aad/models/base.hpp>
#include <dal/math/random/brownianbridge.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/random/pseudorandom.hpp>


namespace Dal::Script {

    template <class T_ = double> struct SimResults_;

    template <>
    struct SimResults_<double> {
        explicit SimResults_() : aggregated_(0.0) {}
        double aggregated_;
    };

    template <>
    struct SimResults_<AAD::Number_> {
        explicit SimResults_(const Vector_<String_>& names) : aggregated_(0.0), risks_(names.size()), names_(names) {}
        double aggregated_;
        Vector_<> risks_;
        Vector_<String_> names_;
    };

    SimResults_<> MCSimulation(const ScriptProduct_& product,
                               const AAD::Model_<double>& model,
                               size_t n_paths,
                               const String_& rsg = "sobol",
                               bool use_bb = false,
                               bool compiled = false);

    SimResults_<AAD::Number_> MCSimulation(const ScriptProduct_& product,
                                           const AAD::Model_<AAD::Number_>& model,
                                           size_t n_paths,
                                           const String_& rsg = "sobol",
                                           bool use_bb = false,
                                           int max_nested_ifs = -1,
                                           double eps = 0.01,
                                           bool compiled = false);
}
