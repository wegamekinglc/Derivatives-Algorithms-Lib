//
// Created by dal-tester on 2026/8/15.
//

#include <gtest/gtest.h>

#include <cmath>
#include <dal/storage/globals.hpp>
#include <limits>
#include <memory>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::Handle_;
using Dal::String_;
using Dal::Vector_;

namespace {
    // A model data type outside the store supported by ValueByMonteCarlo
    class DummyModelData_ : public Dal::ModelData_ {
    public:
        DummyModelData_() : Dal::ModelData_("DummyModelData_", "dummy") {}
        void Write(Dal::Archive::Store_& dst) const override {}

    private:
        std::unique_ptr<Dal::ModelData_> MutantModel(const String_* newName, const Dal::Slide_* slide) const override {
            return std::make_unique<DummyModelData_>();
        }
    };

    // 1y European call, strike 100, paying at 2023/9/25 (exactly ACT/365F = 1y after 2022/9/25)
    Handle_<Dal::ScriptProductData_> MakeEuropeanCall(const char* name) {
        const Vector_<Cell_> dates = {Cell_("STRIKE"), Cell_(Date_(2023, 9, 25))};
        const Vector_<String_> events = {String_("100.0"), String_("call pays MAX(spot() - STRIKE, 0.0)")};
        return Dal::NewScriptProduct(String_(name), dates, events);
    }

    Handle_<Dal::ModelData_> MakeBSModel(const char* name) { return Dal::NewBSModelData(String_(name), 100.0, 0.2, 0.05, 0.02); }

    double NormalCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

    double NormalPdf(double x) { return std::exp(-0.5 * x * x) / std::sqrt(8.0 * std::atan(1.0)); }

    // spot=100, strike=100, vol=0.2, rate=0.05, div=0.02, mat=1
    void BlackScholesReference(double* call, double* delta, double* vega) {
        const double spot = 100.0, strike = 100.0, vol = 0.2, rate = 0.05, div = 0.02, mat = 1.0;
        const double fwd = spot * std::exp((rate - div) * mat);
        const double stdDev = vol * std::sqrt(mat);
        const double d1 = (std::log(fwd / strike) + 0.5 * stdDev * stdDev) / stdDev;
        const double d2 = d1 - stdDev;
        *call = std::exp(-rate * mat) * (fwd * NormalCdf(d1) - strike * NormalCdf(d2));
        *delta = std::exp(-div * mat) * NormalCdf(d1);
        *vega = spot * std::exp(-div * mat) * NormalPdf(d1) * std::sqrt(mat);
    }

    class ScopedEvaluationDate_ {
        Date_ previous_;

    public:
        explicit ScopedEvaluationDate_(const Date_& d) : previous_(Dal::GetEvaluationDate()) { Dal::SetEvaluationDate(d); }
        ~ScopedEvaluationDate_() { Dal::SetEvaluationDate(previous_); }
    };

    template <class F_> void AssertFields(F_ action, std::initializer_list<const char*> fields) {
        try {
            action();
            FAIL() << "expected a field error";
        } catch (const Dal::Exception_& error) {
            for (const auto* field : fields)
                ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
        }
    }
} // namespace

TEST(ValueTest, TestEuropeanCallMatchesBlackScholes) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_bs_call");
    const auto model = MakeBSModel("value_bs_call_model");

    const auto result = Dal::ValueByMonteCarlo(product, model, 65536);

    double call, delta, vega;
    BlackScholesReference(&call, &delta, &vega);
    ASSERT_NEAR(result.at(String_("PV")), call, 0.05);
}

TEST(ValueTest, TestDeterministicAcrossRuns) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_deterministic");
    const auto model = MakeBSModel("value_deterministic_model");

    {
        const auto first = Dal::ValueByMonteCarlo(product, model, 4096, String_("sobol"));
        const auto second = Dal::ValueByMonteCarlo(product, model, 4096, String_("sobol"));
        ASSERT_DOUBLE_EQ(first.at(String_("PV")), second.at(String_("PV")));
    }
    {
        const auto first = Dal::ValueByMonteCarlo(product, model, 4096, String_("mrg32"));
        const auto second = Dal::ValueByMonteCarlo(product, model, 4096, String_("mrg32"));
        ASSERT_DOUBLE_EQ(first.at(String_("PV")), second.at(String_("PV")));
    }
    {
        const auto first = Dal::ValueByMonteCarlo(product, model, 4096, String_("sobol"), true);
        const auto second = Dal::ValueByMonteCarlo(product, model, 4096, String_("sobol"), true);
        ASSERT_DOUBLE_EQ(first.at(String_("PV")), second.at(String_("PV")));
    }
}

