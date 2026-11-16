#include <gtest/gtest.h>

#include <climits>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <sstream>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/compoundedois.hpp>
#include <dal/curve/curvescript.hpp>
#include <dal/curve/curvescriptevaluation.hpp>

#include "compoundedoisfixture.hpp"

using namespace Dal;

namespace {
    CompoundedOisScriptSpec_ ScheduleSpec(const String_& mode = "None", int lookback = 0, int lockout = 0) {
        auto s = CompoundedOisTest::Contract();
        s.ruleId_ = "dal.compounded-ois-cashflows/2";
        s.observationConvention_ = CompoundedOisObservationConvention_{CompoundedOisObservationMode_(mode), lookback, lockout};
        s.observations_.clear();
        const Date_ start(2024, 3, 5), end(2024, 3, 8);
        s.coupons_ = {{"coupon", start, end, end}};
        s.fixedLeg_ = {{start, end, end, 3. / 365.}};
        return s;
    }
    std::vector<std::string> CsvRow(std::string line) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::istringstream input(line);
        std::vector<std::string> result;
        std::string field;
        while (std::getline(input, field, ','))
            result.push_back(field);
        if (!line.empty() && line.back() == ',')
            result.emplace_back();
        return result;
    }
    void AssertRejected(const CompoundedOisScriptSpec_& s, const String_& field) {
        try {
            PrepareCompoundedOis(s, CompoundedOisTest::Valuation());
            FAIL() << "Invalid schedule accepted";
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.field_, field);
            ASSERT_EQ(error.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Prepare);
            ASSERT_EQ(error.Failure().diagnostic_.instrumentId_, s.instrumentId_);
            ASSERT_TRUE(error.Failure().report_);
        }
    }
    void AssertSourceMismatch(const std::function<void()>& action) {
        try {
            action();
            FAIL() << "Mismatched legacy source accepted";
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, "CompoundedOisSourceMismatch");
        }
    }
} // namespace

TEST(CompoundedOisScheduleTest, TestNoneFreezesExplicitBusinessIntervalsWithoutSampling) {
    auto s = ScheduleSpec();
    auto v = CompoundedOisTest::Valuation();
    v.evaluationDate_ = Date_(2024, 3, 5);
    v.fixings_.reset(new MarketFixingSnapshot_());
    const auto prepared = PrepareCompoundedOis(s, v);
    ASSERT_TRUE(prepared->Spec().observations_.empty());
    ASSERT_EQ(prepared->ObservationSchedule().size(), 3u);
    const auto& row = prepared->ObservationSchedule()[0];
    ASSERT_EQ(row.accrualStart_, Date_(2024, 3, 5));
    ASSERT_EQ(row.accrualEnd_, Date_(2024, 3, 6));
    ASSERT_EQ(row.rateStart_, row.accrualStart_);
    ASSERT_EQ(row.weightStart_, row.accrualStart_);
    ASSERT_DOUBLE_EQ(row.compoundingAccrual_, 1. / 360.);
    ASSERT_EQ(row.fixingIndex_, "USD-SOFR");
    ASSERT_EQ(row.sourceKey_, "observation_schedule/0");
    ASSERT_FALSE(row.historicalRequired_);
    ASSERT_TRUE(row.included_);
    ASSERT_FALSE(prepared->ObservationScheduleIdentity().empty());
}

