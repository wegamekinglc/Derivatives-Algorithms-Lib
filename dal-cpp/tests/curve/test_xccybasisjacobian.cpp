//
// Created by Codex on 2026/7/13.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <dal/platform/platform.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/storage/_repository.hpp>
#include <dal/storage/globals.hpp>
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
        MarketFixingSnapshot_::values_t fixingValues_;
        Vector_<> trueParameters_;
    };

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

    void StoreGlobalFixings(const MarketFixingSnapshot_::values_t& values) {
        for (const auto& indexHistory : values) {
            FixHistory_ history;
            for (const auto& fixing : indexHistory.second)
                history.vals_.push_back(fixing);
            XGLOBAL::StoreFixings(indexHistory.first, history, false);
        }
    }

    void AssertCapturedFixingsAndReplaceGlobals(const Handle_<MarketFixingSnapshot_>& snapshot, const MarketFixingSnapshot_::values_t& expected) {
        for (const auto& indexHistory : expected) {
            for (const auto& fixing : indexHistory.second)
                ASSERT_NEAR(snapshot->Require(indexHistory.first, fixing.first, "captured calibration fixing"), fixing.second, 1.0e-12);

            FixHistory_ replacement;
            for (const auto& fixing : indexHistory.second)
                replacement.vals_.push_back({fixing.first, fixing.second + 0.10});
            XGLOBAL::StoreFixings(indexHistory.first, replacement, false);
        }
    }

    void AssertSnapshotFixings(const Handle_<MarketFixingSnapshot_>& snapshot, const MarketFixingSnapshot_::values_t& expected) {
        for (const auto& indexHistory : expected)
            for (const auto& fixing : indexHistory.second)
                ASSERT_NEAR(snapshot->Require(indexHistory.first, fixing.first, "immutable calibration fixing"), fixing.second, 1.0e-12);
    }

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
        result.fixingValues_[config.domesticRateFixing_.indexName_][historicalFixing] = 0.04;
        result.fixingValues_[config.foreignRateFixing_.indexName_][historicalFixing] = 0.03;
        result.fixingValues_[FxIndexName(config.pair_)][historicalFixing] = 1.20;
        const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_(result.fixingValues_));

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

    Fixture_ MakeLadderFixture(int instrumentCount, int maturityOffsetMonths = 0) {
        Fixture_ result;
        const Date_ valuationDate(2025, 1, 16);
        const DateTime_ valuationTime(valuationDate, 9, 0);
        const Ccy_ collateral("USD");
        const auto domestic = MakeBlock("usd_ladder", "USD", valuationDate, 0.02);
        const auto foreign = MakeBlock("eur_ladder", "EUR", valuationDate, 0.01);
        const auto config = MtmConfig();
        const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_());

        result.trueParameters_.reserve(instrumentCount);
        result.spec_.knotDates_.reserve(instrumentCount);
        for (int i = 0; i < instrumentCount; ++i) {
            result.spec_.knotDates_.push_back(Date::AddMonths(valuationDate, 24 * (i + 1)));
            result.trueParameters_.push_back(0.0020);
        }

        CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, valuationTime, collateral, fixings);
        quoteMarket.SetBasisCurve(Handle_<DiscountCurve_>(
            NewDiscountPWC("known_ladder_basis", "USD", PiecewiseConstant_(result.spec_.knotDates_, result.trueParameters_))));

        result.spec_.today_ = valuationDate;
        result.spec_.valuationTime_ = valuationTime;
        result.spec_.collateralCurrency_ = collateral;
        result.spec_.fixings_ = fixings;
        result.spec_.basisPair_ = config.pair_;
        result.spec_.domesticCurveBlock_ = domestic;
        result.spec_.foreignCurveBlock_ = foreign;
        result.spec_.fxSpot_ = 1.10;
        result.spec_.initialGuess_ = result.trueParameters_.front();
        result.spec_.tolerance_ = 1.0e-10;
        result.spec_.maxEvaluations_ = 500;
        for (int i = 0; i < instrumentCount; ++i) {
            const Date_ maturity = Date::AddMonths(valuationDate, 24 * (i + 1) + maturityOffsetMonths);
            result.spec_.instruments_.push_back(QuoteSwap(valuationDate, valuationDate, maturity, config, quoteMarket));
        }
        return result;
    }

    Vector_<> Parameters(const CrossCurrencyCalibrationResult_& result, const CurrencyPair_& pair) {
        const auto* basis = dynamic_cast<const Tape::DiscountPWC_<double>*>(result.basisCurves_.at(pair).get());
        REQUIRE(basis, "Expected a PWC basis curve in staged XCCY sensitivity fixture");
        return basis->FRight();
    }

    CrossCurrencyCalibrationSpec_ BumpQuote(const CrossCurrencyCalibrationSpec_& spec, int instrumentIndex, double bump) {
        CrossCurrencyCalibrationSpec_ result = spec;
        const auto& original = spec.instruments_[instrumentIndex];
        const auto span = original->TimeSpan();
        result.instruments_[instrumentIndex] = Handle_<CrossCurrencySwap_>(
            new CrossCurrencySwap_(span.first, span.first, span.second, original->MarketRate() + bump, original->Config()));
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

    void AssertCalibrationFailsWith(const CrossCurrencyCalibrationSpec_& spec, const std::string& expected) {
        try {
            static_cast<void>(CalibrateCrossCurrencyMarket(spec));
            FAIL() << "Expected staged XCCY calibration to fail with " << expected;
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find(expected), std::string::npos) << exception.what();
        }
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

TEST(XccyBasisJacobianTest, TestDiagnosticsExposeStableAxesScalingAndAvailability) {
    const auto fixture = MakeFixture();
    const auto calibrated = CalibrateCrossCurrencyMarket(fixture.spec_);
    const auto& diagnostics = calibrated.diagnostics_;

    ASSERT_EQ(diagnostics.instrumentNames_.size(), fixture.spec_.instruments_.size());
    ASSERT_EQ(diagnostics.instrumentNames_[0], diagnostics.instrumentNames_[1]);
    ASSERT_EQ(diagnostics.parameterKnotDates_, fixture.spec_.knotDates_);
    ASSERT_EQ(diagnostics.jacobian_.Rows(), static_cast<int>(diagnostics.instrumentNames_.size()));
    ASSERT_EQ(diagnostics.jacobian_.Cols(), static_cast<int>(diagnostics.parameterKnotDates_.size()));
    ASSERT_EQ(diagnostics.effJacobianInverse_.Rows(), static_cast<int>(diagnostics.parameterKnotDates_.size()));
    ASSERT_EQ(diagnostics.effJacobianInverse_.Cols(), static_cast<int>(diagnostics.instrumentNames_.size()));
    ASSERT_NEAR(diagnostics.residualTolerance_, fixture.spec_.tolerance_, 1.0e-20);
    ASSERT_EQ(diagnostics.jacobianScaling_, String_("unscaled"));
    ASSERT_EQ(diagnostics.effJacobianInverseScaling_, String_("solver_scaled"));
    ASSERT_EQ(diagnostics.jacobianAvailability_, String_("available"));
    ASSERT_EQ(diagnostics.effJacobianInverseAvailability_, String_("available"));
}

TEST(XccyBasisJacobianTest, TestEffectiveInverseRequiresToleranceScalingForSmallQuoteBumps) {
    auto fixture = MakeLadderFixture(3, 12);
    fixture.spec_.tolerance_ = 1.0e-8;
    const auto base = CalibrateCrossCurrencyMarket(fixture.spec_);
    const Vector_<> baseParameters = Parameters(base, fixture.spec_.basisPair_);
    constexpr double bump = 1.0e-6;

    for (int instrument = 0; instrument < static_cast<int>(fixture.spec_.instruments_.size()); ++instrument) {
        SCOPED_TRACE("instrument=" + std::to_string(instrument));
        Vector_<> predictedMoves(baseParameters.size());
        for (int parameter = 0; parameter < static_cast<int>(baseParameters.size()); ++parameter)
            predictedMoves[parameter] = base.diagnostics_.effJacobianInverse_(parameter, instrument) * bump / fixture.spec_.tolerance_;
        auto bumpedSpec = BumpQuote(fixture.spec_, instrument, bump);
        bumpedSpec.initialGuess_ = baseParameters.front() + predictedMoves.front();
        CrossCurrencyCalibrationOptions_ reSolveOptions;
        reSolveOptions.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
        reSolveOptions.computeEffJacobianInverse_ = false;
        reSolveOptions.computeForwardJacobian_ = false;
        Vector_<> bumpedParameters;
        try {
            bumpedParameters = Parameters(CalibrateCrossCurrencyMarket(bumpedSpec, reSolveOptions), fixture.spec_.basisPair_);
        } catch (const Dal::Exception_& exception) {
            FAIL() << "instrument=" << instrument << " " << exception.what();
        }
        double observedNorm = 0.0;
        double unscaledError = 0.0;
        Vector_<> observedMoves(baseParameters.size());
        for (int parameter = 0; parameter < static_cast<int>(baseParameters.size()); ++parameter) {
            observedMoves[parameter] = bumpedParameters[parameter] - baseParameters[parameter];
            const double incorrectlyUnscaled = base.diagnostics_.effJacobianInverse_(parameter, instrument) * bump;
            observedNorm = std::max(observedNorm, std::fabs(observedMoves[parameter]));
            unscaledError = std::max(unscaledError, std::fabs(incorrectlyUnscaled - observedMoves[parameter]));
        }
        ASSERT_GT(observedNorm, 1.0e-8) << "instrument=" << instrument;
        for (int parameter = 0; parameter < static_cast<int>(baseParameters.size()); ++parameter)
            ASSERT_NEAR(predictedMoves[parameter], observedMoves[parameter], 0.03 * observedNorm + 1.0e-10)
                << "instrument=" << instrument << " parameter=" << parameter;
        ASSERT_GT(unscaledError, 0.5 * observedNorm) << "instrument=" << instrument;
    }
}

TEST(XccyBasisJacobianTest, TestEffectiveInversePredictsOneBasisPointAcrossFixedLadders) {
    constexpr double bump = 1.0e-4;
    for (const int instrumentCount : {5, 10, 16}) {
        const auto fixture = MakeLadderFixture(instrumentCount);
        const auto base = CalibrateCrossCurrencyMarket(fixture.spec_);
        const Vector_<> baseParameters = Parameters(base, fixture.spec_.basisPair_);
        const int instrument = instrumentCount - 1;
        const Vector_<> bumpedParameters =
            Parameters(CalibrateCrossCurrencyMarket(BumpQuote(fixture.spec_, instrument, bump)), fixture.spec_.basisPair_);

        double observedNorm = 0.0;
        Vector_<> observedMoves(baseParameters.size());
        Vector_<> predictedMoves(baseParameters.size());
        for (int parameter = 0; parameter < static_cast<int>(baseParameters.size()); ++parameter) {
            observedMoves[parameter] = bumpedParameters[parameter] - baseParameters[parameter];
            predictedMoves[parameter] = base.diagnostics_.effJacobianInverse_(parameter, instrument) * bump / base.diagnostics_.residualTolerance_;
            observedNorm = std::max(observedNorm, std::fabs(observedMoves[parameter]));
        }

        ASSERT_GT(observedNorm, 1.0e-8) << "instrumentCount=" << instrumentCount;
        for (int parameter = 0; parameter < static_cast<int>(baseParameters.size()); ++parameter)
            ASSERT_NEAR(predictedMoves[parameter], observedMoves[parameter], 0.03 * observedNorm + 1.0e-10)
                << "instrumentCount=" << instrumentCount << " parameter=" << parameter;
    }
}

TEST(XccyBasisJacobianTest, TestOmittedSnapshotCapturesRequiredGlobalFixingsOnce) {
    const auto fixture = MakeFixture();
    const ScopedGlobalFixingHistoryRestore_ restore(fixture.fixingValues_);
    StoreGlobalFixings(fixture.fixingValues_);

    const auto explicitResult = CalibrateCrossCurrencyMarket(fixture.spec_);
    auto omittedSpec = fixture.spec_;
    omittedSpec.fixings_ = Handle_<MarketFixingSnapshot_>();
    const auto capturedResult = CalibrateCrossCurrencyMarket(omittedSpec);

    ASSERT_TRUE(capturedResult.market_.Fixings());
    ASSERT_NO_FATAL_FAILURE(AssertCapturedFixingsAndReplaceGlobals(capturedResult.market_.Fixings(), fixture.fixingValues_));
    ASSERT_NO_FATAL_FAILURE(AssertSnapshotFixings(capturedResult.market_.Fixings(), fixture.fixingValues_));

    const auto* explicitBasis = dynamic_cast<const Tape::DiscountPWC_<double>*>(explicitResult.basisCurves_.at(fixture.spec_.basisPair_).get());
    const auto* capturedBasis = dynamic_cast<const Tape::DiscountPWC_<double>*>(capturedResult.basisCurves_.at(fixture.spec_.basisPair_).get());
    ASSERT_NE(explicitBasis, nullptr);
    ASSERT_NE(capturedBasis, nullptr);
    ASSERT_EQ(explicitBasis->FRight().size(), capturedBasis->FRight().size());
    for (int i = 0; i < static_cast<int>(explicitBasis->FRight().size()); ++i)
        ASSERT_NEAR(explicitBasis->FRight()[i], capturedBasis->FRight()[i], 1.0e-12);
    ASSERT_EQ(explicitResult.diagnostics_.modelRates_.size(), capturedResult.diagnostics_.modelRates_.size());
    for (int i = 0; i < static_cast<int>(explicitResult.diagnostics_.modelRates_.size()); ++i)
        ASSERT_NEAR(explicitResult.diagnostics_.modelRates_[i], capturedResult.diagnostics_.modelRates_[i], 1.0e-12);
    ASSERT_EQ(explicitResult.fxForwardCurve_.forwards_.size(), capturedResult.fxForwardCurve_.forwards_.size());
    for (int i = 0; i < static_cast<int>(explicitResult.fxForwardCurve_.forwards_.size()); ++i)
        ASSERT_NEAR(explicitResult.fxForwardCurve_.forwards_[i], capturedResult.fxForwardCurve_.forwards_[i], 1.0e-12);
}

TEST(XccyBasisJacobianTest, TestExplicitSnapshotRemainsAuthoritativeWhenFixingsAreMissing) {
    auto fixture = MakeFixture();
    const ScopedGlobalFixingHistoryRestore_ restore(fixture.fixingValues_);
    StoreGlobalFixings(fixture.fixingValues_);

    fixture.spec_.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());
    ASSERT_THROW(static_cast<void>(CalibrateCrossCurrencyMarket(fixture.spec_)), Dal::Exception_);
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

TEST(XccyBasisJacobianTest, TestAvailabilityDistinguishesModeAndRequestFlags) {
    auto fixture = MakeFixture();

    CrossCurrencyCalibrationOptions_ bumpedOptions;
    bumpedOptions.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    const auto bumped = CalibrateCrossCurrencyMarket(fixture.spec_, bumpedOptions);
    ASSERT_EQ(bumped.diagnostics_.jacobianAvailability_, String_("not_available_for_mode"));
    ASSERT_EQ(bumped.diagnostics_.effJacobianInverseAvailability_, String_("available"));
    ASSERT_TRUE(bumped.diagnostics_.jacobian_.Empty());
    ASSERT_FALSE(bumped.diagnostics_.effJacobianInverse_.Empty());

    CrossCurrencyCalibrationOptions_ options;
    options.computeEffJacobianInverse_ = false;
    options.computeForwardJacobian_ = false;
    const auto withoutMatrices = CalibrateCrossCurrencyMarket(fixture.spec_, options);
    ASSERT_EQ(withoutMatrices.diagnostics_.jacobianAvailability_, String_("not_requested"));
    ASSERT_EQ(withoutMatrices.diagnostics_.effJacobianInverseAvailability_, String_("not_requested"));
    ASSERT_TRUE(withoutMatrices.diagnostics_.jacobian_.Empty());
    ASSERT_TRUE(withoutMatrices.diagnostics_.effJacobianInverse_.Empty());

    fixture.spec_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    const auto approximate = CalibrateCrossCurrencyMarket(fixture.spec_, CrossCurrencyCalibrationOptions_());
    ASSERT_TRUE(approximate.diagnostics_.usedApproximateFit_);
    ASSERT_EQ(approximate.diagnostics_.jacobianAvailability_, String_("not_available_for_mode"));
    ASSERT_EQ(approximate.diagnostics_.effJacobianInverseAvailability_, String_("not_available_for_mode"));
    ASSERT_TRUE(approximate.diagnostics_.jacobian_.Empty());
    ASSERT_TRUE(approximate.diagnostics_.effJacobianInverse_.Empty());

    const auto approximateWithoutMatrices = CalibrateCrossCurrencyMarket(fixture.spec_, options);
    ASSERT_EQ(approximateWithoutMatrices.diagnostics_.jacobianAvailability_, String_("not_requested"));
    ASSERT_EQ(approximateWithoutMatrices.diagnostics_.effJacobianInverseAvailability_, String_("not_requested"));
}

TEST(XccyBasisJacobianTest, TestValidationRejectsNonFiniteSolverInputsBeforeSolving) {
    for (const auto& invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        auto fixture = MakeFixture();
        fixture.spec_.smoothingWeight_ = invalid;
        AssertCalibrationFailsWith(fixture.spec_, "smoothing weight must be finite and positive");

        fixture = MakeFixture();
        fixture.spec_.tolerance_ = invalid;
        AssertCalibrationFailsWith(fixture.spec_, "tolerance must be finite and positive");

        fixture = MakeFixture();
        fixture.spec_.fitTolerance_ = invalid;
        AssertCalibrationFailsWith(fixture.spec_, "fit tolerance must be finite and positive");

        fixture = MakeFixture();
        fixture.spec_.initialGuess_ = invalid;
        AssertCalibrationFailsWith(fixture.spec_, "initial guess must be finite");
    }
}

TEST(XccyBasisJacobianTest, TestInstrumentConstructionRejectsNonFiniteMarketRates) {
    const auto fixture = MakeFixture();
    for (const auto& invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        try {
            static_cast<void>(BumpQuote(fixture.spec_, 0, invalid - fixture.spec_.instruments_[0]->MarketRate()));
            FAIL() << "Expected a non-finite XCCY market rate to fail";
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find("requires a finite market rate"), std::string::npos) << exception.what();
        }
    }
}
