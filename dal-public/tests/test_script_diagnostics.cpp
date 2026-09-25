//
// Created by Codex on 2026/9/15.
//

#include <gtest/gtest.h>
#include <rapidjson/document.h>

#include <dal-public/src/global.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>
#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/script/detail/simulationobserver.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;

namespace {
    struct DiagnosticReads_ : Dal::Detail::FixingReadObserver_ {
        size_t histories_ = 0;
        size_t fixings_ = 0;
        bool reject_ = false;
        void BeforeHistory(const String_&) override {
            ++histories_;
            REQUIRE(!reject_, "unexpected history read");
        }
        void BeforeFixing(const Index_&, const Environment_*, const DateTime_&) override {
            ++fixings_;
            REQUIRE(!reject_, "unexpected fixing read");
        }
    };

    struct DiagnosticWorkers_ : Script::Detail::SimulationObserver_ {
        size_t submissions_ = 0;
        void AfterSubmission() override { ++submissions_; }
    };

    Handle_<ScriptProductData_> DiagnosticProduct() {
        return NewScriptProduct("contract\"name", {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22)), Cell_(Date_(2026, 9, 22))},
                                {"2", "x = SCALE * SPOT()", "pay PAYS FIX(EQ[MiXeD], 2026-09-11)", "pay PAYS FIX(eq[mixed])"}, {"eq[MiXeD]"});
    }

    void StoreFixing(double value, const Date_& date = Date_(2026, 9, 11)) {
        FixHistory_ history;
        history.vals_ = {{DateTime_(date, 0.0), value}};
        XGLOBAL::StoreFixings("EQ[DAL196_TEST]", history, false);
    }

    Vector_<Handle_<ModelData_>> DeterministicModels() {
        return {Handle_<ModelData_>(new BSModelData_("BS", 100.0, 0.0, 0.0, 0.0)),
                Handle_<ModelData_>(new DupireModelData_("Dupire", 100.0, 0.0, 0.0, {80.0, 120.0}, {0.0, 1.0}, Matrix_<>(2, 2, 0.0)))};
    }

    template <class F_> void AssertInvalidVolatility(F_ action) {
        try {
            action();
            FAIL() << "expected InvalidModelParameter";
        } catch (const Dal::Exception_& error) {
            for (const auto* field : {"InvalidModelParameter", "vol", "finite and nonnegative"})
                ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
        }
    }
} // namespace

TEST(ScriptApiTest, TestDescribeContractOnly) {
    InitGlobalData(1);
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    DiagnosticReads_ reads;
    reads.reject_ = true;
    DiagnosticWorkers_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    const auto product = DiagnosticProduct();
    const auto text = DescribeScriptProduct(product);
    rapidjson::Document json;
    json.Parse(text.c_str());
    ASSERT_FALSE(json.HasParseError());
    ASSERT_STREQ(json["schema"].GetString(), "dal.script-product/2");
    ASSERT_STREQ(json["name"].GetString(), "contract\"name");
    ASSERT_FALSE(json.HasMember("evaluation_date"));
    ASSERT_STREQ(json["default_index"]["original"].GetString(), "eq[MiXeD]");
    ASSERT_STREQ(json["input_rows"][1]["text"].GetString(), "x = SCALE * SPOT()");
    ASSERT_EQ(json["events"].Size(), 2u);
    const auto& first = json["events"][0];
    ASSERT_FALSE(first.HasMember("phase"));
    ASSERT_EQ(first["origins"][0]["row"].GetUint(), 2u);
    const auto& spot = first["statements"][0]["value"]["children"][1];
    ASSERT_STREQ(spot["id"].GetString(), "n4");
    ASSERT_STREQ(spot["type"].GetString(), "Spot");
    ASSERT_STREQ(spot["index_original"].GetString(), "eq[MiXeD]");
    ASSERT_STREQ(spot["fixing_date_mode"].GetString(), "EventDate");
    ASSERT_TRUE(spot["fixing_date_literal"].IsNull());
    ASSERT_STREQ(spot["fixing_time"].GetString(), "2026-09-11 00:00:00");
    ASSERT_EQ(spot["source"]["row"].GetUint(), 2u);
    ASSERT_EQ(spot["source"]["offset"].GetUint(), 12u);
    ASSERT_EQ(spot["source"]["column"].GetUint(), 13u);
    const auto& last = json["events"][1];
    ASSERT_EQ(last["origins"].Size(), 2u);
    const auto& fix = last["statements"][0]["value"];
    ASSERT_STREQ(fix["id"].GetString(), "n7");
    ASSERT_STREQ(fix["kind"].GetString(), "fix");
    ASSERT_STREQ(fix["type"].GetString(), "Fix");
    ASSERT_STREQ(fix["index_original"].GetString(), "EQ[MiXeD]");
    ASSERT_STREQ(fix["fixing_date_literal"].GetString(), "2026-09-11");
    ASSERT_EQ(last["statements"][1]["value"]["source"]["row"].GetUint(), 4u);
    ASSERT_EQ(last["statements"][1]["value"]["source"]["line"].GetUint(), 2u);
    XGLOBAL::SetEvaluationDate(Date_(2026, 9, 23));
    ASSERT_EQ(DescribeScriptProduct(product), text);
    ASSERT_EQ(reads.histories_, 0u);
    ASSERT_EQ(reads.fixings_, 0u);
    ASSERT_EQ(workers.submissions_, 0u);
}

