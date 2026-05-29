//
// Created by wegam on 2026/4/19.
//

#include <gtest/gtest.h>
#include <cmath>
#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/currency/currency.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    constexpr double SOLVER_INITIAL_GUESS = 0.05;
    constexpr double MIN_DISTANCE_FROM_GUESS = 0.02;

    void AssertQuotesFarFromInitialGuess(const Vector_<Handle_<YCInstrument_>>& instruments) {
        for (const auto& inst : instruments)
            ASSERT_GT(std::fabs(inst->MarketRate() - SOLVER_INITIAL_GUESS), MIN_DISTANCE_FROM_GUESS);
    }

    Handle_<DiscountCurve_> MakeFlatDiscountPWLF(const String_& name,
                                                 const String_& ccy,
                                                 const Date_& today,
                                                 double rate) {
        const Vector_<Date_> knotDates = {
            Date::AddMonths(today, 3),
            Date::AddMonths(today, 6),
            Date::AddMonths(today, 12),
            Date::AddMonths(today, 24),
        };
        const Vector_<> vals(knotDates.size(), rate);
        return Handle_<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knotDates, vals, vals)));
    }

    double ExpectedSimpleForward(const DiscountCurve_& dc,
                                 const Date_& fixingDate,
                                 const PeriodLength_& tenor,
                                 const DayBasis_& basis,
                                 const String_& ccy) {
        const Date_ maturity = Date::NominalMaturity(fixingDate, tenor, Ccy_(ccy));
        const double df = dc(fixingDate, maturity);
        return (1.0 / df - 1.0) / basis(fixingDate, maturity, nullptr);
    }

    CurveCalibrationSpec_ MakeFlatCalibrationSpec(CurveParameterization_ parameterization = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD) {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2024, 1, 15);
        spec.ccy_ = "USD";
        spec.knotDates_ = {
            Date::AddMonths(spec.today_, 3),
            Date::AddMonths(spec.today_, 6),
            Date::AddMonths(spec.today_, 12),
            Date::AddMonths(spec.today_, 24),
        };
        const DayBasis_ basis("ACT_365F");
        const double flatRate = 0.015;
        spec.instruments_.push_back(Handle_<YCInstrument_>(new Deposit_(spec.today_, spec.knotDates_[0], flatRate, basis)));
        spec.instruments_.push_back(Handle_<YCInstrument_>(new Deposit_(spec.today_, spec.knotDates_[1], flatRate, basis)));
        spec.instruments_.push_back(Handle_<YCInstrument_>(new Swap_(spec.today_, spec.knotDates_[2], flatRate, 6, basis)));
        spec.instruments_.push_back(Handle_<YCInstrument_>(new Swap_(spec.today_, spec.knotDates_[3], flatRate, 6, basis)));
        spec.parameterization_ = parameterization;
        return spec;
    }

    RateIndexConvention_ MakeForwardConvention(const PeriodLength_& tenor, const DayBasis_& basis) {
        RateIndexConvention_ convention;
        convention.useProjectionCurve_ = true;
        convention.forecastTenor_ = tenor;
        convention.dayBasis_ = basis;
        convention.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
        return convention;
    }

    RateLegConvention_ MakeLegConvention(const PeriodLength_& freq, const DayBasis_& basis) {
        RateLegConvention_ convention;
        convention.paymentFrequency_ = freq;
        convention.dayBasis_ = basis;
        return convention;
    }
} // namespace

TEST(CurveBlockTest, TestFlatCurveCalibration) {
    const Date_ today(2024, 1, 15);
    const DayBasis_ basis("ACT_365F");
    const String_ ccy = "USD";
    const double flatRate = 0.015;

    Vector_<Handle_<YCInstrument_>> instruments;
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 3), flatRate, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 6), flatRate, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 12), flatRate, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 24), flatRate, 6, basis)));

    Vector_<Date_> knotDates = {
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 24),
    };

    AssertQuotesFarFromInitialGuess(instruments);

    std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, ccy, instruments, knotDates));
    CurveBlock_ yc(*dc);

    for (const auto& inst : instruments) {
        Handle_<YCInstrument_::Rate_> rate = inst->Precompute(inst, Handle_<YieldCurve_>());
        double modelRate = (*rate)(yc);
        ASSERT_NEAR(modelRate, inst->MarketRate(), 1e-8);
    }
}

