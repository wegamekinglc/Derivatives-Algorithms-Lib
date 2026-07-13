//
// Created by Codex on 2026/7/14.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>

#include <string>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/xccyjointcalibration.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    RateIndexConvention_ Index(bool projection) {
        RateIndexConvention_ result;
        result.useProjectionCurve_ = projection;
        result.forecastTenor_ = PeriodLength_("3M");
        result.dayBasis_ = DayBasis_("ACT_365F");
        result.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
        result.fixingLag_ = 0;
        result.fixingHolidays_ = Holidays::None();
        result.accrualHolidays_ = Holidays::None();
        return result;
    }

    Handle_<DiscountCurve_> Pwc(const String_& name, const Ccy_& ccy, const Vector_<Date_>& knots, const Vector_<>& parameters) {
        return Handle_<DiscountCurve_>(NewDiscountPWC(name, ccy.String(), PiecewiseConstant_(knots, parameters)));
    }

    Handle_<CurveBlock_> Block(
        const String_& name, const Ccy_& ccy, const Vector_<Date_>& knots, const Vector_<>& discountParameters, const Vector_<>& forwardParameters) {
        return Handle_<CurveBlock_>(
            new CurveBlock_(name, ccy.String(), {{CollateralType_(CollateralType_::Value_::OIS), Pwc(name + "_ois", ccy, knots, discountParameters)}},
                            {{PeriodLength_("3M"), Pwc(name + "_3m", ccy, knots, forwardParameters)}}, DayBasis_("ACT_365F")));
    }

    Handle_<YCInstrument_> QuoteDeposit(const Date_& today, const Date_& maturity, const RateIndexConvention_& index, const CurveBlock_& block) {
        const Handle_<YCInstrument_> prototype(new Deposit_(today, today, maturity, 0.0, index));
        const double quote = (*prototype->Precompute(Handle_<YieldCurve_>()))(block);
        return Handle_<YCInstrument_>(new Deposit_(today, today, maturity, quote, index));
    }

    JointCurrencyCurveSpec_ CurrencySpec(
        const Date_& today, const Ccy_& ccy, const Vector_<Date_>& knots, const Vector_<Date_>& maturities, const Handle_<CurveBlock_>& market) {
        JointCurveDeclaration_ discount;
        discount.curveName_ = String_(ccy.String()) + "_ois";
        discount.knotDates_ = knots;
        discount.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        discount.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);

        JointCurveDeclaration_ forward;
        forward.curveName_ = String_(ccy.String()) + "_3m";
        forward.knotDates_ = knots;
        forward.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        forward.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        forward.targetTenor_ = PeriodLength_("3M");
        forward.calibrateDiscountCurve_ = false;
        for (const auto& maturity : maturities) {
            discount.instruments_.push_back(QuoteDeposit(today, maturity, Index(false), *market));
            forward.instruments_.push_back(QuoteDeposit(today, maturity, Index(true), *market));
        }

        JointCurrencyCurveSpec_ result;
        result.ccy_ = ccy;
        result.curves_ = {discount, forward};
        return result;
    }

    CrossCurrencySwapConfig_ XccyConfig(const CurrencyPair_& pair, XccyNotionalMode_ mode) {
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
        result.convention_.domesticIndex_ = Index(true);
        result.convention_.foreignIndex_ = Index(true);
        result.convention_.domesticLeg_ = leg;
        result.convention_.foreignLeg_ = leg;
        result.notionalMode_ = mode;
        result.fxReset_.fixingLag_ = 0;
        result.fxReset_.fixingHolidays_ = Holidays::None();
        result.fxReset_.fixingHour_ = 11;
        result.fxReset_.fixingMinute_ = 0;
        result.domesticRateFixing_ = {"USD-JOINT-JAC", 11, 0};
        result.foreignRateFixing_ = {"EUR-JOINT-JAC", 11, 0};
        return result;
    }

    struct Fixture_ {
        JointXccyCalibrationSpec_ spec_;
        Vector_<Date_> knots_;
    };

    Fixture_ MakeFixture() {
        Fixture_ result;
        const Date_ today(2025, 1, 16);
        const Vector_<Date_> maturities = {Date::AddMonths(today, 12), Date::AddMonths(today, 24)};
        result.knots_ = {Date::AddMonths(today, 6), Date::AddMonths(today, 18)};
        const Handle_<CurveBlock_> domestic = Block("usd_true_jac", Ccy_("USD"), result.knots_, {0.015, 0.018}, {0.024, 0.027});
        const Handle_<CurveBlock_> foreign = Block("eur_true_jac", Ccy_("EUR"), result.knots_, {0.010, 0.013}, {0.019, 0.022});
        const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_());
        const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));
        CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, DateTime_(today, 9, 0), pair.domestic_, fixings);
        quoteMarket.SetBasisCurve(Pwc("basis_true_jac", pair.domestic_, result.knots_, {0.0010, 0.0020}));

        result.spec_.valuationTime_ = DateTime_(today, 9, 0);
        result.spec_.pair_ = pair;
        result.spec_.collateralCurrency_ = pair.domestic_;
        result.spec_.fxSpot_ = 1.10;
        result.spec_.domestic_ = CurrencySpec(today, pair.domestic_, result.knots_, maturities, domestic);
        result.spec_.foreign_ = CurrencySpec(today, pair.foreign_, result.knots_, maturities, foreign);
        result.spec_.basis_.curveName_ = "usd_eur_basis_jac";
        result.spec_.basis_.knotDates_ = result.knots_;
        result.spec_.fixings_ = fixings;
        result.spec_.initialGuess_ = 0.005;
        result.spec_.tolerance_ = 1.0e-10;
        result.spec_.fitTolerance_ = 1.0e-8;

        for (int i = 0; i < static_cast<int>(maturities.size()); ++i) {
            const XccyNotionalMode_ mode = i == 0 ? XccyNotionalMode_::Value_::RESETTABLE : XccyNotionalMode_::Value_::MARK_TO_MARKET;
            const CrossCurrencySwapConfig_ config = XccyConfig(pair, mode);
            const CrossCurrencySwap_ prototype(today, today, maturities[i], 0.0, config);
            result.spec_.basis_.instruments_.push_back(
                Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(today, today, maturities[i], (*prototype.Precompute())(quoteMarket), config)));
        }
        return result;
    }

    Vector_<> Parameters(const JointXccyCalibrationResult_& result) {
        auto extract = [](const DiscountCurve_& curve) {
            const auto* typed = dynamic_cast<const Tape::DiscountPWC_<double>*>(&curve);
            REQUIRE(typed, "Expected PWC curve in joint Jacobian fixture");
            return typed->FRight();
        };
        Vector_<> parameters;
        parameters.Append(extract(result.domesticCurveBlock_->Discount(CollateralType_(CollateralType_::Value_::OIS))));
        parameters.Append(extract(result.domesticCurveBlock_->Forward(PeriodLength_("3M"), CollateralType_(CollateralType_::Value_::OIS))));
        parameters.Append(extract(result.foreignCurveBlock_->Discount(CollateralType_(CollateralType_::Value_::OIS))));
        parameters.Append(extract(result.foreignCurveBlock_->Forward(PeriodLength_("3M"), CollateralType_(CollateralType_::Value_::OIS))));
        parameters.Append(extract(*result.basisCurve_));
        return parameters;
    }

    Vector_<> Slice(const Vector_<>& parameters, int offset, int size) {
        return Vector_<>(parameters.begin() + offset, parameters.begin() + offset + size);
    }

    Vector_<> Residuals(const Fixture_& fixture, const Vector_<>& parameters) {
        const Handle_<CurveBlock_> domestic =
            Block("usd_bumped", fixture.spec_.pair_.domestic_, fixture.knots_, Slice(parameters, 0, 2), Slice(parameters, 2, 2));
        const Handle_<CurveBlock_> foreign =
            Block("eur_bumped", fixture.spec_.pair_.foreign_, fixture.knots_, Slice(parameters, 4, 2), Slice(parameters, 6, 2));
        Vector_<> result;
        for (const auto& declaration : fixture.spec_.domestic_.curves_)
            for (const auto& instrument : declaration.instruments_)
                result.push_back((*instrument->Precompute(Handle_<YieldCurve_>()))(*domestic) - instrument->MarketRate());
        for (const auto& declaration : fixture.spec_.foreign_.curves_)
            for (const auto& instrument : declaration.instruments_)
                result.push_back((*instrument->Precompute(Handle_<YieldCurve_>()))(*foreign) - instrument->MarketRate());

        CrossCurrencyMarket_ market(domestic, foreign, fixture.spec_.fxSpot_, fixture.spec_.valuationTime_, fixture.spec_.collateralCurrency_,
                                    fixture.spec_.fixings_);
        market.SetBasisCurve(Pwc("basis_bumped", fixture.spec_.pair_.domestic_, fixture.knots_, Slice(parameters, 8, 2)));
        for (const auto& instrument : fixture.spec_.basis_.instruments_)
            result.push_back((*instrument->Precompute())(market)-instrument->MarketRate());
        return result;
    }

    Matrix_<> CentralJacobian(const Fixture_& fixture, const Vector_<>& parameters) {
        constexpr double bump = 1.0e-6;
        const int rows = static_cast<int>(Residuals(fixture, parameters).size());
        Matrix_<> result(rows, parameters.size());
        for (int column = 0; column < static_cast<int>(parameters.size()); ++column) {
            Vector_<> up = parameters;
            Vector_<> down = parameters;
            up[column] += bump;
            down[column] -= bump;
            const Vector_<> upResiduals = Residuals(fixture, up);
            const Vector_<> downResiduals = Residuals(fixture, down);
            for (int row = 0; row < rows; ++row)
                result(row, column) = (upResiduals[row] - downResiduals[row]) / (2.0 * bump);
        }
        return result;
    }

    void AssertPartition(const Vector_<CalibrationBlockRange_>& ranges, int expectedSize) {
        int offset = 0;
        for (const auto& range : ranges) {
            ASSERT_EQ(range.offset_, offset) << range.name_;
            ASSERT_GT(range.size_, 0) << range.name_;
            offset += range.size_;
        }
        ASSERT_EQ(offset, expectedSize);
    }
} // namespace

