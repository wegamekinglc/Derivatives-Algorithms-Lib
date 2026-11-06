#include <gtest/gtest.h>

#include <functional>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/compoundedois.hpp>

using namespace Dal;

namespace {
    CompoundedOisScriptSpec_ WeekendSpec() {
        CompoundedOisScriptSpec_ s;
        s.instrumentId_ = "weekend";
        s.notional_ = 100.;
        s.coupon_ = .03;
        s.direction_ = "PayFixed";
        s.currency_ = s.collateralCurrency_ = Ccy_("USD");
        s.collateralType_ = CollateralType_("OIS");
        s.discountRoute_ = "USD.OIS";
        s.projectionRoute_ = "USD.SOFR";
        s.fixingIndex_ = "USD-SOFR";
        s.observationBasis_ = s.fixedBasis_ = DayBasis_("ACT_360");
        s.calendar_ = HolidayCenterData_("explicit-weekends", {});
        const Date_ friday(2024, 3, 1), monday(2024, 3, 4);
        s.fixedLeg_ = {{friday, monday, monday, 3. / 360.}};
        s.coupons_ = {{"c1", friday, monday, monday}};
        s.observations_ = {{friday, monday, DateTime_(friday), 3. / 360., "c1", friday, monday}};
        return s;
    }
    Script::ScriptValuationSettings_ Valuation(const Date_& date = Date_(2024, 3, 1)) {
        Script::ScriptValuationSettings_ v;
        v.evaluationDate_ = date;
        v.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());
        return v;
    }
    CompoundedOisScriptSpec_ LeapSpec() {
        auto s = WeekendSpec();
        const Date_ start(2024, 2, 28), leap(2024, 2, 29), end(2024, 3, 1);
        s.fixedLeg_ = {{start, end, end, 2. / 360.}};
        s.coupons_ = {{"c1", start, end, end}};
        s.observations_ = {{start, leap, DateTime_(start), 1. / 360., "c1", start, leap}, {leap, end, DateTime_(leap), 1. / 360., "c1", leap, end}};
        return s;
    }
    void AssertRejected(const CompoundedOisScriptSpec_& s, const String_& path, const Script::ScriptValuationSettings_& v = Valuation()) {
        try {
            PrepareCompoundedOis(s, v);
            FAIL() << "Accepted " << path.c_str();
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.field_, path);
            ASSERT_EQ(error.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Prepare);
            ASSERT_FALSE(error.Failure().report_->Errors().empty());
        }
    }
} // namespace

TEST(CompoundedOisContractTest, TestFridayToMondayIsOneFixingWithThreeDayWeight) {
    const auto prepared = PrepareCompoundedOis(WeekendSpec(), Valuation());
    ASSERT_EQ(prepared->Spec().observations_.size(), 1);
    ASSERT_DOUBLE_EQ(prepared->Spec().observations_[0].accrual_, 3. / 360.);
    ASSERT_DOUBLE_EQ(prepared->Observations()[0].rateAccrual_, 3. / 360.);
    ASSERT_EQ(prepared->Observations()[0].sourceKey_, "observations/0");
    ASSERT_TRUE(prepared->CouponPayments()[0].included_);
    ASSERT_FALSE(prepared->CouponPayments()[0].settled_);
    ASSERT_FALSE(prepared->Observations()[0].historicalRequired_);
    ASSERT_EQ(prepared->RuleId(), "dal.compounded-ois-cashflows/1");
    ASSERT_FALSE(prepared->ContractIdentity().empty());
}

TEST(CompoundedOisContractTest, TestRejectsWrongAccrualWithOwningRowDiagnostic) {
    CalibrationFailure_ failure;
    {
        auto spec = WeekendSpec();
        spec.observations_[0].accrual_ = 1. / 360.;
        try {
            PrepareCompoundedOis(spec, Valuation());
            FAIL() << "Invalid observation accrual was accepted";
        } catch (const CalibrationError_& error) {
            failure = error.Failure();
        }
    }
    ASSERT_EQ(failure.diagnostic_.code_, "InvalidCompoundedOisTable");
    ASSERT_EQ(failure.diagnostic_.field_, "/observations/0/accrual");
    ASSERT_EQ(failure.diagnostic_.sourceRow_, 1);
    ASSERT_EQ(failure.diagnostic_.observationId_, "c1");
    ASSERT_EQ(failure.diagnostic_.instrumentId_, "weekend");
    ASSERT_EQ(failure.diagnostic_.stage_, CalibrationStage_::Value_::Prepare);
    ASSERT_EQ(failure.report_->Errors()[0].field_, failure.diagnostic_.field_);
}

