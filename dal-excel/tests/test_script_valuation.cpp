//
// Created by Codex on 2026/9/15.
//

#include <gtest/gtest.h>
#include <rapidjson/document.h>

#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/__xccy_test_api.hpp>
#include <dal-public/src/global.hpp>
#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/script/detail/simulationobserver.hpp>
#include <dal/storage/globals.hpp>

#include "script_test_observers.hpp"

using namespace Dal;

namespace {
    const Date_ D(2026, 9, 12), H(2026, 9, 11), F(2026, 9, 15), P(2026, 9, 22);

    struct DateScope_ {
        const Date_ previous_;
        explicit DateScope_(const Date_& date) : previous_(Excel::ScriptTestSetDate(date)) {}
        ~DateScope_() { Excel::ScriptTestSetDate(previous_); }
    };

    struct ObserverScope_ {
        Detail::FixingReadObserver_* previousReads_;
        Script::Detail::SimulationObserver_* previousWorkers_;
        explicit ObserverScope_(Detail::FixingReadObserver_* reads, Script::Detail::SimulationObserver_* workers = nullptr)
            : previousReads_(Excel::ScriptTestFixingObserver()), previousWorkers_(Excel::ScriptTestSimulationObserver()) {
            Excel::ScriptTestFixingObserver() = reads;
            Excel::ScriptTestSimulationObserver() = workers;
        }
        ~ObserverScope_() {
            Excel::ScriptTestFixingObserver() = previousReads_;
            Excel::ScriptTestSimulationObserver() = previousWorkers_;
        }
    };

    using Script::TestSupport::FixingReadCounter_;
    using Script::TestSupport::SubmissionCounter_;

    struct FixingCleanup_ {
        const String_ index_;
        explicit FixingCleanup_(const String_& index) : index_(index) {}
        ~FixingCleanup_() { Excel::ScriptTestStoreFixings(index_, {}); }
    };

    Matrix_<Cell_> Setting(const char* key, const Cell_& value) {
        Matrix_<Cell_> result(1, 2);
        result(0, 0) = key;
        result(0, 1) = value;
        return result;
    }

    Handle_<StorableScriptValuationSettings_> Valuation(const Handle_<StorableMarketFixingSnapshot_>& snapshot, bool todayHistorical = false) {
        auto input = Setting("evaluation_date", Cell_(double(Date::ToExcel(D))));
        if (todayHistorical) {
            input.Resize(2, 2);
            input(1, 0) = "today_fixing";
            input(1, 1) = "RequireHistorical";
        }
        Handle_<StorableScriptValuationSettings_> result;
        ScriptValuationSettings_New("valuation", input, snapshot, &result);
        return result;
    }

    Handle_<StorableMarketFixingSnapshot_> Snapshot() {
        Handle_<StorableMarketFixingSnapshot_> result;
        MarketFixingSnapshot_New({"EQ[AAPL]", "EQ[AAPL]"}, {Cell_(double(Date::ToExcel(H))), Cell_(double(Date::ToExcel(D)))}, {80., 80.}, &result);
        return result;
    }

    Handle_<ScriptProductData_> Product(const String_& last = "pay PAYS x", const String_& past = "x = SCALE * FIX(EQ[AAPL])") {
        Handle_<StorableScriptProductSettings_> settings;
        ScriptProductSettings_New("contract", Setting("default_index", Cell_("EQ[AAPL]")), &settings);
        Handle_<ScriptProductData_> product;
        Product_NewWithSettings("history", {Cell_("SCALE"), Cell_(double(Date::ToExcel(H))), Cell_(double(Date::ToExcel(P)))}, {"2.0", past, last},
                                settings, &product);
        return product;
    }

    std::map<String_, double> Result(const Matrix_<Cell_>& cells) {
        REQUIRE(cells.Cols() == 2, "expected two columns");
        std::map<String_, double> result;
        for (int i = 0; i < cells.Rows(); ++i) {
            REQUIRE(Cell::IsString(cells(i, 0)) && Cell::IsDouble(cells(i, 1)), "expected string/numeric row");
            const auto key = Cell::ToString(cells(i, 0));
            REQUIRE(key == "PV" || key.substr(0, 2) == "d_", "unexpected result key");
            REQUIRE(std::isfinite(Cell::ToDouble(cells(i, 1))), "nonfinite price/risk");
            REQUIRE(result.emplace(key, Cell::ToDouble(cells(i, 1))).second, "duplicate result key");
        }
        return result;
    }

