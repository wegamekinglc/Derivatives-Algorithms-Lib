#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <type_traits>

#include <dal/model/factory.hpp>
#include <dal/model/marketivlocalvolcomponent.hpp>
#include <dal/model/marketivlocalvolinternal.hpp>
#include <dal/model/surface/marketivsurfaceinternal.hpp>
#include <dal/script/marketivlocalvol.hpp>
#include <dal/script/passivereplay.hpp>
#include <dal/storage/globals.hpp>
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
        settings.maxStep_ = horizon;
        const auto data = BuildMarketIvLocalVolModelData("long-tail", grid, settings);
        AAD::MarketIvLocalVolModel_ model(*data);
        const Vector_<AAD::SampleDef_> definitions(1);
        model.Allocate({horizon}, definitions);
        model.Init({horizon}, definitions);
        AAD::Scenario_<> path;
        AAD::AllocatePath(definitions, path);
        AAD::InitializePath(path);
        model.GeneratePath(Vector_<>(model.SimDim(), 0.), &path);
        ASSERT_NEAR(path[0].numeraire_, 10., 2e-12);
        ASSERT_TRUE(AAD::IsValidModelPath(path));
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

TEST(MarketIvLocalVolTest, TestOwningModelFactoryRunsPassivePaths) {
    const auto data = BuildMarketIvLocalVolModelData("market", Grid(), Settings());
    auto model = CreateModel<double>(Handle_<ModelData_>(data));
    ASSERT_EQ(model->EvaluationDate(), CarryMarket().EvaluationDate());
    ASSERT_EQ(model->NumFactors(), 1U);
    ASSERT_EQ(model->NumParams(), 0U);
    const Vector_<> times{1.};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"EQ[A]"};
    definitions[0].discountMats_ = {2.25};
    model->Allocate(times, definitions);
    model->Init(times, definitions);
    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    AAD::InitializePath(path);
    model->GeneratePath(Vector_<>(model->SimDim(), 0.), &path);
    ASSERT_TRUE(AAD::IsValidModelPath(path));
    ASSERT_EQ(path.size(), 1U);
    ASSERT_GT(path[0].spot_, 0.);
    ASSERT_NEAR(path[0].numeraire_ * std::exp(ReferenceLogs(1.).second), 1., 2e-12);
    ASSERT_NEAR(path[0].discounts_[0] / path[0].numeraire_, std::exp(ReferenceLogs(2.25).second), 2e-12);
}

TEST(MarketIvLocalVolTest, TestRequestBudgetFailureKeepsPreparedState) {
    const auto grid = Grid();
    auto settings = Settings();
    Vector_<AAD::SampleDef_> definitions(1);
    const auto ample = BuildMarketIvLocalVolModelData("ample", grid, settings);
    AAD::MarketIvLocalVolModel_ probe(*ample);
    probe.Allocate({1.}, definitions);
    settings.workspaceBudgetBytes_ = probe.Execution().plannedWorkspaceBytes_ - 1;
    const auto limited = BuildMarketIvLocalVolModelData("limited", grid, settings);
    AAD::MarketIvLocalVolModel_ model(*limited);
    model.Allocate({0.}, definitions);
    model.Init({0.}, definitions);
    ASSERT_EQ(model.SimDim(), 0U);
    AssertError([&] { model.Allocate({1.}, definitions); }, "MarketIvLocalVolBudgetExceeded", "/settings/workspace_budget_bytes");
    ASSERT_EQ(model.SimDim(), 0U);
    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    AAD::InitializePath(path);
    model.GeneratePath({}, &path);
    ASSERT_DOUBLE_EQ(path[0].spot_, 100.);
}

