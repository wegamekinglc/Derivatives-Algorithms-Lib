//
// Created by Codex on 2026/7/14.
//

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <dal/platform/platform.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/xccyjointcalibration.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/storage/_repository.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    struct CurrencyFixture_ {
        JointCurrencyCurveSpec_ spec_;
        Handle_<CurveBlock_> market_;
        Vector_<> discountParameters_;
        Vector_<> forwardParameters_;
        RateIndexConvention_ discountIndex_;
        RateIndexConvention_ forwardIndex_;
    };

    Vector_<Date_> JointKnots(const Date_& today) {
        return {
            Date::AddMonths(today, 6), Date::AddMonths(today, 18), Date::AddMonths(today, 30), Date::AddMonths(today, 42), Date::AddMonths(today, 54),
        };
    }

    Vector_<Date_> InstrumentMaturities(const Date_& today) {
        return {
            Date::AddMonths(today, 12), Date::AddMonths(today, 24), Date::AddMonths(today, 36),
            Date::AddMonths(today, 48), Date::AddMonths(today, 60),
        };
    }

    RateIndexConvention_ MakeIndex(bool projection, const PeriodLength_& tenor) {
        RateIndexConvention_ result;
        result.useProjectionCurve_ = projection;
        result.forecastTenor_ = tenor;
        result.dayBasis_ = DayBasis_("ACT_365F");
        result.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
        result.fixingLag_ = 0;
        result.fixingHolidays_ = Holidays::None();
        result.accrualHolidays_ = Holidays::None();
        return result;
    }

    Handle_<YCInstrument_> QuotedDeposit(const Date_& today, const Date_& maturity, const RateIndexConvention_& index, const CurveBlock_& market) {
        const Handle_<YCInstrument_> prototype(new Deposit_(today, today, maturity, 0.0, index));
        const double quote = (*prototype->Precompute(Handle_<YieldCurve_>()))(market);
        return Handle_<YCInstrument_>(new Deposit_(today, today, maturity, quote, index));
    }

    CurrencyFixture_ MakeCurrencyFixture(const Date_& today,
                                         const Ccy_& ccy,
                                         const Vector_<Date_>& knots,
                                         const Vector_<Date_>& maturities,
                                         const Vector_<>& discountParameters,
                                         const Vector_<>& forwardParameters) {
        CurrencyFixture_ result;
        result.discountParameters_ = discountParameters;
        result.forwardParameters_ = forwardParameters;
        result.discountIndex_ = MakeIndex(false, PeriodLength_("3M"));
        result.forwardIndex_ = MakeIndex(true, PeriodLength_("3M"));

        const Handle_<DiscountCurve_> discount(
            NewDiscountPWC(String_(ccy.String()) + "_true_ois", ccy.String(), PiecewiseConstant_(knots, discountParameters)));
        const Handle_<DiscountCurve_> forward(
            NewDiscountPWC(String_(ccy.String()) + "_true_3m", ccy.String(), PiecewiseConstant_(knots, forwardParameters)));
        result.market_ = Handle_<CurveBlock_>(new CurveBlock_(String_(ccy.String()) + "_true", ccy.String(),
                                                              {{CollateralType_(CollateralType_::Value_::OIS), discount}},
                                                              {{PeriodLength_("3M"), forward}}, DayBasis_("ACT_365F")));

        JointCurveDeclaration_ discountDeclaration;
        discountDeclaration.curveName_ = String_(ccy.String()) + "_ois";
        discountDeclaration.knotDates_ = knots;
        discountDeclaration.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        discountDeclaration.calibrateDiscountCurve_ = true;
        discountDeclaration.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        discountDeclaration.smoothingWeight_ = 1.0;

        JointCurveDeclaration_ forwardDeclaration;
        forwardDeclaration.curveName_ = String_(ccy.String()) + "_3m";
        forwardDeclaration.knotDates_ = knots;
        forwardDeclaration.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        forwardDeclaration.targetTenor_ = PeriodLength_("3M");
        forwardDeclaration.calibrateDiscountCurve_ = false;
        forwardDeclaration.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        forwardDeclaration.smoothingWeight_ = 1.0;

        for (const auto& maturity : maturities) {
            discountDeclaration.instruments_.push_back(QuotedDeposit(today, maturity, result.discountIndex_, *result.market_));
            forwardDeclaration.instruments_.push_back(QuotedDeposit(today, maturity, result.forwardIndex_, *result.market_));
        }

        result.spec_.ccy_ = ccy;
        result.spec_.liborBasis_ = DayBasis_("ACT_365F");
        result.spec_.curves_ = {discountDeclaration, forwardDeclaration};
        return result;
    }

    CrossCurrencySwapConfig_ MakeXccyConfig(const CurrencyPair_& pair,
                                            const RateIndexConvention_& domesticIndex,
                                            const RateIndexConvention_& foreignIndex,
                                            XccyNotionalMode_ notionalMode) {
        RateLegConvention_ leg;
        leg.paymentFrequency_ = PeriodLength_("3M");
        leg.dayBasis_ = DayBasis_("ACT_365F");
        leg.accrualHolidays_ = Holidays::None();
        leg.paymentHolidays_ = Holidays::None();

        CrossCurrencySwapConfig_ result;
        result.pair_ = pair;
        result.domesticNotional_ = 110.0;
        result.foreignNotional_ = 100.0;
        result.convention_.initialNotionalExchange_ = true;
        result.convention_.finalNotionalExchange_ = true;
        result.convention_.spreadOnForeignLeg_ = true;
        result.convention_.domesticIndex_ = domesticIndex;
        result.convention_.foreignIndex_ = foreignIndex;
        result.convention_.domesticLeg_ = leg;
        result.convention_.foreignLeg_ = leg;
        result.notionalMode_ = notionalMode;
        result.fxReset_.fixingLag_ = 0;
        result.fxReset_.fixingHolidays_ = Holidays::None();
        result.fxReset_.fixingHour_ = 11;
        result.fxReset_.fixingMinute_ = 0;
        result.domesticRateFixing_ = {String_(pair.domestic_.String()) + "-JOINT-3M", 11, 0};
        result.foreignRateFixing_ = {String_(pair.foreign_.String()) + "-JOINT-3M", 11, 0};
        return result;
    }

    struct JointFixture_ {
        JointXccyCalibrationSpec_ spec_;
        Vector_<> domesticDiscountParameters_;
        Vector_<> domesticForwardParameters_;
        Vector_<> foreignDiscountParameters_;
        Vector_<> foreignForwardParameters_;
        Vector_<> basisParameters_;
    };

    JointFixture_ MakeJointFixture() {
        JointFixture_ result;
        const Date_ today(2025, 1, 16);
        const Vector_<Date_> knots = JointKnots(today);
        const Vector_<Date_> maturities = InstrumentMaturities(today);
        result.domesticDiscountParameters_ = {0.0150, 0.0160, 0.0170, 0.0180, 0.0190};
        result.domesticForwardParameters_ = {0.0240, 0.0250, 0.0260, 0.0270, 0.0280};
        result.foreignDiscountParameters_ = {0.0100, 0.0110, 0.0120, 0.0130, 0.0140};
        result.foreignForwardParameters_ = {0.0190, 0.0200, 0.0210, 0.0220, 0.0230};
        result.basisParameters_ = {0.0010, 0.0014, 0.0018, 0.0022, 0.0026};

        const CurrencyFixture_ domestic =
            MakeCurrencyFixture(today, Ccy_("USD"), knots, maturities, result.domesticDiscountParameters_, result.domesticForwardParameters_);
        const CurrencyFixture_ foreign =
            MakeCurrencyFixture(today, Ccy_("EUR"), knots, maturities, result.foreignDiscountParameters_, result.foreignForwardParameters_);
        const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));
        const DateTime_ valuationTime(today, 9, 0);
        const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_());
        CrossCurrencyMarket_ quoteMarket(domestic.market_, foreign.market_, 1.10, valuationTime, pair.domestic_, fixings);
        quoteMarket.SetBasisCurve(
            Handle_<DiscountCurve_>(NewDiscountPWC("true_xccy_basis", "USD", PiecewiseConstant_(knots, result.basisParameters_))));

        result.spec_.valuationTime_ = valuationTime;
        result.spec_.pair_ = pair;
        result.spec_.collateralCurrency_ = pair.domestic_;
        result.spec_.fxSpot_ = 1.10;
        result.spec_.domestic_ = domestic.spec_;
        result.spec_.foreign_ = foreign.spec_;
        result.spec_.basis_.curveName_ = "usd_eur_basis";
        result.spec_.basis_.knotDates_ = knots;
        result.spec_.basis_.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        result.spec_.basis_.smoothingWeight_ = 1.0;
        result.spec_.fixings_ = fixings;
        result.spec_.tolerance_ = 1.0e-10;
        result.spec_.fitTolerance_ = 1.0e-8;
        result.spec_.initialGuess_ = 0.005;
        result.spec_.maxEvaluations_ = 500;
        result.spec_.maxRestarts_ = 40;
        result.spec_.solveMode_ = CurveSolveMode_::Value_::EXACT;
        auto shifted = [](const Vector_<>& parameters) {
            Vector_<> result = parameters;
            for (double& value : result)
                value += 0.001;
            return result;
        };
        result.spec_.domestic_.curves_[0].initialGuessPerNode_ = shifted(result.domesticDiscountParameters_);
        result.spec_.domestic_.curves_[1].initialGuessPerNode_ = shifted(result.domesticForwardParameters_);
        result.spec_.foreign_.curves_[0].initialGuessPerNode_ = shifted(result.foreignDiscountParameters_);
        result.spec_.foreign_.curves_[1].initialGuessPerNode_ = shifted(result.foreignForwardParameters_);
        result.spec_.basis_.initialGuessPerNode_ = shifted(result.basisParameters_);

        for (int i = 0; i < static_cast<int>(maturities.size()); ++i) {
            const XccyNotionalMode_ mode = i % 2 == 0 ? XccyNotionalMode_::Value_::RESETTABLE : XccyNotionalMode_::Value_::MARK_TO_MARKET;
            const CrossCurrencySwapConfig_ config = MakeXccyConfig(pair, domestic.forwardIndex_, foreign.forwardIndex_, mode);
            const CrossCurrencySwap_ prototype(today, today, maturities[i], 0.0, config);
            const double quote = (*prototype.Precompute())(quoteMarket);
            result.spec_.basis_.instruments_.push_back(
                Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(today, today, maturities[i], quote, config)));
        }
        return result;
    }

    Vector_<> PwcParameters(const DiscountCurve_& curve) {
        const auto* typed = dynamic_cast<const Tape::DiscountPWC_<double>*>(&curve);
        REQUIRE(typed, "Expected a piecewise-constant-forward curve");
        return typed->FRight();
    }

    void AssertParameters(const Vector_<>& actual, const Vector_<>& expected) {
        ASSERT_EQ(actual.size(), expected.size());
        for (int i = 0; i < static_cast<int>(actual.size()); ++i)
            ASSERT_NEAR(actual[i], expected[i], 1.0e-8) << "parameter=" << i;
    }

    void AssertCalibrationFailsWith(const JointXccyCalibrationSpec_& spec, const std::string& expected) {
        try {
            static_cast<void>(CalibrateJointXccyMarket(spec));
            FAIL() << "Expected joint XCCY calibration to fail with " << expected;
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find(expected), std::string::npos) << exception.what();
        }
    }

    String_ EscapeRegex(const String_& value) {
        static const String_ specials("\\.^$|()[]{}*+?");
        String_ result;
        for (const char ch : value) {
            if (specials.find(ch) != String_::npos)
                result.push_back('\\');
            result.push_back(ch);
        }
        return result;
    }

    class ScopedGlobalFixingHistoryRestore_ {
        Vector_<pair<String_, FixHistory_>> saved_;

    public:
        explicit ScopedGlobalFixingHistoryRestore_(const MarketFixingSnapshot_::values_t& values) {
            for (const auto& indexHistory : values)
                saved_.push_back({indexHistory.first, Global::Fixings_().History(indexHistory.first)});
        }

        ~ScopedGlobalFixingHistoryRestore_() {
            for (const auto& saved : saved_) {
                if (saved.second.vals_.empty())
                    (void)ObjectAccess_::Erase(EscapeRegex(String_("##GLOBAL##FixingsFor:") + saved.first + String_("~")));
                else
                    XGLOBAL::StoreFixings(saved.first, saved.second, false);
            }
        }
    };

    void StoreGlobalFixings(const MarketFixingSnapshot_::values_t& values, double shift = 0.0) {
        for (const auto& indexHistory : values) {
            FixHistory_ history;
            for (const auto& fixing : indexHistory.second)
                history.vals_.push_back({fixing.first, fixing.second + shift});
            XGLOBAL::StoreFixings(indexHistory.first, history, false);
        }
    }
} // namespace

