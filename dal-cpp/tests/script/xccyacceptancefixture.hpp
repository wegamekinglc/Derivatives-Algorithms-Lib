#pragma once

#include "xccymcfixture.hpp"

namespace Dal::TestXccyAcceptance {
    struct Fixture_ {
        XccyGaussianModelSpec_ spec_;
        Handle_<CalibrationInstrument_> instrument_;
    };
    inline Fixture_ Fixture(const String_& scenario, XccyNotionalMode_ mode, bool foreign, bool receive) {
        auto spec = scenario == "nonflat" ? TestXccyMoments::NonflatSpec() : TestXccyMoments::ModelSpec(TestXccy::TODAY, scenario == "zero");
        auto config = TestXccy::Config(mode);
        config.convention_.spreadOnForeignLeg_ = foreign;
        if (scenario == "no-lag")
            config.convention_.foreignLeg_.paymentLag_ = 0;
        if (scenario == "zero-a")
            spec.dynamics_.domesticA_ = spec.dynamics_.foreignA_ = 0.;
        const CrossCurrencySwap_ native(TestXccy::TODAY, TestXccy::START, TestXccy::END, .001, config);
        return {spec, NewXccyScriptInstrument(native, {"acceptance", "USD.OIS", receive, 1e-12})};
    }
} // namespace Dal::TestXccyAcceptance
