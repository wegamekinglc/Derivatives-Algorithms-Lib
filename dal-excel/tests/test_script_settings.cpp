//
// Created by Codex on 2026/9/15.
//

#include <gtest/gtest.h>

#include <limits>
#include <type_traits>

#include <dal-excel/src/__curve_storable.hpp>
#include <dal-excel/src/__script_test_api.hpp>

using namespace Dal;

namespace {
    Matrix_<Cell_> Rows(const Vector_<std::pair<Cell_, Cell_>>& rows) {
        Matrix_<Cell_> result(rows.size(), 2);
        for (int i = 0; i < rows.size(); ++i) {
            result(i, 0) = rows[i].first;
            result(i, 1) = rows[i].second;
        }
        return result;
    }

    template <class F_> void AssertError(F_ action, const Vector_<String_>& fields) {
        try {
            action();
            FAIL() << "expected an input error";
        } catch (const Exception_& error) {
            for (const auto& field : fields)
                ASSERT_NE(std::string(error.what()).find(std::string(field.c_str(), field.size())), std::string::npos) << error.what();
        }
    }
} // namespace

TEST(ScriptExcelContractTest, TestDefaultProductSettings) {
    Handle_<StorableScriptProductSettings_> settings;
    ScriptProductSettings_New("defaults", {}, &settings);
    ASSERT_TRUE(settings);
    ASSERT_EQ(settings->Name(), String_("defaults"));
    ASSERT_TRUE(settings->val_.defaultIndex_.empty());
}

TEST(ScriptExcelContractTest, TestProductSettingsPreserveRowsAndCopyValues) {
    auto input = Rows({{Cell_(), Cell_()}, {Cell_("default_index"), Cell_("eq[MiXeD]")}, {Cell_(""), Cell_()}});
    Handle_<StorableScriptProductSettings_> settings;
    ScriptProductSettings_New("contract", input, &settings);
    ASSERT_EQ(settings->val_.defaultIndex_, String_("eq[MiXeD]"));
    input(1, 1) = "EQ[OTHER]";
    ASSERT_EQ(std::string(settings->val_.defaultIndex_.c_str()), "eq[MiXeD]");
}

TEST(ScriptExcelContractTest, TestProductSettingsRejectBadRows) {
    Handle_<StorableScriptProductSettings_> settings;
    const auto check = [&](const Matrix_<Cell_>& input, const Vector_<String_>& fields) {
        AssertError([&] { ScriptProductSettings_New("bad", input, &settings); }, fields);
    };
    check(Rows({{Cell_("default_index"), Cell_("EQ[A]")}, {Cell_(), Cell_()}, {Cell_("DEFAULT_INDEX"), Cell_("EQ[B]")}}),
          {"InvalidSetting", "ScriptProductSettings_New", "settings row=3 column=1", "duplicate key", "first row=1"});
    check(Rows({{Cell_("unknown"), Cell_(1.0)}}), {"unknown", "row=1 column=1"});
    check(Rows({{Cell_(), Cell_("EQ[A]")}}), {"row=1 column=1", "non-empty"});
    check(Rows({{Cell_("default_index"), Cell_()}}), {"row=1 column=2", "default_index"});
    check(Rows({{Cell_(3.0), Cell_("EQ[A]")}}), {"row=1 column=1", "string"});
    check(Rows({{Cell_("default_index"), Cell_(3.0)}}), {"row=1 column=2", "default_index", "string"});
    check(Matrix_<Cell_>(2, 3), {"row=1 column=3", "rows=2", "cols=3", "2 columns"});
    check(Matrix_<Cell_>(2, 1), {"row=1 column=2", "2 columns"});
}

TEST(ScriptExcelContractTest, TestValuationDefaultsAndExplicitFields) {
    static_assert(std::is_const_v<decltype(StorableScriptValuationSettings_::val_)>);
    Handle_<StorableScriptValuationSettings_> settings;
    ScriptValuationSettings_New("defaults", {}, {}, &settings);
    ASSERT_FALSE(settings->val_.evaluationDate_);
    ASSERT_FALSE(settings->val_.fixings_);
    ASSERT_EQ(settings->val_.todayFixingPolicy_, TodayFixingPolicy_::Value_::MODEL);
    const Handle_<MarketFixingSnapshot_> empty(new MarketFixingSnapshot_());
    const Handle_<StorableMarketFixingSnapshot_> snapshot(new StorableMarketFixingSnapshot_(empty));
    ScriptValuationSettings_New(
        "explicit", Rows({{Cell_("evaluation_date"), Cell_(46277.0)}, {Cell_(), Cell_()}, {Cell_("today_fixing"), Cell_("RequireHistorical")}}),
        snapshot, &settings);
    ASSERT_EQ(settings->val_.evaluationDate_, Date_(2026, 9, 12));
    ASSERT_EQ(settings->val_.todayFixingPolicy_, TodayFixingPolicy_::Value_::REQUIREHISTORICAL);
    ASSERT_EQ(settings->val_.fixings_, empty);
}