TEST(XccyJointCalibrationTest, TestSyntheticMarketRecoversAllCurveParametersAndRepricesEveryGroup) {
    const JointFixture_ fixture = MakeJointFixture();

    const JointXccyCalibrationResult_ result = CalibrateJointXccyMarket(fixture.spec_);

    ASSERT_TRUE(result.converged_);
    ASSERT_LE(result.jointMaxAbsResidual_, 1.0e-8);
    ASSERT_EQ(result.domesticDiagnostics_.size(), 2);
    ASSERT_EQ(result.foreignDiagnostics_.size(), 2);
    ASSERT_LE(result.domesticDiagnostics_[0].maxAbsResidual_, 1.0e-8);
    ASSERT_LE(result.domesticDiagnostics_[1].maxAbsResidual_, 1.0e-8);
    ASSERT_LE(result.foreignDiagnostics_[0].maxAbsResidual_, 1.0e-8);
    ASSERT_LE(result.foreignDiagnostics_[1].maxAbsResidual_, 1.0e-8);
    ASSERT_LE(result.xccyDiagnostics_.maxAbsResidual_, 1.0e-8);

    AssertParameters(PwcParameters(result.domesticCurveBlock_->Discount(CollateralType_(CollateralType_::Value_::OIS))),
                     fixture.domesticDiscountParameters_);
    AssertParameters(PwcParameters(result.domesticCurveBlock_->Forward(PeriodLength_("3M"), CollateralType_(CollateralType_::Value_::OIS))),
                     fixture.domesticForwardParameters_);
    AssertParameters(PwcParameters(result.foreignCurveBlock_->Discount(CollateralType_(CollateralType_::Value_::OIS))),
                     fixture.foreignDiscountParameters_);
    AssertParameters(PwcParameters(result.foreignCurveBlock_->Forward(PeriodLength_("3M"), CollateralType_(CollateralType_::Value_::OIS))),
                     fixture.foreignForwardParameters_);
    AssertParameters(PwcParameters(*result.basisCurve_), fixture.basisParameters_);

    CrossCurrencyMarket_ solvedMarket(result.domesticCurveBlock_, result.foreignCurveBlock_, fixture.spec_.fxSpot_, fixture.spec_.valuationTime_,
                                      fixture.spec_.collateralCurrency_, result.fixings_);
    solvedMarket.SetBasisCurve(result.basisCurve_);
    ASSERT_EQ(result.fxForwardCurve_.dates_, fixture.spec_.basis_.knotDates_);
    ASSERT_EQ(result.fxForwardCurve_.forwards_.size(), result.fxForwardCurve_.dates_.size());
    for (int i = 0; i < static_cast<int>(result.fxForwardCurve_.dates_.size()); ++i)
        ASSERT_NEAR(result.fxForwardCurve_.forwards_[i], solvedMarket.FxForward(result.fxForwardCurve_.dates_[i]), 1.0e-12);

    ASSERT_EQ(result.marketRates_.size(), result.modelRates_.size());
    ASSERT_EQ(result.marketRates_.size(), result.residuals_.size());
    for (int i = 0; i < static_cast<int>(result.residuals_.size()); ++i)
        ASSERT_NEAR(result.modelRates_[i], result.marketRates_[i], 1.0e-8) << "residual=" << i;
}