TEST(ScriptApiTest, TestNativePreparationCopiesExecutionSettingsBeforeHistory) {
    InitGlobalData(1);
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    StoreFixing(80.0);
    const auto product = NewScriptProduct("copied-execution", {Cell_(Date_(2026, 9, 22))}, {"pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11)"});
    MonteCarloSettings_ simulation;
    simulation.compiled_ = true;
    struct MutatingReads_ : DiagnosticReads_ {
        MonteCarloSettings_* caller_ = nullptr;
        void BeforeHistory(const String_& name) override {
            DiagnosticReads_::BeforeHistory(name);
            caller_->compiled_ = false;
            XGLOBAL::SetEvaluationDate(Date_(2026, 9, 23));
        }
    } reads;
    reads.caller_ = &simulation;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    auto model = CreateModel<double>(DeterministicModels()[0]);
    const auto prepared = Script::PrepareScript(*product, model.get(), ScriptValuationSettings_(), simulation);
    ASSERT_FALSE(*simulation.compiled_);
    ASSERT_TRUE(*prepared.Simulation().compiled_);
    ASSERT_NO_THROW(static_cast<void>(prepared.Compile()));
    ASSERT_EQ(prepared.EvaluationDate(), Date_(2026, 9, 12));
    ASSERT_EQ(reads.histories_, 1u);
    ASSERT_EQ(reads.fixings_, 1u);
    ASSERT_DOUBLE_EQ(prepared.Plan().KnownValue(0), 80.0);
}

TEST(ScriptApiTest, TestDescribeLookAheadIncludesObservationIdentity) {
    InitGlobalData(1);
    const auto product = NewScriptProduct("lookahead", {Cell_(Date_(2026, 9, 12))}, {"pay PAYS FIX(eq[MiXeD], 2026-09-15)"});
    try {
        static_cast<void>(DescribeScriptProduct(product));
        FAIL() << "expected LookAheadObservation";
    } catch (const ScriptError_& error) {
        for (const auto* field : {"LookAheadObservation", "eq[MiXeD]", "EQ[MiXeD]", "2026-09-15 00:00:00", "event=2026-09-12", "row=1", "line=1",
                                  "statement=0", "node=n2"})
            ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
    }
}

TEST(ScriptApiTest, TestDescribeMacroAndScheduleSources) {
    InitGlobalData(1);
    DiagnosticReads_ reads;
    reads.reject_ = true;
    DiagnosticWorkers_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    const String_ schedule = "START: 2026-09-11 END: 2026-09-13 FREQ: 1CD";
    const auto product = NewScriptProduct("schedule", {Cell_("OBS"), Cell_(schedule)}, {"FIX(eq[PeriodBegin], PeriodBegin)", "pay PAYS OBS"});
    rapidjson::Document json;
    json.Parse(DescribeScriptProduct(product).c_str());
    ASSERT_FALSE(json.HasParseError());
    ASSERT_STREQ(json["input_rows"][0]["text"].GetString(), "FIX(eq[PeriodBegin], PeriodBegin)");
    ASSERT_STREQ(json["input_rows"][1]["date_or_definition"].GetString(), schedule.c_str());
    ASSERT_STREQ(json["input_rows"][1]["text"].GetString(), "pay PAYS OBS");
    ASSERT_EQ(json["events"].Size(), 2u);
    for (rapidjson::SizeType i = 0; i < 2; ++i) {
        const auto& event = json["events"][i];
        const auto& fix = event["statements"][0]["value"];
        ASSERT_EQ(event["event_id"].GetUint(), i);
        ASSERT_EQ(event["origins"][0]["row"].GetUint(), 2u);
        ASSERT_STREQ(event["date"].GetString(), i == 0 ? "2026-09-12" : "2026-09-13");
        ASSERT_STREQ(fix["index_original"].GetString(), "eq[PeriodBegin]");
        ASSERT_STREQ(fix["index_canonical"].GetString(), "EQ[PeriodBegin]");
        ASSERT_STREQ(fix["fixing_date_mode"].GetString(), "Explicit");
        ASSERT_STREQ(fix["fixing_date_literal"].GetString(), i == 0 ? "2026-09-11" : "2026-09-12");
        ASSERT_EQ(fix["source"]["row"].GetUint(), 2u);
        ASSERT_EQ(fix["source"]["offset"].GetUint(), 13u);
        ASSERT_EQ(fix["source"]["line"].GetUint(), 1u);
        ASSERT_EQ(fix["source"]["column"].GetUint(), 14u);
        ASSERT_STREQ(fix["source"]["event_date"].GetString(), event["date"].GetString());
    }
    ASSERT_EQ(reads.histories_, 0u);
    ASSERT_EQ(reads.fixings_, 0u);
    ASSERT_EQ(workers.submissions_, 0u);
}

