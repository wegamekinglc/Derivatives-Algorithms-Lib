#include <gtest/gtest.h>

#include <iomanip>
#include <limits>
#include <sstream>

#include "xccygaussianexecutionfixture.hpp"
#include "xccygaussianexecutionreference.hpp"
#include "xccygaussianrecovery.hpp"

using namespace Dal;
using namespace Dal::TestXccyMoments;
using namespace Dal::TestXccyExecution;

namespace {
    void AssertPayments(Payments_ actual, Payments_ expected) {
        const auto sort = [](Payments_* values) {
            std::sort(values->begin(), values->end(), [](const auto& a, const auto& b) {
                return std::make_pair(a.date_, a.discountedAmount_) < std::make_pair(b.date_, b.discountedAmount_);
            });
        };
        sort(&actual);
        sort(&expected);
        ASSERT_EQ(actual.size(), expected.size());
        for (size_t i = 0; i < actual.size(); ++i) {
            ASSERT_EQ(actual[i].date_, expected[i].date_);
            ASSERT_NEAR(actual[i].discountedAmount_, expected[i].discountedAmount_, 1e-10);
        }
    }
    void AssertDates(const std::map<Date_, double>& actual, const std::map<Date_, double>& expected) {
        ASSERT_EQ(actual.size(), expected.size());
        for (const auto& [date, value] : expected) {
            ASSERT_EQ(actual.count(date), 1U);
            ASSERT_NEAR(actual.at(date), value, 1e-10);
        }
    }
    void AssertExpectation(const XccyGaussianMomentValues_<double>& value, const Expectation_& reference) {
        const std::array<double, 4> values{value.a_, value.b_, value.price_, -value.b_ / value.a_};
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& q = reference.quantities_[i];
            const long double budget = i == 3 ? 1e-12L : 1e-10L, floor = i == 3 ? 1e-13L : 1e-11L;
            const long double rounding = std::numeric_limits<long double>::epsilon() * std::max(std::abs(q[0]), std::abs(q[2]));
            const long double oracleIndicator = q[1] + rounding;
            const long double fastIndicator = 4.L * std::max(std::abs(static_cast<long double>(values[i]) - q[2]) + rounding, q[3] + rounding);
            std::ostringstream numeric;
            numeric << std::setprecision(22) << values[i] << ' ' << q[0] << ' ' << oracleIndicator << ' ' << fastIndicator;
            ::testing::Test::RecordProperty(std::string(reference.mode_) + '_' + std::to_string(reference.foreign_) + '_' +
                                                std::to_string(reference.receive_) + '_' + std::to_string(reference.zero_) + '_' +
                                                std::to_string(reference.nonflat_) + '_' + std::to_string(i),
                                            numeric.str());
            ASSERT_LE(oracleIndicator, budget / 4.);
            ASSERT_LE(fastIndicator, budget / 4.);
            ASSERT_LE(std::abs(static_cast<long double>(values[i]) - q[0]), oracleIndicator + fastIndicator + floor);
            ASSERT_LE(oracleIndicator + fastIndicator + floor, budget);
        }
    }
    void AssertPath(const Handle_<CalibrationInstrument_>& instrument,
                    const XccyGaussianModelSpec_& spec,
                    const CalibrationPreparationContext_& context,
                    XccyNotionalMode_ mode,
                    bool foreign,
                    bool receive,
                    double shift,
                    double spread = .001,
                    double notional = 110.,
                    bool initial = true,
                    bool final = true) {
        const Handle_<XccyGaussianModelData_> model(new XccyGaussianModelData_("execution", spec));
        const auto moment = LowerXccyGaussianMoments(PrepareMoment(instrument, context));
        const Script::PreparedXccyScript_ script(instrument, context, model);
        ASSERT_EQ(script.Instrument(), moment.Prepared().Instrument());
        const Script::XccyScriptPathProvider_<> provider(script);
        const auto states = provider.Model().GenerateStates(Normals(provider.Model().SimDim(), shift));
        const auto path = provider.Observe(states);
        const CashflowReference_ coarse(spec, states, context.xccy_->fixings_, 32), fine(spec, states, context.xccy_->fixings_, 64);
        const auto expected = fine.Payments(mode, foreign, receive, spread, notional, initial, final);
        AssertPayments(coarse.Payments(mode, foreign, receive, spread, notional, initial, final), expected);
        const auto lowered = RealizedTerms(moment, provider.Model(), states, spread);
        AssertDates(lowered, ByDate(expected));
        for (const bool compiled : {false, true})
            AssertPayments(script.EvaluatePayments(path, compiled), expected);
    }
} // namespace

