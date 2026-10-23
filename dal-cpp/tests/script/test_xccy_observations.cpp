#include <gtest/gtest.h>

#include <cmath>

#include <dal/curve/tapeguard.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/script/xccyobservations.hpp>
#include <dal/storage/_repository.hpp>
#include <dal/storage/globals.hpp>

#include "../../test-support/xccyinstrumentfixture.hpp"

using namespace Dal;

namespace {
    Handle_<DiscountCurve_> Curve(const String_& currency, const Date_& start, double rate) {
        return Handle_<DiscountCurve_>(NewDiscountLogDF("observation-curve", currency, {start, start.AddDays(1095)}, {0., -3. * rate},
                                                        DayBasis_("ACT365F"), LogDfScheme_::Value_::LOG_LINEAR));
    }
    Handle_<XccyGaussianModelData_> Model(const XccyInstrumentCalibrationSpec_& input, bool projections = true, bool zeroDynamics = false) {
        XccyGaussianModelSpec_ spec;
        spec.market_ = {input.context_.valuationTime_, input.context_.pair_,     input.context_.collateralCurrency_, input.fxSpot_,
                        input.domesticCurveBlock_,     input.foreignCurveBlock_, TestXccy::Flat("USD", .003),        input.context_.discountRoute_};
        const Date_ start = input.context_.valuationTime_.Date();
        const auto domestic = Curve("USD", start, .04), foreign = Curve("EUR", start, .02);
        spec.market_.domesticBlock_ = Handle_<CurveBlock_>(
            new CurveBlock_("d", "USD", {{CollateralType_("OIS"), domestic}},
                            {{PeriodLength_("3M"), Curve("USD", start, .045)}, {PeriodLength_("6M"), Curve("USD", start, .045)}}));
        spec.market_.foreignBlock_ = Handle_<CurveBlock_>(
            new CurveBlock_("f", "EUR", {{CollateralType_("OIS"), foreign}},
                            {{PeriodLength_("6M"), Curve("EUR", start, .026)}, {PeriodLength_("3M"), Curve("EUR", start, .026)}}));
        spec.market_.basis_ = Curve("USD", start, .003);
        spec.dynamics_ = {.03, .07, {start.AddDays(1095)}, {.01}, {.009}, {.12}, {"rate:USD", "rate:EUR", "fx:EUR/USD"}, Matrix_<>(3, 3, 0.)};
        for (int i = 0; i < 3; ++i)
            spec.dynamics_.correlation_(i, i) = 1.;
        if (zeroDynamics)
            spec.dynamics_.domesticSigma_ = spec.dynamics_.foreignSigma_ = spec.dynamics_.fxSigma_ = {0.};
        const Handle_<XccyGaussianModelData_> data(new XccyGaussianModelData_("observations", spec));
        if (projections)
            return data;
        const auto& market = *data->market_;
        const Handle_<XccyGaussianMarket_> historical(new XccyGaussianMarket_("historical", market.valuationTime_.Date(), "USD", "EUR", "USD",
                                                                              market.fxSpot_, market.discountRoute_, market.domestic_,
                                                                              market.foreign_, market.basis_, {}));
        return Handle_<XccyGaussianModelData_>(new XccyGaussianModelData_("historical", historical, data->dynamics_));
    }
    CalibrationPreparationContext_ Context(const XccyInstrumentCalibrationSpec_& spec) {
        CalibrationPreparationContext_ result;
        result.xccy_ = std::make_shared<const XccyScriptContext_>(spec.context_);
        return result;
    }
    Handle_<CalibrationInstrument_> Instrument(XccyNotionalMode_ mode, bool foreignSpread, bool receive, bool initial = true, bool final = true) {
        auto config = TestXccy::Config(mode);
        config.convention_.spreadOnForeignLeg_ = foreignSpread;
        config.convention_.initialNotionalExchange_ = initial;
        config.convention_.finalNotionalExchange_ = final;
        return NewXccyScriptInstrument(CrossCurrencySwap_(TestXccy::TODAY, TestXccy::START, TestXccy::END, .001, config),
                                       {"path-oracle", "USD.OIS", receive});
    }
    double Time(const Date_& date, const Date_& valuation) { return (date - valuation) / 365.; }
    double Variance(double a, double sigma, double t) {
        const auto h = [](double x, double y) { return -std::expm1(-x * y) / x; };
        return sigma * sigma / (a * a) * (t - 2. * h(a, t) + h(2. * a, t));
    }
    double Phi(bool foreign, double t) { return (foreign ? .017 : .04) * t + .5 * Variance(foreign ? .07 : .03, foreign ? .009 : .01, t); }
    AAD::XccyGaussianState_<double> State(const Date_& date, const Date_& valuation) {
        const double t = Time(date, valuation);
        return {date, {.006 * std::sin(t), -.004 * t, .002 * t * t, -.001 * t * t, .03 * t}};
    }
    double Spot(const Date_& date, const Date_& valuation) {
        const auto state = State(date, valuation);
        const double t = Time(date, valuation);
        return 1.1 * std::exp(Phi(false, t) - Phi(true, t) + state.values_[2] - state.values_[3] - .5 * .12 * .12 * t + state.values_[4]);
    }
    double Discount(const Date_& date, const Date_& valuation) {
        return std::exp(-Phi(false, Time(date, valuation)) - State(date, valuation).values_[2]);
    }
    double Direction(bool foreignSpread, bool receive) { return (foreignSpread ? 1. : -1.) * (receive ? 1. : -1.); }
    double Spread(bool foreignSpread, bool foreignLeg) { return foreignSpread == foreignLeg ? .001 : 0.; }
    double Rate(bool foreign, const Date_& fixing, const Date_& start, const Date_& end, const Date_& valuation) {
        const double a = foreign ? .07 : .03, sigma = foreign ? .009 : .01;
        const double t = Time(fixing, valuation), s = Time(start, valuation), e = Time(end, valuation);
        const double hStart = -std::expm1(-a * (s - t)) / a, hEnd = -std::expm1(-a * (e - t)) / a;
        const double x = State(fixing, valuation).values_[foreign ? 1 : 0];
        const double ratio = Phi(foreign, e) - Phi(foreign, s) + (hEnd - hStart) * x + .5 * (Variance(a, sigma, s - t) - Variance(a, sigma, e - t)) +
                             (foreign ? .009 : .005) * (e - s);
        return std::expm1(ratio) / ((end - start) / 360.);
    }
    Vector_<Script::XccyScriptPayment_<double>>
    Oracle(XccyNotionalMode_ mode, bool foreignSpread, bool receive, bool initial = true, bool final = true) {
        const Date_ valuation = TestXccy::TODAY, start = TestXccy::START, reset(2027, 8, 4), end = TestXccy::END;
        const double sign = Direction(foreignSpread, receive);
        const double later = mode == XccyNotionalMode_::Value_::FIXED ? 110. : 100. * Spot(reset, valuation);
        Vector_<Script::XccyScriptPayment_<double>> result;
        const auto pay = [&](const Date_& date, double amount) { result.push_back({date, amount * Discount(date, valuation)}); };
        pay(reset,
            sign * 110. * ((reset - start) / 360.) * (Rate(false, Date_(2027, 4, 30), start, reset, valuation) + Spread(foreignSpread, false)));
        pay(end, sign * later * ((end - reset) / 360.) * (Rate(false, Date_(2027, 8, 2), reset, end, valuation) + Spread(foreignSpread, false)));
        const Date_ foreignPayment(2027, 11, 8);
        pay(foreignPayment, -sign * 100. * ((end - start) / 360.) *
                                (Rate(true, Date_(2027, 4, 30), start, end, valuation) + Spread(foreignSpread, true)) *
                                Spot(foreignPayment, valuation));
        if (initial) {
            pay(start, -sign * 110.);
            pay(start, sign * 100. * Spot(start, valuation));
        }
        if (final) {
            pay(end, sign * later);
            pay(end, -sign * 100. * Spot(end, valuation));
        }
        if (mode == XccyNotionalMode_::Value_::MARK_TO_MARKET)
            pay(reset, sign * (later - 110.));
        return result;
    }
    template <class T_> void SortPayments(Vector_<Script::XccyScriptPayment_<T_>>* payments) {
        std::sort(payments->begin(), payments->end(), [](const auto& x, const auto& y) {
            return std::make_pair(x.date_, static_cast<double>(x.discountedAmount_)) <
                   std::make_pair(y.date_, static_cast<double>(y.discountedAmount_));
        });
    }
    Vector_<AAD::XccyGaussianState_<double>> States(const Script::PreparedXccyScript_& prepared) {
        Vector_<AAD::XccyGaussianState_<double>> result;
        for (const auto& date : prepared.SampleDates())
            result.push_back(State(date, prepared.ModelData()->market_->valuationTime_.Date()));
        return result;
    }
    size_t RequestId(const Script::PreparedXccyScript_& prepared, Script::XccyObservationKind_ kind, const Date_& date) {
        for (size_t id = 0; id < prepared.Requests().size(); ++id)
            if (prepared.Requests()[id].kind_ == kind && prepared.Observations().Requests()[id].key_.fixingTime_ == DateTime_(date))
                return id;
        throw std::runtime_error("missing oracle request");
    }
    template <class F_> void AssertError(const F_& action, const String_& code) {
        try {
            action();
            FAIL() << "expected " << code;
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, code) << error.what();
            ASSERT_TRUE(error.Failure().report_);
            ASSERT_EQ(error.Failure().report_->Errors().size(), 1);
        }
    }
    Handle_<MarketFixingSnapshot_> Snapshot(const MarketFixingSnapshot_::values_t& values) {
        return Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(values));
    }
    MarketFixingSnapshot_::values_t History() {
        return {{"IR:USD,LIBOR_3M_LCH", {{DateTime_(Date_(2027, 4, 30)), .05}, {DateTime_(Date_(2027, 8, 2)), .06}}},
                {"IR:EUR,LIBOR_6M_LCH", {{DateTime_(Date_(2027, 4, 30)), .03}}},
                {"IR:EUR,LIBOR_3M_LCH", {{DateTime_(Date_(2027, 4, 30)), .03}, {DateTime_(Date_(2027, 8, 2)), .04}}}};
    }
    struct HistoryScope_ {
        std::map<String_, FixHistory_> originals_;
        explicit HistoryScope_(const MarketFixingSnapshot_::values_t& values) {
            for (const auto& index : values) {
                originals_[index.first] = Global::Fixings_().History(index.first);
                FixHistory_ history;
                for (const auto& value : index.second)
                    history.vals_.push_back(value);
                XGLOBAL::StoreFixings(index.first, history, false);
            }
        }
        ~HistoryScope_() {
            for (const auto& original : originals_) {
                if (!original.second.vals_.empty())
                    XGLOBAL::StoreFixings(original.first, original.second, false);
                else
                    for (const auto& object : ObjectAccess_::Find(".*"))
                        if (object->Name() == "##GLOBAL##FixingsFor:" + original.first)
                            static_cast<void>(ObjectAccess_::Erase(*object));
            }
        }
    };
    template <class T_> T_ Price(const Script::PreparedXccyScript_& prepared, const Script::XccyScriptPath_<T_>& path, bool compiled) {
        T_ result(0.);
        for (const auto& payment : prepared.EvaluatePayments(path, compiled))
            result += payment.discountedAmount_;
        return result;
    }
} // namespace