TEST(MarketIvLocalVolTest, TestModelMetadataCopiesWarningsAndArchive) {
    const auto data = BuildMarketIvLocalVolModelData("model", Grid(), Settings());
    const auto same = BuildMarketIvLocalVolModelData("renamed", Grid(), Settings());
    ASSERT_EQ(data->ModelIdentity(), same->ModelIdentity());
    const auto summary = data->Summary();
    ASSERT_EQ(summary.evaluationDate_, CarryMarket().EvaluationDate());
    ASSERT_EQ(summary.gridRows_, 3U);
    ASSERT_EQ(summary.gridColumns_, 3U);
    ASSERT_EQ(summary.carryMode_, "FORWARD_DISCOUNT_KNOTS");
    ASSERT_TRUE(data->Capabilities().passivePrice_);
    ASSERT_FALSE(data->Capabilities().aad_);
    ASSERT_FALSE(data->Capabilities().archive_);
    auto terms = data->CarryTerms();
    terms.back().forward_ = 1.;
    ASSERT_GT(data->CarryTerms().back().forward_, 100.);
    auto warnings = data->Warnings();
    ASSERT_TRUE(std::any_of(warnings.begin(), warnings.end(), [](const auto& item) { return item.code_ == "LOCAL_VOL_TIME_TAIL"; }));
    const auto mutant = data->MutantModel("copy", {});
    const auto* copy = dynamic_cast<const MarketIvLocalVolModelData_*>(mutant.get());
    ASSERT_NE(copy, nullptr);
    ASSERT_EQ(copy->ModelIdentity(), data->ModelIdentity());
    ASSERT_THROW(static_cast<void>(data->MutantModel("copy", {Handle_<Slide_>(new Slide_)})), Exception_);
    AssertError([&] { static_cast<void>(JSON::WriteString(*data)); }, "ArchiveUnsupported", "/model");
}

TEST(MarketIvLocalVolTest, TestSubdayTimelineIncludesCarryOnlyAndDiscountKnots) {
    auto settings = Settings();
    settings.maxStep_ = 1. / 1000.;
    const auto data = BuildMarketIvLocalVolModelData("model", Grid(), settings);
    AAD::MarketIvLocalVolModel_ model(*data);
    Vector_<AAD::SampleDef_> definitions(2);
    const Vector_<> times{.5, 1.};
    definitions[0].discountMats_ = {.75, 2.25};
    model.Allocate(times, definitions);
    model.Init(times, definitions);
    const auto execution = model.Execution();
    ASSERT_EQ(execution.carryKnots_, 5U);
    ASSERT_EQ(execution.gridTimeKnots_, 3U);
    ASSERT_EQ(execution.requiredTime_, 2.25);
    ASSERT_EQ(execution.anchorCount_, 10U);
    ASSERT_GT(execution.internalSteps_, 2250U);
    ASSERT_NEAR(execution.resolvedMaxStep_, settings.maxStep_, 1e-15);
    ASSERT_EQ(execution.preparedColumns_, execution.internalSteps_);
    ASSERT_EQ(execution.perPathMarketQueries_, 0U);
}

TEST(MarketIvLocalVolTest, TestModelCloneOwnsDefinitionLifetimeAndPrivatePaths) {
    std::unique_ptr<AAD::Model_<double>> clone;
    {
        const auto data = BuildMarketIvLocalVolModelData("model", Grid(), Settings());
        auto model = CreateModel<double>(Handle_<ModelData_>(data));
        Vector_<AAD::SampleDef_> definitions(1);
        definitions[0].indexNames_ = {"EQ[A]"};
        model->Allocate({1.}, definitions);
        model->Init({1.}, definitions);
        clone = model->Clone();
        definitions[0].indexNames_.clear();
    }
    const auto run = [&] {
        AAD::Scenario_<> path(1);
        path[0].observations_.Resize(1);
        AAD::InitializePath(path);
        clone->GeneratePath(Vector_<>(clone->SimDim(), .1), &path);
        return path[0].spot_;
    };
    const double expected = run();
    Vector_<std::future<double>> jobs;
    for (size_t i = 0; i < 4; ++i)
        jobs.push_back(std::async(std::launch::async, run));
    for (auto& job : jobs)
        ASSERT_DOUBLE_EQ(job.get(), expected);
}