TEST(ScriptApiTest, TestLegacyJsonRejectsUnusedDefault) {
    InitGlobalData(1);
    const auto product = NewScriptProduct("default", {}, {}, {"EQ[UNUSED]"});
    try {
        DebugScriptProductJson(product);
        FAIL() << "schema /1 must reject a default even on an empty product";
    } catch (const ScriptError_& error) {
        for (const auto* field : {"DebugSchemaUnsupported", "DescribeScriptProduct", "dal.script-product/2"})
            ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
    }
    rapidjson::Document json;
    json.Parse(DescribeScriptProduct(product).c_str());
    ASSERT_FALSE(json.HasParseError());
    ASSERT_TRUE(json["payoff_index"].IsNull());
    ASSERT_TRUE(json["events"].Empty());
}

TEST(ScriptApiTest, TestExplainPreparation) {
    InitGlobalData(1);
    const auto product = DiagnosticProduct();
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    valuation.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[MIXED]", {{DateTime_(Date_(2026, 9, 11), 0.0), 80.0}}}}));
    const Handle_<ModelData_> model(new BSModelData_("model", 100.0, 0.0, 0.0, 0.0));
    DiagnosticReads_ reads;
    DiagnosticWorkers_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    rapidjson::Document json;
    json.Parse(ExplainScriptValuation(product, model, valuation).c_str());
    ASSERT_FALSE(json.HasParseError());
    ASSERT_STREQ(json["schema"].GetString(), "dal.script-valuation/1");
    ASSERT_STREQ(json["evaluation_date"].GetString(), "2026-09-12");
    ASSERT_STREQ(json["today_fixing"].GetString(), "Model");
    ASSERT_STREQ(json["source_kind"].GetString(), "ExplicitSnapshot");
    ASSERT_FALSE(json["simulation"]["compiled"].GetBool());
    ASSERT_FALSE(json["all_expired"].GetBool());
    ASSERT_EQ(json["requests"].Size(), 2u);
    const auto& history = json["requests"][0];
    ASSERT_EQ(history["request_id"].GetUint(), 0u);
    ASSERT_STREQ(history["source"].GetString(), "Historical");
    ASSERT_STREQ(history["resolution"].GetString(), "Resolved");
    ASSERT_EQ(history["history_value_id"].GetUint(), 0u);
    ASSERT_DOUBLE_EQ(history["value"].GetDouble(), 80.0);
    ASSERT_TRUE(history["model_slot"].IsNull());
    ASSERT_EQ(history["uses"].Size(), 2u);
    ASSERT_STREQ(history["uses"][0]["index_original"].GetString(), "eq[MiXeD]");
    ASSERT_STREQ(history["uses"][0]["observation_type"].GetString(), "Spot");
    ASSERT_STREQ(history["uses"][0]["node_id"].GetString(), "n4");
    ASSERT_EQ(history["uses"][0]["source"]["offset"].GetUint(), 12u);
    ASSERT_STREQ(history["uses"][1]["index_original"].GetString(), "EQ[MiXeD]");
    ASSERT_STREQ(history["uses"][1]["node_id"].GetString(), "n7");
    ASSERT_STREQ(history["uses"][1]["fixing_date_mode"].GetString(), "Explicit");
    const auto& future = json["requests"][1];
    ASSERT_EQ(future["request_id"].GetUint(), 1u);
    ASSERT_STREQ(future["source"].GetString(), "Model");
    ASSERT_TRUE(future["value"].IsNull());
    ASSERT_EQ(future["model_slot"]["sample_id"].GetUint(), 0u);
    ASSERT_EQ(future["model_slot"]["output_id"].GetUint(), 0u);
    ASSERT_EQ(future["uses"][0]["event_id"].GetUint(), 1u);
    ASSERT_EQ(json["live_events"][0]["event_id"].GetUint(), 1u);
    ASSERT_EQ(json["live_events"][0]["future_event_index"].GetUint(), 0u);
    ASSERT_EQ(json["event_to_sample"][0].GetUint(), 0u);
    ASSERT_EQ(json["numeraire_requests"][0]["sample_id"].GetUint(), 0u);
    ASSERT_EQ(reads.histories_, 0u);
    ASSERT_EQ(reads.fixings_, 1u);
    ASSERT_EQ(workers.submissions_, 0u);
    ASSERT_DOUBLE_EQ(ValueByMonteCarlo(product, model, 257, valuation).at("PV"), 180.0);
    ASSERT_EQ(reads.fixings_, 2u);
    ASSERT_GT(workers.submissions_, 0u);
}

