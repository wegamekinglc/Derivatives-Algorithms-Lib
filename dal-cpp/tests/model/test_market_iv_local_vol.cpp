#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <type_traits>

#include <dal/model/marketivlocalvolcomponent.hpp>
#include <dal/model/marketivlocalvolinternal.hpp>
#include <dal/model/surface/marketivsurfaceinternal.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

namespace {
    MarketIvMarket_ CarryMarket() {
        const Date_ today(2026, 11, 16);
        return {today,
                100.,
                {{today.AddDays(91), 101., .995},
                 {today.AddDays(182), 99., 1.005},
                 {today.AddDays(273), 102., .998},
                 {today.AddDays(365), 100.5, 1.01},
                 {today.AddDays(730), 104., .97}}};
    }

    std::pair<double, double> ReferenceLogs(double time) {
        const Vector_<> times{0., 91. / 365., 182. / 365., 273. / 365., 1., 2.};
        const Vector_<> forwards{100., 101., 99., 102., 100.5, 104.}, discounts{1., .995, 1.005, .998, 1.01, .97};
        size_t hi = 1;
        while (hi + 1 < times.size() && time >= times[hi])
            ++hi;
        const double weight = (time - times[hi - 1]) / (times[hi] - times[hi - 1]);
        return {(1. - weight) * std::log(forwards[hi - 1] / 100.) + weight * std::log(forwards[hi] / 100.),
                (1. - weight) * std::log(discounts[hi - 1]) + weight * std::log(discounts[hi])};
    }

    MarketIvLocalVolModelSettings_ Settings() {
        MarketIvLocalVolModelSettings_ settings;
        settings.index_ = "EQ[A]";
        settings.currency_ = "USD";
        settings.factor_ = "W_EQ";
        settings.horizon_ = 2.25;
        return settings;
    }

    MarketIvDupireResult_ Grid() {
        const auto market = CarryMarket();
        MarketIvFitRequest_ request{market, {}, {}};
        for (size_t term = 0; term < market.Terms().size(); ++term) {
            if (term == 2)
                continue;
            for (const double moneyness : {.85, 1., 1.15}) {
                MarketIvQuote_ quote;
                quote.id_ = String_(std::to_string(request.quotes_.size()));
                quote.expiry_ = market.Terms()[term].expiry_;
                quote.strike_ = moneyness * market.Terms()[term].forward_;
                quote.optionType_ = OptionType_("CALL");
                quote.quoteType_ = MarketIvQuoteType_("DECIMAL_IV");
                quote.mid_ = .2;
                request.quotes_.push_back(quote);
            }
        }
        const auto fit = FitMarketIvSurface(request);
        REQUIRE(fit.IsSuccess(), fit.Failure());
        return CalibrateMarketIvDupire(fit.Snapshot(), {{80., 100., 120.}, {.1, 1., 2.}, {}});
    }

    void AssertFlatCarryTail(double horizon) {
        SCOPED_TRACE(horizon);
        const Date_ today(2026, 11, 16);
        const MarketIvMarket_ market(today, 100., {{today.AddDays(1), 1000., .1}, {today.AddDays(2), 1000., .1}});
        MarketIvFitRequest_ request{market, {}, {}};
        for (const auto& term : market.Terms())
            for (const double moneyness : {.85, 1., 1.15}) {
                MarketIvQuote_ quote;
                quote.id_ = String_(std::to_string(request.quotes_.size()));
                quote.expiry_ = term.expiry_;
                quote.strike_ = moneyness * term.forward_;
                quote.optionType_ = OptionType_("CALL");
                quote.quoteType_ = MarketIvQuoteType_("DECIMAL_IV");
                quote.mid_ = .2;
                request.quotes_.push_back(quote);
            }
        const auto fit = FitMarketIvSurface(request);
        ASSERT_TRUE(fit.IsSuccess()) << fit.Failure().c_str();
        const auto grid = CalibrateMarketIvDupire(fit.Snapshot(), {{800., 1000., 1200.}, {1. / 365., 2. / 365.}, {}});
        ASSERT_TRUE(grid.IsSuccess()) << grid.Failure().c_str();
        auto settings = Settings();
        settings.horizon_ = horizon;
        const auto source = MarketIvLocalVolDetail::BuildSource(grid, settings);
        const auto integrated = source->Carry().Integrated(2. / 365., horizon);
        ASSERT_NEAR(integrated.rate_, 0., 2e-12) << "Q=" << integrated.yield_;
        ASSERT_NEAR(integrated.yield_, 0., 2e-12);
        ASSERT_DOUBLE_EQ(integrated.rate_, 0.);
        ASSERT_DOUBLE_EQ(integrated.yield_, 0.);
        const auto tail = source->Carry().At(horizon);
        ASSERT_NEAR(tail.logForwardOverSpot_, std::log(10.), 2e-12);
        ASSERT_NEAR(tail.logDiscount_, std::log(.1), 2e-12);
        ASSERT_DOUBLE_EQ(tail.rate_, 0.);
        ASSERT_DOUBLE_EQ(tail.yield_, 0.);
    }