TEST(ScriptExcelContractTest, TestValuationRejectsInvalidDatesAndPolicy) {
    Handle_<StorableScriptValuationSettings_> settings;
    for (const auto& date :
         {Cell_(Date_()), Cell_(DateTime_(Date_(2026, 9, 12), 0.0)), Cell_(true), Cell_("2026-09-12"), Cell_(46277.5), Cell_(25568.0), Cell_(91104.0),
          Cell_(1e100), Cell_(std::numeric_limits<double>::infinity()), Cell_(std::numeric_limits<double>::quiet_NaN())}) {
        AssertError([&] { ScriptValuationSettings_New("bad", Rows({{Cell_("evaluation_date"), date}}), {}, &settings); },
                    {"InvalidSetting", "evaluation_date", "row=1 column=2", "valid", "integral"});
    }
    for (const auto& date : {Cell_(25569.0), Cell_(91103.0), Cell_(Date_(2026, 9, 12))}) {
        ScriptValuationSettings_New("date", Rows({{Cell_("evaluation_date"), date}}), {}, &settings);
        ASSERT_TRUE(settings->val_.evaluationDate_->IsValid());
    }
    for (const auto& policy : {Cell_("model"), Cell_("MODEL"), Cell_("UseIfAvailable"), Cell_("Model "), Cell_(true), Cell_(1.0)})
        AssertError([&] { ScriptValuationSettings_New("bad", Rows({{Cell_("today_fixing"), policy}}), {}, &settings); },
                    {"today_fixing", "row=1 column=2"});
    const Handle_<StorableMarketFixingSnapshot_> invalid(new StorableMarketFixingSnapshot_({}));
    AssertError([&] { ScriptValuationSettings_New("bad", {}, invalid, &settings); }, {"fixings", "non-null"});
}

TEST(ScriptExcelContractTest, TestSimulationSettingsDefaultsAndBooleans) {
    static_assert(std::is_const_v<decltype(StorableMonteCarloSettings_::val_)>);
    Handle_<StorableMonteCarloSettings_> settings;
    MonteCarloSettings_New("defaults", {}, &settings);
    ASSERT_EQ(settings->val_.rsg_, String_("sobol"));
    ASSERT_FALSE(settings->val_.useBb_);
    ASSERT_FALSE(settings->val_.enableAad_);
    ASSERT_FALSE(settings->val_.compiled_);
    ASSERT_DOUBLE_EQ(settings->val_.smooth_, .01);
    for (const auto& value : {Cell_(true), Cell_(false), Cell_(1.0), Cell_(0.0)}) {
        const bool expected = Cell::IsBool(value) ? std::get<bool>(value.val_) : Cell::ToDouble(value) == 1.0;
        MonteCarloSettings_New("flags",
                               Rows({{Cell_("use_bb"), value},
                                     {Cell_(), Cell_()},
                                     {Cell_("enable_aad"), value},
                                     {Cell_("compiled"), value},
                                     {Cell_("method"), Cell_("MRG32")},
                                     {Cell_("smooth"), Cell_(.02)}}),
                               &settings);
        ASSERT_EQ(settings->val_.useBb_, expected);
        ASSERT_EQ(settings->val_.enableAad_, expected);
        ASSERT_EQ(settings->val_.compiled_, expected);
        ASSERT_EQ(settings->val_.rsg_, String_("mrg32"));
        ASSERT_DOUBLE_EQ(settings->val_.smooth_, .02);
    }
}

TEST(ScriptExcelContractTest, TestSimulationRejectsInvalidScalarsAndDuplicateKeys) {
    Handle_<StorableMonteCarloSettings_> settings;
    for (const auto* key : {"use_bb", "enable_aad", "compiled"})
        for (const auto& value : {Cell_("TRUE"), Cell_("1"), Cell_(2.0), Cell_(-1.0), Cell_(.5), Cell_(std::numeric_limits<double>::quiet_NaN()),
                                  Cell_(std::numeric_limits<double>::infinity())})
            AssertError([&] { MonteCarloSettings_New("bad", Rows({{Cell_(key), value}}), &settings); },
                        {"InvalidSetting", key, "row=1 column=2", "boolean", "0/1"});
    for (const auto& value : {Cell_(true), Cell_(".01"), Cell_(0.0), Cell_(-.01), Cell_(std::numeric_limits<double>::quiet_NaN()),
                              Cell_(std::numeric_limits<double>::infinity())})
        AssertError([&] { MonteCarloSettings_New("bad", Rows({{Cell_("smooth"), value}}), &settings); },
                    {"smooth", "InvalidSmoothing", "row=1 column=2", "finite", "positive"});
    AssertError([&] { MonteCarloSettings_New("bad", Rows({{Cell_("method"), Cell_(" sobol")}}), &settings); },
                {"method", "row=1 column=2", "sobol", "mrg32", "irn"});
    AssertError([&] { MonteCarloSettings_New("bad", Rows({{Cell_("compiled"), Cell_(true)}, {Cell_("COMPILED"), Cell_(false)}}), &settings); },
                {"duplicate key", "row=2 column=1", "first row=1"});
}