TEST(ScriptApiTest, TestTodayPolicy) {
    InitGlobalData(1);
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 23));
    const auto product = NewScriptProduct("today", {Cell_(Date_(2026, 9, 12))}, {"pay PAYS FIX(EQ[DAL196_TEST])"});
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    for (const auto& model : DeterministicModels()) {
        for (const bool compiled : {false, true}) {
            for (const bool aad : {false, true}) {
                const MonteCarloSettings_ simulation{"sobol", true, aad, 0.01, compiled};
                DiagnosticReads_ reads;
                DiagnosticWorkers_ workers;
                const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
                const Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
                StoreFixing(80.0, Date_(2026, 9, 12));
                valuation.todayFixingPolicy_ = TodayFixingPolicy_::Value_::MODEL;
                ASSERT_DOUBLE_EQ(ValueByMonteCarlo(product, model, 257, valuation, simulation).at("PV"), 100.0);
                ASSERT_EQ(reads.histories_, 0u);
                ASSERT_EQ(reads.fixings_, 0u);
                valuation.todayFixingPolicy_ = TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
                ASSERT_DOUBLE_EQ(ValueByMonteCarlo(product, model, 257, valuation, simulation).at("PV"), 80.0);
                ASSERT_EQ(reads.histories_, 1u);
                ASSERT_EQ(reads.fixings_, 1u);
                StoreFixing(80.0);
                workers.submissions_ = 0;
                ASSERT_THROW(ValueByMonteCarlo(product, model, 257, valuation, simulation), ScriptError_);
                ASSERT_EQ(workers.submissions_, 0u);
            }
        }
    }
    ASSERT_EQ(GetEvaluationDate(), Date_(2026, 9, 23));
}

TEST(ScriptApiTest, TestRepricingAndExplainNeverCache) {
    InitGlobalData(1);
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto product = NewScriptProduct("repricing", {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
                                          {"2", "x = SCALE * FIX(EQ[DAL196_TEST])", "pay PAYS x"});
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    const Handle_<MarketFixingSnapshot_> oldSnapshot(new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), 80.0}}}}));
    const auto model = DeterministicModels()[0];
    for (const bool compiled : {false, true}) {
        DiagnosticReads_ reads;
        const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        valuation.fixings_.reset();
        StoreFixing(80.0);
        const auto oldPlan = Script::PrepareScript(*product, valuation);
        const Vector_<> values{80.0, 90.0, 80.0};
        for (size_t i = 0; i < values.size(); ++i) {
            const double value = values[i];
            if (i == 1)
                StoreFixing(value);
            else if (i == 2)
                valuation.fixings_ = oldSnapshot;
            rapidjson::Document json;
            json.Parse(ExplainScriptValuation(product, model, valuation).c_str());
            ASSERT_FALSE(json.HasParseError());
            ASSERT_STREQ(json["source_kind"].GetString(), i == 2 ? "ExplicitSnapshot" : "GlobalSnapshot");
            ASSERT_DOUBLE_EQ(json["requests"][0]["value"].GetDouble(), value);
            const auto result = ValueByMonteCarlo(product, model, 257, valuation, simulation);
            ASSERT_DOUBLE_EQ(result.at("PV"), 2.0 * value);
            ASSERT_DOUBLE_EQ(result.at("d_SCALE"), value);
        }
        ASSERT_DOUBLE_EQ(oldPlan.Plan().KnownValue(0), 80.0);
        ASSERT_EQ(reads.histories_, 5u);
        ASSERT_EQ(reads.fixings_, 7u);
        valuation.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());
        const auto histories = reads.histories_;
        ASSERT_THROW(ValueByMonteCarlo(product, model, 1, valuation), ScriptError_);
        ASSERT_EQ(reads.histories_, histories);
    }
}