    template <class F_> void AssertError(F_ action, const String_& code, const String_& field) {
        try {
            action();
            FAIL() << "Expected " << code.c_str();
        } catch (const MarketIvLocalVolError_& error) {
            ASSERT_EQ(error.code_, code);
            ASSERT_EQ(error.field_, field);
            ASSERT_FALSE(error.stage_.empty());
            ASSERT_FALSE(error.expected_.empty());
            ASSERT_FALSE(error.actual_.empty());
        }
    }
} // namespace

TEST(MarketIvLocalVolTest, TestCarryEndpointIntegralsAndRightSlopes) {
    const MarketIvLocalVolDetail::Carry_ carry(CarryMarket());
    const Vector_<> times{0., .1, 91. / 365., 182. / 365., 273. / 365., 1., 1.7, 2., 2.25};
    for (const double time : times) {
        const auto expected = ReferenceLogs(time);
        const auto actual = carry.At(time);
        ASSERT_NEAR(actual.logForwardOverSpot_, expected.first, 2e-12);
        ASSERT_NEAR(actual.logDiscount_, expected.second, 2e-12);
        ASSERT_NEAR(actual.forward_ / 100., std::exp(expected.first), 2e-12);
        ASSERT_NEAR(actual.discount_, std::exp(expected.second), 2e-12);
        const auto right = ReferenceLogs(time + 1e-5);
        ASSERT_NEAR(actual.rate_, -(right.second - expected.second) / 1e-5, 2e-11);
        ASSERT_NEAR(actual.yield_, actual.rate_ - (right.first - expected.first) / 1e-5, 2e-11);
        for (const double end : times) {
            if (end < time)
                continue;
            const auto terminal = ReferenceLogs(end);
            const auto integrated = carry.Integrated(time, end);
            const double r = expected.second - terminal.second;
            const double q = r - (terminal.first - expected.first);
            ASSERT_NEAR(integrated.rate_, r, 2e-12);
            ASSERT_NEAR(integrated.yield_, q, 2e-12);
        }
    }
}

TEST(MarketIvLocalVolTest, TestCarryFlatTailHasZeroIntegralsAtLongHorizons) {
    for (const double horizon : {30., 1000.})
        AssertFlatCarryTail(horizon);
}

TEST(MarketIvLocalVolTest, TestSourceOwnsGridAndHasContentIdentity) {
    auto grid = Grid();
    ASSERT_TRUE(grid.IsSuccess());
    const auto source = MarketIvLocalVolDetail::BuildSource(grid, Settings());
    const auto rebuilt = MarketIvLocalVolDetail::BuildSource(Grid(), Settings());
    ASSERT_EQ(source->SourceIdentity(), rebuilt->SourceIdentity());
    ASSERT_EQ(source->ModelIdentity(), rebuilt->ModelIdentity());
    ASSERT_EQ(source->SourceIdentity().size(), 71U);
    ASSERT_EQ(source->SourceIdentity().substr(0, 7), "sha256:");
    auto detached = source->SurfaceData();
    const double vol = detached.vols_(0, 0);
    detached.vols_(0, 0) = .9;
    const_cast<LocalVolSurfaceData_&>(grid.SurfaceData()).vols_(0, 0) = .8;
    ASSERT_DOUBLE_EQ(source->SurfaceData().vols_(0, 0), vol);
    auto configuration = source->Configuration();
    configuration.horizon_ = 1.;
    ASSERT_DOUBLE_EQ(source->Configuration().horizon_, 2.25);
    auto terms = source->CarryTerms();
    ASSERT_EQ(terms.size(), 7U);
    ASSERT_EQ(terms[0].origin_, "ANCHOR");
    ASSERT_EQ(terms[3].date_, CarryMarket().EvaluationDate().AddDays(273));
    ASSERT_EQ(terms.back().origin_, "HORIZON");
    ASSERT_FALSE(terms.back().date_);
    terms.back().forward_ = 0.;
    ASSERT_GT(source->CarryTerms().back().forward_, 0.);
    ASSERT_NEAR(source->Carry().At(2.25).logForwardOverSpot_, ReferenceLogs(2.25).first, 2e-12);
}