TEST(MarketIvLocalVolTest, TestOrdinaryMcPriceDiscountAndParallelExecution) {
    using namespace Script;
    const auto data = BuildMarketIvLocalVolModelData("model", Grid(), Settings());
    const Handle_<ModelData_> base(data);
    const auto today = CarryMarket().EvaluationDate();
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    const ScriptProductData_ call("call", {Cell_(today.AddDays(365))}, {"call PAYS MAX(FIX(EQ[A]) - 100, 0)"});
    const ScriptProductData_ payment("payment", {Cell_(today.AddDays(365))}, {"pay PAYS 100 ON " + Date::ToString(today.AddDays(821))});
    const ScriptProductData_ bond("bond", {Cell_(today.AddDays(365))}, {"pay PAYS FIX(IR[USD,DF," + Date::ToString(today.AddDays(730)) + "])"});
    const auto originalThreads = ThreadPool_::GetInstance()->NumThreads();
    struct Restore_ {
        size_t threads_;
        ~Restore_() { ThreadPool_::GetInstance()->Start(threads_, true); }
    } restore{originalThreads};
    double serial = 0.;
    for (const size_t workers : {1U, 4U}) {
        ThreadPool_::GetInstance()->Start(workers, true);
        MonteCarloSettings_ simulation;
        simulation.rsg_ = "mrg32";
        simulation.normalPrecision_ = "Precise";
        const double value = MCSimulation<double>(call, base, 4096, valuation, simulation).aggregated_ / 4096.;
        ASSERT_GT(value, 0.);
        if (workers == 1)
            serial = value;
        else
            ASSERT_NEAR(value, serial, 1e-10);
        const auto execution = InspectMarketIvLocalVolExecution(call, *data, valuation, simulation);
        ASSERT_EQ(execution.workers_, workers);
        ASSERT_EQ(execution.requiredTime_, 1.);
        ASSERT_EQ(execution.requestIdentity_.size(), 71U);
        ASSERT_LE(execution.numericPayloadBytes_, execution.plannedNumericPayloadBytes_);
        ASSERT_LE(execution.peakWorkspaceBytes_, execution.plannedWorkspaceBytes_);
        for (const String_ method : {"sobol", "mrg32", "irn"})
            for (const bool compiled : {true, false}) {
                simulation.rsg_ = method;
                simulation.compiled_ = compiled;
                simulation.useBb_ = compiled;
                const double pv = MCSimulation<double>(payment, base, 31, valuation, simulation).aggregated_ / 31.;
                ASSERT_NEAR(pv / 100., std::exp(ReferenceLogs(821. / 365.).second), 2e-12);
                const double df = MCSimulation<double>(bond, base, 31, valuation, simulation).aggregated_ / 31.;
                ASSERT_NEAR(df, .97, 2e-12);
            }
    }
}

