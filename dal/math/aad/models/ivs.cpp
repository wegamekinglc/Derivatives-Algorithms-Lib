//
// Created by wegam on 2022/9/11.
//

#include <dal/math/aad/models/ivs.hpp>

namespace Dal::AAD {
    double MertonIVS_::ImpliedVol(double strike, double mat) const {
        const double call = Merton(Spot(), strike, vol_, mat, intensity_, averageJmp_, jmpStd);
        return BlackScholesIVol(Spot(), strike, call, mat);
    }
}
