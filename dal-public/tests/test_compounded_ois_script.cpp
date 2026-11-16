#include <gtest/gtest.h>

#include "scriptquoteriskfixture.hpp"
#include <dal-public/src/calibrationinstrument.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/scriptcurvequoterisk.hpp>

using namespace Dal;

namespace {
    CurveScriptEvaluationResult_ EvaluateOis(CalibrationDerivative_ method = CalibrationDerivative_::Value_::None) {
        InitGlobalData(1);
        const Date_ today(2024, 3, 1), a(2024, 2, 29), b(2024, 3, 5), c(2024, 3, 6), pay(2024, 3, 8);
        CompoundedOisScriptSpec_ s;
        s.instrumentId_ = "facade-ois";
        s.notional_ = 100.;
        s.coupon_ = .02;
        s.direction_ = "PayFixed";
        s.currency_ = s.collateralCurrency_ = Ccy_("USD");
        s.collateralType_ = CollateralType_("OIS");
        s.discountRoute_ = "USD.OIS";
        s.projectionRoute_ = "USD.SOFR";
        s.fixingIndex_ = "USD-SOFR";
        s.fixedBasis_ = DayBasis_("ACT_365F");
        s.observationBasis_ = DayBasis_("ACT_360");
        s.calendar_ = HolidayCenterData_("explicit", {Date_(2024, 3, 4)});
        s.fixedLeg_ = {{a, c, pay, 6. / 365.}};
        s.coupons_ = {{"coupon", a, c, pay}};
        s.observations_ = {{a, today, DateTime_(a), 1. / 360., "coupon", a, today},
                           {today, b, DateTime_(today), 4. / 360., "coupon", today, b},
                           {b, c, DateTime_(b), 1. / 360., "coupon", b, c}};
        CurveScriptEvaluationRequest_ request;
        request.valuation_.evaluationDate_ = today;
        request.valuation_.fixings_.reset(new MarketFixingSnapshot_({{"USD-SOFR", {{DateTime_(a), -.012}}}}));
        request.compoundedOis_ = PrepareCompoundedOis(s, request.valuation_);
        request.geometry_.today_ = today;
        request.geometry_.ccy_ = "USD";
        request.geometry_.parameterization_ = CurveParameterization_("LOG_DISCOUNT");
        request.geometry_.knotDates_ = {today, today.AddDays(20)};
        request.geometry_.calibrateDiscountCurve_ = false;
        request.geometry_.targetOvernightRoute_ = "USD.SOFR";
        request.market_.routes_ = {{"USD.OIS", false, CollateralType_("OIS"), {}, false, true}, {"USD.SOFR", true, CollateralType_("OIS"), {}, true}};
        request.parameters_ = {-.002};
        request.derivativeMethod_ = method;
        return EvaluateCurveScriptInstrument(NewCompoundedOisScriptInstrument(request.compoundedOis_), request);
    }
} // namespace

TEST(CompoundedOisScriptPublicTest, TestMixedHolidayOracleAndUnavailablePartials) {
    const auto result = EvaluateOis();
    const double floating = (1. - .012 / 360.) * std::exp(.0005) - 1., df = std::exp(-.0007);
    ASSERT_NEAR(CurveScriptEvaluationValue(result, "Price"), 100. * (floating - .02 * 6. / 365.) * df, 1e-10);
    ASSERT_NEAR(CurveScriptEvaluationValue(result, "ParRate"), floating / (6. / 365.), 1e-10);
    ASSERT_THROW(CurveScriptEvaluationPartials(result), Exception_);
    ASSERT_EQ(result.measureUnits_, (Vector_<String_>{"Price:USD", "Rate"}));
    ASSERT_EQ(result.observations_[0].source_, "Historical");
    ASSERT_EQ(result.observations_[1].source_, "Projection");
    ASSERT_EQ(CurveScriptEvaluationTable(result, "cashflows").Rows(), 3);
    ASSERT_EQ(CurveScriptEvaluationTable(result, "observations").Rows(), 4);
    ASSERT_FALSE(result.contextIdentity_.empty());
    ASSERT_FALSE(result.report_->QuoteRiskAvailable());
}

TEST(CompoundedOisScriptPublicTest, TestNativeAxesAndRawPartials) {
    for (const auto method : {CalibrationDerivative_("AAD"), CalibrationDerivative_("FiniteDifference")}) {
        const auto result = EvaluateOis(method);
        const auto partials = CurveScriptEvaluationPartials(result);
        ASSERT_EQ(result.freeParameters_.size(), 1u);
        ASSERT_EQ(partials.Rows(), 2);
        ASSERT_EQ(partials.Cols(), 1);
        const auto oracle = [](double theta) {
            const double floating = (1. - .012 / 360.) * std::exp(-theta * 5. / 20.) - 1.;
            return Vector_<>{100. * (floating - .02 * 6. / 365.) * std::exp(theta * 7. / 20.), floating / (6. / 365.)};
        };
        for (const double h : {1e-4, 5e-5}) {
            const auto up = oracle(-.002 + h), down = oracle(-.002 - h);
            for (int i = 0; i < 2; ++i) {
                const double fd = (up[i] - down[i]) / (2. * h);
                ASSERT_NEAR(partials(i, 0), fd, 1e-7 + 1e-5 * std::abs(fd));
            }
        }
        ASSERT_THROW(CurveScriptEvaluationValue(result, "price"), Exception_);
    }
}