TEST(XccyObservationTest, TestDedicatedPlanIncludesFixingDatesBeforePaymentEvents) {
    const auto spec = TestXccy::Spec();
    const Script::PreparedXccyScript_ prepared(spec.observations_[0]->Instrument(), Context(spec), Model(spec));
    const auto& dates = prepared.SampleDates();
    ASSERT_TRUE(std::binary_search(dates.begin(), dates.end(), Date_(2027, 4, 30)));
    ASSERT_TRUE(std::binary_search(dates.begin(), dates.end(), Date_(2027, 11, 8)));
}

TEST(XccyObservationTest, TestIndependentPathwiseCashflowsForBothDirectionsAndSpreadLegs) {
    const auto spec = TestXccy::Spec();
    for (const auto mode : XccyNotionalModeListAll())
        for (const bool foreignSpread : {false, true})
            for (const bool receive : {false, true}) {
                const Script::PreparedXccyScript_ prepared(Instrument(mode, foreignSpread, receive), Context(spec), Model(spec));
                const Script::XccyScriptPathProvider_<> provider(prepared);
                Vector_<AAD::XccyGaussianState_<double>> states;
                for (const auto& date : prepared.SampleDates())
                    states.push_back(State(date, TestXccy::TODAY));
                const auto path = provider.Observe(states);
                auto expected = Oracle(mode, foreignSpread, receive);
                SortPayments(&expected);
                for (const bool compiled : {false, true}) {
                    auto actual = prepared.EvaluatePayments(path, compiled);
                    SortPayments(&actual);
                    ASSERT_EQ(actual.size(), expected.size());
                    for (size_t i = 0; i < actual.size(); ++i) {
                        ASSERT_EQ(actual[i].date_, expected[i].date_);
                        ASSERT_NEAR(actual[i].discountedAmount_, expected[i].discountedAmount_, 1e-10);
                    }
                }
            }
}