TEST(ScriptApiTest, TestExpiredAndEmpty) {
    InitGlobalData(1);
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    const auto product = NewScriptProduct("expired", {Cell_("SCALE"), Cell_(Date_(2026, 9, 11))}, {"2", "pay PAYS SCALE * FIX(EQ[NO_HISTORY])"});
    DiagnosticReads_ reads;
    reads.reject_ = true;
    DiagnosticWorkers_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    for (const auto& model : DeterministicModels()) {
        for (const bool compiled : {false, true}) {
            MonteCarloSettings_ simulation;
            simulation.enableAad_ = true;
            simulation.compiled_ = compiled;
            const auto result = ValueByMonteCarlo(product, model, 257, valuation, simulation);
            ASSERT_GT(result.size(), 1u);
            for (const auto& entry : result)
                ASSERT_DOUBLE_EQ(entry.second, 0.0);
        }
        rapidjson::Document json;
        json.Parse(ExplainScriptValuation(product, model, valuation).c_str());
        ASSERT_TRUE(json["all_expired"].GetBool());
        ASSERT_STREQ(json["requests"][0]["resolution"].GetString(), "SkippedExpired");
        ASSERT_TRUE(json["requests"][0]["value"].IsNull());
        ASSERT_TRUE(json["requests"][0]["history_value_id"].IsNull());
        ASSERT_TRUE(json["sample_dates"].Empty());
        ASSERT_TRUE(json["numeraire_requests"].Empty());
        for (const auto& empty : {NewScriptProduct("", {}, {}), NewScriptProduct("", {Cell_("SCALE")}, {"2"}),
                                  NewScriptProduct("", {Cell_(Date_(2026, 9, 22))}, {"x = 1"})}) {
            ASSERT_THROW(ValueByMonteCarlo(empty, model, 1, valuation), ScriptError_);
            ASSERT_THROW(ExplainScriptValuation(empty, model, valuation), ScriptError_);
            json.Parse(DescribeScriptProduct(empty).c_str());
            ASSERT_FALSE(json.HasParseError());
            ASSERT_TRUE(json["payoff_index"].IsNull());
        }
    }
    ASSERT_EQ(reads.histories_, 0u);
    ASSERT_EQ(reads.fixings_, 0u);
    ASSERT_EQ(workers.submissions_, 0u);
}

TEST(ScriptApiTest, TestDefaultSpotDeduplicatesAndLegacyGuards) {
    InitGlobalData(1);
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto model = DeterministicModels()[0];
    StoreFixing(80.0);
    const Vector_<Cell_> dates{Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))};
    const Vector_<String_> events{"x = SPOT() + FIX(eq[dal196_test])", "pay PAYS x"};
    const auto bound = NewScriptProduct("bound", dates, events, {"EQ[DAL196_TEST]"});
    DiagnosticReads_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    ASSERT_DOUBLE_EQ(ValueByMonteCarlo(bound, model, 257).at("PV"), 160.0);
    ASSERT_EQ(reads.fixings_, 1u);
    ASSERT_EQ(reads.histories_, 1u);
    ASSERT_THROW(ValueByMonteCarlo(NewScriptProduct("unbound", dates, events), model, 1), ScriptError_);
    for (const String_ text : {"pay PAYS FIX(EQ[DAL196_TEST]) + SPOT()", "pay PAYS SPOT(EQ[DAL196_TEST])", "pay PAYS FIX()"}) {
        ASSERT_THROW(ValueByMonteCarlo(NewScriptProduct("bad", {Cell_(Date_(2026, 9, 22))}, {text}), model, 1), ScriptError_);
    }
}

TEST(ScriptApiTest, TestExplainDoesNotFreezeGlobalMarket) {
    InitGlobalData(1);
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto product = NewScriptProduct("market", {Cell_(Date_(2026, 9, 22))}, {"pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11)"});
    const auto model = DeterministicModels()[0];
    StoreFixing(80.0);
    DiagnosticReads_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    rapidjson::Document json;
    json.Parse(ExplainScriptValuation(product, model).c_str());
    ASSERT_DOUBLE_EQ(json["requests"][0]["value"].GetDouble(), 80.0);
    StoreFixing(90.0);
    ASSERT_DOUBLE_EQ(ValueByMonteCarlo(product, model, 257).at("PV"), 90.0);
    ASSERT_EQ(reads.histories_, 2u);
    ASSERT_EQ(reads.fixings_, 2u);
}

TEST(ScriptApiTest, TestExplainRetainedFixingAndPaymentSamples) {
    InitGlobalData(1);
    const auto product = NewScriptProduct("samples", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
                                          {"x = 2", "pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15) + x"});
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    DiagnosticReads_ reads;
    reads.reject_ = true;
    DiagnosticWorkers_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    const Handle_<ModelData_> model(new BSModelData_("model", 100.0, 0.0, 0.05, 0.0));
    rapidjson::Document json;
    json.Parse(ExplainScriptValuation(product, model, valuation).c_str());
    ASSERT_FALSE(json.HasParseError());
    ASSERT_EQ(json["sample_dates"].Size(), 2u);
    ASSERT_STREQ(json["sample_dates"][0].GetString(), "2026-09-15");
    ASSERT_STREQ(json["sample_dates"][1].GetString(), "2026-09-22");
    ASSERT_EQ(json["requests"][0]["model_slot"]["sample_id"].GetUint(), 0u);
    ASSERT_EQ(json["event_to_sample"][0].GetUint(), 1u);
    ASSERT_EQ(json["live_events"][0]["event_id"].GetUint(), 1u);
    ASSERT_FALSE(json["sample_definitions"][0]["numeraire"].GetBool());
    ASSERT_TRUE(json["sample_definitions"][1]["numeraire"].GetBool());
    ASSERT_EQ(json["numeraire_requests"][0]["sample_id"].GetUint(), 1u);
    ASSERT_EQ(workers.submissions_, 0u);
    const double expected = (100.0 * std::exp(0.05 * 3.0 / DAYS_PER_YEAR) + 2.0) * std::exp(-0.05 * 10.0 / DAYS_PER_YEAR);
    ASSERT_NEAR(ValueByMonteCarlo(product, model, 1, valuation).at("PV"), expected, 1.0e-12 * expected);
    ASSERT_EQ(reads.histories_, 0u);
    ASSERT_EQ(reads.fixings_, 0u);
}