    rapidjson::Document Json(const Vector_<String_>& chunks) {
        std::string text;
        for (const auto& chunk : chunks) {
            REQUIRE(chunk.size() <= 30000, "chunk too long");
            for (unsigned char c : chunk)
                REQUIRE(c < 128, "non-ASCII chunk");
            text.append(chunk.data(), chunk.size());
        }
        rapidjson::Document result;
        result.Parse(text.data(), text.size());
        REQUIRE(!result.HasParseError(), "invalid diagnostic JSON");
        return result;
    }

    template <class F_> void Error(F_ action, const Vector_<String_>& fields) {
        try {
            action();
            FAIL() << "expected rejection";
        } catch (const Exception_& e) {
            for (const auto& field : fields)
                ASSERT_NE(std::string(e.what()).find(field.c_str()), std::string::npos) << e.what();
        }
    }
} // namespace

TEST(ScriptExcelContractTest, TestHistoricalPriceAndAadOracle) {
    Excel::ScriptTestInitialize(2);
    const DateScope_ restore(D.AddDays(20));
    const Handle_<ModelData_> model(new BSModelData_("model", 100., 0., .05, .02));
    const auto valuation = Valuation(Snapshot());
    const auto product = Product();
    const double t = 10. / 365., expected = 160. * std::exp(-.05 * t);
    for (bool compiled : {false, true})
        for (bool aad : {false, true})
            for (int paths : {1, 257, 8193}) {
                auto input = Setting("compiled", Cell_(compiled));
                input.Resize(2, 2);
                input(1, 0) = "enable_aad";
                input(1, 1) = aad;
                Handle_<StorableMonteCarloSettings_> simulation;
                MonteCarloSettings_New("simulation", input, &simulation);
                Matrix_<Cell_> cells;
                MonteCarlo_ValueWithSettings(product, model, paths, valuation, simulation, &cells);
                const auto result = Result(cells);
                ASSERT_NEAR(result.at("PV"), expected, 1e-12 * expected);
                ASSERT_EQ(result.size(), aad ? 6u : 1u);
                if (aad) {
                    ASSERT_NEAR(result.at("d_SCALE"), expected / 2., 1e-10);
                    ASSERT_NEAR(result.at("d_rate"), -t * expected, 1e-10);
                    for (const auto* key : {"d_spot", "d_vol", "d_div"})
                        ASSERT_NEAR(result.at(key), 0., 1e-10);
                }
            }
}

TEST(ScriptExcelValuationTest, TestProductTableAcceptsNumericVectorsAndFor) {
    const DateScope_ date(D);
    Handle_<ScriptProductData_> product;
    Product_New("excel_vector_loop", {Cell_("WEIGHTS"), Cell_(P)}, {"[0.25, 0.75]", "FOR(i, 0, 2) pay PAYS WEIGHTS[i] * SPOT() END"}, &product);
    ASSERT_NE(DebugScriptProduct(product).find("PAYS"), String_::npos);
}