TEST(ValueTest, TestCompiledMatchesInterpreted) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_compiled");
    const auto model = MakeBSModel("value_compiled_model");

    const auto interpreted = Dal::ValueByMonteCarlo(product, model, 4096, String_("sobol"), false, false, 0.01, false);
    const auto compiled = Dal::ValueByMonteCarlo(product, model, 4096, String_("sobol"), false, false, 0.01, true);

    ASSERT_DOUBLE_EQ(interpreted.at(String_("PV")), compiled.at(String_("PV")));
}

TEST(ValueTest, TestAadReturnsValueAndGreeks) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_aad");
    const auto model = MakeBSModel("value_aad_model");

    const auto result = Dal::ValueByMonteCarlo(product, model, 65536, String_("sobol"), false, true);

    double call, delta, vega;
    BlackScholesReference(&call, &delta, &vega);
    ASSERT_NEAR(result.at(String_("PV")), call, 0.05);
    ASSERT_NEAR(result.at(String_("d_spot")), delta, 0.01);
    ASSERT_NEAR(result.at(String_("d_vol")), vega, 0.2);
    ASSERT_TRUE(result.find(String_("d_rate")) != result.end());
    ASSERT_TRUE(result.find(String_("d_div")) != result.end());
}

TEST(ValueTest, TestRejectsNonPositivePathCounts) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_bad_paths");
    const auto model = MakeBSModel("value_bad_paths_model");

    ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, 0), Dal::Exception_);
    ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, -1), Dal::Exception_);
}

TEST(ValueTest, TestRejectsUnsupportedModelType) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_bad_model");
    const Handle_<Dal::ModelData_> model(new DummyModelData_);

    ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, 1), Dal::Exception_);
}

TEST(ValueTest, TestRejectsUnknownRngMethod) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_bad_rsg");
    const auto model = MakeBSModel("value_bad_rsg_model");

    ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, 16, String_("not_a_rng")), Dal::Exception_);
}

TEST(ScriptApiTest, TestLegacyEntryPreparesHistoricalParameterRisk) {
    Dal::InitGlobalData(1);
    const ScopedEvaluationDate_ evalDate(Date_(2026, 9, 12));
    Dal::FixHistory_ history;
    history.vals_ = {{Dal::DateTime_(Date_(2026, 9, 11), 0.0), 80.0}};
    Dal::XGLOBAL::StoreFixings("EQ[DAL196_TEST]", history, false);
    const auto product = Dal::NewScriptProduct("historical-risk", {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
                                               {"2.0", "x = SCALE * FIX(EQ[DAL196_TEST])", "pay PAYS x"});
    const auto model = Dal::NewBSModelData("model", 100.0, 0.0, 0.05, 0.0);
    const double time = 10.0 / Dal::DAYS_PER_YEAR;
    const double expected = 160.0 * std::exp(-0.05 * time);
    for (const bool compiled : {false, true}) {
        const auto result = Dal::ValueByMonteCarlo(product, model, 257, "sobol", false, true, 0.01, compiled);
        ASSERT_NEAR(result.at("PV"), expected, 1.0e-12 * expected);
        ASSERT_NEAR(result.at("d_SCALE"), expected / 2.0, 1.0e-10);
        ASSERT_NEAR(result.at("d_rate"), -time * expected, 1.0e-10);
        ASSERT_DOUBLE_EQ(result.at("d_spot"), 0.0);
        ASSERT_DOUBLE_EQ(result.at("d_vol"), 0.0);
        ASSERT_EQ(result.size(), 6);
    }
}

TEST(ScriptApiTest, TestSettingsExplicitDateAndCopiedContract) {
    Dal::InitGlobalData(1);
    const ScopedEvaluationDate_ evalDate(Date_(2026, 9, 23));
    Dal::ScriptProductSettings_ contract;
    contract.defaultIndex_ = "eq[DAL196_TEST]";
    const auto product = Dal::NewScriptProduct("settings", {Cell_(Date_(2026, 9, 12))}, {"pay PAYS SPOT()"}, contract);
    contract.defaultIndex_ = "EQ[CHANGED]";
    ASSERT_EQ(product->Settings().defaultIndex_, String_("eq[DAL196_TEST]"));
    Dal::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    Dal::MarketFixingSnapshot_::values_t values{{"EQ[DAL196_TEST]", {{Dal::DateTime_(Date_(2026, 9, 12), 0.0), 80.0}}}};
    valuation.fixings_ = Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(values));
    values.clear();
    const auto model = Dal::NewBSModelData("model", 100.0, 0.0, 0.0, 0.0);
    ASSERT_DOUBLE_EQ(Dal::ValueByMonteCarlo(product, model, 1, valuation).at("PV"), 100.0);
    valuation.todayFixingPolicy_ = Dal::TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
    ASSERT_DOUBLE_EQ(Dal::ValueByMonteCarlo(product, model, 1, valuation).at("PV"), 80.0);
    ASSERT_EQ(Dal::GetEvaluationDate(), Date_(2026, 9, 23));
}