TEST(CompoundedOisScheduleTest, TestAllModesMatchIndependentHolidayCsv) {
    std::ifstream input(std::filesystem::path(__FILE__).parent_path() / "data/compounded_ois_observation_schedule.csv");
    ASSERT_TRUE(input.good());
    std::string line;
    std::getline(input, line);
    size_t count = 0;
    while (std::getline(input, line)) {
        const auto expected = CsvRow(line);
        ASSERT_EQ(expected.size(), 17u);
        SCOPED_TRACE(line);
        const auto tables =
            PrepareCompoundedOis(ScheduleSpec(String_(expected[0]), std::stoi(expected[1]), std::stoi(expected[2])), CompoundedOisTest::Valuation());
        ASSERT_EQ(tables->ObservationSchedule().size(), 3u);
        const auto& row = tables->ObservationSchedule()[std::stoul(expected[3])];
        ASSERT_EQ(CalibrationInternal::Bytes(Date::ToString(row.accrualStart_)), expected[4]);
        ASSERT_EQ(CalibrationInternal::Bytes(Date::ToString(row.accrualEnd_)), expected[5]);
        ASSERT_NEAR(row.accrual_, std::stod(expected[6]) / 360., 1e-15);
        ASSERT_EQ(CalibrationInternal::Bytes(Date::ToString(row.rateStart_)), expected[7]);
        ASSERT_EQ(CalibrationInternal::Bytes(Date::ToString(row.rateEnd_)), expected[8]);
        ASSERT_NEAR(row.rateAccrual_, std::stod(expected[9]) / 360., 1e-15);
        ASSERT_EQ(CalibrationInternal::Bytes(Date::ToString(row.weightStart_)), expected[10]);
        ASSERT_EQ(CalibrationInternal::Bytes(Date::ToString(row.weightEnd_)), expected[11]);
        ASSERT_NEAR(row.compoundingAccrual_, std::stod(expected[12]) / 360., 1e-15);
        ASSERT_EQ(CalibrationInternal::Bytes(row.fixingIndex_), expected[13]);
        ASSERT_EQ(CalibrationInternal::Bytes(Date::ToString(row.fixingTime_.Date())), expected[14]);
        ASSERT_EQ(row.reusedFromRow_.has_value(), !expected[16].empty());
        if (row.reusedFromRow_)
            ASSERT_EQ(*row.reusedFromRow_, std::stoul(expected[16]));
        ++count;
    }
    ASSERT_EQ(count, 12u);
}

TEST(CompoundedOisScheduleTest, TestWorkingWeekendLeapDayAndAct365Weights) {
    auto s = ScheduleSpec("Lookback", 1);
    s.calendar_ = HolidayCenterData_("working-Saturday", {Date_(2024, 3, 4)}, {Date_(2024, 3, 2)});
    const auto working = PrepareCompoundedOis(s, CompoundedOisTest::Valuation());
    ASSERT_EQ(working->ObservationSchedule()[0].rateStart_, Date_(2024, 3, 2));
    ASSERT_EQ(working->ObservationSchedule()[0].rateEnd_, Date_(2024, 3, 5));
    ASSERT_DOUBLE_EQ(working->ObservationSchedule()[0].rateAccrual_, 3. / 360.);
    s = ScheduleSpec("ObservationShift", 1);
    s.observationBasis_ = DayBasis_("ACT_365F");
    s.coupons_ = {{"leap", Date_(2024, 2, 29), Date_(2024, 3, 1), Date_(2024, 3, 1)}};
    const auto leap = PrepareCompoundedOis(s, CompoundedOisTest::Valuation());
    ASSERT_EQ(leap->ObservationSchedule()[0].rateStart_, Date_(2024, 2, 28));
    ASSERT_EQ(leap->ObservationSchedule()[0].rateEnd_, Date_(2024, 2, 29));
    ASSERT_DOUBLE_EQ(leap->ObservationSchedule()[0].compoundingAccrual_, 1. / 365.);
    ASSERT_EQ(leap->Spec().fixedLeg_[0].payment_, Date_(2024, 3, 8));
}

TEST(CompoundedOisScheduleTest, TestMappedFixingPolicySettlementAndFrozenInputs) {
    auto s = ScheduleSpec("Lookback", 1);
    auto v = CompoundedOisTest::Valuation();
    v.evaluationDate_ = Date_(2024, 3, 5);
    v.fixings_.reset(new MarketFixingSnapshot_());
    const auto model = PrepareCompoundedOis(s, v);
    ASSERT_TRUE(model->ObservationSchedule()[0].historicalRequired_);
    ASSERT_FALSE(model->ObservationSchedule()[1].historicalRequired_);
    ASSERT_FALSE(model->ObservationSchedule()[2].historicalRequired_);
    v.todayFixingPolicy_ = TodayFixingPolicy_("RequireHistorical");
    const auto historical = PrepareCompoundedOis(s, v);
    ASSERT_TRUE(historical->ObservationSchedule()[1].historicalRequired_);
    ASSERT_EQ(model->ContractIdentity(), historical->ContractIdentity());
    ASSERT_EQ(model->ObservationScheduleIdentity(), historical->ObservationScheduleIdentity());
    ASSERT_NE(model->PreparationIdentity(), historical->PreparationIdentity());
    v.evaluationDate_ = Date_(2024, 3, 8);
    ASSERT_TRUE(PrepareCompoundedOis(s, v)->ObservationSchedule()[0].included_);
    v.evaluationDate_ = Date_(2024, 3, 9);
    const auto settled = PrepareCompoundedOis(s, v);
    ASSERT_TRUE(settled->ObservationSchedule()[0].settled_);
    ASSERT_FALSE(settled->ObservationSchedule()[0].historicalRequired_);
    ASSERT_EQ(settled->ContractIdentity(), model->ContractIdentity());
    s.calendar_->holidays_.clear();
    s.observationConvention_->lookbackBusinessDays_ = 2;
    ASSERT_EQ(model->ObservationSchedule()[0].rateStart_, Date_(2024, 3, 1));
    ASSERT_EQ(model->Spec().calendar_->holidays_.size(), 1u);
}

