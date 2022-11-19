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

    std::unique_ptr<Random_> CreateRNG(const String_& method, size_t n_dim, bool use_bb = false);


    template <class T_ = double> struct SimResults_;

    template <>
    struct SimResults_<double> {
        SimResults_(int nPath) : aggregated_(nPath) {}
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
                               bool use_bb = false);

    SimResults_<AAD::Number_> MCSimulation(const ScriptProduct_& product,
                                 const AAD::Model_<AAD::Number_>& model,
                                 int n_paths,
                                 const String_& rsg = "sobol",
                                 bool use_bb = false);
}