TEST(XccyGaussianExecutionTest, TestSameSourceTreeCompiledTermsAndIndependentExpectations) {
    for (const auto& reference : Expectations()) {
        SCOPED_TRACE(reference.mode_);
        SCOPED_TRACE(reference.foreign_);
        SCOPED_TRACE(reference.receive_);
        SCOPED_TRACE(reference.zero_);
        SCOPED_TRACE(reference.nonflat_);
        const auto spec = reference.nonflat_ ? NonflatSpec() : ModelSpec(TestXccy::TODAY, reference.zero_);
        const auto instrument = Instrument(XccyNotionalMode_(reference.mode_), reference.foreign_, reference.receive_);
        const auto context = Context(spec, Empty());
        for (const double shift : {-.6, 0., .7})
            AssertPath(instrument, spec, context, XccyNotionalMode_(reference.mode_), reference.foreign_, reference.receive_, shift);
        const XccyGaussianModelData_ model("expectation", spec);
        const auto value = EvaluateXccyGaussianMoments<double>(PrepareMoment(instrument, context), model);
        AssertExpectation(value, reference);
        ASSERT_NEAR(value.Value(PrepareMoment(instrument, context, "BasisSpread")), -value.b_ / value.a_, 1e-12);
        if (!reference.zero_)
            continue;
        const Handle_<XccyGaussianModelData_> data(new XccyGaussianModelData_("zero", spec));
        const Script::PreparedXccyScript_ script(instrument, context, data);
        const Script::XccyScriptPathProvider_<> provider(script);
        const auto path = provider.GeneratePath(Normals(provider.Model().SimDim(), .3));
        const Script::PreparedXccyScript_ zero(Rebind(instrument, 0.), context, data), unit(Rebind(instrument, 1.), context, data);
        for (const bool compiled : {false, true}) {
            ASSERT_NEAR(Sum(script.EvaluatePayments(path, compiled)), value.price_, 1e-10);
            const double b = ZeroPrice(zero, compiled), a = ZeroPrice(unit, compiled) - b;
            ASSERT_NEAR(b, value.b_, 1e-10);
            ASSERT_NEAR(a, value.a_, 1e-10);
            ASSERT_NEAR(-b / a, -value.b_ / value.a_, 1e-12);
        }
    }
}

TEST(XccyGaussianExecutionTest, TestPrincipalSwitchesAndImmutableRebindingReachEveryBackend) {
    const auto spec = ModelSpec(TestXccy::TODAY, true);
    const auto context = Context(spec, Empty());
    for (const auto mode : XccyNotionalModeListAll())
        for (const bool initial : {false, true})
            for (const bool final : {false, true}) {
                auto config = TestXccy::Config(mode);
                config.convention_.spreadOnForeignLeg_ = false;
                config.convention_.initialNotionalExchange_ = initial;
                config.convention_.finalNotionalExchange_ = final;
                const auto instrument = NewXccyScriptInstrument(CrossCurrencySwap_(TestXccy::TODAY, TestXccy::START, TestXccy::END, .001, config),
                                                                {"principal-switch", "USD.OIS"});
                const auto rebound = Rebind(instrument, .002, 220.);
                AssertPath(rebound, spec, context, mode, false, true, .3, .002, 220., initial, final);
                const XccyGaussianModelData_ model("binding", spec);
                const auto before = EvaluateXccyGaussianMoments<double>(PrepareMoment(instrument, context), model);
                const auto after = EvaluateXccyGaussianMoments<double>(PrepareMoment(rebound, context), model);
                ASSERT_NEAR(after.price_, 2. * (before.b_ + .002 * before.a_), 1e-10);
                ASSERT_EQ(EvaluateXccyGaussianMoments<double>(PrepareMoment(instrument, context), model).price_, before.price_);
            }
}