TEST(CompoundedOisContractTest, TestSingleAndConsecutiveHolidaysKeepOneRateInterval) {
    for (int holidays : {1, 2}) {
        auto s = WeekendSpec();
        s.calendar_->holidays_ = {Date_(2024, 3, 4)};
        if (holidays == 2)
            s.calendar_->holidays_.push_back(Date_(2024, 3, 5));
        const Date_ end(2024, 3, 4 + holidays);
        s.fixedLeg_[0].end_ = s.fixedLeg_[0].payment_ = end;
        s.fixedLeg_[0].accrual_ = (3. + holidays) / 360.;
        s.coupons_[0].end_ = s.coupons_[0].payment_ = end;
        s.observations_[0].end_ = s.observations_[0].rateEnd_ = end;
        s.observations_[0].accrual_ = (3. + holidays) / 360.;
        const auto p = PrepareCompoundedOis(s, Valuation());
        ASSERT_EQ(p->Spec().observations_.size(), 1);
        ASSERT_DOUBLE_EQ(p->Observations()[0].rateAccrual_, (3. + holidays) / 360.);
    }
}

TEST(CompoundedOisContractTest, TestShortStubsShareFullRateAndFixedLegIsIndependent) {
    auto s = WeekendSpec();
    const Date_ friday(2024, 3, 1), saturday(2024, 3, 2), sunday(2024, 3, 3), monday(2024, 3, 4);
    s.fixedBasis_ = DayBasis_("ACT_365F");
    s.fixedLeg_ = {{Date_(2024, 2, 29), monday, monday, 4. / 365.}};
    s.coupons_ = {{"C", saturday, sunday, monday}, {"c", sunday, monday, monday}};
    s.observations_ = {{saturday, sunday, DateTime_(friday), 1. / 360., "C", friday, monday},
                       {sunday, monday, DateTime_(friday), 1. / 360., "c", friday, monday}};
    const auto p = PrepareCompoundedOis(s, Valuation());
    ASSERT_EQ(p->Observations()[0].couponRow_, 0);
    ASSERT_EQ(p->Observations()[1].couponRow_, 1);
    ASSERT_DOUBLE_EQ(p->Observations()[0].rateAccrual_, 3. / 360.);
    ASSERT_DOUBLE_EQ(p->Observations()[1].rateAccrual_, 3. / 360.);
    ASSERT_DOUBLE_EQ(p->Spec().observations_[1].accrual_, 1. / 360.);
    ASSERT_DOUBLE_EQ(p->Spec().fixedLeg_[0].accrual_, 4. / 365.);
}

TEST(CompoundedOisContractTest, TestLeapDayWithExplicit365FixedBasis) {
    auto s = LeapSpec();
    s.fixedBasis_ = s.observationBasis_ = DayBasis_("ACT_365F");
    s.fixedLeg_[0].accrual_ = 2. / 365.;
    for (auto& row : s.observations_)
        row.accrual_ = 1. / 365.;
    const auto p = PrepareCompoundedOis(s, Valuation());
    ASSERT_EQ(p->Spec().observations_[1].start_, Date_(2024, 2, 29));
    ASSERT_DOUBLE_EQ(p->Observations()[1].rateAccrual_, 1. / 365.);
}

TEST(CompoundedOisContractTest, TestPaymentYesterdayTodayTomorrowPreservesAllRows) {
    for (int day : {3, 4, 5}) {
        const auto p = PrepareCompoundedOis(WeekendSpec(), Valuation(Date_(2024, 3, day)));
        const bool included = day <= 4;
        ASSERT_EQ(p->CouponPayments()[0].included_, included);
        ASSERT_EQ(p->FixedPayments()[0].settled_, !included);
        ASSERT_EQ(p->Observations()[0].historicalRequired_, included);
        ASSERT_EQ(p->Spec().observations_.size(), 1);
        ASSERT_EQ(p->Spec().coupons_.size(), 1);
        ASSERT_EQ(p->Spec().fixedLeg_.size(), 1);
    }
}

