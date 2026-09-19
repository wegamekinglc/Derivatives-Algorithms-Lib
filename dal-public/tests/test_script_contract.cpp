//
// Created by dal-tester on 2026/9/15.
//

#include <gtest/gtest.h>
#include <rapidjson/document.h>

#include <dal-public/src/global.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>
#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/detail/simulationobserver.hpp>
#include <dal/storage/globals.hpp>
#include <dal/storage/json.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::DateTime_;
using Dal::Handle_;
using Dal::MarketFixingSnapshot_;
using Dal::String_;
using Dal::Vector_;

namespace {
    struct ContractReads_ : Dal::Detail::FixingReadObserver_ {
        size_t histories_ = 0;
        Vector_<String_> names_;
        Vector_<DateTime_> times_;
        void BeforeHistory(const String_&) override { ++histories_; }
        void BeforeFixing(const Dal::Index_& index, const Dal::Environment_*, const DateTime_& time) override {
            names_.push_back(index.Name());
            times_.push_back(time);
        }
    };

    struct ContractWorkers_ : Dal::Script::Detail::SimulationObserver_ {
        size_t submissions_ = 0;
        void AfterSubmission() override { ++submissions_; }
    };
} // namespace

TEST(ScriptContractTest, TestExplainInterleavesModelAndInverseFxHistoryIds) {
    Dal::InitGlobalData(1);
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 23));
    const auto product =
        Dal::NewScriptProduct("interleaved", {Cell_(Date_(2026, 9, 22))},
                              {"pay PAYS FIX(EQ[Z], 2026-09-11) + FIX(EQ[A], 2026-09-15) + FIX(FX[EUR/USD], 2026-09-11) + FIX(eq[z], 2026-09-11)"});
    Dal::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    const DateTime_ historyTime(Date_(2026, 9, 11), 0.0);
    valuation.fixings_ =
        Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[Z]", {{historyTime, 80.0}}}, {"FX[USD/EUR]", {{historyTime, 0.8}}}}));
    const Handle_<Dal::ModelData_> model(new Dal::BSModelData_("model", 100.0, 0.0, 0.0, 0.0));
    ContractReads_ reads;
    ContractWorkers_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const Dal::Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);

    rapidjson::Document explanation;
    explanation.Parse(Dal::ExplainScriptValuation(product, model, valuation).c_str());
    ASSERT_FALSE(explanation.HasParseError());
    ASSERT_EQ(explanation["requests"].Size(), 3u);
    const auto& requests = explanation["requests"];
    const Vector_<String_> names{"EQ[Z]", "EQ[A]", "FX[EUR/USD]"};
    const Vector_<String_> nodeIds{"n5", "n6", "n7"};
    for (rapidjson::SizeType i = 0; i < requests.Size(); ++i) {
        ASSERT_EQ(requests[i]["request_id"].GetUint(), i);
        ASSERT_STREQ(requests[i]["index_canonical"].GetString(), names[i].c_str());
        ASSERT_STREQ(requests[i]["uses"][0]["node_id"].GetString(), nodeIds[i].c_str());
        ASSERT_EQ(requests[i]["uses"][0]["source"]["row"].GetUint(), 1u);
    }
    ASSERT_EQ(requests[0]["history_value_id"].GetUint(), 0u);
    ASSERT_DOUBLE_EQ(requests[0]["value"].GetDouble(), 80.0);
    ASSERT_EQ(requests[0]["uses"].Size(), 2u);
    ASSERT_STREQ(requests[0]["uses"][1]["index_original"].GetString(), "eq[z]");
    ASSERT_STREQ(requests[0]["uses"][1]["node_id"].GetString(), "n8");
    ASSERT_TRUE(requests[0]["model_slot"].IsNull());
    ASSERT_TRUE(requests[1]["history_value_id"].IsNull());
    ASSERT_TRUE(requests[1]["value"].IsNull());
    ASSERT_EQ(requests[1]["model_slot"]["sample_id"].GetUint(), 0u);
    ASSERT_EQ(requests[2]["history_value_id"].GetUint(), 1u);
    ASSERT_DOUBLE_EQ(requests[2]["value"].GetDouble(), 1.25);
    ASSERT_TRUE(requests[2]["model_slot"].IsNull());
    ASSERT_EQ(explanation["event_to_sample"][0].GetUint(), 1u);
    ASSERT_EQ(reads.names_, (Vector_<String_>{"EQ[Z]", "FX[EUR/USD]"}));
    ASSERT_EQ(reads.times_, (Vector_<DateTime_>{historyTime, historyTime}));
    ASSERT_EQ(reads.histories_, 0u);
    ASSERT_EQ(workers.submissions_, 0u);

    for (const bool compiled : {false, true}) {
        for (const bool aad : {false, true}) {
            const Dal::MonteCarloSettings_ simulation{"sobol", false, aad, 0.01, compiled};
            const auto count = reads.names_.size();
            const auto result = Dal::ValueByMonteCarlo(product, model, 257, valuation, simulation);
            ASSERT_NEAR(result.at("PV"), 80.0 + 100.0 + 1.0 / 0.8 + 80.0, 261.25e-12);
            if (aad)
                ASSERT_NEAR(result.at("d_spot"), 1.0, 1.0e-10);
            ASSERT_EQ(reads.names_.size(), count + 2);
            ASSERT_EQ(reads.names_[count], String_("EQ[Z]"));
            ASSERT_EQ(reads.names_[count + 1], String_("FX[EUR/USD]"));
            ASSERT_EQ(reads.histories_, 0u);
        }
    }
    ASSERT_GT(workers.submissions_, 0u);
    ASSERT_EQ(Dal::GetEvaluationDate(), Date_(2026, 9, 23));
}

