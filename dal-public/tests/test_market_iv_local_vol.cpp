#include <gtest/gtest.h>

#include <iomanip>

#include <dal-public/src/global.hpp>
#include <dal-public/src/marketivsurface.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/script/marketivlocalvol.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

TEST(PublicMarketIvLocalVolTest, TestPassiveFactoryAndExecution) {
    InitGlobalData(1);
    const Date_ today(2026, 11, 16), expiry = today.AddDays(365);
    const MarketIvMarket_ market(today, 100., {{expiry, 102., .98}});
    const MarketIvFitRequest_ request{market, {{"atm", expiry, 102., OptionType_("CALL"), MarketIvQuoteType_("DECIMAL_IV"), .2}}, {}};
    const auto grid = CalibrateMarketIvDupire(FitMarketIvSurface(request).Snapshot(), {{80., 100., 120.}, {.1, 1.}, {}});
    MarketIvLocalVolModelSettings_ settings;
    settings.index_ = "EQ[A]";
    settings.currency_ = "USD";
    settings.factor_ = "W_EQ";
    settings.horizon_ = 1.25;
    const auto model = NewMarketIvLocalVolModelData("market", grid, settings);
    const auto product = NewScriptProduct("df", {Cell_(expiry)}, {"pay PAYS 1"});
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    ASSERT_NEAR(ValueByMonteCarlo(product, Handle_<ModelData_>(model), 16, valuation).at("PV"), .98, 2e-12);
    const auto report = Dal::InspectMarketIvLocalVolExecution(*product, *model, valuation);
    ASSERT_EQ(report.modelIdentity_, model->ModelIdentity());
    ASSERT_EQ(report.preparedColumns_, report.internalSteps_);
    ASSERT_EQ(report.perPathMarketQueries_, 0U);
    {
        struct RestoreThreads_ {
            size_t count_ = ThreadPool_::GetInstance()->NumThreads();
            ~RestoreThreads_() { ThreadPool_::GetInstance()->Start(count_, true); }
        } restore;
        ThreadPool_::GetInstance()->Start(1, true);
        const auto identityProduct = NewScriptProduct("ScriptProductData_", {Cell_(expiry)}, {"pay PAYS 1"});
        auto frozenValuation = valuation;
        // Independent Python hashlib over canonical JSON with fixed worker/settings and exact noon fraction.
        ASSERT_EQ(Dal::InspectMarketIvLocalVolExecution(*identityProduct, *model, frozenValuation).requestIdentity_,
                  "sha256:0f0aa577def22e9db4b2bab9ab3ccc4237b6fc00cec12eb2f444b85345a1b5e2");
        frozenValuation.fixings_.reset(new MarketFixingSnapshot_({{"EQ[A]", {{DateTime_(today.AddDays(-1), .5), 99.}}}}));
        const auto noon = Dal::InspectMarketIvLocalVolExecution(*identityProduct, *model, frozenValuation);
        ASSERT_EQ(noon.requestIdentity_, "sha256:04f36f435aec453a561fbcd494742776ea35fa821be56291492de3eeb19d2339");
        ASSERT_EQ(noon.requestIdentity_, Script::InspectMarketIvLocalVolExecution(*identityProduct, *model, frozenValuation).requestIdentity_);
        ASSERT_EQ(noon.schema_, "dal.market-iv-local-vol-execution/1");
        ASSERT_EQ(noon.sourceIdentity_, model->SourceIdentity());
        ASSERT_EQ(noon.modelIdentity_, model->ModelIdentity());
        size_t fields = 0;
        MarketIvLocalVolFields::Visit(noon, [&](const auto&, const auto&) { ++fields; });
        ASSERT_EQ(fields, 23U);
        ASSERT_NEAR(ValueByMonteCarlo(identityProduct, Handle_<ModelData_>(model), 16, frozenValuation).at("PV"), .98, 2e-12);
    }
    const auto vanilla = NewScriptProduct("call", {Cell_(expiry)}, {"pay PAYS MAX(FIX(EQ[A]) - 102, 0)"});
    MonteCarloSettings_ simulation;
    simulation.rsg_ = "mrg32";
    const auto native = Script::MCSimulation<double>(*vanilla, Handle_<ModelData_>(model), 64, valuation, simulation);
    const double pv = ValueByMonteCarlo(vanilla, Handle_<ModelData_>(model), 64, valuation, simulation).at("PV");
    ASSERT_DOUBLE_EQ(pv, native.aggregated_ / 64.);
    std::cout << std::setprecision(17) << "DAL543_ALIGNMENT " << model->SourceIdentity() << " " << model->ModelIdentity() << " " << pv << "\n";
    simulation.enableAad_ = true;
    ASSERT_THROW(ValueByMonteCarlo(product, Handle_<ModelData_>(model), 16, valuation, simulation), MarketIvLocalVolError_);
    const auto exercise = NewScriptProduct("exercise", {Cell_(expiry)}, {"EXERCISE MAX(spot() - 100, 0)"});
    ASSERT_THROW(ValueByMonteCarlo(exercise, Handle_<ModelData_>(model), 16, valuation), MarketIvLocalVolError_);
    ASSERT_THROW(static_cast<void>(JSON::WriteString(*model)), MarketIvLocalVolError_);
}