TEST(MarketIvLocalVolTest, TestZeroVolTermComponentAndLogDfReuse) {
    const auto market = CarryMarket();
    const LocalVolSurfaceData_ surface("zero", {80., 120.}, {.1, 1.}, Matrix_<>(2, 2, 0.));
    MarketIvLocalVolDetail::TermCarryEquity_ equity(market, surface, Settings());
    const auto& carry = MarketIvLocalVolDetail::Carry_(market);
    AAD::HybridLogDfRate_<double> rate(HybridLogDfRateData_("rate", "USD", carry.Times(), carry.LogDiscounts(), "LOG_LINEAR"));
    const Vector_<> times{0., .1, 91. / 365., 182. / 365., 273. / 365., 1., 2., 2.25};
    Vector_<> integrated;
    for (size_t i = 1; i < times.size(); ++i)
        integrated.push_back(ReferenceLogs(times[i - 1]).second - ReferenceLogs(times[i]).second);
    equity.Prepare(times, integrated);
    const auto knots = equity.TimelineKnots();
    ASSERT_NE(std::find(knots.begin(), knots.end(), 273. / 365.), knots.end());
    Vector_<> state(1);
    equity.ResetState(&state, 0);
    ASSERT_NEAR(equity.Observe(0, 0, state, 0, true) / 100., 1., 2e-12);
    for (size_t step = 0; step < integrated.size(); ++step) {
        equity.EvolveWithoutCarry(step, {.7}, {0}, &state, 0);
        const auto expected = ReferenceLogs(times[step + 1]);
        ASSERT_NEAR(equity.Observe(0, 0, state, 0, false) / 100., std::exp(expected.first), 2e-12);
        ASSERT_NEAR(rate.LogDiscount(times[step + 1]), expected.second, 2e-12);
        ASSERT_NEAR(rate.Numeraire(times[step + 1]) * std::exp(expected.second), 1., 2e-12);
        Vector_<> discounts(1);
        rate.FillDiscounts(0, times[step], {times.back()}, {}, 0, &discounts);
        ASSERT_NEAR(discounts[0] / rate.Numeraire(times[step]), std::exp(ReferenceLogs(times.back()).second), 2e-12);
    }
}

TEST(MarketIvLocalVolTest, TestSourceAnchorAndCarryReportDates) {
    const auto grid = Grid();
    auto settings = Settings();
    settings.horizon_ = 182. / 365.;
    const auto source = MarketIvLocalVolDetail::BuildSource(grid, settings);
    const auto terms = source->CarryTerms();
    ASSERT_EQ(terms.size(), 6U);
    ASSERT_EQ(terms.front().forward_, 100.);
    ASSERT_EQ(terms.front().discount_, 1.);
    ASSERT_EQ(terms[2].origin_, "MARKET_AND_HORIZON");
    ASSERT_EQ(terms[2].date_, CarryMarket().EvaluationDate().AddDays(182));
    settings.horizon_ = 200. / 365.;
    const auto interior = MarketIvLocalVolDetail::BuildSource(grid, settings)->CarryTerms();
    ASSERT_EQ(interior[3].origin_, "HORIZON");
    ASSERT_EQ(interior[3].date_, CarryMarket().EvaluationDate().AddDays(200));
}