TEST(ScriptApiTest, TestPublicPreparationFailuresSubmitNoWorkersAndRecover) {
    InitGlobalData(1);
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    FixHistory_ history;
    history.vals_ = {{DateTime_(Date_(2026, 9, 10), 0.0), 70.0}, {DateTime_(Date_(2026, 9, 11), 0.0), 80.0}};
    XGLOBAL::StoreFixings("EQ[DAL196_TEST]", history, false);
    const auto product =
        NewScriptProduct("barrier", {Cell_(Date_(2026, 9, 22))}, {"pay PAYS FIX(EQ[DAL196_TEST], 2026-09-10) + FIX(EQ[DAL196_TEST], 2026-09-11)"});
    struct FailureReads_ : DiagnosticReads_ {
        size_t failAt_ = 0;
        void BeforeFixing(const Index_& index, const Environment_* environment, const DateTime_& time) override {
            DiagnosticReads_::BeforeFixing(index, environment, time);
            REQUIRE(fixings_ != failAt_, "injected last fixing failure");
        }
    } reads;
    struct FailureWorkers_ : DiagnosticWorkers_ {
        bool failCompilation_ = false;
        void BeforeCompilation() override { REQUIRE(!failCompilation_, "injected compilation failure"); }
    } workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    const auto model = DeterministicModels()[0];
    const Handle_<ModelData_> badModel(new BSModelData_("bad", 100.0, -0.1, 0.0, 0.0));
    for (const bool legacy : {false, true}) {
        for (const bool aad : {false, true}) {
            MonteCarloSettings_ simulation;
            simulation.enableAad_ = aad;
            simulation.compiled_ = true;
            const auto value = [&](const auto& data, const auto& modelData) {
                return legacy ? ValueByMonteCarlo(data, modelData, 8193, "sobol", false, aad, 0.01, true)
                              : ValueByMonteCarlo(data, modelData, 8193, ScriptValuationSettings_(), simulation);
            };
            reads.fixings_ = 0;
            reads.failAt_ = 2;
            workers.submissions_ = 0;
            ASSERT_THROW(value(product, model), ScriptError_);
            ASSERT_EQ(reads.fixings_, 2u);
            ASSERT_EQ(workers.submissions_, 0u);
            reads.failAt_ = 0;
            reads.fixings_ = 0;
            reads.histories_ = 0;
            ASSERT_NO_FATAL_FAILURE(AssertInvalidVolatility([&] { value(product, badModel); }));
            ASSERT_EQ(reads.histories_, 0u);
            ASSERT_EQ(reads.fixings_, 0u);
            ASSERT_EQ(workers.submissions_, 0u);
            workers.failCompilation_ = true;
            ASSERT_THROW(value(product, model), Dal::Exception_);
            ASSERT_EQ(workers.submissions_, 0u);
            workers.failCompilation_ = false;
            ASSERT_DOUBLE_EQ(value(product, model).at("PV"), 150.0);
            const auto invalidPath = NewScriptProduct("path", {Cell_(Date_(2026, 9, 22))}, {"pay PAYS LOG(-1)"});
            workers.submissions_ = 0;
            ASSERT_THROW(value(invalidPath, model), Dal::Exception_);
            ASSERT_GT(workers.submissions_, 0u);
            ASSERT_DOUBLE_EQ(value(product, model).at("PV"), 150.0);
        }
    }
    workers.submissions_ = 0;
    reads.fixings_ = 0;
    reads.failAt_ = 2;
    ASSERT_THROW(ExplainScriptValuation(product, model), ScriptError_);
    ASSERT_EQ(reads.fixings_, 2u);
    reads.fixings_ = 0;
    reads.failAt_ = 0;
    ASSERT_NO_FATAL_FAILURE(AssertInvalidVolatility([&] { ExplainScriptValuation(product, badModel); }));
    ASSERT_EQ(reads.fixings_, 0u);
    ASSERT_EQ(workers.submissions_, 0u);
}