TEST(CompoundedOisContractTest, TestSameDayPoliciesAndFutureSnapshotAreExplicitMetadata) {
    auto v = Valuation();
    const auto model = PrepareCompoundedOis(WeekendSpec(), v);
    v.todayFixingPolicy_ = TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
    v.fixings_ = Handle_<MarketFixingSnapshot_>(
        new MarketFixingSnapshot_({{"USD-SOFR", {{DateTime_(Date_(2024, 3, 1)), -.01}, {DateTime_(Date_(2024, 3, 8)), .99}}}}));
    const auto historical = PrepareCompoundedOis(WeekendSpec(), v);
    ASSERT_FALSE(model->Observations()[0].historicalRequired_);
    ASSERT_TRUE(historical->Observations()[0].historicalRequired_);
    ASSERT_EQ(model->ContractIdentity(), historical->ContractIdentity());
    ASSERT_NE(model->PreparationIdentity(), historical->PreparationIdentity());
    ASSERT_DOUBLE_EQ(*historical->Valuation().fixings_->Find("USD-SOFR", DateTime_(Date_(2024, 3, 1))), -.01);
    const auto future = PrepareCompoundedOis(WeekendSpec(), Valuation(Date_(2024, 2, 29)));
    ASSERT_FALSE(future->Observations()[0].historicalRequired_);
}

TEST(CompoundedOisContractTest, TestRejectsUnorderedMissingOverlappingAndDuplicateObservations) {
    {
        auto s = LeapSpec();
        std::swap(s.observations_[0], s.observations_[1]);
        AssertRejected(s, "/observations/0/start");
    }
    {
        auto s = LeapSpec();
        s.observations_.erase(s.observations_.begin());
        AssertRejected(s, "/observations/0/start");
    }
    {
        auto s = LeapSpec();
        s.observations_.pop_back();
        AssertRejected(s, "/coupons/0/end");
    }
    {
        auto s = LeapSpec();
        s.observations_[1] = s.observations_[0];
        AssertRejected(s, "/observations/1/start");
    }
    {
        auto s = LeapSpec();
        s.observations_[1].start_ = s.observations_[0].start_;
        AssertRejected(s, "/observations/1/start");
    }
    {
        auto s = LeapSpec();
        s.observations_[0].couponId_ = "unknown";
        AssertRejected(s, "/observations/0/coupon_id");
    }
}

