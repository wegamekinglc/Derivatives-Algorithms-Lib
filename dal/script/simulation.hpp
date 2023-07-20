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

    constexpr const size_t BATCH_SIZE = 8192;
    std::unique_ptr<Random_> CreateRNG(const String_& method, size_t n_dim, bool use_bb = false);
    AAD::Position_ InitModel4ParallelAAD(AAD::Tape_& tape, const ScriptProduct_& prd, AAD::Model_<AAD::Number_>& clonedMdl, Scenario_<AAD::Number_>& path);

    template <class T_ = double> struct SimResults_;

    template <>
    struct SimResults_<double> {
        explicit SimResults_() : aggregated_(0.0) {}
        double aggregated_;
    };

    template <>
    struct SimResults_<AAD::Number_> {
        explicit SimResults_(int nParam) : aggregated_(0.0), risks_(nParam) {}
        double aggregated_;
        Vector_<> risks_;
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