TEST(XccyJointJacobianTest, TestAnalyticStackMatchesCentralDifferencesAndRangesPartitionRowsAndColumns) {
    const Fixture_ fixture = MakeFixture();
    const JointXccyCalibrationResult_ calibrated = CalibrateJointXccyMarket(fixture.spec_);
    const Vector_<> parameters = Parameters(calibrated);
    const Matrix_<> central = CentralJacobian(fixture, parameters);

    ASSERT_EQ(calibrated.jacobianAtSolution_.Rows(), central.Rows());
    ASSERT_EQ(calibrated.jacobianAtSolution_.Cols(), central.Cols());
    for (int row = 0; row < central.Rows(); ++row)
        for (int column = 0; column < central.Cols(); ++column)
            ASSERT_NEAR(calibrated.jacobianAtSolution_(row, column), central(row, column), 2.0e-7) << "row=" << row << " column=" << column;

    AssertPartition(calibrated.parameterRanges_, central.Cols());
    AssertPartition(calibrated.residualRanges_, central.Rows());
    ASSERT_EQ(calibrated.parameterRanges_.size(), 5);
    ASSERT_EQ(calibrated.residualRanges_.size(), 5);
    ASSERT_NE(calibrated.parameterRanges_[0].name_.find("domestic"), String_::npos);
    ASSERT_NE(calibrated.parameterRanges_[2].name_.find("foreign"), String_::npos);
    ASSERT_NE(calibrated.parameterRanges_[4].name_.find("basis"), String_::npos);
    ASSERT_NE(calibrated.residualRanges_[4].name_.find("xccy"), String_::npos);
}