TEST(XccyObservationTest, TestAtValuationPaymentSpotAndSuppliedResetUseDifferentSlots) {
    auto spec = TestXccy::Spec();
    spec.context_.valuationTime_ = DateTime_(Date_(2027, 8, 4));
    auto values = History();
    values["FX[USD/EUR]"][spec.context_.valuationTime_] = 1. / 1.6;
    spec.context_.fixings_ = Snapshot(values);
    auto config = TestXccy::Config(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    config.convention_.foreignLeg_.paymentFrequency_ = config.convention_.foreignIndex_.forecastTenor_ = PeriodLength_("3M");
    config.convention_.foreignLeg_.paymentLag_ = 0;
    config.foreignRateFixing_.indexName_ = "IR:EUR,LIBOR_3M_LCH";
    const auto instrument =
        NewXccyScriptInstrument(CrossCurrencySwap_(TestXccy::TODAY, TestXccy::START, TestXccy::END, .001, config), {"same-time", "USD.OIS"});
    const Script::PreparedXccyScript_ prepared(instrument, Context(spec), Model(spec));
    const auto path = Script::XccyScriptPathProvider_<>(prepared).Observe(States(prepared));
    const auto reset = RequestId(prepared, Script::XccyObservationKind_::Value_::ResetFxFixing, spec.context_.valuationTime_.Date());
    const auto spot = RequestId(prepared, Script::XccyObservationKind_::Value_::PaymentFxSpot, spec.context_.valuationTime_.Date());
    ASSERT_NE(reset, spot);
    ASSERT_DOUBLE_EQ(prepared.Observations().Read(reset, &path.Samples()), 1.6);
    ASSERT_DOUBLE_EQ(prepared.Observations().Read(spot, &path.Samples()), 1.1);
    ASSERT_TRUE(prepared.Observations().TryKnownValue(reset));
    ASSERT_FALSE(prepared.Observations().TryKnownValue(spot));
    const auto& uses = prepared.Observations().Requests()[reset].uses_;
    ASSERT_GE(uses.size(), 2U);
    ASSERT_GT(uses[0].source_.row_, 0U);
    ASSERT_FALSE(uses[0].indexOriginal_.empty());
    ASSERT_NEAR(Price(prepared, path, false), Price(prepared, path, true), 1e-12);
}

TEST(XccyObservationTest, TestAtValuationRatesSuppliedEmptyAndOmittedFreezeDifferentProvenance) {
    auto spec = TestXccy::Spec();
    spec.context_.valuationTime_ = DateTime_(Date_(2027, 4, 30));
    auto values = History();
    const HistoryScope_ globals(values);
    const auto instrument = Instrument(XccyNotionalMode_::Value_::RESETTABLE, false, true);
    Vector_<String_> identities;
    for (int mode = 0; mode < 3; ++mode) {
        spec.context_.fixings_ = mode == 0 ? Snapshot(values) : (mode == 1 ? Snapshot({}) : Handle_<MarketFixingSnapshot_>());
        const Script::PreparedXccyScript_ prepared(instrument, Context(spec), Model(spec));
        const auto path = Script::XccyScriptPathProvider_<>(prepared).Observe(States(prepared));
        const auto id = RequestId(prepared, Script::XccyObservationKind_::Value_::RateFixing, spec.context_.valuationTime_.Date());
        ASSERT_EQ(prepared.Observations().TryKnownValue(id).has_value(), mode == 0);
        const auto& request = prepared.Requests()[id];
        ASSERT_NEAR(prepared.Observations().Read(id, &path.Samples()),
                    mode == 0 ? .05
                              : Rate(false, spec.context_.valuationTime_.Date(), request.start_, request.end_, spec.context_.valuationTime_.Date()),
                    1e-12);
        identities.push_back(prepared.PlanIdentity());
    }
    ASSERT_NE(identities[0], identities[1]);
    ASSERT_NE(identities[1], identities[2]);
}

TEST(XccyObservationTest, TestOmittedHistoryIsCapturedOnceAndFutureSnapshotIsIgnored) {
    auto spec = TestXccy::Spec();
    spec.context_.valuationTime_ = DateTime_(Date_(2027, 8, 3));
    auto values = History();
    values["FX[EUR/USD]"][DateTime_(Date_(2027, 8, 4))] = 9.;
    const HistoryScope_ globals(values);
    spec.context_.fixings_.reset();
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true);
    const Script::PreparedXccyScript_ omitted(instrument, Context(spec), Model(spec));
    const auto before = Script::XccyScriptPathProvider_<>(omitted).Observe(States(omitted));
    auto changed = values;
    changed["IR:USD,LIBOR_3M_LCH"][DateTime_(Date_(2027, 8, 2))] = .99;
    const HistoryScope_ changedGlobals(changed);
    omitted.RequireCompatible(instrument, Context(spec));
    ASSERT_DOUBLE_EQ(Price(omitted, before, false), Price(omitted, Script::XccyScriptPathProvider_<>(omitted).Observe(States(omitted)), true));
    spec.context_.fixings_ = Snapshot(values);
    const Script::PreparedXccyScript_ supplied(instrument, Context(spec), Model(spec));
    const auto path = Script::XccyScriptPathProvider_<>(supplied).Observe(States(supplied));
    const auto reset = RequestId(supplied, Script::XccyObservationKind_::Value_::ResetFxFixing, Date_(2027, 8, 4));
    ASSERT_FALSE(supplied.Observations().TryKnownValue(reset));
    ASSERT_NEAR(supplied.Observations().Read(reset, &path.Samples()), Spot(Date_(2027, 8, 4), spec.context_.valuationTime_.Date()), 1e-12);
    spec.context_.fixings_ = Snapshot({});
    AssertError([&] { Script::PreparedXccyScript_(instrument, Context(spec), Model(spec)); }, "MissingFixing");
}