TEST(XccyJointCalibrationTest, TestValidationRejectsCurrencyAndCollateralMismatchesWithPairNames) {
    JointFixture_ fixture = MakeJointFixture();
    fixture.spec_.domestic_.ccy_ = Ccy_("GBP");
    AssertCalibrationFailsWith(fixture.spec_, "USD");

    fixture = MakeJointFixture();
    fixture.spec_.foreign_.ccy_ = Ccy_("GBP");
    AssertCalibrationFailsWith(fixture.spec_, "EUR");

    fixture = MakeJointFixture();
    fixture.spec_.collateralCurrency_ = Ccy_("GBP");
    AssertCalibrationFailsWith(fixture.spec_, "USD/EUR");
}

TEST(XccyJointCalibrationTest, TestValidationRejectsUnorderedBasisKnotsWithSlotName) {
    JointFixture_ fixture = MakeJointFixture();
    std::swap(fixture.spec_.basis_.knotDates_[1], fixture.spec_.basis_.knotDates_[2]);
    AssertCalibrationFailsWith(fixture.spec_, fixture.spec_.basis_.curveName_.c_str());
}

TEST(XccyJointCalibrationTest, TestValidationRejectsMissingCurveSlotsWithTenor) {
    JointFixture_ fixture = MakeJointFixture();
    fixture.spec_.domestic_.curves_.erase(fixture.spec_.domestic_.curves_.begin() + 1);
    AssertCalibrationFailsWith(fixture.spec_, "domestic forward slot 3M");
}

