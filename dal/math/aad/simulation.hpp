//
// Created by wegam on 2021/7/11.
//

#pragma once

#include "dal/math/random/pseudorandom.hpp"
#include <dal/math/aad/aad.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>

namespace Dal {
    /*
     * random number generators
     */

    template <class T_> class Product_;

    template <class T_> class Model_;

    /*
     * Template algorithms
     * check compatibility of model and products
     * At the moment, only check that assets are the same in both cases
     * may be easily extended in the future
     */

    template <class T_ = double> inline bool CheckCompatibility(const Product_<T_>& prd, const Model_<T_>& mdl) {
        return prd.AssetNames() == mdl.AssetNames();
    }

    Matrix_<> MCSimulation(const Product_<double>& prd,
                           const Model_<double>& mdl,
                           const std::unique_ptr<Random_>& rng,
                           int nPath);

    /*
     * Parallel equivalent of MCSimulation
     */

    Matrix_<> MCParallelSimulation(const Product_<double>& prd,
                                   const Model_<double>& mdl,
                                   const std::unique_ptr<PseudoRandom_>& rng,
                                   int nPath);

    /*
     * MC simulation of AAD
     */
    struct AADResults_ {
        AADResults_(int nPath, int nPay, int nParam) : payoffs_(nPath, nPay), aggregated_(nPath), risks_(nParam) {}
        Matrix_<> payoffs_;
        Vector_<> aggregated_;
        Vector_<> risks_;
    };
    const auto DEFAULT_AGGREGATOR = [](const Vector_<Number_>& v) { return v[0]; };

    template <class F_ = decltype(DEFAULT_AGGREGATOR)>
    AADResults_ MCSimulationAAD(const Product_<Number_>& prd,
                                const Model_<Number_>& mdl,
                                const std::unique_ptr<Random_>& rng,
                                int nPath) {
        REQUIRE(CheckCompatibility(prd, mdl), "model and products are not compatible");
        return AADResults_(1, 1, 1);
    }

} // namespace Dal