TEST(ScriptExcelContractTest, TestTodayPolicyAcrossExecutionModes) {
    Excel::ScriptTestInitialize(1);
    const DateScope_ restore(D);
    FixHistory_ history;
    history.vals_ = {{DateTime_(H, 0.0), 80.}, {DateTime_(D, 0.0), 80.}};
    Excel::ScriptTestStoreFixings("EQ[AAPL]", history);
    FixingReadCounter_ reads;
    const ObserverScope_ observe(&reads);
    Vector_<Handle_<ModelData_>> models = {
        Handle_<ModelData_>(new BSModelData_("bs", 100., 0., 0., 0.)),
        Handle_<ModelData_>(new DupireModelData_("dupire", 100., 0., 0., {80., 120.}, {0., 1.}, Matrix_<>(2, 2, 0.)))};
    Handle_<ScriptProductData_> today;
    Product_New("today", {Cell_(double(Date::ToExcel(D)))}, {"pay PAYS FIX(EQ[AAPL])"}, &today);
    for (const auto& model : models)
        for (bool compiled : {false, true})
            for (bool aad : {false, true})
                for (bool require : {false, true}) {
                    auto flags = Setting("compiled", Cell_(compiled));
                    flags.Resize(2, 2);
                    flags(1, 0) = "enable_aad";
                    flags(1, 1) = aad;
                    Handle_<StorableMonteCarloSettings_> simulation;
                    MonteCarloSettings_New("today-mode", flags, &simulation);
                    reads.histories_ = reads.fixings_ = 0;
                    Matrix_<Cell_> cells;
                    MonteCarlo_ValueWithSettings(today, model, 257, Valuation(Snapshot(), require), simulation, &cells);
                    ASSERT_DOUBLE_EQ(Result(cells).at("PV"), require ? 80. : 100.);
                    ASSERT_EQ(reads.fixings_, require ? 1 : 0);
                    ASSERT_EQ(reads.histories_, 0);
                }
}

TEST(ScriptExcelContractTest, TestExactTimestampAndExplicitEmpty) {
    Excel::ScriptTestInitialize(1);
    const DateScope_ restore(D);
    FixHistory_ history;
    history.vals_ = {{DateTime_(H, 0.0), 80.}, {DateTime_(D, 0.0), 80.}};
    Excel::ScriptTestStoreFixings("EQ[AAPL]", history);
    FixingReadCounter_ reads;
    const ObserverScope_ observe(&reads);
    const Handle_<ModelData_> model(new BSModelData_("bs", 100., 0., 0., 0.));
    Handle_<ScriptProductData_> today;
    Product_New("today", {Cell_(double(Date::ToExcel(D)))}, {"pay PAYS FIX(EQ[AAPL])"}, &today);
    Matrix_<Cell_> cells;
    reads.histories_ = reads.fixings_ = 0;
    MonteCarlo_ValueWithSettings(Product(), model, 1, Valuation({}), {}, &cells);
    ASSERT_DOUBLE_EQ(Result(cells).at("PV"), 160.);
    ASSERT_EQ(reads.histories_, 1);
    ASSERT_EQ(reads.fixings_, 1);
    Handle_<StorableMarketFixingSnapshot_> empty, intraday;
    MarketFixingSnapshot_New({}, {}, {}, &empty);
    ASSERT_TRUE(empty && empty->val_);
    MarketFixingSnapshot_New({"EQ[AAPL]"}, {Cell_(Date::ToExcel(H) + 11. / 24.)}, {80.}, &intraday);
    ASSERT_FALSE(intraday->val_->Find("EQ[AAPL]", DateTime_(H, 0.0)));
    for (const auto& snapshot : {empty, intraday}) {
        reads.histories_ = 0;
        Error([&] { MonteCarlo_ValueWithSettings(Product(), model, 1, Valuation(snapshot), {}, &cells); },
              {"MissingFixing", "ExplicitSnapshot", "2026-09-11 00:00:00"});
        ASSERT_EQ(reads.histories_, 0);
    }
    Error([&] { MonteCarlo_ValueWithSettings(today, model, 1, Valuation(empty, true), {}, &cells); }, {"MissingFixing"});
}