TEST(XccyJointCalibrationTest, TestValidationRejectsInstrumentWithNoRemainingXccyAnnuity) {
    JointFixture_ fixture = MakeJointFixture();
    const CrossCurrencySwapConfig_ config = fixture.spec_.basis_.instruments_.front()->Config();
    const Date_ maturity = fixture.spec_.valuationTime_.Date().AddDays(-1);
    const Date_ start = Date::AddMonths(maturity, -12);
    fixture.spec_.basis_.instruments_ = {
        Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(start, start, maturity, 0.0, config)),
    };
    AssertCalibrationFailsWith(fixture.spec_, "no remaining foreign spread annuity");
}

TEST(XccyJointCalibrationTest, TestValidationRejectsInvalidSpotWithPairName) {
    JointFixture_ fixture = MakeJointFixture();
    fixture.spec_.fxSpot_ = 0.0;
    AssertCalibrationFailsWith(fixture.spec_, "USD/EUR");

    fixture = MakeJointFixture();
    fixture.spec_.fxSpot_ = std::numeric_limits<double>::quiet_NaN();
    AssertCalibrationFailsWith(fixture.spec_, "USD/EUR");
}

TEST(XccyJointCalibrationTest, TestValidationRejectsDuplicateDeclarationsWithSlotName) {
    JointFixture_ fixture = MakeJointFixture();
    fixture.spec_.domestic_.curves_.push_back(fixture.spec_.domestic_.curves_.front());
    AssertCalibrationFailsWith(fixture.spec_, fixture.spec_.domestic_.curves_.front().curveName_.c_str());
}