TEST(ScriptApiTest, TestSettingErrorsHaveFieldValueAndConstraint) {
    Dal::InitGlobalData(1);
    const ScopedEvaluationDate_ evalDate(Date_(2026, 9, 12));
    const auto product = Dal::NewScriptProduct("settings", {Cell_(Date_(2026, 9, 22))}, {"pay PAYS 1"});
    const auto model = MakeBSModel("model");
    Dal::ScriptValuationSettings_ valuation;
    Dal::MonteCarloSettings_ simulation;
    simulation.smooth_ = 0.0;
    ASSERT_NO_FATAL_FAILURE(AssertFields([&] { Dal::ValueByMonteCarlo(product, model, 1, valuation, simulation); },
                                         {"InvalidSetting", "InvalidSmoothing", "simulation.smooth_=0", "finite", "positive"}));
    simulation.smooth_ = 0.01;
    simulation.rsg_ = "bad_rng";
    ASSERT_NO_FATAL_FAILURE(AssertFields([&] { Dal::ValueByMonteCarlo(product, model, 1, valuation, simulation); },
                                         {"InvalidSetting", "simulation.rsg_=bad_rng", "sobol", "mrg32", "irn"}));
    ASSERT_NO_FATAL_FAILURE(AssertFields([&] { Dal::ValueByMonteCarlo(product, model, 0); }, {"InvalidPathCount", "numPath=0", "positive"}));
    ASSERT_NO_FATAL_FAILURE(AssertFields([&] { Dal::ValueByMonteCarlo({}, model, 1); }, {"InvalidSetting", "product=null", "non-null"}));
    ASSERT_NO_FATAL_FAILURE(AssertFields([&] { Dal::ValueByMonteCarlo(product, {}, 1); }, {"InvalidSetting", "modelData=null", "non-null"}));
}

TEST(ScriptApiTest, TestNativeTailArgumentsRejectConflictingSettings) {
    Dal::InitGlobalData(1);
    const auto product = Dal::NewScriptProduct("native", {Cell_(Date_(2026, 9, 22))}, {"pay PAYS 1"}, Dal::ScriptProductSettings_{"eq[DAL196_TEST]"});
    Dal::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    auto model = Dal::CreateModel<double>(MakeBSModel("model"));
    ASSERT_NO_FATAL_FAILURE(AssertFields([&] { Dal::Script::PrepareScript(*product, model.get(), valuation, {}, {}, {"EQ[OTHER]"}); },
                                         {"InvalidSetting", "product.defaultIndex_", "contract.defaultIndex_", "same canonical"}));
    valuation.fixings_ = Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
    const Handle_<Dal::MarketFixingSnapshot_> other(new Dal::MarketFixingSnapshot_());
    ASSERT_NO_FATAL_FAILURE(AssertFields([&] { Dal::Script::PrepareScript(*product, model.get(), valuation, {}, other); },
                                         {"InvalidSetting", "valuation.fixings_", "snapshot", "one explicit"}));
    const auto prepared = Dal::Script::PrepareScript(*product, model.get(), valuation, {}, valuation.fixings_, {"EQ[dal196_test]"});
    ASSERT_EQ(prepared.Settings().fixings_, valuation.fixings_);
    ASSERT_EQ(prepared.EvaluationDate(), Date_(2026, 9, 12));
    ASSERT_EQ(std::string(product->Settings().defaultIndex_.c_str()), "eq[DAL196_TEST]");
}