TEST(CompoundedOisScheduleTest, TestIdentityLayersIncludeCalendarAndCompleteSnapshot) {
    auto s = ScheduleSpec();
    auto v = CompoundedOisTest::Valuation();
    const auto original = PrepareCompoundedOis(s, v);
    auto fixings = v.fixings_->Values();
    fixings[s.fixingIndex_][DateTime_(Date_(2024, 2, 29))] = .7;
    v.fixings_.reset(new MarketFixingSnapshot_(fixings));
    const auto changedHistory = PrepareCompoundedOis(s, v);
    ASSERT_EQ(changedHistory->ContractIdentity(), original->ContractIdentity());
    ASSERT_EQ(changedHistory->ObservationScheduleIdentity(), original->ObservationScheduleIdentity());
    ASSERT_NE(changedHistory->PreparationIdentity(), original->PreparationIdentity());
    s.notional_ *= 2.;
    const auto changedNotional = PrepareCompoundedOis(s, v);
    ASSERT_NE(changedNotional->ContractIdentity(), original->ContractIdentity());
    ASSERT_EQ(changedNotional->ObservationScheduleIdentity(), original->ObservationScheduleIdentity());
    s.calendar_->center_ = "different-frozen-calendar";
    const auto changedCalendar = PrepareCompoundedOis(s, v);
    ASSERT_NE(changedCalendar->ObservationScheduleIdentity(), changedNotional->ObservationScheduleIdentity());
    ASSERT_NE(changedCalendar->ContractIdentity(), changedNotional->ContractIdentity());
    v.fixings_.reset();
    ASSERT_THROW(PrepareCompoundedOis(s, v), CalibrationError_);
}

TEST(CompoundedOisScheduleTest, TestLockoutMultipleCouponsKeepsGlobalAnchorAndOwnWeights) {
    auto s = ScheduleSpec("Lockout", 0, 1);
    s.coupons_ = {{"first", Date_(2024, 2, 28), Date_(2024, 3, 1), Date_(2024, 3, 8)},
                  {"second", Date_(2024, 3, 1), Date_(2024, 3, 6), Date_(2024, 3, 8)}};
    const auto tables = PrepareCompoundedOis(s, CompoundedOisTest::Valuation());
    const auto& rows = tables->ObservationSchedule();
    ASSERT_EQ(rows.size(), 4u);
    ASSERT_EQ(rows[1].reusedFromRow_, std::optional<size_t>(0));
    ASSERT_EQ(rows[3].reusedFromRow_, std::optional<size_t>(2));
    ASSERT_EQ(rows[2].rateStart_, rows[3].rateStart_);
    ASSERT_EQ(rows[2].fixingTime_, rows[3].fixingTime_);
    ASSERT_DOUBLE_EQ(rows[2].compoundingAccrual_, 4. / 360.);
    ASSERT_DOUBLE_EQ(rows[3].compoundingAccrual_, 1. / 360.);
    ASSERT_FALSE(rows[3].historicalRequired_);
}