TEST(MarketIvLocalVolTest, TestUnsupportedRequestsRejectBeforeDispatchAndTape) {
    using namespace Script;
    const auto data = BuildMarketIvLocalVolModelData("model", Grid(), Settings());
    const Handle_<ModelData_> base(data);
    const auto today = CarryMarket().EvaluationDate();
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    const ScriptProductData_ product("call", {Cell_(today.AddDays(365))}, {"call PAYS MAX(spot() - 100, 0)"});
    const auto tapeEvents = AAD::Tape()->ReverseEventCount();
    AssertError([&] { static_cast<void>(CreateModel<AAD::Number_>(base)); }, "DerivativeUnavailable", "/simulation/enable_aad");
    AssertError([&] { static_cast<void>(MCSimulation<double>(product, base, std::numeric_limits<size_t>::max(), valuation)); },
                "MarketIvLocalVolBudgetExceeded", "/simulation/rsg");
    MonteCarloSettings_ hugeMrg;
    hugeMrg.rsg_ = "mrg32";
    AssertError([&] { static_cast<void>(MCSimulation<double>(product, base, std::numeric_limits<size_t>::max(), valuation, hugeMrg)); },
                "MarketIvLocalVolBudgetExceeded", "/simulation/rsg");
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    AssertError([&] { static_cast<void>(MCSimulation<double>(product, base, 16, valuation, simulation)); }, "DerivativeUnavailable",
                "/simulation/enable_aad");
    AssertError([&] { static_cast<void>(MCSimulation<AAD::Number_>(product, base, 16, valuation)); }, "DerivativeUnavailable",
                "/simulation/enable_aad");
    auto model = CreateModel<double>(base);
    AssertError([&] { static_cast<void>(PrepareScript(product, model.get(), valuation, simulation)); }, "DerivativeUnavailable",
                "/simulation/enable_aad");
    const ScriptProductData_ exercise("exercise", {Cell_(today.AddDays(365))}, {"EXERCISE MAX(spot() - 100, 0)"});
    AssertError([&] { static_cast<void>(InspectMarketIvLocalVolExecution(exercise, *data, valuation)); }, "UnsupportedMarketIvLocalVolExercise",
                "/product/exercise/0");
    valuation.evaluationDate_ = today.AddDays(1);
    AssertError([&] { static_cast<void>(PrepareScript(product, model.get(), valuation, {})); }, "InvalidModelEvaluationDate",
                "/valuation/evaluation_date");
    valuation.evaluationDate_ = today;
    const auto prepared = PrepareScript(product, model.get(), valuation, {});
    AssertError([&] { static_cast<void>(Script::Detail::EvaluatePassiveOutputReplay(prepared, base, 16, {})); }, "DerivativeUnavailable",
                "/risk/model");
    const auto plan = PlanWeightedRiskRequest(prepared.Product(), {}, today, {}, false);
    AssertError(
        [&] {
            static_cast<void>(MCDoubleSimulationWithObjective(prepared, model.get(), 16, "sobol", false, true, true,
                                                              Script::Detail::WeightedSimulationObjective_(plan)));
        },
        "DerivativeUnavailable", "/risk/model");
    ASSERT_EQ(AAD::Tape()->ReverseEventCount(), tapeEvents);
    AssertError([&] { static_cast<void>(MCSimulation<double>(product, base, std::numeric_limits<size_t>::max(), valuation)); },
                "MarketIvLocalVolBudgetExceeded", "/simulation/rsg");
    simulation.enableAad_ = false;
    simulation.rsg_ = "mrg32";
    AssertError([&] { static_cast<void>(MCSimulation<double>(product, base, std::numeric_limits<size_t>::max(), valuation, simulation)); },
                "MarketIvLocalVolBudgetExceeded", "/simulation/rsg");
}

TEST(MarketIvLocalVolTest, TestPreparedRejectsActualValuationDateBeforeAllocation) {
    using namespace Script;
    const Handle_<ModelData_> data(BuildMarketIvLocalVolModelData("model", Grid(), Settings()));
    const auto today = CarryMarket().EvaluationDate();
    const ScriptProductData_ payment("payment", {Cell_(today.AddDays(365))}, {"pay PAYS 1"});
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today.AddDays(1);
    AAD::BlackScholes_<double> metadata(100., .2, .03);
    const auto prepared = PrepareScript(payment, &metadata, valuation, {});
    ASSERT_EQ(prepared.EvaluationDate(), today.AddDays(1));
    AssertError([&] { static_cast<void>(MCSimulation<double>(payment, data, 16, valuation)); }, "InvalidModelEvaluationDate",
                "/valuation/evaluation_date");
    AssertError([&] { static_cast<void>(MCSimulation<double>(prepared, data, 16)); }, "InvalidModelEvaluationDate", "/valuation/evaluation_date");
    auto model = CreateModel<double>(data);
    AssertError([&] { static_cast<void>(MCDoubleSimulation(prepared, model.get(), 16, "sobol", false, true)); }, "InvalidModelEvaluationDate",
                "/valuation/evaluation_date");
    ASSERT_EQ(model->SimDim(), 0U);
    const ScriptProductData_ expired("expired", {Cell_(today.AddDays(-1))}, {"pay PAYS 1"});
    const auto wrongExpired = PrepareScript(expired, &metadata, valuation, {});
    ASSERT_TRUE(wrongExpired.AllExpired());
    AssertError([&] { static_cast<void>(MCSimulation<double>(wrongExpired, data, 16)); }, "InvalidModelEvaluationDate", "/valuation/evaluation_date");
    valuation.evaluationDate_ = today;
    const auto valid = PrepareScript(payment, &metadata, valuation, {});
    ASSERT_NEAR(MCSimulation<double>(valid, data, 16).aggregated_ / 16., 1.01, 2e-12);
    const auto validExpired = PrepareScript(expired, &metadata, valuation, {});
    ASSERT_DOUBLE_EQ(MCSimulation<double>(validExpired, data, 16).aggregated_, 0.);
}

