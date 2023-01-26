//
// Created by wegam on 2022/10/30.
//

#include <dal/math/operators.hpp>
#include <dal/math/random/brownianbridge.hpp>
#include <dal/math/random/sobol.hpp>
#include <gtest/gtest.h>

using namespace Dal;


TEST(BrownianBridgeTest, TestFillNormal) {
    int ndim = 10;
    auto bw = std::make_unique<BrownianBridge_>(std::unique_ptr<Random_>(NewSobol(ndim, 1024)));
    int n_paths = pow(2, 24);
    Vector_<> deviates;

    Vector_<> means(ndim, 0.);
    Vector_<> vars(ndim, 0.);

    for (int i = 0; i < n_paths; ++i) {
        bw->FillNormal(&deviates);
        for (int j = 0; j < ndim; ++j) {
            means[j] += deviates[j];
            vars[j] += Square(deviates[j]);
        }
    }

    for (auto k = 0 ; k < ndim; ++k) {
        means[k] /= n_paths;
        vars[k] /= n_paths;
        ASSERT_NEAR(means[k], 0.0, 1e-3);
        ASSERT_NEAR(vars[k], 1.0, 1e-3);
    }
}