TEST(XccyJointCalibrationTest, TestValidationKeepsXccyCurveNamesStrict) {
    JointFixture_ fixture = MakeJointFixture();
    fixture.spec_.domestic_.curves_.front().curveName_.clear();
    AssertCalibrationFailsWith(fixture.spec_, "Domestic slot");
}

TEST(XccyJointCalibrationTest, TestValidationRejectsEmptyInstrumentGroupsWithPairName) {
    JointFixture_ fixture = MakeJointFixture();
    fixture.spec_.basis_.instruments_.clear();
    AssertCalibrationFailsWith(fixture.spec_, "USD/EUR");

    fixture = MakeJointFixture();
    fixture.spec_.foreign_.curves_.front().instruments_.clear();
    AssertCalibrationFailsWith(fixture.spec_, fixture.spec_.foreign_.curves_.front().curveName_.c_str());
}

TEST(XccyJointCalibrationTest, TestValidationRejectsNullHandlesBeforeInstrumentUse) {
    JointFixture_ fixture = MakeJointFixture();
    fixture.spec_.domestic_.curves_.front().instruments_.front() = Handle_<YCInstrument_>();
    AssertCalibrationFailsWith(fixture.spec_, "Domestic slot");

    fixture = MakeJointFixture();
    fixture.spec_.foreign_.curves_.front().instruments_.front() = Handle_<YCInstrument_>();
    AssertCalibrationFailsWith(fixture.spec_, "Foreign slot");

    fixture = MakeJointFixture();
    fixture.spec_.basis_.instruments_.front() = Handle_<CrossCurrencySwap_>();
    AssertCalibrationFailsWith(fixture.spec_, "empty XCCY instrument at index 0");
}