TEST(ScriptApiTest, TestObservationErrorsIncludeSourceAndConstraints) {
    Dal::InitGlobalData(1);
    Dal::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    const auto model = MakeBSModel("model");
    auto product = Dal::NewScriptProduct("lookahead", {Cell_(Date_(2026, 9, 22))}, {"pay PAYS FIX(eq[FIELD], 2026-09-23)"});
    ASSERT_NO_FATAL_FAILURE(AssertFields([&] { Dal::ValueByMonteCarlo(product, model, 1, valuation); },
                                         {"LookAheadObservation", "fixing=2026-09-23", "event=2026-09-22", "original=eq[FIELD]",
                                          "canonical=EQ[FIELD]", "row=1", "statement=0", "node=n2", "expected"}));
    product = Dal::NewScriptProduct("indices", {Cell_(Date_(2026, 9, 22))}, {"pay PAYS FIX(EQ[FIELD]) + FIX(EQ[OTHER])"});
    ASSERT_NO_FATAL_FAILURE(AssertFields([&] { Dal::ValueByMonteCarlo(product, model, 1, valuation); },
                                         {"MultipleModelIndices", "EQ[FIELD]", "EQ[OTHER]", "expected"}));
    product = Dal::NewScriptProduct("spot", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))}, {"x = SPOT()", "pay PAYS x"});
    ASSERT_NO_FATAL_FAILURE(AssertFields([&] { Dal::ValueByMonteCarlo(product, model, 1, valuation); },
                                         {"UnboundHistoricalSpot", "product.defaultIndex_", "row=1", "column=5", "event=2026-09-11", "expected"}));
    product = Dal::NewScriptProduct("date", {Cell_(Date_())}, {"pay PAYS 1"});
    ASSERT_NO_FATAL_FAILURE(AssertFields([&] { Dal::ValueByMonteCarlo(product, model, 1, valuation); },
                                         {"InvalidFixingDate", "dates/events", "row=1", "expected a valid event date"}));
}

TEST(ScriptApiTest, TestInvalidSettingsOnExpiredProduct) {
    Dal::InitGlobalData(1);
    const ScopedEvaluationDate_ evalDate(Date_(2026, 9, 12));
    const auto product = Dal::NewScriptProduct("expired", {Cell_(Date_(2026, 9, 11))}, {"pay PAYS 1"});
    const auto model = MakeBSModel("model");
    Dal::ScriptValuationSettings_ valuation;
    for (const double smooth :
         {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        for (const bool aad : {false, true}) {
            const Dal::MonteCarloSettings_ simulation{"sobol", false, aad, smooth, std::nullopt};
            ASSERT_NO_FATAL_FAILURE(AssertFields([&] { Dal::ValueByMonteCarlo(product, model, 1, valuation, simulation); },
                                                 {"InvalidSetting", "simulation.smooth_", "finite positive"}));
            ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, 1, "sobol", false, aad, smooth), Dal::ScriptError_);
        }
    }
    valuation.evaluationDate_ = Date_();
    ASSERT_NO_FATAL_FAILURE(
        AssertFields([&] { Dal::ValueByMonteCarlo(product, model, 1, valuation); }, {"InvalidSetting", "valuation.evaluationDate_", "valid date"}));
    valuation.evaluationDate_.reset();
    for (const int invalid : {-1, 127}) {
        valuation.todayFixingPolicy_.val_ = static_cast<Dal::TodayFixingPolicy_::Value_>(invalid);
        ASSERT_NO_FATAL_FAILURE(
            AssertFields([&] { Dal::ValueByMonteCarlo(product, model, 1, valuation); },
                         {"InvalidSetting", "InvalidTodayFixingPolicy", "valuation.todayFixingPolicy_", "Model or RequireHistorical"}));
    }
    ASSERT_NO_FATAL_FAILURE(AssertFields([&] { Dal::NewScriptProduct("length", {Cell_(Date_(2026, 9, 11))}, {}); },
                                         {"InvalidSetting", "dates.size=1", "events.size=0", "equal lengths"}));
}