TEST(CompoundedOisContractTest, TestRejectsNonfiniteNonpositiveAndWrongAccrualsOnBothLegs) {
    for (double value : {0., -.01, .1, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        auto s = WeekendSpec();
        s.observations_[0].accrual_ = value;
        AssertRejected(s, "/observations/0/accrual");
        s = WeekendSpec();
        s.fixedLeg_[0].accrual_ = value;
        AssertRejected(s, "/fixed_leg/0/accrual");
    }
}

TEST(CompoundedOisContractTest, TestRejectsUnsupportedTimeAndRateIntervalRules) {
    using Edit = std::function<void(CompoundedOisScriptSpec_&)>;
    const std::vector<std::pair<Edit, String_>> cases = {
        {[](auto& s) { s.observations_[0].fixingTime_ = DateTime_(Date_(2024, 3, 1), 12); }, "/observations/0/fixing_time"},
        {[](auto& s) { s.observations_[0].fixingTime_ = DateTime_(Date_(2024, 2, 29)); }, "/observations/0/fixing_time"},
        {[](auto& s) { s.observations_[0].rateStart_ = Date_(2024, 3, 2); }, "/observations/0/rate_start"},
        {[](auto& s) { s.observations_[0].rateEnd_ = Date_(2024, 3, 3); }, "/observations/0/rate_end"},
        {[](auto& s) { s.ruleId_ = "lookback/1"; }, "/rule"},
        {[](auto& s) { s.observationBasis_ = DayBasis_("BOND"); }, "/observation_basis"},
        {[](auto& s) { s.fixedBasis_ = DayBasis_("ACT_ACT"); }, "/fixed_basis"},
        {[](auto& s) { s.direction_ = "payfixed"; }, "/direction"},
        {[](auto& s) { s.collateralCurrency_ = Ccy_("EUR"); }, "/collateral_currency"},
        {[](auto& s) { s.collateralType_ = CollateralType_("GC"); }, "/collateral_type"}};
    for (const auto& [edit, path] : cases) {
        auto s = WeekendSpec();
        edit(s);
        AssertRejected(s, path);
    }
}

TEST(CompoundedOisContractTest, TestRejectsMissingRequiredSpecAndValuationFields) {
    using Edit = std::function<void(CompoundedOisScriptSpec_&)>;
    const std::vector<std::pair<Edit, String_>> cases = {{[](auto& s) { s.notional_ = 0.; }, "/notional"},
                                                         {[](auto& s) { s.coupon_ = std::numeric_limits<double>::quiet_NaN(); }, "/coupon"},
                                                         {[](auto& s) { s.minAbsAnnuity_ = 0.; }, "/min_abs_annuity"},
                                                         {[](auto& s) { s.currency_ = Ccy_(); }, "/currency"},
                                                         {[](auto& s) { s.calendar_.reset(); }, "/calendar"},
                                                         {[](auto& s) { s.discountRoute_.clear(); }, "/discount_route"},
                                                         {[](auto& s) { s.projectionRoute_.clear(); }, "/projection_route"},
                                                         {[](auto& s) { s.fixingIndex_.clear(); }, "/fixing_index"},
                                                         {[](auto& s) { s.fixedLeg_.clear(); }, "/fixed_leg"},
                                                         {[](auto& s) { s.coupons_.clear(); }, "/coupons"},
                                                         {[](auto& s) { s.observations_.clear(); }, "/observations"}};
    for (const auto& [edit, path] : cases) {
        auto s = WeekendSpec();
        edit(s);
        AssertRejected(s, path);
    }
    auto v = Valuation();
    v.evaluationDate_.reset();
    AssertRejected(WeekendSpec(), "/valuation/evaluation_date", v);
    v = Valuation();
    v.fixings_.reset();
    AssertRejected(WeekendSpec(), "/valuation/fixings", v);
}

TEST(CompoundedOisContractTest, TestFixedAndCouponDatesAreValidatedBeforeSettlementFiltering) {
    {
        auto s = WeekendSpec();
        s.fixedLeg_[0].payment_ = Date_(2024, 3, 3);
        AssertRejected(s, "/fixed_leg/0/payment", Valuation(Date_(2024, 3, 8)));
    }
    {
        auto s = WeekendSpec();
        s.coupons_[0].end_ = Date_();
        AssertRejected(s, "/coupons/0/end");
    }
    {
        auto s = WeekendSpec();
        s.fixedLeg_.push_back(s.fixedLeg_[0]);
        AssertRejected(s, "/fixed_leg/1/start");
    }
    {
        auto s = WeekendSpec();
        const Date_ end(2024, 3, 5);
        s.coupons_.push_back({"c1", Date_(2024, 3, 4), end, end});
        AssertRejected(s, "/coupons/1/coupon_id");
    }
}

TEST(CompoundedOisContractTest, TestCalendarExceptionsAreExplicitAndRejectConflicts) {
    {
        auto s = WeekendSpec();
        s.calendar_->workWeekends_ = {Date_(2024, 3, 2)};
        s.observations_[0].rateEnd_ = s.observations_[0].end_ = Date_(2024, 3, 2);
        s.observations_[0].accrual_ = 1. / 360.;
        s.observations_.push_back(
            {Date_(2024, 3, 2), Date_(2024, 3, 4), DateTime_(Date_(2024, 3, 2)), 2. / 360., "c1", Date_(2024, 3, 2), Date_(2024, 3, 4)});
        ASSERT_EQ(PrepareCompoundedOis(s, Valuation())->Observations().size(), 2);
    }
    {
        auto s = WeekendSpec();
        s.calendar_->holidays_ = {Date_(2024, 3, 5), Date_(2024, 3, 4)};
        AssertRejected(s, "/calendar/holidays/1");
        s.calendar_->holidays_ = {Date_(2024, 3, 5), Date_(2024, 3, 5)};
        AssertRejected(s, "/calendar/holidays/1");
    }
    {
        auto s = WeekendSpec();
        s.calendar_->holidays_ = s.calendar_->workWeekends_ = {Date_(2024, 3, 2)};
        AssertRejected(s, "/calendar/work_weekends/0");
    }
}

TEST(CompoundedOisContractTest, TestCallerAssignmentCannotChangeFrozenTablesCalendarOrValuation) {
    auto s = WeekendSpec();
    auto v = Valuation();
    const auto p = PrepareCompoundedOis(s, v);
    const auto identity = p->PreparationIdentity();
    auto tableCopy = p->Spec().observations_;
    tableCopy[0].accrual_ = 99.;
    s.notional_ = 2.;
    s.observations_.clear();
    *s.calendar_ = HolidayCenterData_("changed", {Date_(2024, 3, 4)});
    v.evaluationDate_ = Date_(2024, 3, 8);
    v.fixings_.reset();
    ASSERT_EQ(p->PreparationIdentity(), identity);
    ASSERT_EQ(p->Spec().observations_.size(), 1);
    ASSERT_DOUBLE_EQ(p->Spec().notional_, 100.);
    ASSERT_DOUBLE_EQ(p->Spec().observations_[0].accrual_, 3. / 360.);
    ASSERT_TRUE(p->Spec().calendar_->holidays_.empty());
    ASSERT_EQ(*p->Valuation().evaluationDate_, Date_(2024, 3, 1));
    ASSERT_TRUE(p->Valuation().fixings_);
}

TEST(CompoundedOisContractTest, TestJsonRoundTripRebuildsAllIdentitiesAndPaymentMetadata) {
    auto v = Valuation(Date_(2024, 3, 4));
    v.todayFixingPolicy_ = TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
    v.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"USD-SOFR", {{DateTime_(Date_(2024, 3, 1)), -.01}}}}));
    const auto p = PrepareCompoundedOis(WeekendSpec(), v);
    const auto json = DumpPreparedCompoundedOisJson(p);
    const auto loaded = LoadPreparedCompoundedOisJson(json);
    ASSERT_EQ(DumpPreparedCompoundedOisJson(loaded), json);
    ASSERT_EQ(loaded->ContractIdentity(), p->ContractIdentity());
    ASSERT_EQ(loaded->PreparationIdentity(), p->PreparationIdentity());
    ASSERT_EQ(loaded->CalendarIdentity(), p->CalendarIdentity());
    ASSERT_EQ(loaded->Observations()[0].sourceKey_, "observations/0");
    ASSERT_TRUE(loaded->Observations()[0].historicalRequired_);
    ASSERT_TRUE(loaded->CouponPayments()[0].included_);
}