TEST(XccyObservationTest, TestStartedMtmKeepsPreviousNotionalButDropsSettledRateDependencies) {
    auto spec = TestXccy::Spec();
    spec.context_.valuationTime_ = DateTime_(Date_(2027, 11, 4));
    MarketFixingSnapshot_::values_t values{{"IR:USD,LIBOR_3M_LCH", {{DateTime_(Date_(2027, 8, 2)), .04}, {DateTime_(Date_(2027, 11, 2)), .05}}},
                                           {"IR:EUR,LIBOR_3M_LCH", {{DateTime_(Date_(2027, 8, 2)), .02}, {DateTime_(Date_(2027, 11, 2)), .03}}},
                                           {"FX[EUR/USD]", {{DateTime_(Date_(2027, 8, 4)), 1.25}}}};
    spec.context_.fixings_ = Snapshot(values);
    auto config = TestXccy::Config(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    config.convention_.spreadOnForeignLeg_ = false;
    config.convention_.foreignLeg_.paymentFrequency_ = config.convention_.foreignIndex_.forecastTenor_ = PeriodLength_("3M");
    config.convention_.foreignLeg_.paymentLag_ = 0;
    config.foreignRateFixing_.indexName_ = "IR:EUR,LIBOR_3M_LCH";
    const auto instrument =
        NewXccyScriptInstrument(CrossCurrencySwap_(TestXccy::TODAY, TestXccy::START, Date_(2028, 2, 4), .001, config), {"started", "USD.OIS"});
    const Script::PreparedXccyScript_ prepared(instrument, Context(spec), Model(spec));
    const auto path = Script::XccyScriptPathProvider_<>(prepared).Observe(States(prepared));
    const auto reset = RequestId(prepared, Script::XccyObservationKind_::Value_::ResetFxFixing, Date_(2027, 8, 4));
    ASSERT_DOUBLE_EQ(prepared.Observations().Read(reset, &path.Samples()), 1.25);
    for (const auto& request : prepared.Observations().Requests())
        ASSERT_NE(request.key_.fixingTime_.Date(), Date_(2027, 4, 30));
    const auto payments = prepared.EvaluatePayments(path, true);
    ASSERT_NEAR(Price(prepared, path, false), Price(prepared, path, true), 1e-12);
    bool delta = false;
    for (const auto& payment : payments)
        delta = delta || (payment.date_ == Date_(2027, 11, 4) && std::abs(payment.discountedAmount_ - 15.) < 1e-12);
    ASSERT_TRUE(delta);
    values["FX[EUR/USD]"].clear();
    spec.context_.fixings_ = Snapshot(values);
    AssertError([&] { Script::PreparedXccyScript_(instrument, Context(spec), Model(spec)); }, "MissingFixing");
}

TEST(XccyObservationTest, TestCandidateMismatchFailsBeforeAadTapeAllocation) {
    const auto spec = TestXccy::Spec();
    const auto instrument = Instrument(XccyNotionalMode_::Value_::FIXED, false, true);
    const Script::PreparedXccyScript_ prepared(instrument, Context(spec), Model(spec));
    auto changed = spec;
    changed.context_.valuationTime_ = DateTime_(TestXccy::TODAY.AddDays(1));
    const auto candidate = Model(changed);
    const TapeGuard_ guard(AAD::Tape());
    const int before = AAD::Tape()->nodes_.Size();
    AssertError([&] { Script::XccyScriptPathProvider_<AAD::Number_>(prepared, candidate); }, "CalibrationContextMismatch");
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), before);
}