TEST(ScriptApiTest, TestExplainScriptSimulation) {
    InitGlobalData(1);
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto model = Handle_<ModelData_>(new BSModelData_("bs", 100.0, 0.2, 0.05, 0.0));
    const auto product = NewScriptProduct("bermudan", {Cell_(Date_(2026, 12, 12)), Cell_(Date_(2027, 3, 12))},
                                          {"EXERCISE MAX(100.0 - spot(), 0.0)", "EXERCISE MAX(100.0 - spot(), 0.0)"});
    const auto text = ExplainScriptSimulation(product, model, 4096);
    rapidjson::Document json;
    json.Parse(text.c_str());
    ASSERT_FALSE(json.HasParseError());
    ASSERT_STREQ(json["schema"].GetString(), "dal.script-simulation/1");
    ASSERT_STREQ(json["evaluation_date"].GetString(), "2026-09-12");
    ASSERT_STREQ(json["simulation"]["rsg"].GetString(), "sobol");
    ASSERT_FALSE(json["simulation"]["enable_aad"].GetBool());
    ASSERT_EQ(json["simulation"]["lsmc_basis_degree"].GetInt(), 3);
    ASSERT_EQ(json["n_paths"].GetInt(), 4096);
    ASSERT_STREQ(json["uncertainty"]["mode"].GetString(), "deterministic");
    ASSERT_TRUE(json["uncertainty"]["replicate_mean_se"].IsNull());
    ASSERT_TRUE(json["uncertainty"]["payoff_dispersion_se"].IsNumber());

    const auto& events = json["exercise_events"];
    ASSERT_TRUE(events.IsArray());
    ASSERT_EQ(events.Size(), 2u);
    ASSERT_EQ(events[0]["event_id"].GetInt(), 0);
    ASSERT_STREQ(events[0]["date"].GetString(), "2026-12-12");
    ASSERT_EQ(events[0]["basis_degree"].GetInt(), 3);
    ASSERT_STREQ(events[0]["basis"].GetString(), "NormalizedMonomial");
    ASSERT_EQ(events[0]["effective_rank"].GetInt(), 4);
    ASSERT_STREQ(events[0]["solver"].GetString(), "MomentsCholesky");
    ASSERT_TRUE(events[0]["fallback_reason"].IsNull());
    ASSERT_TRUE(events[0]["validation_mse"].IsNull());
    ASSERT_TRUE(events[0]["regressor_index"].IsNull());
    //  in-the-money condition-true paths enter the regression: 1926 of 4096 on this ATM put
    ASSERT_EQ(events[0]["num_cond_true_paths"].GetInt(), 1926);
    ASSERT_EQ(events[0]["num_coefficients"].GetInt(), 4);
    ASSERT_EQ(events[0]["coefficients"].Size(), 4u);
    ASSERT_FALSE(events[0]["degenerate"].GetBool());
    ASSERT_TRUE(events[0]["degenerate_reason"].IsNull());
    ASSERT_GT(events[0]["exercise_rate"].GetDouble(), 0.0);

    { //  the compiled mode joins the LSMC driver (T3): the echo reports it and the events stay consistent
        MonteCarloSettings_ simulation;
        simulation.compiled_ = true;
        const auto compiledText = ExplainScriptSimulation(product, model, 4096, ScriptValuationSettings_(), simulation);
        rapidjson::Document compiledJson;
        compiledJson.Parse(compiledText.c_str());
        ASSERT_FALSE(compiledJson.HasParseError());
        ASSERT_TRUE(compiledJson["simulation"]["compiled"].GetBool());
        const auto& compiledEvents = compiledJson["exercise_events"];
        ASSERT_EQ(compiledEvents.Size(), events.Size());
        for (rapidjson::SizeType k = 0; k < events.Size(); ++k) {
            ASSERT_EQ(compiledEvents[k]["event_id"].GetInt(), events[k]["event_id"].GetInt());
            ASSERT_NEAR(compiledEvents[k]["exercise_rate"].GetDouble(), events[k]["exercise_rate"].GetDouble(), 1e-4);
        }
    }
    { // held-out selection is visible without exposing validation rows as pricing observations
        MonteCarloSettings_ simulation;
        simulation.lsmcTrainingPaths_ = 1024;
        simulation.lsmcValidationPaths_ = 256;
        simulation.lsmcBasisDegree_ = 5;
        rapidjson::Document adaptive;
        adaptive.Parse(ExplainScriptSimulation(product, model, 257, ScriptValuationSettings_(), simulation).c_str());
        ASSERT_FALSE(adaptive.HasParseError());
        ASSERT_EQ(adaptive["simulation"]["lsmc_validation_paths"].GetInt(), 256);
        for (const auto& event : adaptive["exercise_events"].GetArray()) {
            ASSERT_GE(event["basis_degree"].GetInt(), 1);
            ASSERT_LE(event["basis_degree"].GetInt(), 5);
            ASSERT_TRUE(event["validation_mse"].IsNumber());
            ASSERT_TRUE(event["effective_rank"].IsUint64());
        }
    }
    { //  RQMC diagnostics report per-replicate means and their conditional error.
        MonteCarloSettings_ simulation;
        simulation.lsmcTrainingPaths_ = 1024;
        simulation.lsmcRqmcReplicates_ = 4;
        simulation.lsmcTrainingSeed_ = 17;
        simulation.lsmcPricingSeed_ = 29;
        rapidjson::Document rqmc;
        rqmc.Parse(ExplainScriptSimulation(product, model, 257, ScriptValuationSettings_(), simulation).c_str());
        ASSERT_FALSE(rqmc.HasParseError());
        ASSERT_EQ(rqmc["simulation"]["lsmc_rqmc_replicates"].GetInt(), 4);
        const auto& uncertainty = rqmc["uncertainty"];
        ASSERT_STREQ(uncertainty["mode"].GetString(), "rqmc_conditional_policy");
        ASSERT_STREQ(uncertainty["scramble"].GetString(), "sobol-digital-shift-splitmix64-v1");
        ASSERT_EQ(uncertainty["training_seed"].GetInt(), 17);
        ASSERT_EQ(uncertainty["pricing_seed"].GetInt(), 29);
        ASSERT_EQ(uncertainty["training_paths"].GetUint64(), 1024u);
        ASSERT_EQ(uncertainty["pricing_paths_per_replicate"].GetUint64(), 257u);
        ASSERT_EQ(uncertainty["pricing_paths_total"].GetUint64(), 4u * 257u);
        ASSERT_EQ(uncertainty["replicate_means"].Size(), 4u);
        ASSERT_TRUE(uncertainty["replicate_mean_se"].IsNumber());
        ASSERT_TRUE(uncertainty["payoff_dispersion_se"].IsNumber());
    }
    { //  products without EXERCISE return an empty exercise_events array and burn no simulation
        DiagnosticWorkers_ workers;
        const Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
        const auto plain = NewScriptProduct("plain", {Cell_(Date_(2027, 3, 12))}, {"pay PAYS 1.0"});
        const auto plainText = ExplainScriptSimulation(plain, model, 1024);
        rapidjson::Document plainJson;
        plainJson.Parse(plainText.c_str());
        ASSERT_FALSE(plainJson.HasParseError());
        ASSERT_STREQ(plainJson["uncertainty"]["mode"].GetString(), "not_applicable");
        ASSERT_TRUE(plainJson["uncertainty"]["payoff_dispersion_se"].IsNull());
        ASSERT_TRUE(plainJson["exercise_events"].IsArray());
        ASSERT_EQ(plainJson["exercise_events"].Size(), 0u);
        ASSERT_EQ(plainJson["n_paths"].GetInt(), 1024);
        ASSERT_EQ(workers.submissions_, 0u);
    }
    { //  a negative path count is rejected before the size_t conversion
        try {
            static_cast<void>(ExplainScriptSimulation(product, model, -1));
            FAIL() << "expected InvalidPathCount";
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string(error.what()).find("InvalidPathCount"), std::string::npos);
            ASSERT_NE(std::string(error.what()).find("numPath=-1"), std::string::npos);
        }
    }
}