TEST(ScriptExcelContractTest, TestLegacyDefaultsAndRetainedFutureOracle) {
    Excel::ScriptTestInitialize(1);
    const DateScope_ restore(D);
    const Handle_<ModelData_> model(new BSModelData_("carry", 100., 0., .05, .02));
    Handle_<ScriptProductData_> legacy;
    Product_New("legacy", {Cell_(double(Date::ToExcel(P)))}, {"pay PAYS SPOT()"}, &legacy);
    Matrix_<Cell_> oldCells, newCells;
    MonteCarlo_Value(legacy, model, 257., "sobol", false, false, .01, &oldCells);
    MonteCarlo_ValueWithSettings(legacy, model, 257., {}, {}, &newCells);
    const double legacyPv = 100. * std::exp(-.02 * 10. / 365.);
    ASSERT_NEAR(Result(oldCells).at("PV"), legacyPv, 1e-12 * legacyPv);
    ASSERT_NEAR(Result(newCells).at("PV"), legacyPv, 1e-12 * legacyPv);
    Handle_<ScriptProductData_> future;
    Product_New("future", {Cell_(double(Date::ToExcel(P)))}, {"pay PAYS FIX(EQ[AAPL], 2026-09-15)"}, &future);
    Handle_<StorableMonteCarloSettings_> simulation;
    MonteCarloSettings_New("aad", Setting("enable_aad", Cell_(true)), &simulation);
    MonteCarlo_ValueWithSettings(future, model, 257., Valuation(Snapshot()), simulation, &newCells);
    const auto result = Result(newCells);
    const double tf = 3. / 365., tp = 10. / 365., expected = 100. * std::exp(.03 * tf - .05 * tp);
    ASSERT_NEAR(result.at("PV"), expected, 1e-12 * expected);
    ASSERT_NEAR(result.at("d_spot"), expected / 100., 1e-10);
    ASSERT_NEAR(result.at("d_rate"), (tf - tp) * expected, 1e-10);
    ASSERT_NEAR(result.at("d_div"), -tf * expected, 1e-10);
    for (double paths : {0., -1., 1.5, std::numeric_limits<double>::infinity()})
        Error([&] { MonteCarlo_ValueWithSettings(legacy, model, paths, {}, {}, &newCells); }, {"n_paths", "InvalidPathCount"});
    Error([&] { Product_NewWithSettings("bad", {}, {}, {}, &future); }, {"settings", "non-null"});
    Error([&] { Product_New("bad-date", {Cell_(46277.5)}, {"pay PAYS 1"}, &future); }, {"integer"});
}

TEST(ScriptExcelContractTest, TestDiagnosticsHaveNoCacheOrWorkersAndPreserveJson) {
    Excel::ScriptTestInitialize(1);
    const DateScope_ restore(D);
    const auto product = Product("pay PAYS x + FIX(EQ[AAPL], 2026-09-15)");
    const Handle_<ModelData_> model(new BSModelData_("zero", 100., 0., 0., 0.));
    FixingReadCounter_ reads;
    SubmissionCounter_ workers;
    const ObserverScope_ observe(&reads, &workers);
    Vector_<String_> chunks;
    Product_Describe(product, &chunks);
    auto describe = Json(chunks);
    ASSERT_EQ(reads.histories_, 0);
    ASSERT_EQ(reads.fixings_, 0);
    ASSERT_EQ(workers.submissions_, 0);
    ASSERT_STREQ(describe["schema"].GetString(), "dal.script-product/2");
    ASSERT_EQ(describe["events"].Size(), 2u);
    {
        const DateScope_ later(D.AddDays(20));
        Product_Describe(product, &chunks);
        ASSERT_TRUE(describe == Json(chunks));
    }
    const auto valuation = Valuation(Snapshot());
    for (int i = 1; i <= 2; ++i) {
        ScriptValuation_Explain(product, model, valuation, &chunks);
        auto explain = Json(chunks);
        ASSERT_EQ(reads.fixings_, 2 * i - 1);
        ASSERT_EQ(reads.histories_, 0);
        ASSERT_EQ(workers.submissions_, 0);
        ASSERT_STREQ(explain["schema"].GetString(), "dal.script-valuation/1");
        ASSERT_EQ(explain["requests"].Size(), 2u);
        ASSERT_EQ(explain["event_to_sample"][0].GetUint(), 1u);
        ASSERT_DOUBLE_EQ(explain["requests"][0]["value"].GetDouble(), 80.);
        ASSERT_EQ(explain["requests"][1]["model_slot"]["sample_id"].GetUint(), 0u);
        rapidjson::Document native;
        native.Parse(Excel::ScriptTestNativeExplain(product, model, valuation->val_).c_str());
        ASSERT_TRUE(explain == native);
        ASSERT_EQ(reads.fixings_, 2 * i); // The independent native call also prepares once.
    }
    Matrix_<Cell_> values;
    MonteCarlo_ValueWithSettings(product, model, 257, valuation, {}, &values);
    ASSERT_DOUBLE_EQ(Result(values).at("PV"), 260.);
    ASSERT_GT(workers.submissions_, 0);
}