TEST(XccyObservationTest, TestProviderFailureOwnsCandidateAndEvaluateContext) {
    const auto spec = TestXccy::Spec();
    const auto data = Model(spec);
    const auto instrument = Instrument(XccyNotionalMode_::Value_::FIXED, false, true);
    const Script::PreparedXccyScript_ prepared(instrument, Context(spec), data);
    auto states = States(prepared);
    states.back().values_[4] = 1e4;
    std::shared_ptr<const CalibrationFailure_> saved;
    try {
        (void)Script::XccyScriptPathProvider_<>(prepared).Observe(states);
        FAIL() << "expected numerical overflow";
    } catch (const CalibrationError_& error) {
        saved = std::make_shared<const CalibrationFailure_>(error.Failure());
    }
    ASSERT_TRUE(saved);
    ASSERT_EQ(saved->candidate_.get(), data.get());
    ASSERT_EQ(saved->diagnostic_.stage_, CalibrationStage_::Value_::Evaluate);
    ASSERT_EQ(saved->diagnostic_.instrumentId_, instrument->Id());
    ASSERT_EQ(saved->diagnostic_.measureId_, "Price");
    ASSERT_EQ(saved->diagnostic_.eventDate_, states.back().date_);
    ASSERT_TRUE(saved->report_);
}

TEST(XccyObservationTest, TestPrincipalSwitchesDoNotIntroduceUnrequestedExchanges) {
    const auto spec = TestXccy::Spec();
    const auto mode = XccyNotionalMode_::Value_::MARK_TO_MARKET;
    for (const bool initial : {false, true})
        for (const bool final : {false, true}) {
            const Script::PreparedXccyScript_ prepared(Instrument(mode, false, true, initial, final), Context(spec), Model(spec));
            const auto path = Script::XccyScriptPathProvider_<>(prepared).Observe(States(prepared));
            auto expected = Oracle(mode, false, true, initial, final);
            SortPayments(&expected);
            for (const bool compiled : {false, true}) {
                auto actual = prepared.EvaluatePayments(path, compiled);
                SortPayments(&actual);
                ASSERT_EQ(actual.size(), expected.size());
                for (size_t i = 0; i < actual.size(); ++i) {
                    ASSERT_EQ(actual[i].date_, expected[i].date_);
                    ASSERT_NEAR(actual[i].discountedAmount_, expected[i].discountedAmount_, 1e-10);
                }
            }
        }
}

TEST(XccyObservationTest, TestOneLegSettledAndAllExpiredKeepOnlyOutstandingCashflowsWithoutProjection) {
    auto spec = TestXccy::Spec();
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true);
    for (const Date_ valuation : {Date_(2027, 11, 5), Date_(2027, 11, 9)}) {
        spec.context_.valuationTime_ = DateTime_(valuation);
        spec.context_.fixings_ =
            valuation == Date_(2027, 11, 5) ? Snapshot({{"IR:EUR,LIBOR_6M_LCH", {{DateTime_(Date_(2027, 4, 30)), .03}}}}) : Snapshot({});
        const Script::PreparedXccyScript_ prepared(instrument, Context(spec), Model(spec, false, true));
        const Script::XccyScriptPathProvider_<> provider(prepared);
        const auto path = provider.GeneratePath(Vector_<>(provider.Model().SimDim(), 0.));
        for (const bool compiled : {false, true}) {
            const auto payments = prepared.EvaluatePayments(path, compiled);
            if (valuation == Date_(2027, 11, 5)) {
                ASSERT_EQ(payments.size(), 1U);
                ASSERT_EQ(payments[0].date_, Date_(2027, 11, 8));
                const double expected = 100. * ((TestXccy::END - TestXccy::START) / 360.) * .03 * 1.1 * std::exp(-.017 * (3. / 365.));
                ASSERT_NEAR(payments[0].discountedAmount_, expected, 1e-12);
            } else {
                ASSERT_TRUE(payments.empty());
                ASSERT_DOUBLE_EQ(Price(prepared, path, compiled), 0.);
                ASSERT_EQ(provider.Model().SimDim(), 0U);
            }
        }
    }
}