TEST(ScriptApiTest, TestSimulationEchoFieldSetMatchesAcrossSchemas) {
    InitGlobalData(1);
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const Handle_<ModelData_> model(new BSModelData_("model", 100.0, 0.0, 0.0, 0.0));
    const auto product = NewScriptProduct("echo", {Cell_(Date_(2026, 9, 22))}, {"pay PAYS 1"});
    rapidjson::Document valuation, simulation;
    valuation.Parse(ExplainScriptValuation(product, model).c_str());
    ASSERT_FALSE(valuation.HasParseError());
    ASSERT_STREQ(valuation["schema"].GetString(), "dal.script-valuation/1");
    simulation.Parse(ExplainScriptSimulation(product, model, 16).c_str());
    ASSERT_FALSE(simulation.HasParseError());
    ASSERT_STREQ(simulation["schema"].GetString(), "dal.script-simulation/1");
    for (const auto* json : {&valuation, &simulation}) {
        const auto& echo = (*json)["simulation"];
        ASSERT_EQ(echo.MemberCount(), 11u);
        for (const auto* field : {"rsg", "use_bb", "enable_aad", "smooth", "compiled", "lsmc_basis_degree", "lsmc_training_paths",
                                  "lsmc_validation_paths", "lsmc_rqmc_replicates", "lsmc_training_seed", "lsmc_pricing_seed"})
            ASSERT_TRUE(echo.HasMember(field)) << field;
        ASSERT_TRUE(echo["lsmc_training_paths"].IsNull());
        ASSERT_TRUE(echo["lsmc_validation_paths"].IsNull());
        ASSERT_TRUE(echo["lsmc_rqmc_replicates"].IsNull());
        ASSERT_TRUE(echo["lsmc_training_seed"].IsNull());
        ASSERT_TRUE(echo["lsmc_pricing_seed"].IsNull());
    }
    ASSERT_TRUE(valuation["simulation"] == simulation["simulation"]);
}