TEST(CompoundedOisScheduleTest, TestRejectsMissingMutuallyExclusiveAndOutOfRangeConventions) {
    using Edit = std::function<void(CompoundedOisScriptSpec_*)>;
    const std::vector<std::pair<Edit, String_>> cases{
        {[](auto* s) { s->ruleId_ = "dal.compounded-ois-cashflows/1"; }, "/observation_convention"},
        {[](auto* s) { s->observationConvention_.reset(); }, "/observation_convention"},
        {[](auto* s) { s->observationConvention_->mode_ = CompoundedOisObservationMode_(); }, "/observation_convention/mode"},
        {[](auto* s) { s->observationConvention_->lookbackBusinessDays_ = -1; }, "/observation_convention/lookback_business_days"},
        {[](auto* s) { s->observationConvention_->lockoutBusinessDays_ = 1; }, "/observation_convention/lockout_business_days"},
        {[](auto* s) { s->observations_ = CompoundedOisTest::Contract().observations_; }, "/observations"},
        {[](auto* s) { s->coupons_[0].start_ = Date_(2024, 3, 4); }, "/coupons/0"},
        {[](auto* s) {
             s->coupons_[0].end_ = Date_(2024, 3, 9);
             s->coupons_[0].payment_ = Date_(2024, 3, 9);
         },
         "/coupons/0"},
        {[](auto* s) { s->observationConvention_ = CompoundedOisObservationConvention_{CompoundedOisObservationMode_("Lookback"), 0, 0}; },
         "/observation_convention/lookback_business_days"},
        {[](auto* s) { s->observationConvention_ = CompoundedOisObservationConvention_{CompoundedOisObservationMode_("Lookback"), INT_MAX, 0}; },
         "/observation_convention/lookback_business_days"},
        {[](auto* s) { s->observationConvention_ = CompoundedOisObservationConvention_{CompoundedOisObservationMode_("Lockout"), 0, 3}; },
         "/observation_convention/lockout_business_days"}};
    for (const auto& [edit, field] : cases) {
        auto s = ScheduleSpec();
        edit(&s);
        ASSERT_NO_FATAL_FAILURE(AssertRejected(s, field));
    }
}

TEST(CompoundedOisScheduleTest, TestVersionTwoArchiveRoundTripAndStructuralIdentity) {
    for (const auto& mode : {String_("None"), String_("Lookback"), String_("ObservationShift"), String_("Lockout")}) {
        auto s = ScheduleSpec(mode, mode == "Lookback" || mode == "ObservationShift" ? 1 : 0, mode == "Lockout" ? 1 : 0);
        const auto tables = PrepareCompoundedOis(s, CompoundedOisTest::Valuation());
        const auto json = DumpPreparedCompoundedOisJson(tables);
        const auto loaded = LoadPreparedCompoundedOisJson(json);
        ASSERT_EQ(CalibrationInternal::Bytes(DumpPreparedCompoundedOisJson(loaded)), CalibrationInternal::Bytes(json));
        ASSERT_EQ(loaded->ContractIdentity(), tables->ContractIdentity());
        ASSERT_EQ(loaded->PreparationIdentity(), tables->PreparationIdentity());
        ASSERT_EQ(loaded->ObservationScheduleIdentity(), tables->ObservationScheduleIdentity());
        ASSERT_TRUE(loaded->Spec().observations_.empty());
    }
}

TEST(CompoundedOisScheduleTest, TestArchiveRejectsEveryChangedDerivedFieldAndSchemaMismatch) {
    using namespace CalibrationInternal;
    const auto tables = PrepareCompoundedOis(ScheduleSpec("Lockout", 0, 1), CompoundedOisTest::Valuation());
    const auto original = Parse(DumpPreparedCompoundedOisJson(tables));
    const auto& row = original.object_.at("observation_schedule").array_[2];
    for (const auto& [key, value] : row.object_) {
        auto changed = original;
        changed.object_.at("observation_schedule").array_[2].object_[key] = Json_();
        try {
            LoadPreparedCompoundedOisJson(String_(Jcs(changed)));
            FAIL() << "Accepted tampered " << key;
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Load);
            ASSERT_EQ(error.Failure().diagnostic_.field_, "/observation_schedule/2");
            ASSERT_EQ(error.Failure().diagnostic_.sourceRow_, std::optional<int>(3));
        }
    }
    auto invalid = original;
    invalid.object_["schema"] = Str("dal.compounded-ois-preparation/1");
    ASSERT_THROW(LoadPreparedCompoundedOisJson(String_(Jcs(invalid))), CalibrationError_);
    invalid = original;
    invalid.object_.at("spec").object_["rule"] = Str("dal.compounded-ois-cashflows/1");
    ASSERT_THROW(LoadPreparedCompoundedOisJson(String_(Jcs(invalid))), CalibrationError_);
    invalid = original;
    invalid.object_.at("spec").object_.at("observation_convention").object_["mode"] = Str("lockout");
    ASSERT_THROW(LoadPreparedCompoundedOisJson(String_(Jcs(invalid))), CalibrationError_);
}