TEST(CurveBlockTest, TestUpwardSlopingCurve) {
    const Date_ today(2024, 1, 15);
    const String_ ccy = "USD";
    const DayBasis_ basis("ACT_365F");

    Vector_<Handle_<YCInstrument_>> instruments;
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 1), 0.0110, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 3), 0.0125, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 6), 0.0140, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 12), 0.0160, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 24), 0.0175, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 36), 0.0190, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 60), 0.0210, 6, basis)));

    Vector_<Date_> knotDates = {
        Date::AddMonths(today, 1),
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 18),
        Date::AddMonths(today, 24),
        Date::AddMonths(today, 36),
        Date::AddMonths(today, 48),
        Date::AddMonths(today, 60),
    };

    AssertQuotesFarFromInitialGuess(instruments);

    std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, ccy, instruments, knotDates));
    CurveBlock_ yc(*dc);

    for (const auto& inst : instruments) {
        Handle_<YCInstrument_::Rate_> rate = inst->Precompute(inst, Handle_<YieldCurve_>());
        double modelRate = (*rate)(yc);
        ASSERT_NEAR(modelRate, inst->MarketRate(), 1e-8);
    }
}

TEST(CurveBlockTest, TestDiscountRoutingFallsBackToOisCurve) {
    const Date_ today(2024, 1, 15);
    const Handle_<DiscountCurve_> ois = MakeFlatDiscountPWLF("ois", "USD", today, 0.02);
    CurveBlock_ curve(ois);

    const double gcDf = curve.Discount(CollateralType_(CollateralType_::Value_::GC))(today, Date::AddMonths(today, 6));
    const double oisDf = (*ois)(today, Date::AddMonths(today, 6));
    ASSERT_NEAR(gcDf, oisDf, 1e-12);
}

TEST(CurveBlockTest, TestFwdLiborUsesDiscountCurveWhenNoTenorCurveExists) {
    const Date_ today(2024, 1, 15);
    const DayBasis_ basis("ACT_365F");
    const Handle_<DiscountCurve_> ois = MakeFlatDiscountPWLF("ois", "USD", today, 0.02);
    CurveBlock_ curve(ois, basis);

    const PeriodLength_ tenor("3M");
    const double expected = ExpectedSimpleForward(*ois, today, tenor, basis, "USD");
    ASSERT_NEAR(curve.FwdLibor(tenor, today), expected, 1e-12);
}

TEST(CurveBlockTest, TestFwdLiborUsesTenorSpecificForwardCurve) {
    const Date_ today(2024, 1, 15);
    const DayBasis_ basis("ACT_365F");
    const Handle_<DiscountCurve_> ois = MakeFlatDiscountPWLF("ois", "USD", today, 0.01);
    const Handle_<DiscountCurve_> libor3m = MakeFlatDiscountPWLF("libor3m", "USD", today, 0.03);
    CurveBlock_ curve("bundle",
                      "USD",
                      {{CollateralType_(CollateralType_::Value_::OIS), ois}},
                      {{PeriodLength_("3M"), libor3m}},
                      basis);

    const PeriodLength_ tenor("3M");
    const double expected = ExpectedSimpleForward(*libor3m, today, tenor, basis, "USD");
    ASSERT_NEAR(curve.FwdLibor(tenor, today), expected, 1e-12);
}

TEST(CurveBlockTest, TestRoundTrip) {
    const Date_ today(2024, 1, 15);
    const String_ ccy = "USD";
    const DayBasis_ basis("ACT_365F");

    Vector_<Date_> knotDates = {
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 24),
    };

    Vector_<> origLeft = {0.012, 0.014, 0.017, 0.020};
    Vector_<> origRight = {0.013, 0.015, 0.018, 0.021};

    PiecewiseLinear_ origPwl(knotDates, origLeft, origRight);
    std::unique_ptr<DiscountCurve_> origDc(NewDiscountPWLF(String_("original"), ccy, origPwl));

    Vector_<Handle_<YCInstrument_>> instruments;
    instruments.push_back(Handle_<YCInstrument_>(
        new Deposit_(today, knotDates[0], 0.012, basis)));
    instruments.push_back(Handle_<YCInstrument_>(
        new Deposit_(today, knotDates[1], 0.014, basis)));
    instruments.push_back(Handle_<YCInstrument_>(
        new Swap_(today, knotDates[2], 0.017, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(
        new Swap_(today, knotDates[3], 0.020, 6, basis)));

    AssertQuotesFarFromInitialGuess(instruments);

    std::unique_ptr<DiscountCurve_> calibDc(CalibrateYieldCurve(today, ccy, instruments, knotDates));
    CurveBlock_ yc(*calibDc);

    for (const auto& inst : instruments) {
        Handle_<YCInstrument_::Rate_> rate = inst->Precompute(inst, Handle_<YieldCurve_>());
        double modelRate = (*rate)(yc);
        ASSERT_NEAR(modelRate, inst->MarketRate(), 1e-8);
    }
}

TEST(CurveBlockTest, TestCalibrationWithSTIR) {
    const Date_ today(2024, 1, 15);
    const String_ ccy = "USD";
    const DayBasis_ basis("ACT_365F");

    Vector_<Handle_<YCInstrument_>> instruments;
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 1), 0.0110, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new STIR_(today, Date::AddMonths(today, 3), Date::AddMonths(today, 6), 0.0130, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new STIR_(today, Date::AddMonths(today, 6), Date::AddMonths(today, 9), 0.0140, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 12), 0.0160, 3, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 24), 0.0180, 6, basis)));

    Vector_<Date_> knotDates = {
        Date::AddMonths(today, 1),
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 9),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 24),
    };

    AssertQuotesFarFromInitialGuess(instruments);

    std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, ccy, instruments, knotDates));
    CurveBlock_ yc(*dc);

    for (const auto& inst : instruments) {
        Handle_<YCInstrument_::Rate_> rate = inst->Precompute(inst, Handle_<YieldCurve_>());
        double modelRate = (*rate)(yc);
        ASSERT_NEAR(modelRate, inst->MarketRate(), 1e-8);
    }
}