TEST(MarketIvLocalVolTest, TestGridAdmissionRejectsFailureAndAliasedCorruption) {
    auto grid = Grid();
    auto request = grid.Request();
    request.settings_.evaluation_.minTotalVariance_ = 1.;
    const auto failed = CalibrateMarketIvDupire(grid.Snapshot(), request);
    ASSERT_FALSE(failed.IsSuccess());
    AssertError([&] { MarketIvLocalVolDetail::BuildSource(failed, Settings()); }, "MarketIvDupireUnavailable", "/grid");
    auto& surface = const_cast<LocalVolSurfaceData_&>(grid.SurfaceData());
    surface.vols_(0, 0) += .01;
    AssertError([&] { MarketIvLocalVolDetail::BuildSource(grid, Settings()); }, "MarketIvSourceMismatch", "/grid/surface/vols/0/0");
    surface.spots_[0] += .01;
    AssertError([&] { MarketIvLocalVolDetail::BuildSource(grid, Settings()); }, "MarketIvSourceMismatch", "/grid/surface/axes");
    const auto restored = handle_cast<LocalVolSurfaceData_>(JSON::ReadString(JSON::WriteString(surface), false));
    ASSERT_TRUE(restored);
    static_assert(
        !std::is_invocable_v<decltype(MarketIvLocalVolDetail::BuildSource), const LocalVolSurfaceData_&, const MarketIvLocalVolModelSettings_&>);
}

TEST(MarketIvLocalVolTest, TestGridValidationPrecedesBudgetAndChecksAxesBeforeNodes) {
    auto grid = Grid();
    auto settings = Settings();
    settings.numericPayloadBudgetBytes_ = 0;
    auto& surface = const_cast<LocalVolSurfaceData_&>(grid.SurfaceData());
    surface.vols_(0, 0) += .01;
    AssertError([&] { MarketIvLocalVolDetail::BuildSource(grid, settings); }, "MarketIvSourceMismatch", "/grid/surface/vols/0/0");
    surface.spots_[0] += .01;
    AssertError([&] { MarketIvLocalVolDetail::BuildSource(grid, settings); }, "MarketIvSourceMismatch", "/grid/surface/axes");
}

TEST(MarketIvLocalVolTest, TestSourceBudgetsExactZeroAndOverflow) {
    const auto grid = Grid();
    auto settings = Settings();
    const auto execution = MarketIvLocalVolDetail::BuildSource(grid, settings)->ConstructionExecution();
    ASSERT_LE(execution.numericPayloadBytes_, execution.plannedNumericPayloadBytes_);
    ASSERT_LE(execution.peakWorkspaceBytes_, execution.plannedWorkspaceBytes_);
    settings.numericPayloadBudgetBytes_ = execution.plannedNumericPayloadBytes_;
    settings.workspaceBudgetBytes_ = execution.plannedWorkspaceBytes_;
    ASSERT_NO_THROW(MarketIvLocalVolDetail::BuildSource(grid, settings));
    --settings.numericPayloadBudgetBytes_;
    AssertError([&] { MarketIvLocalVolDetail::BuildSource(grid, settings); }, "MarketIvLocalVolBudgetExceeded",
                "/settings/numeric_payload_budget_bytes");
    ++settings.numericPayloadBudgetBytes_;
    --settings.workspaceBudgetBytes_;
    AssertError([&] { MarketIvLocalVolDetail::BuildSource(grid, settings); }, "MarketIvLocalVolBudgetExceeded", "/settings/workspace_budget_bytes");
    for (const auto field : {&MarketIvLocalVolModelSettings_::numericPayloadBudgetBytes_, &MarketIvLocalVolModelSettings_::workspaceBudgetBytes_,
                             &MarketIvLocalVolModelSettings_::maxInternalSteps_, &MarketIvLocalVolModelSettings_::maxSimulationDimension_}) {
        settings = Settings();
        settings.*field = 0;
        ASSERT_THROW(MarketIvLocalVolDetail::BuildSource(grid, settings), MarketIvLocalVolError_);
    }
    const size_t max = std::numeric_limits<size_t>::max();
    ASSERT_EQ(MarketIvLocalVolDetail::CheckedAdd(max, 0), max);
    ASSERT_EQ(MarketIvLocalVolDetail::CheckedMultiply(max, 1), max);
    ASSERT_THROW(MarketIvLocalVolDetail::CheckedAdd(max, 1), MarketIvLocalVolError_);
    ASSERT_THROW(MarketIvLocalVolDetail::CheckedMultiply(max, 2), MarketIvLocalVolError_);
}