TEST(ScriptContractTest, TestArchiveStaysContractOnlyAfterExplainAndAadRepricing) {
    Dal::InitGlobalData(1);
    const auto product = Dal::NewScriptProduct("archive-after-pricing", {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
                                               {"2", "x = SCALE * SPOT()", "pay PAYS x"}, Dal::ScriptProductSettings_{"eq[Contract]"});
    const auto original = Dal::JSON::WriteString(*product);
    const Handle_<Dal::ModelData_> model(new Dal::BSModelData_("model", 100.0, 0.0, 0.05, 0.0));
    for (const auto date : {Date_(2026, 9, 12), Date_(2026, 9, 15)}) {
        for (const double fixing : {80.0, 90.0}) {
            Dal::ScriptValuationSettings_ valuation;
            valuation.evaluationDate_ = date;
            valuation.fixings_ =
                Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[CONTRACT]", {{DateTime_(Date_(2026, 9, 11), 0.0), fixing}}}}));
            rapidjson::Document explanation;
            explanation.Parse(Dal::ExplainScriptValuation(product, model, valuation).c_str());
            ASSERT_FALSE(explanation.HasParseError());
            ASSERT_DOUBLE_EQ(explanation["requests"][0]["value"].GetDouble(), fixing);
            for (const bool compiled : {false, true}) {
                const Dal::MonteCarloSettings_ simulation{"sobol", false, true, 0.01, compiled};
                const auto result = Dal::ValueByMonteCarlo(product, model, 257, valuation, simulation);
                const double time = (Date_(2026, 9, 22) - date) / Dal::DAYS_PER_YEAR;
                const double expected = 2.0 * fixing * std::exp(-0.05 * time);
                ASSERT_NEAR(result.at("PV"), expected, expected * 1.0e-12);
                ASSERT_NEAR(result.at("d_SCALE"), expected / 2.0, 1.0e-10);
                ASSERT_NEAR(result.at("d_rate"), -time * expected, 1.0e-10);
                ASSERT_EQ(std::string(Dal::JSON::WriteString(*product).c_str()), std::string(original.c_str()));
            }
        }
    }
    rapidjson::Document archive;
    archive.Parse(original.c_str());
    ASSERT_FALSE(archive.HasParseError());
    ASSERT_EQ(archive.MemberCount(), 5u);
    ASSERT_STREQ(archive["~type"].GetString(), "ScriptProductData_v2");
    ASSERT_STREQ(archive["default_index"].GetString(), "eq[Contract]");
    const auto restored = Dal::handle_cast<Dal::ScriptProductData_>(Dal::JSON::ReadString(original, false));
    ASSERT_TRUE(restored);
    ASSERT_EQ(std::string(Dal::JSON::WriteString(*restored).c_str()), std::string(original.c_str()));
}

TEST(ScriptContractTest, TestErrorsRetainFunctionFieldConstraintAndSource) {
    Dal::InitGlobalData(1);
    const auto product = Dal::NewScriptProduct("error-source", {Cell_(Date_(2026, 9, 22))}, {"pay PAYS FIX(eq[Contract], 2026-09-11)"});
    const Handle_<Dal::ModelData_> model(new Dal::BSModelData_("model", 100.0, 0.0, 0.0, 0.0));
    Dal::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    valuation.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());
    const auto assertFields = [](const auto& action, const Vector_<String_>& fields) {
        try {
            action();
            FAIL() << "expected a contract error";
        } catch (const Dal::Exception_& error) {
            for (const auto& field : fields)
                ASSERT_NE(std::string(error.what()).find(field.c_str()), std::string::npos) << error.what();
        }
    };
    ASSERT_NO_FATAL_FAILURE(
        assertFields([&] { Dal::ExplainScriptValuation(product, model, valuation); },
                     {"Resolve", "MissingFixing", "original=eq[Contract]", "canonical=EQ[Contract]", "2026-09-11 00:00:00", "event=2026-09-22",
                      "row=1", "column=14", "statement=0", "node=n2", "ExplicitSnapshot", "exact historical fixing required", "no model fallback"}));
    const Dal::MonteCarloSettings_ simulation{"sobol", false, false, 0.0, false};
    ASSERT_NO_FATAL_FAILURE(assertFields([&] { Dal::ValueByMonteCarlo(product, model, 1, valuation, simulation); },
                                         {"ValidateSimulationSettings", "InvalidSetting", "simulation.smooth_=0", "finite positive"}));
    ASSERT_NO_FATAL_FAILURE(assertFields([&] { Dal::ValueByMonteCarlo(product, model, 0, valuation); },
                                         {"ValueByMonteCarlo", "InvalidPathCount", "numPath=0", "positive integer"}));
}
