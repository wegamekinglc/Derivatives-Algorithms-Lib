//
// Created by wegam on 2021/7/11.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/aad/aad.hpp>

namespace Dal {
    /*
     * random number generators
     */

    template <class T_>
    class Product_;

    template <class T_>
    class Model_;

    class RNG_ {
    public:
        virtual void Init(const size_t& simDim) = 0;
        virtual void NextU(Vector_<>* uVec) = 0;
        virtual void NextG(Vector_<>* gaussVec) = 0;

        virtual std::unique_ptr<RNG_> Clone() const = 0;
        virtual ~RNG_() = default;
        virtual void SkipTo(const size_t& b) = 0;
    };

    /*
     * Template algorithms
     * check compatibility of model and products
     * At the moment, only check that assets are the same in both cases
     * may be easily extended in the future
     */

    template <class T_ = double>
    inline bool CheckCompatibility(
        const Product_<T_>& prd,
        const Model_<T_>& mdl
        ) {
        return prd.AssetNames() == mdl.AssetNames();
    }

    Vector_<Vector_<>> MCSimulation(
        const Product_<double>& prd,
        const Model_<double>& mdl,
        const RNG_& rng,
        const size_t& nPath
        );

    /*
     * Parallel equivalent of MCSimulation
     */

    Vector_<Vector_<>> MCParallelSimulation(
        const Product_<double>& prd,
        const Model_<double>& mdl,
        const RNG_& rng,
        const size_t& nPath
        );
}