TEST(ScriptExcelContractTest, TestLegacyObservationErrorsAndRequestSharing) {
    Excel::ScriptTestInitialize(1);
    const DateScope_ restore(D);
    const Handle_<ModelData_> model(new BSModelData_("zero", 100., 0., 0., 0.));
    const auto valuation = Valuation(Snapshot());
    Matrix_<Cell_> values;
    Handle_<ScriptProductData_> product;
    Product_New("unbound", {Cell_(H), Cell_(P)}, {"x = SPOT()", "pay PAYS x"}, &product);
    Error([&] { MonteCarlo_ValueWithSettings(product, model, 1, valuation, {}, &values); }, {"UnboundHistoricalSpot"});
    Product_New("mixed", {Cell_(P)}, {"pay PAYS SPOT() + FIX(EQ[AAPL])"}, &product);
    Error([&] { MonteCarlo_ValueWithSettings(product, model, 1, valuation, {}, &values); }, {"MissingDefaultIndex"});
    for (const auto* event : {"pay PAYS SPOT(EQ[AAPL])", "pay PAYS FIX()"}) {
        Product_New("syntax", {Cell_(P)}, {event}, &product);
        ASSERT_THROW(MonteCarlo_ValueWithSettings(product, model, 1, valuation, {}, &values), Exception_);
    }
    product = Product("pay PAYS x", "x = SPOT() + FIX(eq[aapl])");
    MonteCarlo_ValueWithSettings(product, model, 257, valuation, {}, &values);
    ASSERT_DOUBLE_EQ(Result(values).at("PV"), 160.);
    Vector_<String_> chunks;
    ScriptValuation_Explain(product, model, valuation, &chunks);
    const auto explanation = Json(chunks);
    ASSERT_EQ(explanation["requests"].Size(), 1u);
    ASSERT_EQ(explanation["requests"][0]["uses"].Size(), 2u);
}

TEST(ScriptExcelContractTest, TestLongDiagnosticUnicodeAndInvalidEncoding) {
    const String_ name = String_(std::string(70000, 'x')) + "\"\\\n\xe4\xb8\xad\xf0\x9f\x98\x80-END";
    Handle_<ScriptProductData_> product;
    Product_New(name, {Cell_(P)}, {"pay PAYS 1"}, &product);
    Vector_<String_> chunks;
    Product_Describe(product, &chunks);
    ASSERT_GT(chunks.size(), 2);
    auto result = Json(chunks);
    ASSERT_EQ(std::string(result["name"].GetString(), result["name"].GetStringLength()), std::string(name.data(), name.size()));
    rapidjson::Document native;
    native.Parse(Excel::ScriptTestNativeDescribe(product).c_str());
    ASSERT_TRUE(result == native);
    for (const auto* bad : {"\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82", "\x80"})
        Error([&] { Excel::ScriptDiagnosticChunks(String_(bad), "Product_Describe"); }, {"DiagnosticEncoding", "Product_Describe"});
}

TEST(ScriptExcelContractTest, TestDefaultValuationCapturesDateAtEachCall) {
    Excel::ScriptTestInitialize(1);
    const DateScope_ restore(H);
    FixingReadCounter_ reads;
    SubmissionCounter_ workers;
    const ObserverScope_ observe(&reads, &workers);
    Handle_<StorableScriptValuationSettings_> valuation;
    ScriptValuationSettings_New("floating-date", {}, Snapshot(), &valuation);
    ASSERT_FALSE(valuation->val_.evaluationDate_);
    ASSERT_EQ(reads.histories_, 0);
    ASSERT_EQ(reads.fixings_, 0);
    Handle_<ScriptProductData_> product;
    Product_New("date-reuse", {Cell_(P)}, {"pay PAYS FIX(EQ[AAPL], 2026-09-12)"}, &product);
    const Handle_<ModelData_> model(new BSModelData_("zero", 100., 0., 0., 0.));
    for (int offset : {0, 1}) {
        const DateScope_ date(D.AddDays(offset));
        reads.fixings_ = workers.submissions_ = 0;
        Vector_<String_> chunks;
        ScriptValuation_Explain(product, model, valuation, &chunks);
        const auto explanation = Json(chunks);
        ASSERT_STREQ(explanation["evaluation_date"].GetString(), offset == 0 ? "2026-09-12" : "2026-09-13");
        ASSERT_EQ(reads.fixings_, offset);
        ASSERT_EQ(reads.histories_, 0);
        ASSERT_EQ(workers.submissions_, 0);
        Matrix_<Cell_> values;
        MonteCarlo_ValueWithSettings(product, model, 1, valuation, {}, &values);
        ASSERT_DOUBLE_EQ(Result(values).at("PV"), offset == 0 ? 100. : 80.);
        ASSERT_EQ(reads.fixings_, 2 * offset);
        ASSERT_GT(workers.submissions_, 0);
    }
    ASSERT_FALSE(valuation->val_.evaluationDate_);
}