TEST(XccyObservationTest, TestSourceDatesBindingsAndMappingCannotReuseStalePaths) {
    const auto spec = TestXccy::Spec();
    const auto source = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true);
    const Script::PreparedXccyScript_ original(source, Context(spec), Model(spec));
    const auto path = Script::XccyScriptPathProvider_<>(original).Observe(States(original));
    auto definition = source->Definition()->Spec();
    for (auto& row : definition.source_.rows_)
        if (row.key_ == "2027-11-08")
            row.key_ = "2027-11-09";
    const auto changedDate = NewScriptCalibrationInstrument({"date-mutant", NewScriptInstrumentDefinition(definition), {}, {}});
    const auto changedN = BindCalibrationInstrumentParameters(source, {{{"N", 220., CalibrationValueUnit_::Parse("Price:USD")}}, {}});
    for (const auto& changed : {changedDate, changedN}) {
        AssertError([&] { original.RequireCompatible(changed, Context(spec)); }, "InvalidCalibrationPlan");
        const Script::PreparedXccyScript_ next(changed, Context(spec), Model(spec));
        ASSERT_NE(next.PlanIdentity(), original.PlanIdentity());
        AssertError([&] { static_cast<void>(next.EvaluatePayments(path, false)); }, "InvalidCalibrationPlan");
        const auto nextPath = Script::XccyScriptPathProvider_<>(next).Observe(States(next));
        if (changed == changedN)
            ASSERT_NEAR(Price(next, nextPath, true), 2. * Price(original, path, false), 1e-11);
        else
            ASSERT_NE(Price(next, nextPath, true), Price(original, path, false));
    }
    definition = source->Definition()->Spec();
    definition.source_.rows_.back().key_ = "2027-08-05";
    const auto movedMtm = NewScriptCalibrationInstrument({"moved-mtm", NewScriptInstrumentDefinition(definition), {}, {}});
    AssertError([&] { Script::PreparedXccyScript_(movedMtm, Context(spec), Model(spec)); }, "UnsupportedFastEvaluator");
}

TEST(XccyObservationTest, TestPolicyCollateralRoutesAndIntradayRefuseBeforeTapeOrWorkers) {
    const auto spec = TestXccy::Spec();
    const auto instrument = Instrument(XccyNotionalMode_::Value_::FIXED, false, true);
    const auto data = Model(spec);
    const TapeGuard_ guard(AAD::Tape());
    const auto count = AAD::Tape()->nodes_.Size();
    for (const auto policy : {TodayFixingPolicy_::Value_::MODEL, TodayFixingPolicy_::Value_::REQUIREHISTORICAL}) {
        auto changed = spec;
        changed.context_.todayFixingOverride_ = policy;
        AssertError([&] { Script::PreparedXccyScript_(instrument, Context(changed), data); }, "InvalidCalibrationFixingPolicy");
    }
    auto changed = spec;
    changed.context_.valuationTime_ = DateTime_(TestXccy::TODAY, .25);
    AssertError([&] { Script::PreparedXccyScript_(instrument, Context(changed), data); }, "UnsupportedCalibrationFixingTime");
    changed = spec;
    changed.context_.collateralCurrency_ = Ccy_("EUR");
    AssertError([&] { Script::PreparedXccyScript_(instrument, Context(changed), data); }, "CalibrationContextMismatch");
    changed = spec;
    changed.context_.discountRoute_ = "another-route";
    AssertError([&] { Script::PreparedXccyScript_(instrument, Context(changed), data); }, "CalibrationContextMismatch");
    AssertError([&] { Script::PreparedXccyScript_(instrument, Context(spec), Model(spec, false)); }, "CalibrationContextMismatch");
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), count);
}

TEST(XccyObservationTest, TestGeneratedFiveSlotPathsAndStateShapeAreChecked) {
    const auto spec = TestXccy::Spec();
    const Script::PreparedXccyScript_ prepared(Instrument(XccyNotionalMode_::Value_::RESETTABLE, false, true), Context(spec), Model(spec));
    const Script::XccyScriptPathProvider_<> provider(prepared);
    const Vector_<> normals(provider.Model().SimDim(), .17);
    const auto path = provider.GeneratePath(normals);
    ASSERT_NEAR(Price(prepared, path, true), Price(prepared, path, false), 1e-12);
    ASSERT_EQ(provider.Model().SimDim(), 5 * (prepared.SampleDates().size() - 1));
    AssertError([&] { static_cast<void>(provider.GeneratePath(Vector_<>(normals.size() + 1, .17))); }, "InvalidCalibrationPlan");
    auto states = States(prepared);
    states.pop_back();
    AssertError([&] { static_cast<void>(provider.Observe(states)); }, "InvalidCalibrationPlan");
    states = States(prepared);
    states[1].date_ = states[1].date_.AddDays(1);
    AssertError([&] { static_cast<void>(provider.Observe(states)); }, "InvalidCalibrationPlan");
}