TEST(CompoundedOisContractTest, TestJsonRejectsMalformedMissingUnknownAndDuplicateFields) {
    using namespace CalibrationInternal;
    const auto json = DumpPreparedCompoundedOisJson(PrepareCompoundedOis(WeekendSpec(), Valuation()));
    auto invalid = Parse(json);
    invalid.object_.at("spec").object_.erase("notional");
    try {
        LoadPreparedCompoundedOisJson(String_(Jcs(invalid)));
        FAIL() << "Missing required field accepted";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.field_, "/spec/notional");
        ASSERT_EQ(error.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Load);
    }
    invalid = Parse(json);
    invalid.object_.at("spec").object_["lookback"] = Num(2.);
    ASSERT_THROW(LoadPreparedCompoundedOisJson(String_(Jcs(invalid))), CalibrationError_);
    invalid = Parse(json);
    invalid.object_.at("spec").object_.at("observations").array_[0].object_.at("accrual") = Bool(true);
    CalibrationFailure_ failure;
    try {
        LoadPreparedCompoundedOisJson(String_(Jcs(invalid)));
        FAIL() << "Boolean accrual accepted";
    } catch (const CalibrationError_& error) {
        failure = error.Failure();
    }
    ASSERT_EQ(failure.diagnostic_.field_, "/spec/observations/0/accrual");
    ASSERT_EQ(failure.diagnostic_.sourceRow_, 1);
    ASSERT_EQ(failure.diagnostic_.observationId_, "c1");
    ASSERT_EQ(failure.diagnostic_.instrumentId_, "weekend");
    ASSERT_EQ(failure.report_->Errors()[0].stage_, CalibrationStage_::Value_::Load);
    ASSERT_THROW(LoadPreparedCompoundedOisJson("{"), CalibrationError_);
    ASSERT_THROW(LoadPreparedCompoundedOisJson("{\"schema\":1,\"schema\":2}"), CalibrationError_);
    invalid = Parse(json);
    invalid.object_.at("schema") = Str("dal.compounded-ois-preparation/2");
    ASSERT_THROW(LoadPreparedCompoundedOisJson(String_(Jcs(invalid))), CalibrationError_);
}