TEST(ScriptExcelContractTest, TestReusedGlobalSettingsRefreshHistoryAndKeepExplicitSnapshot) {
    Excel::ScriptTestInitialize(1);
    const DateScope_ restore(D);
    const String_ index = "EQ[DAL245_REPRICE]";
    const FixingCleanup_ cleanup(index);
    Handle_<ScriptProductData_> product;
    Product_New("history-reuse", {Cell_("SCALE"), Cell_(H), Cell_(P)}, {"2", "x = SCALE * FIX(EQ[DAL245_REPRICE])", "pay PAYS x"}, &product);
    Handle_<StorableMarketFixingSnapshot_> snapshot;
    MarketFixingSnapshot_New({index}, {Cell_(DateTime_(H, 0.))}, {80.}, &snapshot);
    Handle_<StorableScriptValuationSettings_> global, explicitSnapshot;
    ScriptValuationSettings_New("global", Setting("evaluation_date", Cell_(D)), {}, &global);
    ScriptValuationSettings_New("fixed", Setting("evaluation_date", Cell_(D)), snapshot, &explicitSnapshot);
    const Handle_<ModelData_> model(new BSModelData_("zero", 100., 0., 0., 0.));
    FixingReadCounter_ reads;
    SubmissionCounter_ workers;
    const ObserverScope_ observe(&reads, &workers);
    Vector_<String_> original;
    Product_Describe(product, &original);
    for (double fixing : {80., 90.}) {
        FixHistory_ history;
        history.vals_ = {{DateTime_(H, 0.), fixing}};
        Excel::ScriptTestStoreFixings(index, history);
        for (const auto& valuation : {global, explicitSnapshot}) {
            const bool isGlobal = valuation == global;
            const double expected = isGlobal ? fixing : 80.;
            reads.histories_ = reads.fixings_ = workers.submissions_ = 0;
            Vector_<String_> description;
            Product_Describe(product, &description);
            ASSERT_EQ(description, original);
            ASSERT_EQ(reads.histories_, 0);
            ASSERT_EQ(reads.fixings_, 0);
            for (int call = 1; call <= 2; ++call) {
                Vector_<String_> chunks;
                ScriptValuation_Explain(product, model, valuation, &chunks);
                const auto explanation = Json(chunks);
                ASSERT_STREQ(explanation["source_kind"].GetString(), isGlobal ? "GlobalSnapshot" : "ExplicitSnapshot");
                ASSERT_DOUBLE_EQ(explanation["requests"][0]["value"].GetDouble(), expected);
                ASSERT_EQ(reads.histories_, isGlobal ? call : 0);
                ASSERT_EQ(reads.fixings_, call);
                ASSERT_EQ(workers.submissions_, 0);
            }
            Matrix_<Cell_> values;
            MonteCarlo_ValueWithSettings(product, model, 257, valuation, {}, &values);
            ASSERT_DOUBLE_EQ(Result(values).at("PV"), 2. * expected);
            ASSERT_GT(workers.submissions_, 0);
        }
    }
}