TEST(MarketIvLocalVolTest, TestExpiredPreparedAadRejectsBeforeEmptyReturnAndTape) {
    using namespace Script;
    const Handle_<ModelData_> data(BuildMarketIvLocalVolModelData("model", Grid(), Settings()));
    const auto today = CarryMarket().EvaluationDate();
    const ScriptProductData_ payment("expired", {Cell_(today.AddDays(-1))}, {"pay PAYS 1"});
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    AAD::BlackScholes_<double> metadata(100., .2, .03);
    const auto prepared = PrepareScript(payment, &metadata, valuation, simulation);
    ASSERT_TRUE(prepared.AllExpired());
    const auto tapeEvents = AAD::Tape()->ReverseEventCount();
    AssertError([&] { static_cast<void>(MCSimulation<double>(prepared, data, 16)); }, "DerivativeUnavailable", "/simulation/enable_aad");
    AssertError([&] { static_cast<void>(MCSimulation<AAD::Number_>(prepared, data, 16)); }, "DerivativeUnavailable", "/simulation/enable_aad");
    auto model = CreateModel<double>(data);
    AssertError([&] { static_cast<void>(MCDoubleSimulation(prepared, model.get(), 16, "sobol", false, true)); }, "DerivativeUnavailable",
                "/simulation/enable_aad");
    ASSERT_EQ(model->SimDim(), 0U);
    ASSERT_EQ(AAD::Tape()->ReverseEventCount(), tapeEvents);
    const Handle_<ModelData_> legacy(new BSModelData_("legacy", 100., .2, .03));
    ASSERT_DOUBLE_EQ(MCSimulation<double>(prepared, legacy, 16).aggregated_, 0.);
}

TEST(MarketIvLocalVolTest, TestPreparedExerciseRejectsBeforeAllocationAndTape) {
    using namespace Script;
    const Handle_<ModelData_> data(BuildMarketIvLocalVolModelData("model", Grid(), Settings()));
    const auto today = CarryMarket().EvaluationDate();
    const ScriptProductData_ exercise("exercise", {Cell_(today.AddDays(365))}, {"EXERCISE MAX(spot() - 100, 0)"});
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    AAD::BlackScholes_<double> metadata(100., .2, .03);
    const auto prepared = PrepareScript(exercise, &metadata, valuation, {});
    const auto tapeEvents = AAD::Tape()->ReverseEventCount();
    auto model = CreateModel<double>(data);
    AssertError([&] { static_cast<void>(MCSimulation<double>(prepared, data, 16)); }, "UnsupportedMarketIvLocalVolExercise", "/product/exercise/0");
    AssertError([&] { static_cast<void>(MCDoubleSimulation(prepared, model.get(), 16, "sobol", false, true)); },
                "UnsupportedMarketIvLocalVolExercise", "/product/exercise/0");
    ASSERT_EQ(model->SimDim(), 0U);
    ASSERT_EQ(AAD::Tape()->ReverseEventCount(), tapeEvents);
}

