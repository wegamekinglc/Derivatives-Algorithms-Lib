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

    constexpr const int BATCH_SIZE = 8192;
    std::unique_ptr<Random_> CreateRNG(const String_& method, size_t n_dim, bool use_bb = false);
    void InitModel4ParallelAAD(const ScriptProduct_& prd, AAD::Model_<AAD::Number_>& clonedMdl, Scenario_<AAD::Number_>& path);

    template <class T_ = double> struct SimResults_;

    template <>
    struct SimResults_<double> {
        explicit SimResults_(int n_paths) : aggregated_(n_paths) {}
        [[nodiscard]] int Rows() const { return aggregated_.size();  }
        Vector_<> aggregated_;
    };

    template <>
    struct SimResults_<AAD::Number_> {
        SimResults_(int nPath, int nParam) : aggregated_(nPath), risks_(nParam) {}
        [[nodiscard]] int Rows() const { return aggregated_.size();  }
        Vector_<> aggregated_;
        Vector_<> risks_;
    };

    SimResults_<> MCSimulation(const ScriptProduct_& product,
                               const AAD::Model_<double>& model,
                               int n_paths,
                               const String_& rsg = "sobol",
                               bool use_bb = false,
                               int max_nested_ifs = -1);

    SimResults_<AAD::Number_> MCSimulation(const ScriptProduct_& product,
                                           const AAD::Model_<AAD::Number_>& model,
                                           int n_paths,
                                           const String_& rsg = "sobol",
                                           bool use_bb = false,
                                           int max_nested_ifs = -1);
}