TEST(CompoundedOisContractTest, TestJsonRejectsDuplicateFixingRowsBeforeMapInsertion) {
    using namespace CalibrationInternal;
    auto v = Valuation();
    v.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"USD-SOFR", {{DateTime_(Date_(2024, 3, 1)), -.01}}}}));
    const auto json = DumpPreparedCompoundedOisJson(PrepareCompoundedOis(WeekendSpec(), v));
    for (double value : {-.01, .02}) {
        auto invalid = Parse(json);
        auto& fixings = invalid.object_.at("valuation").object_.at("fixings").array_[0].object_.at("history").array_;
        auto duplicate = fixings[0];
        duplicate.object_.at("value") = Num(value);
        fixings.push_back(duplicate);
        try {
            LoadPreparedCompoundedOisJson(String_(Jcs(invalid)));
            FAIL() << "Duplicate fixing accepted";
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.field_, "/valuation/fixings/0/history/1");
            ASSERT_EQ(error.Failure().diagnostic_.sourceRow_, 2);
        }
    }
    auto invalid = Parse(json);
    auto& series = invalid.object_.at("valuation").object_.at("fixings").array_;
    series.push_back(series[0]);
    ASSERT_THROW(LoadPreparedCompoundedOisJson(String_(Jcs(invalid))), CalibrationError_);
}

TEST(CompoundedOisContractTest, TestJsonBoundaryRowErrorsKeepOwnedLocations) {
    using namespace CalibrationInternal;
    const auto json = DumpPreparedCompoundedOisJson(PrepareCompoundedOis(LeapSpec(), Valuation()));
    for (const auto index : {0u, 1u}) {
        for (const auto* field : {"accrual", "coupon_id"}) {
            CalibrationFailure_ failure;
            {
                auto invalid = Parse(json);
                invalid.object_.at("spec").object_.at("observations").array_.at(index).object_.at(field) = Bool(true);
                try {
                    LoadPreparedCompoundedOisJson(String_(Jcs(invalid)));
                    FAIL() << "Invalid boundary row accepted";
                } catch (const CalibrationError_& error) {
                    failure = error.Failure();
                }
            }
            ASSERT_EQ(failure.diagnostic_.code_, "InvalidCalibrationArchive");
            ASSERT_EQ(failure.diagnostic_.stage_, CalibrationStage_::Value_::Load);
            ASSERT_EQ(failure.diagnostic_.field_, "/spec/observations/" + OrdinalText(index) + "/" + field);
            ASSERT_EQ(failure.diagnostic_.sourceRow_, static_cast<int>(index + 1));
            ASSERT_EQ(failure.diagnostic_.instrumentId_, "weekend");
            if (std::string(field) == "coupon_id")
                ASSERT_FALSE(failure.diagnostic_.observationId_);
            else
                ASSERT_EQ(failure.diagnostic_.observationId_, "c1");
            ASSERT_EQ(failure.report_->Errors()[0].field_, failure.diagnostic_.field_);
            ASSERT_EQ(failure.report_->Errors()[0].sourceRow_, failure.diagnostic_.sourceRow_);
        }
    }
}