TEST(MarketIvLocalVolTest, TestIdentityChangesForSourceAndResolvedSettings) {
    const auto grid = Grid();
    const auto original = MarketIvLocalVolDetail::BuildSource(grid, Settings());
    auto settings = Settings();
    settings.currency_ = "usd";
    const auto normalized = MarketIvLocalVolDetail::BuildSource(grid, settings);
    ASSERT_EQ(normalized->ModelIdentity(), original->ModelIdentity());
    settings = Settings();
    settings.horizon_ += .1;
    const auto changed = MarketIvLocalVolDetail::BuildSource(grid, settings);
    ASSERT_EQ(changed->SourceIdentity(), original->SourceIdentity());
    ASSERT_NE(changed->ModelIdentity(), original->ModelIdentity());
    if constexpr (std::numeric_limits<size_t>::digits > 53) {
        settings.maxInternalSteps_ = size_t(1) << 53;
        const auto large = MarketIvLocalVolDetail::BuildSource(grid, settings);
        ++settings.maxInternalSteps_;
        ASSERT_NE(large->ModelIdentity(), MarketIvLocalVolDetail::BuildSource(grid, settings)->ModelIdentity());
    }
    auto request = grid.Request();
    request.settings_.evaluation_.densityFactorFloor_ *= 2.;
    const auto newGrid = CalibrateMarketIvDupire(grid.Snapshot(), request);
    ASSERT_TRUE(newGrid.IsSuccess());
    ASSERT_NE(original->SourceIdentity(), MarketIvLocalVolDetail::BuildSource(newGrid, Settings())->SourceIdentity());
    auto raw = grid.Snapshot().Request();
    auto marketTerms = raw.market_.Terms();
    marketTerms[2].forward_ += .1;
    marketTerms[2].discount_ += .001;
    raw.market_ = MarketIvMarket_(raw.market_.EvaluationDate(), raw.market_.Spot(), marketTerms);
    const auto newCarryFit = FitMarketIvSurface(raw);
    ASSERT_TRUE(newCarryFit.IsSuccess());
    ASSERT_NE(original->SourceIdentity(),
              MarketIvLocalVolDetail::BuildSource(CalibrateMarketIvDupire(newCarryFit.Snapshot(), grid.Request()), Settings())->SourceIdentity());
    auto quotes = grid.Snapshot().Request().quotes_;
    quotes[0].mid_ += 1e-6;
    const auto refit = RefitMarketIvSurface(grid.Snapshot(), quotes);
    ASSERT_TRUE(refit.IsSuccess()) << refit.Failure().c_str();
    ASSERT_NE(original->SourceIdentity(),
              MarketIvLocalVolDetail::BuildSource(CalibrateMarketIvDupire(refit.Snapshot(), grid.Request()), Settings())->SourceIdentity());
    std::reverse(quotes.begin(), quotes.end());
    auto reorderedRequest = grid.Snapshot().Request();
    reorderedRequest.quotes_ = quotes;
    const auto reordered = FitMarketIvSurface(reorderedRequest);
    ASSERT_TRUE(reordered.IsSuccess());
    ASSERT_NE(MarketIvLocalVolDetail::BuildSource(CalibrateMarketIvDupire(refit.Snapshot(), grid.Request()), Settings())->SourceIdentity(),
              MarketIvLocalVolDetail::BuildSource(CalibrateMarketIvDupire(reordered.Snapshot(), grid.Request()), Settings())->SourceIdentity());
}

TEST(MarketIvLocalVolTest, TestPreparationRejectsSubnormalDtAndVarianceOverflow) {
    const auto market = CarryMarket();
    const LocalVolSurfaceData_ zero("zero", {100.}, {0.}, Matrix_<>(1, 1, 0.));
    MarketIvLocalVolDetail::TermCarryEquity_ component(market, zero, Settings());
    AssertError([&] { component.Prepare({0., std::numeric_limits<double>::denorm_min()}, {0.}); }, "MarketIvLocalVolNumericRange",
                "/execution/steps/0");
    const LocalVolSurfaceData_ huge("huge", {100.}, {0.}, Matrix_<>(1, 1, 1e200));
    MarketIvLocalVolDetail::TermCarryEquity_ excessive(market, huge, Settings());
    const double r = -ReferenceLogs(1.).second;
    AssertError([&] { excessive.Prepare({0., 1.}, {r}); }, "MarketIvLocalVolNumericRange", "/execution/steps/0");
}