TEST(XccyGaussianExecutionTest, TestHistoricalAtValuationResetAndFutureSnapshotParity) {
    const auto mode = XccyNotionalMode_::Value_::MARK_TO_MARKET;
    const auto instrument = Instrument(mode);
    const auto spec = ModelSpec(Date_(2027, 8, 4), true);
    MarketFixingSnapshot_::values_t values{{"IR:USD,LIBOR_3M_LCH", {{DateTime_(Date_(2027, 4, 30)), .05}, {DateTime_(Date_(2027, 8, 2)), .06}}},
                                           {"IR:EUR,LIBOR_6M_LCH", {{DateTime_(Date_(2027, 4, 30)), .03}}}};
    const Handle_<XccyGaussianModelData_> model(new XccyGaussianModelData_("history", spec));
    const auto missing = Context(spec, Empty());
    ASSERT_THROW(static_cast<void>(PrepareMoment(instrument, missing)), CalibrationError_);
    ASSERT_THROW(static_cast<void>(Script::PreparedXccyScript_(instrument, missing, model)), CalibrationError_);
    double activePrice = 0.;
    for (const bool supplied : {false, true}) {
        auto suppliedValues = values;
        if (supplied)
            suppliedValues["FX[EUR/USD]"][spec.market_.valuationTime_] = 1.6;
        suppliedValues["FX[EUR/USD]"][DateTime_(Date_(2027, 11, 4))] = 99.;
        suppliedValues["IR:USD,LIBOR_3M_LCH"][DateTime_(Date_(2027, 11, 4))] = 99.;
        const auto context = Context(spec, Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(suppliedValues)));
        AssertPath(instrument, spec, context, mode, false, true, .4);
        const Script::PreparedXccyScript_ script(instrument, context, model);
        const Script::XccyScriptPathProvider_<> provider(script);
        const auto path = provider.GeneratePath(Normals(provider.Model().SimDim(), .4));
        const auto result = EvaluateXccyGaussianMoments<double>(PrepareMoment(instrument, context), *model);
        ASSERT_NEAR(result.price_, Sum(script.EvaluatePayments(path, true)), 1e-10);
        if (supplied)
            ASSERT_GT(std::abs(result.price_ - activePrice), 1.);
        else
            activePrice = result.price_;
    }
}

TEST(XccyGaussianExecutionTest, TestOmittedHistoryFreezesPastAndExcludesGlobalAtValuationReset) {
    const auto spec = ModelSpec(Date_(2027, 8, 4), true);
    const Handle_<XccyGaussianModelData_> model(new XccyGaussianModelData_("omitted", spec));
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const ScopedHistory_ usd("IR:USD,LIBOR_3M_LCH", {{{DateTime_(Date_(2027, 4, 30)), .05}, {DateTime_(Date_(2027, 8, 2)), .06}}});
    const ScopedHistory_ eur("IR:EUR,LIBOR_6M_LCH", {{{DateTime_(Date_(2027, 4, 30)), .03}}});
    const ScopedHistory_ fx("FX[EUR/USD]", {{{spec.market_.valuationTime_, 1.9}}});
    const auto omitted = Context(spec);
    const auto moment = PrepareMoment(instrument, omitted);
    const auto program = LowerXccyGaussianMoments(moment);
    const Script::PreparedXccyScript_ script(instrument, omitted, model);
    MarketFixingSnapshot_::values_t values{{"IR:USD,LIBOR_3M_LCH", {{DateTime_(Date_(2027, 4, 30)), .05}, {DateTime_(Date_(2027, 8, 2)), .06}}},
                                           {"IR:EUR,LIBOR_6M_LCH", {{DateTime_(Date_(2027, 4, 30)), .03}}}};
    const auto suppliedPast = Context(spec, Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(values)));
    const Script::PreparedXccyScript_ explicitScript(instrument, suppliedPast, model);
    const auto before = EvaluateXccyGaussianMoments<double>(program, *model);
    ASSERT_NE(moment.ContextIdentity(), PrepareMoment(instrument, suppliedPast).ContextIdentity());
    usd.Set({{{DateTime_(Date_(2027, 4, 30)), .5}, {DateTime_(Date_(2027, 8, 2)), .6}}});
    eur.Set({{{DateTime_(Date_(2027, 4, 30)), .3}}});
    fx.Set({{{spec.market_.valuationTime_, 2.1}}});
    for (const bool compiled : {false, true}) {
        ASSERT_NEAR(ZeroPrice(script, compiled), before.price_, 1e-10);
        ASSERT_NEAR(ZeroPrice(script, compiled), ZeroPrice(explicitScript, compiled), 1e-10);
    }
    ASSERT_EQ(EvaluateXccyGaussianMoments<double>(program, *model).price_, before.price_);
}