TEST(CompoundedOisContractTest, TestIdentityIncludesEveryEffectiveContractField) {
    using Edit = std::function<void(CompoundedOisScriptSpec_&)>;
    const std::vector<Edit> edits = {[](auto& s) { s.instrumentId_ = "other"; },
                                     [](auto& s) { s.notional_ = 101.; },
                                     [](auto& s) { s.coupon_ = -.01; },
                                     [](auto& s) { s.direction_ = "ReceiveFixed"; },
                                     [](auto& s) { s.currency_ = s.collateralCurrency_ = Ccy_("EUR"); },
                                     [](auto& s) { s.discountRoute_ = "other.discount"; },
                                     [](auto& s) { s.projectionRoute_ = "other.projection"; },
                                     [](auto& s) { s.fixingIndex_ = "other.fixing"; },
                                     [](auto& s) { s.calendar_->center_ = "other.calendar"; },
                                     [](auto& s) { s.calendar_->holidays_ = {Date_(2024, 12, 25)}; },
                                     [](auto& s) { s.calendar_->workWeekends_ = {Date_(2024, 12, 28)}; },
                                     [](auto& s) { s.minAbsAnnuity_ = 1e-10; },
                                     [](auto& s) {
                                         s.fixedBasis_ = DayBasis_("ACT_365F");
                                         s.fixedLeg_[0].accrual_ = 3. / 365.;
                                     },
                                     [](auto& s) {
                                         s.observationBasis_ = DayBasis_("ACT_365F");
                                         s.observations_[0].accrual_ = 3. / 365.;
                                     },
                                     [](auto& s) {
                                         s.fixedLeg_[0].start_ = Date_(2024, 2, 29);
                                         s.fixedLeg_[0].accrual_ = 4. / 360.;
                                     },
                                     [](auto& s) { s.fixedLeg_[0].payment_ = Date_(2024, 3, 5); },
                                     [](auto& s) { s.coupons_[0].payment_ = Date_(2024, 3, 5); },
                                     [](auto& s) { s.coupons_[0].couponId_ = s.observations_[0].couponId_ = "other.coupon"; },
                                     [](auto& s) {
                                         s.coupons_[0].start_ = s.observations_[0].start_ = Date_(2024, 3, 2);
                                         s.observations_[0].accrual_ = 2. / 360.;
                                     },
                                     [](auto& s) {
                                         s.coupons_[0].end_ = s.observations_[0].end_ = Date_(2024, 3, 3);
                                         s.observations_[0].accrual_ = 2. / 360.;
                                     },
                                     [](auto& s) {
                                         s.fixedLeg_[0] = {Date_(2024, 3, 8), Date_(2024, 3, 11), Date_(2024, 3, 11), 3. / 360.};
                                         s.coupons_[0] = {"c1", Date_(2024, 3, 8), Date_(2024, 3, 11), Date_(2024, 3, 11)};
                                         s.observations_[0] = {Date_(2024, 3, 8), Date_(2024, 3, 11), DateTime_(Date_(2024, 3, 8)), 3. / 360., "c1",
                                                               Date_(2024, 3, 8), Date_(2024, 3, 11)};
                                     }};
    const auto original = PrepareCompoundedOis(WeekendSpec(), Valuation());
    const auto json = DumpPreparedCompoundedOisJson(original);
    for (const auto& edit : edits) {
        auto s = WeekendSpec();
        edit(s);
        const auto changed = PrepareCompoundedOis(s, Valuation());
        ASSERT_NE(changed->ContractIdentity(), original->ContractIdentity());
        ASSERT_NE(changed->PreparationIdentity(), original->PreparationIdentity());
    }
    ASSERT_EQ(DumpPreparedCompoundedOisJson(original), json);
    auto v = Valuation(Date_(2024, 3, 2));
    const auto changedDate = PrepareCompoundedOis(WeekendSpec(), v);
    ASSERT_EQ(changedDate->ContractIdentity(), original->ContractIdentity());
    ASSERT_NE(changedDate->PreparationIdentity(), original->PreparationIdentity());
}

TEST(CompoundedOisContractTest, TestEmptyNamedFixingHistorySurvivesJsonAndHasDistinctIdentity) {
    auto v = Valuation();
    const auto empty = PrepareCompoundedOis(WeekendSpec(), v);
    v.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"USD-SOFR", {}}}));
    const auto named = PrepareCompoundedOis(WeekendSpec(), v);
    const auto loaded = LoadPreparedCompoundedOisJson(DumpPreparedCompoundedOisJson(named));
    ASSERT_EQ(loaded->Valuation().fixings_->Values().size(), 1);
    ASSERT_NE(named->PreparationIdentity(), empty->PreparationIdentity());
    ASSERT_EQ(loaded->PreparationIdentity(), named->PreparationIdentity());
}

TEST(CompoundedOisContractTest, TestMalformedSnapshotIndexThrowsOwningPreparationError) {
    const String_ badUtf8(1, static_cast<char>(0xff));
    const String_ embeddedNul(std::string("SOFR\0suffix", 11));
    for (const auto& index : {badUtf8, embeddedNul}) {
        auto v = Valuation();
        v.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{index, {}}}));
        AssertRejected(WeekendSpec(), "/valuation/fixings/0/index", v);
    }
}