TEST(XccyJointJacobianTest, TestExactApproximateAnalyticAndBumpedMatrixContracts) {
    const Fixture_ fixture = MakeFixture();
    const JointXccyCalibrationResult_ analytic = CalibrateJointXccyMarket(fixture.spec_);
    ASSERT_FALSE(analytic.jacobianAtSolution_.Empty());
    ASSERT_FALSE(analytic.effJacobianInverse_.Empty());
    ASSERT_FALSE(analytic.usedApproximateFit_);

    JointXccyCalibrationOptions_ bumpedOptions;
    bumpedOptions.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    const JointXccyCalibrationResult_ bumped = CalibrateJointXccyMarket(fixture.spec_, bumpedOptions);
    ASSERT_TRUE(bumped.jacobianAtSolution_.Empty());
    ASSERT_FALSE(bumped.effJacobianInverse_.Empty());

    JointXccyCalibrationOptions_ noMatrices;
    noMatrices.computeEffJacobianInverse_ = false;
    noMatrices.computeForwardJacobian_ = false;
    const JointXccyCalibrationResult_ omitted = CalibrateJointXccyMarket(fixture.spec_, noMatrices);
    ASSERT_TRUE(omitted.jacobianAtSolution_.Empty());
    ASSERT_TRUE(omitted.effJacobianInverse_.Empty());

    JointXccyCalibrationSpec_ approximateSpec = fixture.spec_;
    approximateSpec.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    const JointXccyCalibrationResult_ approximate = CalibrateJointXccyMarket(approximateSpec);
    ASSERT_TRUE(approximate.usedApproximateFit_);
    ASSERT_TRUE(approximate.jacobianAtSolution_.Empty());
    ASSERT_TRUE(approximate.effJacobianInverse_.Empty());
    ASSERT_TRUE(approximate.xccyDiagnostics_.usedApproximateFit_);
    for (const auto& diagnostics : approximate.domesticDiagnostics_)
        ASSERT_TRUE(diagnostics.usedApproximateFit_);
    for (const auto& diagnostics : approximate.foreignDiagnostics_)
        ASSERT_TRUE(diagnostics.usedApproximateFit_);
}

TEST(XccyJointJacobianTest, TestAnalyticIneligibleFailsWithReasonWhileBumpedRemainsAvailable) {
    Fixture_ fixture = MakeFixture();
    fixture.spec_.foreign_.liborBasis_ = DayBasis_("ACT_360");
    try {
        static_cast<void>(CalibrateJointXccyMarket(fixture.spec_));
        FAIL() << "Expected analytic eligibility failure";
    } catch (const Dal::Exception_& exception) {
        ASSERT_NE(std::string(exception.what()).find("ACT_365F"), std::string::npos) << exception.what();
        ASSERT_NE(std::string(exception.what()).find("Foreign"), std::string::npos) << exception.what();
    }

    JointXccyCalibrationOptions_ bumped;
    bumped.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    ASSERT_NO_THROW(static_cast<void>(CalibrateJointXccyMarket(fixture.spec_, bumped)));
}