TEST(ScriptExcelContractTest, TestSimulationExplainRunsValuationAndReportsExerciseEvents) {
    Excel::ScriptTestInitialize(2);
    const DateScope_ restore(D);
    const Handle_<ModelData_> model(new BSModelData_("bs", 100., .2, .05, 0.));
    Handle_<ScriptProductData_> bermudan;
    Product_New("bermudan", {Cell_(F), Cell_(P)}, {"EXERCISE MAX(100.0 - SPOT(), 0.0)", "EXERCISE MAX(100.0 - SPOT(), 0.0)"}, &bermudan);
    FixingReadCounter_ reads;
    SubmissionCounter_ workers;
    const ObserverScope_ observe(&reads, &workers);
    Vector_<String_> chunks;
    ScriptSimulation_Explain(bermudan, model, 1024., {}, {}, &chunks);
    const auto diagnostic = Json(chunks);
    ASSERT_STREQ(diagnostic["schema"].GetString(), "dal.script-simulation/1");
    ASSERT_STREQ(diagnostic["evaluation_date"].GetString(), "2026-09-12");
    ASSERT_STREQ(diagnostic["simulation"]["rsg"].GetString(), "sobol");
    ASSERT_EQ(diagnostic["simulation"]["lsmc_basis_degree"].GetInt(), 3);
    ASSERT_EQ(diagnostic["n_paths"].GetInt(), 1024);
    const auto& events = diagnostic["exercise_events"];
    ASSERT_EQ(events.Size(), 2u);
    ASSERT_EQ(events[0]["event_id"].GetInt(), 0);
    ASSERT_STREQ(events[0]["date"].GetString(), "2026-09-15");
    ASSERT_EQ(events[0]["basis_degree"].GetInt(), 3);
    ASSERT_TRUE(events[0]["regressor_index"].IsNull());
    //  in-the-money condition-true paths enter the regression: 507 of 1024 on this ATM put
    ASSERT_EQ(events[0]["num_cond_true_paths"].GetInt(), 507);
    ASSERT_EQ(events[0]["num_coefficients"].GetInt(), 4);
    ASSERT_EQ(events[0]["coefficients"].Size(), 4u);
    ASSERT_FALSE(events[0]["degenerate"].GetBool());
    ASSERT_TRUE(events[0]["degenerate_reason"].IsNull());
    ASSERT_GT(events[0]["exercise_rate"].GetDouble(), 0.0);
    ASSERT_GT(workers.submissions_, 0);
    { //  the settings handle wires through: degree 5 echo, six coefficients
        Handle_<StorableMonteCarloSettings_> simulation;
        MonteCarloSettings_New("degree", Setting("lsmc_basis_degree", Cell_(5.)), &simulation);
        ScriptSimulation_Explain(bermudan, model, 1024., {}, simulation, &chunks);
        const auto tuned = Json(chunks);
        ASSERT_EQ(tuned["simulation"]["lsmc_basis_degree"].GetInt(), 5);
        ASSERT_EQ(tuned["exercise_events"][0]["num_coefficients"].GetInt(), 6);
    }
    { //  plain products report an empty array, not an omitted key
        Handle_<ScriptProductData_> plain;
        Product_New("plain", {Cell_(P)}, {"pay PAYS 1"}, &plain);
        ScriptSimulation_Explain(plain, model, 256., {}, {}, &chunks);
        const auto withoutExercise = Json(chunks);
        ASSERT_TRUE(withoutExercise["exercise_events"].IsArray());
        ASSERT_EQ(withoutExercise["exercise_events"].Size(), 0u);
        ASSERT_EQ(withoutExercise["n_paths"].GetInt(), 256);
    }
    { // Fixed training paths preserve the fitted policy when the pricing count changes.
        Handle_<StorableMonteCarloSettings_> simulation;
        MonteCarloSettings_New("training", Setting("lsmc_training_paths", Cell_(1024.)), &simulation);
        ScriptSimulation_Explain(bermudan, model, 257., {}, simulation, &chunks);
        const auto tuned = Json(chunks);
        ASSERT_EQ(tuned["simulation"]["lsmc_training_paths"].GetInt(), 1024);
        ASSERT_EQ(tuned["n_paths"].GetInt(), 257);
        for (rapidjson::SizeType i = 0; i < events.Size(); ++i) {
            ASSERT_TRUE(tuned["exercise_events"][i]["coefficients"] == events[i]["coefficients"]);
            ASSERT_TRUE(tuned["exercise_events"][i]["num_cond_true_paths"] == events[i]["num_cond_true_paths"]);
        }
    }
    Handle_<StorableMonteCarloSettings_> aad;
    MonteCarloSettings_New("aad", Setting("enable_aad", Cell_(true)), &aad);
    Error([&] { ScriptSimulation_Explain(bermudan, model, 1024., {}, aad, &chunks); }, {"UnsupportedExecutionMode", "enable_aad"});
    for (double paths : {0., -1., 1.5})
        Error([&] { ScriptSimulation_Explain(bermudan, model, paths, {}, {}, &chunks); }, {"n_paths", "InvalidPathCount"});
}