TEST(MarketIvLocalVolTest, TestPreparationPreflightsColumnsBeforeAllocation) {
    auto settings = Settings();
    settings.workspaceBudgetBytes_ = 1024;
    Vector_<> spots;
    for (int i = 0; i < 100; ++i)
        spots.push_back(50. + i);
    const LocalVolSurfaceData_ surface("vol", spots, {0.}, Matrix_<>(100, 1, .2));
    MarketIvLocalVolDetail::TermCarryEquity_ component(CarryMarket(), surface, settings);
    const Vector_<> times{0., .1, .2};
    const Vector_<> rates{-ReferenceLogs(.1).second, ReferenceLogs(.1).second - ReferenceLogs(.2).second};
    AssertError([&] { component.Prepare(times, rates); }, "MarketIvLocalVolBudgetExceeded", "/settings/workspace_budget_bytes");
}

TEST(MarketIvLocalVolTest, TestPreparationFailurePreservesPreviousStateAndErrorOrder) {
    const LocalVolSurfaceData_ zero("zero", {100.}, {0.}, Matrix_<>(1, 1, 0.));
    MarketIvLocalVolDetail::TermCarryEquity_ component(CarryMarket(), zero, Settings());
    const Vector_<> times{0., .5, 1.};
    const Vector_<> rates{-ReferenceLogs(.5).second, ReferenceLogs(.5).second - ReferenceLogs(1.).second};
    component.Prepare(times, rates);
    AssertError([&] { component.Prepare({0., .5, .4}, {rates[0], 0.}); }, "InvalidMarketIvLocalVolRequest", "/execution/timeline");
    AssertError([&] { component.Prepare({0., std::numeric_limits<double>::denorm_min()}, {1.}); }, "MarketIvLocalVolNumericRange",
                "/execution/steps/0");
    Vector_<> state(1);
    component.ResetState(&state, 0);
    for (size_t step = 0; step < rates.size(); ++step) {
        component.EvolveWithoutCarry(step, {0.}, {0}, &state, 0);
        ASSERT_NEAR(component.Observe(0, 0, state, 0, false) / 100., std::exp(ReferenceLogs(times[step + 1]).first), 2e-12);
    }
}

TEST(MarketIvLocalVolTest, TestSourceTextFailureHasFieldContext) {
    const auto grid = Grid();
    auto request = grid.Snapshot().Request();
    request.quotes_[0].id_ = String_(1, char(0xff));
    const auto fit = FitMarketIvSurface(request);
    ASSERT_TRUE(fit.IsSuccess());
    const auto invalid = CalibrateMarketIvDupire(fit.Snapshot(), grid.Request());
    AssertError([&] { MarketIvLocalVolDetail::BuildSource(invalid, Settings()); }, "InvalidMarketIvLocalVolRequest", "/grid/snapshot/quotes/0/id");
}

TEST(MarketIvLocalVolTest, TestFlatComponentMatchesLegacyAndCloneIsIndependent) {
    const Date_ today(2026, 11, 16);
    const auto market = NewFlatMarketIvMarket(today, 100., -.03, -.05, {today.AddDays(365), today.AddDays(730)});
    const LocalVolSurfaceData_ surface("flat", {80., 120.}, {0., 1.}, Matrix_<>(2, 2, .2));
    MarketIvLocalVolDetail::TermCarryEquity_ component(market, surface, Settings());
    const Handle_<LocalVolSurfaceData_> legacySurface(new LocalVolSurfaceData_("flat", surface.spots_, surface.times_, surface.vols_));
    AAD::HybridLocalVolEquity_<double> legacy(HybridLocalVolEquityData_("equity", "EQ[A]", "USD", "W_EQ", 100., -.05, legacySurface));
    const Vector_<> timeline{0., .3, 1., 2.25}, rates{-.03 * .3, -.03 * .7, -.03 * 1.25};
    component.Prepare(timeline, rates);
    legacy.Prepare(timeline, rates);
    auto clone = component.Clone();
    clone->Prepare({0., .5}, {-.015});
    Vector_<> current(1), reference(1);
    component.ResetState(&current, 0);
    legacy.ResetState(&reference, 0);
    for (size_t step = 0; step < rates.size(); ++step) {
        component.EvolveWithoutCarry(step, {.37}, {0}, &current, 0);
        legacy.EvolveWithoutCarry(step, {.37}, {0}, &reference, 0);
        ASSERT_NEAR(component.Observe(0, 0, current, 0, false) / 100., legacy.Observe(0, 0, reference, 0, false) / 100., 2e-12);
    }
    ASSERT_TRUE(component.Parameters().empty());
    const MarketIvLocalVolDetail::Carry_ carry(market);
    ASSERT_DOUBLE_EQ(carry.At(1.).rate_, -.03);
    ASSERT_DOUBLE_EQ(carry.At(1.).yield_, -.05);
}