TEST(MarketIvLocalVolTest, TestLegacyRejectsActualValuationDateBeforeEmptyReturn) {
    using namespace Script;
    const Handle_<ModelData_> data(BuildMarketIvLocalVolModelData("model", Grid(), Settings()));
    const auto today = CarryMarket().EvaluationDate();
    for (const int days : {-1, 365}) {
        ScriptProduct_ payment({Cell_(today.AddDays(days))}, {"pay PAYS 1"});
        payment.PartitionEvents(today.AddDays(1));
        payment.PreProcess(false, false);
        AssertError([&] { static_cast<void>(MCSimulation<double>(payment, data, 16)); }, "InvalidModelEvaluationDate", "/valuation/evaluation_date");
    }
}

TEST(MarketIvLocalVolTest, TestRequestBudgetExactBoundaryAndStepLimits) {
    const auto grid = Grid();
    auto settings = Settings();
    const Vector_<AAD::SampleDef_> definitions(1);
    const auto ample = BuildMarketIvLocalVolModelData("ample", grid, settings);
    AAD::MarketIvLocalVolModel_ probe(*ample);
    probe.Allocate({1.}, definitions);
    const auto execution = probe.Execution();
    settings.workspaceBudgetBytes_ = execution.plannedWorkspaceBytes_;
    settings.maxInternalSteps_ = execution.internalSteps_;
    settings.maxSimulationDimension_ = execution.simulationDimension_;
    const auto exact = BuildMarketIvLocalVolModelData("exact", grid, settings);
    AAD::MarketIvLocalVolModel_ accepted(*exact);
    ASSERT_NO_THROW(accepted.Allocate({1.}, definitions));
    --settings.workspaceBudgetBytes_;
    const auto shortData = BuildMarketIvLocalVolModelData("short", grid, settings);
    AAD::MarketIvLocalVolModel_ shortModel(*shortData);
    AssertError([&] { shortModel.Allocate({1.}, definitions); }, "MarketIvLocalVolBudgetExceeded", "/settings/workspace_budget_bytes");
    settings = Settings();
    settings.maxSimulationDimension_ = execution.simulationDimension_ - 1;
    const auto dimension = BuildMarketIvLocalVolModelData("dimension", grid, settings);
    AAD::MarketIvLocalVolModel_ dimensionModel(*dimension);
    AssertError([&] { dimensionModel.Allocate({1.}, definitions); }, "MarketIvLocalVolBudgetExceeded", "/settings/max_simulation_dimension");
    settings = Settings();
    settings.maxInternalSteps_ = execution.internalSteps_ - 1;
    const auto steps = BuildMarketIvLocalVolModelData("steps", grid, settings);
    AAD::MarketIvLocalVolModel_ stepModel(*steps);
    AssertError([&] { stepModel.Allocate({1.}, definitions); }, "MarketIvLocalVolBudgetExceeded", "/settings/max_internal_steps");
    settings.maxInternalSteps_ = 0;
    AssertError([&] { static_cast<void>(BuildMarketIvLocalVolModelData("zero", grid, settings)); }, "MarketIvLocalVolBudgetExceeded",
                "/settings/max_internal_steps");
    AssertError([&] { accepted.PrepareWorkers(std::numeric_limits<size_t>::max()); }, "MarketIvLocalVolBudgetExceeded", "/ledger");
}

TEST(MarketIvLocalVolTest, TestHorizonGuardsAndOverflowBeforePreparation) {
    const auto grid = Grid();
    auto settings = Settings();
    const auto data = BuildMarketIvLocalVolModelData("model", grid, settings);
    AAD::MarketIvLocalVolModel_ model(*data);
    Vector_<AAD::SampleDef_> definitions(1);
    AssertError([&] { model.Allocate({2.5}, definitions); }, "MarketIvLocalVolHorizonExceeded", "/product/timeline/0");
    definitions[0].discountMats_ = {2.5};
    AssertError([&] { model.Allocate({1.}, definitions); }, "MarketIvLocalVolHorizonExceeded", "/product/discount_maturities/0");
    definitions[0].discountMats_.clear();
    definitions[0].indexNames_ = {"IR[USD,DF,2030-11-16]"};
    AssertError([&] { model.Allocate({1.}, definitions); }, "MarketIvLocalVolHorizonExceeded", "/product/discount_maturity");
    definitions[0].indexNames_.clear();
    settings.maxStep_ = std::numeric_limits<double>::denorm_min();
    const auto tiny = BuildMarketIvLocalVolModelData("tiny", grid, settings);
    AAD::MarketIvLocalVolModel_ tinyModel(*tiny);
    AssertError([&] { tinyModel.Allocate({1.}, definitions); }, "MarketIvLocalVolBudgetExceeded", "/settings/max_internal_steps");
    settings = Settings();
    settings.maxStep_ = 1. / 22000.;
    const auto high = BuildMarketIvLocalVolModelData("high", grid, settings);
    AAD::MarketIvLocalVolModel_ highModel(*high);
    highModel.ValidateRequest(false, false, model.EvaluationDate(), "sobol");
    AssertError([&] { highModel.Allocate({1.}, definitions); }, "MarketIvLocalVolBudgetExceeded", "/simulation/rsg");
    ASSERT_EQ(highModel.SimDim(), 0U);
}