TEST(ScriptExcelContractTest, TestLegacyAndTypedAadTablesAgree) {
    Excel::ScriptTestInitialize(1);
    const DateScope_ restore(D);
    const Handle_<ModelData_> model(new BSModelData_("carry", 100., .2, .05, .02));
    Handle_<ScriptProductData_> product;
    Product_New("legacy-aad", {Cell_(P)}, {"pay PAYS SPOT()"}, &product);
    Handle_<StorableMonteCarloSettings_> simulation;
    MonteCarloSettings_New("aad", Setting("enable_aad", Cell_(true)), &simulation);
    Matrix_<Cell_> oldCells, newCells;
    MonteCarlo_Value(product, model, 257, "sobol", false, true, .01, &oldCells);
    MonteCarlo_ValueWithSettings(product, model, 257, {}, simulation, &newCells);
    const auto legacy = Result(oldCells), typed = Result(newCells);
    ASSERT_EQ(legacy.size(), 5u);
    ASSERT_EQ(typed.size(), legacy.size());
    for (const auto& entry : legacy) {
        ASSERT_NE(typed.find(entry.first), typed.end());
        ASSERT_NEAR(typed.at(entry.first), entry.second, 1e-8);
    }
}

TEST(ScriptExcelContractTest, TestExerciseValuesAboveEuropeanAndReportsAadRisks) {
    Excel::ScriptTestInitialize(2);
    const DateScope_ restore(Date_(2026, 9, 20));
    const Handle_<ModelData_> model(new BSModelData_("bs", 100., .2, .05, 0.));
    //  the two-date Bermudan of dal-cpp's test_exercise_lsmc.cpp: a 100-strike put
    //  exercisable at the 1y mid date and at the 18m maturity; the single-date leg
    //  (exercise at maturity only) is the matching European put
    const String_ exercise = "EXERCISE MAX(100.0 - SPOT(), 0.0)";
    Handle_<ScriptProductData_> european, bermudan;
    Product_New("european", {Cell_(Date_(2028, 3, 20))}, {exercise}, &european);
    Product_New("bermudan", {Cell_(Date_(2027, 9, 20)), Cell_(Date_(2028, 3, 20))}, {exercise, exercise}, &bermudan);
    Matrix_<Cell_> europeanCells, bermudanCells;
    MonteCarlo_ValueWithSettings(european, model, 4096, {}, {}, &europeanCells);
    MonteCarlo_ValueWithSettings(bermudan, model, 4096, {}, {}, &bermudanCells);
    const double europeanPv = Result(europeanCells).at("PV");
    const double bermudanPv = Result(bermudanCells).at("PV");
    //  the mid-date early-exercise right only adds value; the deterministic sobol
    //  driver keeps the LSMC regression bias far below the premium on this product
    ASSERT_LE(europeanPv, bermudanPv);
    { //  fuzzy-AAD path: the typed table carries PV plus one finite risk per model parameter
        Handle_<StorableMonteCarloSettings_> aad;
        MonteCarloSettings_New("aad", Setting("enable_aad", Cell_(true)), &aad);
        Matrix_<Cell_> aadCells;
        MonteCarlo_ValueWithSettings(bermudan, model, 4096, {}, aad, &aadCells);
        const auto risks = Result(aadCells);
        ASSERT_EQ(risks.size(), 5u);
        for (const auto* key : {"d_spot", "d_vol", "d_rate", "d_div"})
            ASSERT_EQ(risks.count(key), 1u) << key;
        ASSERT_NEAR(risks.at("PV"), bermudanPv, 0.02);
    }
}