TEST(CurveBlockTest, TestExactCalibrationDiagnosticsExposeEffectiveJacobianInverse) {
    CurveCalibrationSpec_ spec = MakeFlatCalibrationSpec();
    AssertQuotesFarFromInitialGuess(spec.instruments_);

    CurveCalibrationResult_ result = CalibrateYieldCurve(spec);
    ASSERT_TRUE(result.curve_);
    ASSERT_EQ(result.diagnostics_.instrumentNames_.size(), spec.instruments_.size());
    ASSERT_EQ(result.diagnostics_.marketRates_.size(), spec.instruments_.size());
    ASSERT_EQ(result.diagnostics_.modelRates_.size(), spec.instruments_.size());
    ASSERT_EQ(result.diagnostics_.residuals_.size(), spec.instruments_.size());
    ASSERT_EQ(result.diagnostics_.effJacobianInverse_.Cols(), static_cast<int>(spec.instruments_.size()));
    ASSERT_EQ(result.diagnostics_.effJacobianInverse_.Rows(), 2 * static_cast<int>(spec.knotDates_.size()));
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1e-8);
    ASSERT_FALSE(result.diagnostics_.usedApproximateFit_);
}

TEST(CurveBlockTest, TestApproximateCalibrationUsesRequestedSolveMode) {
    CurveCalibrationSpec_ spec = MakeFlatCalibrationSpec();
    spec.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    spec.fitTolerance_ = 1e-4;
    AssertQuotesFarFromInitialGuess(spec.instruments_);

    CurveCalibrationResult_ result = CalibrateYieldCurve(spec);
    ASSERT_TRUE(result.curve_);
    ASSERT_TRUE(result.diagnostics_.usedApproximateFit_);
    ASSERT_LT(result.diagnostics_.rmsResidual_, spec.fitTolerance_);
}

TEST(CurveBlockTest, TestInstrumentDrivenKnotPolicyBuildsCalibratedCurve) {
    CurveCalibrationSpec_ spec = MakeFlatCalibrationSpec();
    spec.knotDates_.clear();
    spec.knotPolicy_ = CurveKnotPolicy_::Value_::INSTRUMENTS;
    AssertQuotesFarFromInitialGuess(spec.instruments_);

    CurveCalibrationResult_ result = CalibrateYieldCurve(spec);
    ASSERT_TRUE(result.curve_);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1e-8);
}

TEST(CurveBlockTest, TestPiecewiseConstantCalibration) {
    CurveCalibrationSpec_ spec = MakeFlatCalibrationSpec(CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD);
    spec.knotPolicy_ = CurveKnotPolicy_::Value_::INSTRUMENTS;
    spec.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    spec.fitTolerance_ = 1e-4;
    AssertQuotesFarFromInitialGuess(spec.instruments_);

    CurveCalibrationResult_ result = CalibrateYieldCurve(spec);
    ASSERT_TRUE(result.curve_);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, spec.fitTolerance_);
}