TEST(XccyJointCalibrationTest, TestOmittedFixingsCaptureAllHistoricalRequestsOnceAndExplicitSnapshotRemainsAuthoritative) {
    JointFixture_ fixture = MakeJointFixture();
    const Date_ today = fixture.spec_.valuationTime_.Date();
    const Vector_<Date_> knots = JointKnots(today);
    const Vector_<Date_> maturities = InstrumentMaturities(today);
    const CurrencyFixture_ domestic = MakeCurrencyFixture(today, fixture.spec_.pair_.domestic_, knots, maturities,
                                                          fixture.domesticDiscountParameters_, fixture.domesticForwardParameters_);
    const CurrencyFixture_ foreign = MakeCurrencyFixture(today, fixture.spec_.pair_.foreign_, knots, maturities, fixture.foreignDiscountParameters_,
                                                         fixture.foreignForwardParameters_);
    const CrossCurrencySwapConfig_ config =
        MakeXccyConfig(fixture.spec_.pair_, domestic.forwardIndex_, foreign.forwardIndex_, XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const Date_ startedDate(2024, 10, 15);
    const Date_ startedMaturity = Date::AddMonths(startedDate, 12);
    const DateTime_ historicalFixing(Date::AddMonths(startedDate, 3), 11, 0);
    MarketFixingSnapshot_::values_t values;
    values[config.domesticRateFixing_.indexName_][historicalFixing] = 0.040;
    values[config.foreignRateFixing_.indexName_][historicalFixing] = 0.030;
    values[FxIndexName(config.pair_)][historicalFixing] = 1.20;
    const Handle_<MarketFixingSnapshot_> explicitFixings(new MarketFixingSnapshot_(values));

    CrossCurrencyMarket_ quoteMarket(domestic.market_, foreign.market_, fixture.spec_.fxSpot_, fixture.spec_.valuationTime_,
                                     fixture.spec_.collateralCurrency_, explicitFixings);
    quoteMarket.SetBasisCurve(
        Handle_<DiscountCurve_>(NewDiscountPWC("true_started_basis", "USD", PiecewiseConstant_(knots, fixture.basisParameters_))));
    const CrossCurrencySwap_ prototype(startedDate, startedDate, startedMaturity, 0.0, config);
    fixture.spec_.basis_.instruments_[0] = Handle_<CrossCurrencySwap_>(
        new CrossCurrencySwap_(startedDate, startedDate, startedMaturity, (*prototype.Precompute())(quoteMarket), config));
    fixture.spec_.fixings_ = explicitFixings;

    const ScopedGlobalFixingHistoryRestore_ restore(values);
    StoreGlobalFixings(values);
    const JointXccyCalibrationResult_ explicitResult = CalibrateJointXccyMarket(fixture.spec_);
    JointXccyCalibrationSpec_ omittedSpec = fixture.spec_;
    omittedSpec.fixings_ = Handle_<MarketFixingSnapshot_>();
    const JointXccyCalibrationResult_ capturedResult = CalibrateJointXccyMarket(omittedSpec);

    ASSERT_TRUE(capturedResult.fixings_);
    for (const auto& indexHistory : values)
        for (const auto& fixing : indexHistory.second)
            ASSERT_NEAR(capturedResult.fixings_->Require(indexHistory.first, fixing.first, "captured joint XCCY fixing"), fixing.second, 1.0e-12);
    StoreGlobalFixings(values, 0.10);
    for (const auto& indexHistory : values)
        for (const auto& fixing : indexHistory.second)
            ASSERT_NEAR(capturedResult.fixings_->Require(indexHistory.first, fixing.first, "immutable joint XCCY fixing"), fixing.second, 1.0e-12);

    AssertParameters(PwcParameters(*capturedResult.basisCurve_), PwcParameters(*explicitResult.basisCurve_));
    ASSERT_NEAR(capturedResult.jointMaxAbsResidual_, explicitResult.jointMaxAbsResidual_, 1.0e-12);

    JointXccyCalibrationSpec_ emptyExplicit = fixture.spec_;
    emptyExplicit.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());
    AssertCalibrationFailsWith(emptyExplicit, FxIndexName(config.pair_).c_str());
}
