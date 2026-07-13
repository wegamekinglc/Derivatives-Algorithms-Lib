//
// Created by Codex on 2026/7/13.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>

#include <cmath>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    Handle_<DiscountCurve_> MakeFlatCurve(const String_& name, const String_& ccy, const Date_& today, double rate) {
        const Vector_<Date_> knots = {
            Date::AddMonths(today, 6),
            Date::AddMonths(today, 12),
            Date::AddMonths(today, 24),
            Date::AddMonths(today, 36),
        };
        const Vector_<> values(knots.size(), rate);
        return Handle_<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knots, values, values)));
    }

    Handle_<CurveBlock_> MakeBlock(const String_& name, const String_& ccy, const Date_& today, double rate) {
        return Handle_<CurveBlock_>(new CurveBlock_(MakeFlatCurve(name, ccy, today, rate)));
    }

    CrossCurrencySwapConfig_ MtmConfig() {
        RateIndexConvention_ index;
        index.useProjectionCurve_ = true;
        index.forecastTenor_ = PeriodLength_("3M");
        index.dayBasis_ = DayBasis_("ACT_365F");
        index.collateral_ = CollateralType_(CollateralType_::Value_::OIS);

        RateLegConvention_ leg;
        leg.paymentFrequency_ = PeriodLength_("3M");
        leg.dayBasis_ = DayBasis_("ACT_365F");

        CrossCurrencySwapConfig_ result;
        result.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        result.domesticNotional_ = 110.0;
        result.foreignNotional_ = 100.0;
        result.convention_.initialNotionalExchange_ = true;
        result.convention_.finalNotionalExchange_ = true;
        result.convention_.spreadOnForeignLeg_ = true;
        result.convention_.domesticIndex_ = index;
        result.convention_.foreignIndex_ = index;
        result.convention_.domesticLeg_ = leg;
        result.convention_.foreignLeg_ = leg;
        result.notionalMode_ = XccyNotionalMode_::Value_::MARK_TO_MARKET;
        result.fxReset_.fixingLag_ = 0;
        result.fxReset_.fixingHour_ = 11;
        result.fxReset_.fixingMinute_ = 0;
        result.domesticRateFixing_ = {"USD-XCCY-JAC", 11, 0};
        result.foreignRateFixing_ = {"EUR-XCCY-JAC", 11, 0};
        return result;
    }

    Handle_<CrossCurrencySwap_> QuoteSwap(const Date_& tradeDate,
                                          const Date_& start,
                                          const Date_& maturity,
                                          const CrossCurrencySwapConfig_& config,
                                          const CrossCurrencyMarket_& market) {
        const CrossCurrencySwap_ prototype(tradeDate, start, maturity, 0.0, config);
        const double quote = (*prototype.Precompute())(market);
        return Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(tradeDate, start, maturity, quote, config));
    }

    struct Fixture_ {
        CrossCurrencyCalibrationSpec_ spec_;
        Vector_<> trueParameters_;
    };

    Fixture_ MakeFixture() {
        Fixture_ result;
        const Date_ valuationDate(2025, 1, 16);
        const DateTime_ valuationTime(valuationDate, 9, 0);
        const Ccy_ collateral("USD");
        const auto domestic = MakeBlock("usd_jac", "USD", valuationDate, 0.02);
        const auto foreign = MakeBlock("eur_jac", "EUR", valuationDate, 0.01);
        const auto config = MtmConfig();

        const Date_ startedDate(2024, 10, 15);
        const DateTime_ historicalFixing(Date::AddMonths(startedDate, 3), 11, 0);
        MarketFixingSnapshot_::values_t values;
        values[config.domesticRateFixing_.indexName_][historicalFixing] = 0.04;
        values[config.foreignRateFixing_.indexName_][historicalFixing] = 0.03;
        values[FxIndexName(config.pair_)][historicalFixing] = 1.20;
        const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_(values));

        const Date_ futureStart = Date::AddMonths(valuationDate, 1);
        const Vector_<Date_> maturities = {
            Date::AddMonths(startedDate, 12),
            Date::AddMonths(futureStart, 12),
            Date::AddMonths(futureStart, 18),
        };
        result.trueParameters_ = {0.0010, 0.0020, 0.0030};
        CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, valuationTime, collateral, fixings);
        quoteMarket.SetBasisCurve(
            Handle_<DiscountCurve_>(NewDiscountPWC("known_jac_basis", "USD", PiecewiseConstant_(maturities, result.trueParameters_))));

        result.spec_.today_ = valuationDate;
        result.spec_.valuationTime_ = valuationTime;
        result.spec_.collateralCurrency_ = collateral;
        result.spec_.fixings_ = fixings;
        result.spec_.basisPair_ = config.pair_;
        result.spec_.domesticCurveBlock_ = domestic;
        result.spec_.foreignCurveBlock_ = foreign;
        result.spec_.fxSpot_ = 1.10;
        result.spec_.knotDates_ = maturities;
        result.spec_.initialGuess_ = 0.0;
        result.spec_.tolerance_ = 1.0e-10;
        result.spec_.instruments_ = {
            QuoteSwap(startedDate, startedDate, maturities[0], config, quoteMarket),
            QuoteSwap(valuationDate, futureStart, maturities[1], config, quoteMarket),
            QuoteSwap(valuationDate, futureStart, maturities[2], config, quoteMarket),
        };
        return result;
    }

    Vector_<> Residuals(const CrossCurrencyCalibrationSpec_& spec, const Vector_<>& parameters) {
        CrossCurrencyMarket_ market(spec.domesticCurveBlock_, spec.foreignCurveBlock_, spec.fxSpot_, spec.valuationTime_, spec.collateralCurrency_,
                                    spec.fixings_);
        market.SetBasisCurve(Handle_<DiscountCurve_>(
            NewDiscountPWC("bumped_jac_basis", spec.basisPair_.domestic_.String(), PiecewiseConstant_(spec.knotDates_, parameters))));
        Vector_<> result;
        for (const auto& instrument : spec.instruments_)
            result.push_back((*instrument->Precompute())(market)-instrument->MarketRate());
        return result;
    }

    Matrix_<> CentralJacobian(const CrossCurrencyCalibrationSpec_& spec, const Vector_<>& parameters) {
        constexpr double h = 1.0e-6;
        Matrix_<> result(static_cast<int>(spec.instruments_.size()), static_cast<int>(parameters.size()));
        for (int column = 0; column < static_cast<int>(parameters.size()); ++column) {
            auto up = parameters;
            auto down = parameters;
            up[column] += h;
            down[column] -= h;
            const Vector_<> upResiduals = Residuals(spec, up);
            const Vector_<> downResiduals = Residuals(spec, down);
            for (int row = 0; row < static_cast<int>(spec.instruments_.size()); ++row)
                result(row, column) = (upResiduals[row] - downResiduals[row]) / (2.0 * h);
        }
        return result;
    }
} // namespace

