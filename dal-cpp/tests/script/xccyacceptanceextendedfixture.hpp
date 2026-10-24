#pragma once

#include "xccyacceptancefixture.hpp"

namespace Dal::TestXccyAcceptance {
    inline Vector_<Date_> StagedKnots() { return {Date_(2027, 11, 8), Date_(2028, 5, 8), Date_(2028, 11, 8)}; }

    inline Vector_<> StagedTarget() {
        Vector_<> result;
        for (const auto& date : StagedKnots())
            result.push_back(-.003 * (date - TestXccy::TODAY) / 365.);
        return result;
    }

    inline XccyInstrumentCalibrationSpec_ StagedSpec(const Vector_<>& quotes) {
        REQUIRE(quotes.size() == 3, "three independently supplied staged quotes");
        const auto model = TestXccyMoments::ModelSpec();
        XccyInstrumentCalibrationSpec_ spec;
        spec.context_.valuationTime_ = model.market_.valuationTime_;
        spec.context_.pair_ = *model.market_.pair_;
        spec.context_.collateralCurrency_ = model.market_.collateralCurrency_;
        spec.context_.discountRoute_ = model.market_.discountRoute_;
        spec.context_.fixings_ = TestXccyExecution::Empty();
        spec.domesticCurveBlock_ = model.market_.domesticBlock_;
        spec.foreignCurveBlock_ = model.market_.foreignBlock_;
        spec.fxSpot_ = model.market_.fxSpot_;
        spec.gaussianDynamics_.reset(new XccyGaussianDynamics_("independent-staged", model.dynamics_));
        spec.basisParameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotDates_ = StagedKnots();
        spec.solver_.initialGuessPerNode_ = StagedTarget();
        spec.solver_.fitWeight_ = 1e8;
        spec.solver_.fitNormTolerance_ = .5;
        for (auto& value : spec.solver_.initialGuessPerNode_)
            value += .001;
        const Vector_<Date_> maturities{Date_(2027, 11, 4), Date_(2028, 5, 4), Date_(2028, 11, 4)};
        for (size_t i = 0; i < maturities.size(); ++i) {
            const auto id = "independent-staged-" + String::FromInt(i);
            const auto quote = NewCalibrationMarketQuote(id, quotes[i], CalibrationValueUnit_::Parse("BasisSpread"));
            const auto instrument = TestXccyMoments::Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true, maturities[i]);
            spec.observations_.push_back(NewCalibrationObservation(id, instrument, "BasisSpread", quote, 1e-4));
        }
        return spec;
    }

    inline XccyGaussianModelSpec_ StagedModel() {
        auto spec = TestXccyMoments::ModelSpec();
        Vector_<Date_> dates{TestXccy::TODAY};
        dates.Append(StagedKnots());
        Vector_<> values{0.};
        values.Append(StagedTarget());
        spec.market_.basis_ =
            Handle_<DiscountCurve_>(NewDiscountLogDF("staged-target", "USD", dates, values, DayBasis_("ACT365F"), LogDfScheme_::Value_::LOG_LINEAR));
        return spec;
    }

    inline XccyGaussianModelSpec_ SignedModel(const String_& scenario) {
        auto spec = TestXccyMoments::ModelSpec();
        const std::map<String_, std::pair<int, int>> flips{{"rho-df", {0, 1}}, {"rho-dx", {0, 2}}, {"rho-fx", {1, 2}}};
        const auto found = flips.find(scenario);
        if (found != flips.end()) {
            const auto [i, j] = found->second;
            spec.dynamics_.correlation_(i, j) *= -1.;
            spec.dynamics_.correlation_(j, i) *= -1.;
        }
        if (scenario == "negative-basis")
            spec.market_.basis_ = TestXccyMoments::Curve("USD", -.003);
        if (scenario == "zero-basis")
            spec.market_.basis_ = TestXccyMoments::Curve("USD", 0.);
        return spec;
    }

    inline XccyGaussianModelSpec_ OwnProjectionModel() {
        auto spec = TestXccyMoments::ModelSpec();
        spec.market_.domesticBlock_.reset(new CurveBlock_("own-usd", "USD", {{CollateralType_("OIS"), TestXccyMoments::Curve("USD", .04)}},
                                                          {{PeriodLength_("3M"), TestXccyMoments::Curve("USD", .04)}}));
        spec.market_.foreignBlock_.reset(new CurveBlock_("own-eur", "EUR", {{CollateralType_("OIS"), TestXccyMoments::Curve("EUR", .02)}},
                                                         {{PeriodLength_("6M"), TestXccyMoments::Curve("EUR", .02)}}));
        return spec;
    }

    inline Fixture_ BroadFixture(const String_& scenario, XccyNotionalMode_ mode, bool foreign, bool receive) {
        auto spec = scenario == "own-projection" ? OwnProjectionModel() : SignedModel(scenario);
        if (scenario == "nonflat")
            spec = TestXccyMoments::NonflatSpec();
        auto config = TestXccy::Config(mode);
        config.convention_.spreadOnForeignLeg_ = foreign;
        if (mode == XccyNotionalMode_::Value_::FIXED)
            config.convention_.foreignLeg_.paymentLag_ = 0;
        const CrossCurrencySwap_ native(TestXccy::TODAY, TestXccy::START, TestXccy::END, .001, config);
        return {spec, NewXccyScriptInstrument(native, {"broad", "USD.OIS", receive, 1e-12})};
    }
} // namespace Dal::TestXccyAcceptance
