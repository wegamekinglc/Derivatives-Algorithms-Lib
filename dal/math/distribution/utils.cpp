//
// Created by wegam on 2022/5/5.
//

#include <dal/math/operators.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/math/distribution/utils.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/protocol/optiontype.hpp>

namespace Dal::Distribution {
    double BlackIV(const Distribution_& model, double strike, double guess, int n_steps) {
        const double f = model.Forward();
        const OptionType_ type = strike > f ? OptionType_::Value_::CALL : OptionType_::Value_::PUT;
        if (n_steps > 1) {
            const double fMid = strike > f ? strike * pow(f / strike, 1.0 / n_steps) : strike + (f - strike) / n_steps;
            guess = BlackIV(model, fMid, guess, n_steps - 1);
        }
        return BlackIV(f, strike, type, model.OptionPrice(strike ,type), guess);
    }
}