TEST(XccyBasisJacobianTest, TestAnalyticMatchesCentralDifferenceIncludingHistoricalResetRow) {
    const auto fixture = MakeFixture();
    CrossCurrencyCalibrationOptions_ options;
    options.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    const auto calibrated = CalibrateCrossCurrencyMarket(fixture.spec_, options);

    const auto basis = calibrated.basisCurves_.at(fixture.spec_.basisPair_);
    const auto* typed = dynamic_cast<const Tape::DiscountPWC_<double>*>(basis.get());
    ASSERT_NE(typed, nullptr);
    const Vector_<> parameters = typed->FRight();
    ASSERT_LE(calibrated.diagnostics_.maxAbsResidual_, 1.0e-8);

    const Matrix_<> central = CentralJacobian(fixture.spec_, parameters);
    const Matrix_<>& analytic = calibrated.diagnostics_.jacobian_;
    ASSERT_EQ(analytic.Rows(), central.Rows());
    ASSERT_EQ(analytic.Cols(), central.Cols());
    for (int row = 0; row < analytic.Rows(); ++row)
        for (int column = 0; column < analytic.Cols(); ++column)
            ASSERT_NEAR(analytic(row, column), central(row, column), 1.0e-7) << "row=" << row << " column=" << column;
}

TEST(XccyBasisJacobianTest, TestBumpedModeKeepsOnlyEffectiveInverse) {
    const auto fixture = MakeFixture();
    CrossCurrencyCalibrationOptions_ options;
    options.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    const auto calibrated = CalibrateCrossCurrencyMarket(fixture.spec_, options);
    ASSERT_TRUE(calibrated.diagnostics_.jacobian_.Empty());
    ASSERT_FALSE(calibrated.diagnostics_.effJacobianInverse_.Empty());
    ASSERT_LE(calibrated.diagnostics_.maxAbsResidual_, 1.0e-8);
}

TEST(XccyBasisJacobianTest, TestDiagnosticFlagsAndApproximateModeLeaveMatricesEmpty) {
    auto fixture = MakeFixture();
    CrossCurrencyCalibrationOptions_ options;
    options.computeEffJacobianInverse_ = false;
    options.computeForwardJacobian_ = false;
    const auto withoutMatrices = CalibrateCrossCurrencyMarket(fixture.spec_, options);
    ASSERT_TRUE(withoutMatrices.diagnostics_.jacobian_.Empty());
    ASSERT_TRUE(withoutMatrices.diagnostics_.effJacobianInverse_.Empty());

    fixture.spec_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    const auto approximate = CalibrateCrossCurrencyMarket(fixture.spec_, CrossCurrencyCalibrationOptions_());
    ASSERT_TRUE(approximate.diagnostics_.usedApproximateFit_);
    ASSERT_TRUE(approximate.diagnostics_.jacobian_.Empty());
    ASSERT_TRUE(approximate.diagnostics_.effJacobianInverse_.Empty());
}