TEST(XccyObservationTest, TestAadTreeCompiledAndCandidateSpotFiniteDifference) {
    auto spec = TestXccy::Spec();
    spec.context_.valuationTime_ = DateTime_(Date_(2027, 8, 4));
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true);
    for (const bool suppliedReset : {false, true}) {
        auto values = History();
        if (suppliedReset)
            values["FX[EUR/USD]"][spec.context_.valuationTime_] = 1.6;
        spec.context_.fixings_ = Snapshot(values);
        const Script::PreparedXccyScript_ prepared(instrument, Context(spec), Model(spec));
        const auto value = [&](double spot) {
            auto bumped = spec;
            bumped.fxSpot_ = spot;
            const Script::XccyScriptPathProvider_<> provider(prepared, Model(bumped));
            return Price(prepared, provider.GeneratePath(Vector_<>(provider.Model().SimDim(), .17)), true);
        };
        const double fd = (value(1.10001) - value(1.09999)) / .00002;
        for (const bool compiled : {false, true}) {
            const TapeGuard_ guard(AAD::Tape());
            const Script::XccyScriptPathProvider_<AAD::Number_> provider(prepared);
            auto result = Price(prepared, provider.GeneratePath(Vector_<>(provider.Model().SimDim(), .17)), compiled);
            AAD::Adjoint(result) = 1.;
            AAD::PropagateToStart(*AAD::Tape());
            ASSERT_NEAR(static_cast<double>(result), value(1.1), 1e-11);
            ASSERT_NEAR(AAD::Adjoint(*provider.Model().Parameters()[0]), fd, 1e-7);
        }
    }
}

TEST(XccyObservationTest, TestSixMonthDomesticThreeMonthForeignAndLaggedResetOracle) {
    const auto spec = TestXccy::Spec();
    const Date_ valuation = TestXccy::TODAY, start = TestXccy::START, reset(2027, 11, 4), resetFix(2027, 11, 2), end(2028, 5, 4);
    for (const bool foreignSpread : {false, true})
        for (const bool receive : {false, true}) {
            auto config = TestXccy::Config(XccyNotionalMode_::Value_::MARK_TO_MARKET);
            config.convention_.domesticLeg_.paymentFrequency_ = config.convention_.domesticIndex_.forecastTenor_ = PeriodLength_("6M");
            config.convention_.foreignLeg_.paymentFrequency_ = config.convention_.foreignIndex_.forecastTenor_ = PeriodLength_("3M");
            config.domesticRateFixing_.indexName_ = "IR:USD,LIBOR_6M_LCH";
            config.foreignRateFixing_.indexName_ = "IR:EUR,LIBOR_3M_LCH";
            config.fxReset_.fixingLag_ = 2;
            config.convention_.spreadOnForeignLeg_ = foreignSpread;
            const auto instrument = NewXccyScriptInstrument(CrossCurrencySwap_(valuation, start, end, .001, config), {"6m-3m", "USD.OIS", receive});
            const Script::PreparedXccyScript_ prepared(instrument, Context(spec), Model(spec));
            const auto path = Script::XccyScriptPathProvider_<>(prepared).Observe(States(prepared));
            const double sign = Direction(foreignSpread, receive);
            const double later = 100. * Spot(resetFix, valuation);
            Vector_<Script::XccyScriptPayment_<double>> expected;
            const auto pay = [&](const Date_& date, double amount) { expected.push_back({date, amount * Discount(date, valuation)}); };
            pay(reset,
                sign * 110. * ((reset - start) / 360.) * (Rate(false, Date_(2027, 4, 30), start, reset, valuation) + Spread(foreignSpread, false)));
            pay(end, sign * later * ((end - reset) / 360.) * (Rate(false, resetFix, reset, end, valuation) + Spread(foreignSpread, false)));
            const Vector_<Date_> accrual{start, Date_(2027, 8, 4), reset, Date_(2028, 2, 4), end};
            const Vector_<Date_> fixing{Date_(2027, 4, 30), Date_(2027, 8, 2), resetFix, Date_(2028, 2, 2)};
            const Vector_<Date_> payment{Date_(2027, 8, 6), Date_(2027, 11, 8), Date_(2028, 2, 8), Date_(2028, 5, 8)};
            for (size_t i = 0; i < 4; ++i)
                pay(payment[i], -sign * 100. * ((accrual[i + 1] - accrual[i]) / 360.) *
                                    (Rate(true, fixing[i], accrual[i], accrual[i + 1], valuation) + Spread(foreignSpread, true)) *
                                    Spot(payment[i], valuation));
            pay(start, -sign * 110.);
            pay(start, sign * 100. * Spot(start, valuation));
            pay(end, sign * later);
            pay(end, -sign * 100. * Spot(end, valuation));
            pay(reset, sign * (later - 110.));
            SortPayments(&expected);
            for (const bool compiled : {false, true}) {
                auto actual = prepared.EvaluatePayments(path, compiled);
                SortPayments(&actual);
                ASSERT_EQ(actual.size(), expected.size());
                for (size_t i = 0; i < actual.size(); ++i) {
                    ASSERT_EQ(actual[i].date_, expected[i].date_);
                    ASSERT_NEAR(actual[i].discountedAmount_, expected[i].discountedAmount_, 1e-10);
                }
            }
        }
}

TEST(XccyObservationTest, TestFrozenConventionsRequireRepreparation) {
    const auto spec = TestXccy::Spec();
    const auto instrument = Instrument(XccyNotionalMode_::Value_::FIXED, false, true);
    const auto model = Model(spec);
    const Script::PreparedXccyScript_ prepared(instrument, Context(spec), model);
    auto changed = Context(spec);
    changed.conventionState_ = "different-conventions";
    AssertError([&] { Script::PreparedXccyScript_(instrument, changed, model); }, "CalibrationContextMismatch");
    AssertError([&] { prepared.RequireCompatible(instrument, changed); }, "CalibrationContextMismatch");
}