TEST(MarketIvLocalVolTest, TestTodayOnlyAndPathRangeFailure) {
    const auto data = BuildMarketIvLocalVolModelData("model", Grid(), Settings());
    AAD::MarketIvLocalVolModel_ model(*data);
    const Vector_<AAD::SampleDef_> definitions(1);
    model.Allocate({0.}, definitions);
    model.Init({0.}, definitions);
    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    AAD::InitializePath(path);
    ASSERT_EQ(model.SimDim(), 0U);
    model.GeneratePath({}, &path);
    ASSERT_DOUBLE_EQ(path[0].spot_, 100.);
    ASSERT_DOUBLE_EQ(path[0].numeraire_, 1.);
    model.Allocate({1.}, definitions);
    model.Init({1.}, definitions);
    Vector_<> normals(model.SimDim(), 0.);
    normals[0] = std::numeric_limits<double>::infinity();
    AssertError([&] { model.GeneratePath(normals, &path); }, "MarketIvLocalVolNumericRange", "/path/spot");
    normals[0] = 0.;
    ASSERT_NO_THROW(model.GeneratePath(normals, &path));
    ASSERT_TRUE(AAD::IsValidModelPath(path));
    ASSERT_TRUE(model.SupportsIndex(*Index::Parse("EQ[A]")));
    ASSERT_FALSE(model.SupportsIndex(*Index::Parse("EQ[B]")));
    ASSERT_FALSE(model.SupportsIndex(*Index::Parse("IR[EUR,DF,2027-11-16]")));
    ASSERT_FALSE(model.SupportsIndex(*Index::Parse("IR[USD,LIBOR_3M_LCH]")));
}

TEST(MarketIvLocalVolTest, TestInspectionIdentitySeparatesProductAndConfiguration) {
    using namespace Script;
    const auto data = BuildMarketIvLocalVolModelData("model", Grid(), Settings());
    const auto today = CarryMarket().EvaluationDate();
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    const ScriptProductData_ one("one", {Cell_(today.AddDays(365))}, {"cash PAYS 1"});
    const ScriptProductData_ two("two", {Cell_(today.AddDays(365))}, {"cash PAYS 2"});
    const auto first = InspectMarketIvLocalVolExecution(one, *data, valuation);
    ASSERT_EQ(first.requestIdentity_, InspectMarketIvLocalVolExecution(one, *data, valuation).requestIdentity_);
    ASSERT_NE(first.requestIdentity_, InspectMarketIvLocalVolExecution(two, *data, valuation).requestIdentity_);
    MonteCarloSettings_ simulation;
    simulation.useBb_ = true;
    ASSERT_NE(first.requestIdentity_, InspectMarketIvLocalVolExecution(one, *data, valuation, simulation).requestIdentity_);
    const auto overrideDate = XGLOBAL::SetEvaluationDateInScope(today.AddDays(1));
    AssertError([&] { static_cast<void>(InspectMarketIvLocalVolExecution(one, *data)); }, "InvalidModelEvaluationDate", "/valuation/evaluation_date");
}

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