TEST(XccyGaussianExecutionTest, TestAtValuationRatesSuppliedOrActiveAcrossBackends) {
    const auto spec = ModelSpec(Date_(2027, 4, 30), true);
    const Handle_<XccyGaussianModelData_> model(new XccyGaussianModelData_("at-fixing", spec));
    const auto instrument = Instrument(XccyNotionalMode_::Value_::RESETTABLE);
    const ScopedHistory_ usd("IR:USD,LIBOR_3M_LCH", {{{spec.market_.valuationTime_, .9}}});
    const ScopedHistory_ eur("IR:EUR,LIBOR_6M_LCH", {{{spec.market_.valuationTime_, .8}}});
    const auto empty = Context(spec, Empty());
    const auto active = EvaluateXccyGaussianMoments<double>(PrepareMoment(instrument, empty), *model);
    const Script::PreparedXccyScript_ omittedScript(instrument, Context(spec), model);
    ASSERT_NEAR(ZeroPrice(omittedScript, false), active.price_, 1e-10);
    ASSERT_NEAR(ZeroPrice(omittedScript, true), active.price_, 1e-10);
    MarketFixingSnapshot_::values_t values{{"IR:USD,LIBOR_3M_LCH", {{spec.market_.valuationTime_, .051}}},
                                           {"IR:EUR,LIBOR_6M_LCH", {{spec.market_.valuationTime_, .031}}}};
    const auto supplied = Context(spec, Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(values)));
    for (const auto& context : {empty, supplied}) {
        AssertPath(instrument, spec, context, XccyNotionalMode_::Value_::RESETTABLE, false, true, .1);
        const Script::PreparedXccyScript_ script(instrument, context, model);
        const auto result = EvaluateXccyGaussianMoments<double>(PrepareMoment(instrument, context), *model);
        ASSERT_NEAR(ZeroPrice(script, false), result.price_, 1e-10);
        ASSERT_NEAR(ZeroPrice(script, true), result.price_, 1e-10);
    }
    ASSERT_GT(std::abs(EvaluateXccyGaussianMoments<double>(PrepareMoment(instrument, supplied), *model).price_ - active.price_), .01);
}

TEST(XccyGaussianExecutionTest, TestSettledBoundariesRetainPriceAndParRefusals) {
    for (const auto& valuation : {Date_(2027, 11, 5), Date_(2027, 11, 9)})
        for (const bool foreign : {false, true}) {
            const auto spec = ModelSpec(valuation, true);
            const Handle_<XccyGaussianModelData_> model(new XccyGaussianModelData_("settled", spec));
            const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, foreign);
            MarketFixingSnapshot_::values_t history{{"IR:EUR,LIBOR_6M_LCH", {{DateTime_(Date_(2027, 4, 30)), .031}}}};
            const auto context = Context(spec, Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(history)));
            AssertPath(instrument, spec, context, XccyNotionalMode_::Value_::MARK_TO_MARKET, foreign, true, .2);
            const Script::PreparedXccyScript_ script(instrument, context, model);
            const Script::XccyScriptPathProvider_<> provider(script);
            const auto path = provider.GeneratePath(Normals(provider.Model().SimDim(), .2));
            const auto price = PrepareMoment(instrument, context), par = PrepareMoment(instrument, context, "BasisSpread");
            const auto result = EvaluateXccyGaussianMoments<double>(price, *model);
            ASSERT_NEAR(Sum(script.EvaluatePayments(path, true)), result.price_, 1e-10);
            if (foreign && valuation == Date_(2027, 11, 5))
                ASSERT_NEAR(result.Value(par), -.031, 1e-12);
            else
                ASSERT_THROW(static_cast<void>(result.Value(par)), CalibrationError_);
            ASSERT_THROW(static_cast<void>(Native(price, spec)), CalibrationError_);
        }
}

TEST(XccyGaussianExecutionTest, TestRecoveredCandidateUsesOriginalThreeSourceInstruments) {
    const auto spec = SyntheticSpec();
    CurveInstrumentCalibrationOptions_ options;
    options.computeQuoteJacobian_ = true;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    const auto fitted = CalibrateXccyInstruments(spec, options);
    ASSERT_TRUE(TestXccyRecovery::Qualify(spec, fitted).Accepted());
    const auto context = Context(ModelSpec(), Empty());
    for (size_t row = 0; row < spec.observations_.size(); ++row) {
        const auto instrument = spec.observations_[row]->Instrument();
        const Script::PreparedXccyScript_ script(instrument, context, fitted.gaussianModel_);
        const Script::XccyScriptPathProvider_<> provider(script);
        const auto program = LowerXccyGaussianMoments(PrepareMoment(instrument, context));
        const auto states = provider.Model().GenerateStates(Normals(provider.Model().SimDim(), .4));
        const auto path = provider.Observe(states);
        const auto terms = RealizedTerms(program, provider.Model(), states, .001);
        for (const bool compiled : {false, true})
            AssertDates(ByDate(script.EvaluatePayments(path, compiled)), terms);
        const auto result = EvaluateXccyGaussianMoments<double>(program, *fitted.gaussianModel_);
        ASSERT_NEAR(-result.b_ / result.a_, static_cast<double>(TestXccyRecovery::Reference(row, fitted.parameters_).value_), 1e-12);
        ASSERT_NEAR(-result.b_ / result.a_, fitted.modelMeasures_[row], 1e-12);
    }
}