TEST(CurveBlockTest, TestSequentialMultiCurveCalibration) {
    const Date_ today(2024, 1, 15);
    const DayBasis_ basis("ACT_360");
    const Handle_<DiscountCurve_> ois = MakeFlatDiscountPWLF("ois", "USD", today, 0.01);

    const Vector_<Date_> forwardKnots = {
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 24),
    };
    const Vector_<> spreadVals(forwardKnots.size(), 0.02);
    const Handle_<DiscountCurve_> libor3m(
        NewDiscountPWLF("libor3m", "USD", PiecewiseLinear_(forwardKnots, spreadVals, spreadVals), ois));
    const CurveBlock_ marketCurve("market",
                                  "USD",
                                  {{CollateralType_(CollateralType_::Value_::OIS), ois}},
                                  {{PeriodLength_("3M"), libor3m}},
                                  basis);

    const RateLegConvention_ fixedLeg = MakeLegConvention(PeriodLength_("6M"), basis);
    const RateLegConvention_ floatLeg = MakeLegConvention(PeriodLength_("3M"), basis);
    const RateIndexConvention_ ibor3m = MakeForwardConvention(PeriodLength_("3M"), basis);
    RateIndexConvention_ oisIndex;
    oisIndex.dayBasis_ = basis;
    oisIndex.collateral_ = CollateralType_(CollateralType_::Value_::OIS);

    const Handle_<YCInstrument_> oisDeposit(new Deposit_(today, Date::AddMonths(today, 3), 0.0, basis));
    const Handle_<YCInstrument_> oisSwap(new OISSwap_(today,
                                                      today,
                                                      Date::AddMonths(today, 24),
                                                      0.0,
                                                      fixedLeg,
                                                      oisIndex,
                                                      floatLeg));
    const Handle_<YCInstrument_> fra(new FRA_(today,
                                              Date::AddMonths(today, 3),
                                              Date::AddMonths(today, 6),
                                              0.0,
                                              ibor3m));
    const Handle_<YCInstrument_> irs(new Swap_(today,
                                               today,
                                               Date::AddMonths(today, 24),
                                               0.0,
                                               fixedLeg,
                                               ibor3m,
                                               floatLeg));

    const auto oisDepositRate = oisDeposit->Precompute(oisDeposit, Handle_<YieldCurve_>());
    const auto oisSwapRate = oisSwap->Precompute(oisSwap, Handle_<YieldCurve_>());
    const auto fraRate = fra->Precompute(fra, Handle_<YieldCurve_>());
    const auto irsRate = irs->Precompute(irs, Handle_<YieldCurve_>());

    CurveCalibrationSpec_ oisStage;
    oisStage.today_ = today;
    oisStage.ccy_ = "USD";
    oisStage.curveName_ = "ois";
    oisStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
    oisStage.instruments_ = {
        Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 3), (*oisDepositRate)(marketCurve), basis)),
        Handle_<YCInstrument_>(new OISSwap_(today,
                                            today,
                                            Date::AddMonths(today, 24),
                                            (*oisSwapRate)(marketCurve),
                                            fixedLeg,
                                            oisIndex,
                                            floatLeg)),
    };
    oisStage.knotDates_ = {
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 24),
    };

    CurveCalibrationSpec_ forwardStage;
    forwardStage.today_ = today;
    forwardStage.ccy_ = "USD";
    forwardStage.curveName_ = "libor3m";
    forwardStage.calibrateDiscountCurve_ = false;
    forwardStage.targetTenor_ = PeriodLength_("3M");
    forwardStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
    forwardStage.instruments_ = {
        Handle_<YCInstrument_>(new FRA_(today,
                                        Date::AddMonths(today, 3),
                                        Date::AddMonths(today, 6),
                                        (*fraRate)(marketCurve),
                                        ibor3m)),
        Handle_<YCInstrument_>(new Swap_(today,
                                         today,
                                         Date::AddMonths(today, 24),
                                         (*irsRate)(marketCurve),
                                         fixedLeg,
                                         ibor3m,
                                         floatLeg)),
    };
    forwardStage.knotDates_ = oisStage.knotDates_;

    MultiCurveCalibrationSpec_ spec;
    spec.name_ = "bundle";
    spec.ccy_ = "USD";
    spec.liborBasis_ = basis;
    spec.stages_ = {oisStage, forwardStage};

    MultiCurveCalibrationResult_ result = CalibrateMultiCurve(spec);
    ASSERT_EQ(result.discountCurves_.size(), 1);
    ASSERT_EQ(result.forwardCurves_.size(), 1);
    ASSERT_EQ(result.diagnostics_.size(), 2);

    CurveBlock_ calibrated("bundle", "USD", result.discountCurves_, result.forwardCurves_, basis);
    ASSERT_NEAR((*oisDepositRate)(calibrated), (*oisDepositRate)(marketCurve), 1e-8);
    ASSERT_NEAR((*oisSwapRate)(calibrated), (*oisSwapRate)(marketCurve), 1e-8);
    ASSERT_NEAR((*fraRate)(calibrated), (*fraRate)(marketCurve), 1e-8);
    ASSERT_NEAR((*irsRate)(calibrated), (*irsRate)(marketCurve), 1e-8);
}