TEST(CompoundedOisScriptPublicTest, TestPerPvAdmissionRequiresRegisteredTables) {
    const auto result = EvaluateOis();
    const auto spec = Dal::Test::ScriptQuoteRiskSpec();
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), {{1e-12, 1e-12}, 1e-10, 1e-12});
    ASSERT_TRUE(snapshot.RiskAvailable());
    const auto risk = ValueCalibrationInstrumentWithQuoteRisk(result.instrument_, snapshot, spec);
    ASSERT_FALSE(risk.Available());
    ASSERT_FALSE(risk.Diagnostics().empty());
    ASSERT_EQ(risk.Diagnostics()[0].code_, "UnsupportedCompoundedOisContext");
    ASSERT_THROW(static_cast<void>(risk.PV()), Exception_);
}

TEST(CompoundedOisScriptPublicTest, TestOwningObservationScheduleKeepsLegacyTable) {
    const auto result = EvaluateOis();
    const auto legacy = CurveScriptEvaluationTable(result, "observations");
    const auto audit = CurveScriptEvaluationTable(result, "observation_schedule");
    ASSERT_EQ(legacy.Cols(), 15);
    ASSERT_EQ(audit.Rows(), 4);
    ASSERT_EQ(audit.Cols(), 26);
    ASSERT_EQ(audit(0, 0), Cell_("rule"));
    ASSERT_EQ(audit(1, 0), Cell_("dal.compounded-ois-cashflows/1"));
    ASSERT_EQ(audit(1, 1), Cell_("Legacy"));
    ASSERT_EQ(audit(1, 5), Cell_("observations/0"));
    ASSERT_EQ(audit(1, 22), Cell_("Historical"));
    ASSERT_EQ(audit(2, 22), Cell_("Projection"));
}

namespace {
    CompoundedOisScriptSpec_ FutureAuditSpec(const String_& mode) {
        CompoundedOisScriptSpec_ s;
        s.instrumentId_ = "audit-v2";
        s.direction_ = "PayFixed";
        s.discountRoute_ = "USD.OIS";
        s.projectionRoute_ = "USD.SOFR";
        s.fixingIndex_ = "USD-SOFR";
        s.notional_ = 100.;
        s.coupon_ = .02;
        s.currency_ = s.collateralCurrency_ = Ccy_("USD");
        s.collateralType_ = CollateralType_("OIS");
        s.observationBasis_ = DayBasis_("ACT_360");
        s.fixedBasis_ = DayBasis_("ACT_365F");
        s.calendar_ = HolidayCenterData_("explicit", {Date_(2024, 3, 4)}, {Date_(2024, 3, 9)});
        s.fixedLeg_ = {{Date_(2024, 3, 5), Date_(2024, 3, 8), Date_(2024, 3, 12), 3. / 365.}};
        s.coupons_ = {{"coupon", Date_(2024, 3, 5), Date_(2024, 3, 8), Date_(2024, 3, 12)}};
        s.ruleId_ = "dal.compounded-ois-cashflows/2";
        s.observationConvention_ = CompoundedOisObservationConvention_{
            CompoundedOisObservationMode_(mode), mode == "Lookback" || mode == "ObservationShift" ? 1 : 0, mode == "Lockout" ? 1 : 0};
        return s;
    }
} // namespace

TEST(CompoundedOisScriptPublicTest, TestPreparedAuditMatchesArchiveAndOutlivesTablesFourModes) {
    for (const String_ mode : {"None", "Lookback", "ObservationShift", "Lockout"}) {
        SCOPED_TRACE(mode);
        Matrix_<Cell_> before, after;
        {
            Script::ScriptValuationSettings_ valuation;
            valuation.evaluationDate_ = Date_(2024, 3, 1);
            valuation.fixings_.reset(new MarketFixingSnapshot_());
            const auto prepared = PrepareCompoundedOis(FutureAuditSpec(mode), valuation);
            const auto loaded = LoadPreparedCompoundedOisJson(DumpPreparedCompoundedOisJson(prepared));
            before = CompoundedOisObservationScheduleTable(prepared);
            after = CompoundedOisObservationScheduleTable(loaded);
            ASSERT_EQ(loaded->ContractIdentity(), prepared->ContractIdentity());
            ASSERT_EQ(loaded->ObservationScheduleIdentity(), prepared->ObservationScheduleIdentity());
        }
        ASSERT_EQ(before.Rows(), 4);
        ASSERT_EQ(before.Cols(), 22);
        ASSERT_EQ(before(1, 0), Cell_("dal.compounded-ois-cashflows/2"));
        ASSERT_EQ(before(1, 1), Cell_(mode));
        ASSERT_EQ(before(1, 5), Cell_("observation_schedule/0"));
        for (int row = 0; row < before.Rows(); ++row)
            for (int column = 0; column < before.Cols(); ++column)
                ASSERT_EQ(before(row, column), after(row, column));
    }
    ASSERT_THROW(CompoundedOisObservationScheduleTable({}), Exception_);
}