TEST(MarketIvLocalVolTest, TestCarryLeftRightSingleTermAndNumericRange) {
    const auto market = CarryMarket();
    const MarketIvLocalVolDetail::Carry_ carry(market);
    const double knot = 182. / 365.;
    const double leftR = -std::log(1.005 / .995) / (91. / 365.);
    const double rightR = -std::log(.998 / 1.005) / (91. / 365.);
    ASSERT_NEAR(carry.At(std::nextafter(knot, 0.)).rate_, leftR, 2e-12);
    ASSERT_NEAR(carry.At(knot).rate_, rightR, 2e-12);
    ASSERT_NEAR(carry.At(std::nextafter(knot, 1.)).rate_, rightR, 2e-12);
    ASSERT_LT(carry.At(.3).rate_, 0.);
    ASSERT_LT(carry.At(.6).yield_, 0.);
    const MarketIvLocalVolDetail::Carry_ single(MarketIvMarket_(market.EvaluationDate(), 100., {{market.EvaluationDate().AddDays(365), 102., .97}}));
    ASSERT_NEAR(single.At(2.).logForwardOverSpot_, 2. * std::log(1.02), 2e-12);
    ASSERT_NEAR(single.At(2.).logDiscount_, 2. * std::log(.97), 2e-12);
    ASSERT_THROW(single.At(-1.), MarketIvLocalVolError_);
    ASSERT_THROW(single.At(std::numeric_limits<double>::infinity()), MarketIvLocalVolError_);
    ASSERT_THROW(single.At(std::numeric_limits<double>::max()), MarketIvLocalVolError_);
    ASSERT_THROW(single.Integrated(1., 0.), MarketIvLocalVolError_);
    const MarketIvLocalVolDetail::Carry_ extreme(
        MarketIvMarket_(market.EvaluationDate(), 100.,
                        {{market.EvaluationDate().AddDays(365), 1e200, 1e-200}, {market.EvaluationDate().AddDays(730), 1e-200, 1e200}}));
    ASSERT_THROW(extreme.Integrated(1., 2.), MarketIvLocalVolError_);
}

TEST(MarketIvLocalVolTest, TestDetachedWarningsDestroyInputsAndConcurrentSources) {
    const auto source = MarketIvLocalVolDetail::BuildSource(Grid(), Settings());
    const auto originalId = source->SourceIdentity();
    const auto& fit = MarketIvData(source->Snapshot());
    auto warnings = source->Warnings();
    ASSERT_FALSE(warnings.empty());
    const auto has = [&](const String_& code) {
        return std::any_of(warnings.begin(), warnings.end(), [&](const auto& warning) { return warning.code_ == code; });
    };
    ASSERT_EQ(has("FIT_UNDERIDENTIFIED"), fit.underidentified_);
    ASSERT_EQ(has("FIT_NEAR_TIE"), fit.nearTie_);
    ASSERT_TRUE(has("LOCAL_VOL_FIRST_COLUMN_AT_ZERO"));
    ASSERT_TRUE(has("LOCAL_VOL_TIME_TAIL"));
    ASSERT_TRUE(has("MARKET_CARRY_TAIL"));
    warnings.clear();
    ASSERT_FALSE(source->Warnings().empty());
    const auto grid = Grid();
    Vector_<std::future<std::shared_ptr<const MarketIvLocalVolDetail::Source_>>> futures;
    for (int worker = 0; worker < 4; ++worker)
        futures.push_back(std::async(std::launch::async, [&grid] { return MarketIvLocalVolDetail::BuildSource(grid, Settings()); }));
    for (auto& future : futures) {
        const auto concurrent = future.get();
        ASSERT_EQ(concurrent->SourceIdentity(), originalId);
        ASSERT_EQ(concurrent->ModelIdentity(), source->ModelIdentity());
    }
    ASSERT_NEAR(source->SurfaceData().vols_(0, 0), .2, 1e-7);
}