TEST(CompoundedOisScheduleTest, TestVersionTwoSourceRequiresItsOwnFrozenTables) {
    const auto tables = PrepareCompoundedOis(ScheduleSpec(), CompoundedOisTest::Valuation());
    ASSERT_TRUE(NewCompoundedOisScriptInstrument(tables));
    const auto calibration = CompoundedOisTest::Calibration("Shared");
    auto request = CompoundedOisTest::Request(calibration);
    request.compoundedOis_ = tables;
    const auto legacy = NewCompoundedOisScriptInstrument(calibration.compoundedOis_[1]);
    ASSERT_NO_FATAL_FAILURE(AssertSourceMismatch([&] { EvaluateCurveScriptInstrument(legacy, request); }));
}

TEST(CompoundedOisScheduleTest, TestDateUnderflowFailsBeforeCalendarAllocation) {
    auto s = ScheduleSpec("Lookback", 3);
    s.coupons_ = {{"minimum", Date_(1970, 1, 5), Date_(1970, 1, 6), Date_(1970, 1, 6)}};
    ASSERT_NO_FATAL_FAILURE(AssertRejected(s, "/observation_convention/lookback_business_days"));
    s.observationConvention_->lookbackBusinessDays_ = INT_MAX;
    ASSERT_NO_FATAL_FAILURE(AssertRejected(s, "/observation_convention/lookback_business_days"));
}

TEST(CompoundedOisScheduleTest, TestArchiveRejectsInvalidCountTypesAndUnknownFields) {
    using namespace CalibrationInternal;
    const auto original = Parse(DumpPreparedCompoundedOisJson(PrepareCompoundedOis(ScheduleSpec("Lookback", 1), CompoundedOisTest::Valuation())));
    for (const auto& count : {Num(-1.), Num(.5), Num(static_cast<double>(INT_MAX) + 1.), Bool(true), Str("1")}) {
        auto changed = original;
        changed.object_.at("spec").object_.at("observation_convention").object_["lookback_business_days"] = count;
        ASSERT_THROW(LoadPreparedCompoundedOisJson(String_(Jcs(changed))), CalibrationError_);
    }
    for (const auto* key : {"unknown", "Mode"}) {
        auto changed = original;
        changed.object_.at("spec").object_.at("observation_convention").object_[key] = Str("Lookback");
        ASSERT_THROW(LoadPreparedCompoundedOisJson(String_(Jcs(changed))), CalibrationError_);
    }
    auto changed = original;
    changed.object_.at("observation_schedule").array_.pop_back();
    ASSERT_THROW(LoadPreparedCompoundedOisJson(String_(Jcs(changed))), CalibrationError_);
}

TEST(CompoundedOisScheduleTest, TestMissingArchiveFieldsKeepOwningLoadErrors) {
    for (const auto* json : {"{}", "[]", "null"})
        ASSERT_THROW(LoadPreparedCompoundedOisJson(json), CalibrationError_);
    using namespace CalibrationInternal;
    auto value = Parse(DumpPreparedCompoundedOisJson(PrepareCompoundedOis(ScheduleSpec(), CompoundedOisTest::Valuation())));
    value.object_.at("spec").object_.erase("observation_convention");
    ASSERT_THROW(LoadPreparedCompoundedOisJson(String_(Jcs(value))), CalibrationError_);
}

TEST(CompoundedOisScheduleTest, TestCountDiagnosticsRetainActualValues) {
    try {
        PrepareCompoundedOis(ScheduleSpec("Lookback", 0), CompoundedOisTest::Valuation());
        FAIL() << "Invalid zero lookback accepted";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.field_, "/observation_convention/lookback_business_days");
        ASSERT_EQ(error.Failure().diagnostic_.actual_, std::optional<String_>("0"));
    }
}