TEST(XccyObservationTest, TestPreparationErrorsOwnInstrumentAndExactIntradayConstraint) {
    const auto spec = TestXccy::Spec();
    const auto instrument = Instrument(XccyNotionalMode_::Value_::FIXED, false, true);
    try {
        Script::PreparedXccyScript_(instrument, Context(spec), Model(spec, false));
        FAIL() << "expected missing live projection";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.instrumentId_, instrument->Id());
        ASSERT_EQ(error.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Prepare);
        ASSERT_EQ(error.Failure().report_->Errors()[0].instrumentId_, instrument->Id());
    }
    auto changed = spec;
    changed.context_.valuationTime_ = DateTime_(TestXccy::TODAY, .25);
    try {
        Script::PreparedXccyScript_(instrument, Context(changed), Model(spec));
        FAIL() << "expected intraday refusal";
    } catch (const CalibrationError_& error) {
        const auto& d = error.Failure().diagnostic_;
        ASSERT_EQ(d.actual_, DateTime::ToString(changed.context_.valuationTime_));
        ASSERT_TRUE(d.expected_);
        ASSERT_NE(d.expected_->find("midnight"), String_::npos);
    }
}

TEST(XccyObservationTest, TestZeroDynamicsMatchesLegacyOnlyInItsCommonSupport) {
    const auto spec = TestXccy::Spec();
    const auto domestic = Curve("USD", TestXccy::TODAY, .04), foreign = Curve("EUR", TestXccy::TODAY, .02);
    const auto domesticProjection = Curve("USD", TestXccy::TODAY, .045), foreignProjection = Curve("EUR", TestXccy::TODAY, .026);
    const auto basis = Curve("USD", TestXccy::TODAY, .003);
    Tape::JointCurveBlock_<double> domesticBlock, foreignBlock;
    domesticBlock.discountCurves[CollateralType_("OIS")] = domestic.get();
    foreignBlock.discountCurves[CollateralType_("OIS")] = foreign.get();
    domesticBlock.forwardCurves[PeriodLength_("3M")] = domesticProjection.get();
    foreignBlock.forwardCurves[PeriodLength_("6M")] = foreignProjection.get();
    const XccyMarketView_<double> view{
        spec.context_.valuationTime_, spec.context_.pair_, Ccy_("USD"), 1.1, &domesticBlock, &foreignBlock, basis.get()};
    for (const auto mode : XccyNotionalModeListAll())
        for (const bool foreignSpread : {false, true})
            for (const bool receive : {false, true}) {
                const auto instrument = Instrument(mode, foreignSpread, receive);
                const Script::PreparedXccyScript_ prepared(instrument, Context(spec), Model(spec, true, true));
                const Script::XccyScriptPathProvider_<> provider(prepared);
                const auto path = provider.GeneratePath(Vector_<>(provider.Model().SimDim(), 0.));
                const auto legacy = PrepareCalibrationInstrument(instrument, "Price", Context(spec));
                for (const bool compiled : {false, true})
                    ASSERT_NEAR(Price(prepared, path, compiled), EvaluateXccyScript(legacy, view), 1e-10);
            }
}

TEST(XccyObservationTest, TestCandidateEvaluationIdentityAndFrozenSource) {
    auto spec = TestXccy::Spec();
    const Script::PreparedXccyScript_ prepared(Instrument(XccyNotionalMode_::Value_::FIXED, false, true), Context(spec), Model(spec));
    const auto original = Script::XccyScriptPathProvider_<>(prepared).Observe(States(prepared));
    spec.fxSpot_ = 1.2;
    const auto candidate = Script::XccyScriptPathProvider_<>(prepared, Model(spec)).Observe(States(prepared));
    ASSERT_NE(original.EvaluationIdentity(), candidate.EvaluationIdentity());
    ASSERT_NE(Price(prepared, original, false), Price(prepared, candidate, false));
    const auto repeated = Script::XccyScriptPathProvider_<>(prepared).Observe(States(prepared));
    ASSERT_EQ(original.EvaluationIdentity(), repeated.EvaluationIdentity());
    ASSERT_DOUBLE_EQ(Price(prepared, original, true), Price(prepared, repeated, true));
}

TEST(XccyObservationTest, TestLeadingReceiverZeroDoesNotAddAnInnovationInterval) {
    const auto spec = TestXccy::Spec();
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true);
    auto definition = instrument->Definition()->Spec();
    definition.source_.rows_.push_back({ScriptSourceRowKind_::Value_::Event, "2027-01-04", "PAY = 0"});
    const auto initialized = NewScriptCalibrationInstrument({"leading-zero", NewScriptInstrumentDefinition(definition), {}, {}});
    const Script::PreparedXccyScript_ original(instrument, Context(spec), Model(spec));
    const Script::PreparedXccyScript_ prepared(initialized, Context(spec), Model(spec));
    ASSERT_EQ(original.SampleDates(), prepared.SampleDates());
    const Script::XccyScriptPathProvider_<> provider(prepared);
    const auto path = provider.GeneratePath(Vector_<>(provider.Model().SimDim(), .17));
    const Script::XccyScriptPathProvider_<> originalProvider(original);
    const auto reference = originalProvider.GeneratePath(Vector_<>(originalProvider.Model().SimDim(), .17));
    ASSERT_EQ(provider.Model().SimDim(), originalProvider.Model().SimDim());
    for (const bool compiled : {false, true})
        ASSERT_DOUBLE_EQ(Price(prepared, path, compiled), Price(original, reference, compiled));
}
