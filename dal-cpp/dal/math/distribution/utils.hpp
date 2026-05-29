//
// Created by wegam on 2022/5/5.
//

#pragma once

#include <dal/math/distribution/base.hpp>

namespace Dal::Distribution {
    double BlackIV(const Distribution_& model, double strike, double guess, int n_steps);
}