TEST(MarketIvLocalVolTest, TestSettingsAndComponentRejectInvalidInputs) {
    const auto grid = Grid();
    ASSERT_THROW(ValidateMarketIvLocalVolModelSettings({}), MarketIvLocalVolError_);
    for (const auto field : {&MarketIvLocalVolModelSettings_::horizon_, &MarketIvLocalVolModelSettings_::maxStep_}) {
        for (const double value : {0., -1., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            auto settings = Settings();
            settings.*field = value;
            ASSERT_THROW(MarketIvLocalVolDetail::BuildSource(grid, settings), MarketIvLocalVolError_);
        }
    }
    for (const auto& pair : {std::make_pair(&MarketIvLocalVolModelSettings_::index_, String_("FX[EURUSD]")),
                             {&MarketIvLocalVolModelSettings_::currency_, String_("UNKNOWN")},
                             {&MarketIvLocalVolModelSettings_::factor_, String_(std::string("bad\0factor", 10))}}) {
        auto settings = Settings();
        settings.*pair.first = pair.second;
        ASSERT_THROW(MarketIvLocalVolDetail::BuildSource(grid, settings), MarketIvLocalVolError_);
    }
    const LocalVolSurfaceData_ surface("vol", {100.}, {0.}, Matrix_<>(1, 1, .2));
    MarketIvLocalVolDetail::TermCarryEquity_ component(CarryMarket(), surface, Settings());
    ASSERT_THROW(component.Prepare({}, {}), MarketIvLocalVolError_);
    ASSERT_THROW(component.Prepare({0., 0.}, {0.}), MarketIvLocalVolError_);
    ASSERT_THROW(component.Prepare({0., 1.}, {0.}), MarketIvLocalVolError_);
    const double r = -ReferenceLogs(1.).second;
    component.Prepare({0., 1.}, {r});
    Vector_<> state(1);
    component.ResetState(&state, 0);
    ASSERT_THROW(component.Evolve(0, {0.}, {0}, .01, &state, 0), MarketIvLocalVolError_);
    ASSERT_THROW(component.EvolveWithoutCarry(0, {1e308}, {0}, &state, 0), MarketIvLocalVolError_);
}

TEST(MarketIvLocalVolTest, TestModelIdentityMatchesIndependentSha256Fixture) {
    auto settings = Settings();
    settings.maxStep_ = .5;
    // Python hashlib over sorted compact JSON; the fixture contains only exact binary fractions.
    ASSERT_EQ(MarketIvLocalVolDetail::ModelIdentity("sha256:test", settings),
              "sha256:c07091ca1a0b98bd6e4c9b760de8bbd98ba2a46d9086f8f938304e423ef5425e");
}

TEST(MarketIvLocalVolTest, TestCarryReportRecognizesExactDaysWithoutRoundingSubdays) {
    const auto grid = Grid();
    auto settings = Settings();
    settings.horizon_ = 3. / 365.;
    const auto terms = MarketIvLocalVolDetail::BuildSource(grid, settings)->CarryTerms();
    ASSERT_EQ(terms[1].date_, CarryMarket().EvaluationDate().AddDays(3));
    settings.horizon_ = std::nextafter(settings.horizon_, 1.);
    const auto subday = MarketIvLocalVolDetail::BuildSource(grid, settings)->CarryTerms();
    ASSERT_FALSE(subday[1].date_);
    settings.horizon_ = 1000.;
    ASSERT_FALSE(MarketIvLocalVolDetail::BuildSource(grid, settings)->CarryTerms().back().date_);
}
