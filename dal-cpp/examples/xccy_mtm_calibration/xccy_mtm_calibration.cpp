//
// Created by Codex on 2026/7/14.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>

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
    const CollateralType_ OIS(CollateralType_::Value_::OIS);
    const PeriodLength_ THREE_MONTHS("3M");

    struct CurrencyFixture_ {
        JointCurrencyCurveSpec_ declaration_;
        Handle_<CurveBlock_> market_;
        RateIndexConvention_ discountIndex_;
        RateIndexConvention_ forwardIndex_;
        Vector_<> parameters_;
    };

    Handle_<DiscountCurve_> Pwc(const String_& name, const Ccy_& ccy, const Vector_<Date_>& knots, const Vector_<>& values) {
        return Handle_<DiscountCurve_>(NewDiscountPWC(name, ccy.String(), PiecewiseConstant_(knots, values)));
    }

    Vector_<> ShiftedInitialGuess(Vector_<> parameters) {
        for (auto& parameter : parameters)
            parameter += 0.001;
        return parameters;
    }

    RateIndexConvention_ Index(bool projection) {
        RateIndexConvention_ result;
        result.useProjectionCurve_ = projection;
        result.forecastTenor_ = THREE_MONTHS;
        result.dayBasis_ = DayBasis_("ACT_365F");
        result.collateral_ = OIS;
        result.fixingHolidays_ = Holidays::None();
        result.accrualHolidays_ = Holidays::None();
        return result;
    }

    Handle_<YCInstrument_> QuotedDeposit(const Date_& today, const Date_& maturity, const RateIndexConvention_& index, const CurveBlock_& market) {
        const Handle_<YCInstrument_> prototype(new Deposit_(today, today, maturity, 0.0, index));
        const double quote = (*prototype->Precompute(Handle_<YieldCurve_>()))(market);
        return Handle_<YCInstrument_>(new Deposit_(today, today, maturity, quote, index));
    }

    CurrencyFixture_ MakeCurrency(const Date_& today,
                                  const Ccy_& ccy,
                                  const Vector_<Date_>& knots,
                                  const Vector_<Date_>& maturities,
                                  const Vector_<>& discountParameters,
                                  const Vector_<>& forwardParameters) {
        CurrencyFixture_ result;
        result.discountIndex_ = Index(false);
        result.forwardIndex_ = Index(true);
        result.market_ = Handle_<CurveBlock_>(new CurveBlock_(
            String_(ccy.String()) + "_truth", ccy.String(), {{OIS, Pwc(String_(ccy.String()) + "_truth_ois", ccy, knots, discountParameters)}},
            {{THREE_MONTHS, Pwc(String_(ccy.String()) + "_truth_3m", ccy, knots, forwardParameters)}}, DayBasis_("ACT_365F")));

        JointCurveDeclaration_ discount;
        discount.curveName_ = String_(ccy.String()) + "_ois";
        discount.knotDates_ = knots;
        discount.targetCollateral_ = OIS;
        discount.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        discount.initialGuessPerNode_ = ShiftedInitialGuess(discountParameters);
        JointCurveDeclaration_ forward;
        forward.curveName_ = String_(ccy.String()) + "_3m";
        forward.knotDates_ = knots;
        forward.targetCollateral_ = OIS;
        forward.targetTenor_ = THREE_MONTHS;
        forward.calibrateDiscountCurve_ = false;
        forward.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        forward.initialGuessPerNode_ = ShiftedInitialGuess(forwardParameters);
        for (const auto& maturity : maturities) {
            discount.instruments_.push_back(QuotedDeposit(today, maturity, result.discountIndex_, *result.market_));
            forward.instruments_.push_back(QuotedDeposit(today, maturity, result.forwardIndex_, *result.market_));
        }
        result.declaration_.ccy_ = ccy;
        result.declaration_.curves_ = {discount, forward};
        result.parameters_ = discountParameters;
        result.parameters_.Append(forwardParameters);
        return result;
    }

    CrossCurrencySwapConfig_ Config(const CurrencyPair_& pair,
                                    const RateIndexConvention_& domesticIndex,
                                    const RateIndexConvention_& foreignIndex,
                                    const FixingIdentity_& domesticRateFixing,
                                    const FixingIdentity_& foreignRateFixing) {
        RateLegConvention_ leg;
        leg.paymentFrequency_ = THREE_MONTHS;
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
        result.convention_.domesticLeg_ = leg;
        result.convention_.foreignIndex_ = foreignIndex;
        result.convention_.foreignLeg_ = leg;
        result.notionalMode_ = XccyNotionalMode_::Value_::MARK_TO_MARKET;
        result.fxReset_.fixingLag_ = 0;
        result.fxReset_.fixingHolidays_ = Holidays::None();
        result.fxReset_.fixingHour_ = 11;
        result.fxReset_.fixingMinute_ = 0;
        result.domesticRateFixing_ = domesticRateFixing;
        result.foreignRateFixing_ = foreignRateFixing;
        return result;
    }

    XccyBasisCurveDeclaration_ MakeBasisDeclaration(const Date_& today,
                                                    const Date_& inProgressStart,
                                                    const Vector_<Date_>& knots,
                                                    const Vector_<Date_>& maturities,
                                                    const CrossCurrencySwapConfig_& config,
                                                    const CurrencyFixture_& domestic,
                                                    const CurrencyFixture_& foreign,
                                                    const Handle_<MarketFixingSnapshot_>& fixings,
                                                    const Vector_<>& basisParameters) {
        CrossCurrencyMarket_ quoteMarket(domestic.market_, foreign.market_, 1.10, DateTime_(today, 9, 0), config.pair_.domestic_, fixings);
        quoteMarket.SetBasisCurve(Pwc("truth_basis", config.pair_.domestic_, knots, basisParameters));

        XccyBasisCurveDeclaration_ result;
        result.curveName_ = "USD_EUR_basis";
        result.knotDates_ = knots;
        result.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        result.initialGuessPerNode_ = ShiftedInitialGuess(basisParameters);
        for (int i = 0; i < static_cast<int>(maturities.size()); ++i) {
            const Date_ start = i == 0 ? inProgressStart : today;
            CrossCurrencySwapConfig_ instrumentConfig = config;
            // Keep the started trade MTM while future resettable quotes identify the basis term structure.
            if (i > 0)
                instrumentConfig.notionalMode_ = XccyNotionalMode_::Value_::RESETTABLE;
            const CrossCurrencySwap_ prototype(start, start, maturities[i], 0.0, instrumentConfig);
            result.instruments_.push_back(Handle_<CrossCurrencySwap_>(
                new CrossCurrencySwap_(start, start, maturities[i], (*prototype.Precompute())(quoteMarket), instrumentConfig)));
        }
        return result;
    }

    Vector_<> CurveParameters(const DiscountCurve_& curve) {
        const auto* typed = dynamic_cast<const Tape::DiscountPWC_<double>*>(&curve);
        if (!typed)
            return {};
        return typed->FRight();
    }

    Vector_<> RecoveredParameters(const JointXccyCalibrationResult_& result) {
        Vector_<> parameters = CurveParameters(result.domesticCurveBlock_->Discount(OIS));
        parameters.Append(CurveParameters(result.domesticCurveBlock_->Forward(THREE_MONTHS, OIS)));
        parameters.Append(CurveParameters(result.foreignCurveBlock_->Discount(OIS)));
        parameters.Append(CurveParameters(result.foreignCurveBlock_->Forward(THREE_MONTHS, OIS)));
        parameters.Append(CurveParameters(*result.basisCurve_));
        return parameters;
    }

    double MaxDifference(const Vector_<>& actual, const Vector_<>& expected, int offset, int size) {
        double result = 0.0;
        for (int i = offset; i < offset + size; ++i)
            result = std::max(result, std::abs(actual[i] - expected[i]));
        return result;
    }

    JointXccyCalibrationSpec_ JointSpec(const DateTime_& valuationTime,
                                        const CurrencyPair_& pair,
                                        const JointCurrencyCurveSpec_& domestic,
                                        const JointCurrencyCurveSpec_& foreign,
                                        const XccyBasisCurveDeclaration_& basis,
                                        const Handle_<MarketFixingSnapshot_>& fixings) {
        JointXccyCalibrationSpec_ result;
        result.valuationTime_ = valuationTime;
        result.pair_ = pair;
        result.collateralCurrency_ = pair.domestic_;
        result.fxSpot_ = 1.10;
        result.domestic_ = domestic;
        result.foreign_ = foreign;
        result.basis_ = basis;
        result.fixings_ = fixings;
        result.tolerance_ = 1.0e-10;
        result.initialGuess_ = 0.005;
        return result;
    }

    bool PrintAndValidate(const JointXccyCalibrationResult_& result,
                          const Vector_<>& domesticParameters,
                          const Vector_<>& foreignParameters,
                          const Vector_<>& basisParameters) {
        Vector_<> truth = domesticParameters;
        truth.Append(foreignParameters);
        truth.Append(basisParameters);
        const Vector_<> recovered = RecoveredParameters(result);
        double maxParameterError = 0.0;
        std::cout << std::scientific << std::setprecision(3) << "converged=" << std::boolalpha << result.converged_
                  << " maxResidual=" << result.jointMaxAbsResidual_ << " jacobian=" << result.jacobianAtSolution_.Rows() << "x"
                  << result.jacobianAtSolution_.Cols() << '\n';
        for (int i = 0; i < static_cast<int>(result.parameterRanges_.size()); ++i) {
            const auto& parameterRange = result.parameterRanges_[i];
            const auto& residualRange = result.residualRanges_[i];
            const double blockError = MaxDifference(recovered, truth, parameterRange.offset_, parameterRange.size_);
            maxParameterError = std::max(maxParameterError, blockError);
            std::cout << parameterRange.name_ << " params=[" << parameterRange.offset_ << "," << parameterRange.offset_ + parameterRange.size_
                      << ") residuals=[" << residualRange.offset_ << "," << residualRange.offset_ + residualRange.size_
                      << ") recoveryError=" << blockError << '\n';
        }
        return result.converged_ && result.jointMaxAbsResidual_ < 1.0e-8 && maxParameterError < 1.0e-8 && !result.jacobianAtSolution_.Empty() &&
               !result.parameterRanges_.empty();
    }
} // namespace

int main() {
    const Date_ today(2025, 1, 16);
    const DateTime_ valuationTime(today, 9, 0);
    const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));
    const FixingIdentity_ domesticRateFixing{"USD-JOINT-3M", 11, 0};
    const FixingIdentity_ foreignRateFixing{"EUR-JOINT-3M", 11, 0};
    const Date_ inProgressStart(2024, 10, 15);
    const DateTime_ historicalFixing(Date::AddMonths(inProgressStart, 3), 11, 0);
    MarketFixingSnapshot_::values_t observations;
    observations[domesticRateFixing.indexName_][historicalFixing] = 0.040;
    observations[foreignRateFixing.indexName_][historicalFixing] = 0.030;
    observations[FxIndexName(pair)][historicalFixing] = 1.20;
    const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_(observations));
    const Vector_<Date_> knots = {Date::AddMonths(today, 6), Date::AddMonths(today, 18), Date::AddMonths(today, 30), Date::AddMonths(today, 42),
                                  Date::AddMonths(today, 54)};
    const Vector_<Date_> maturities = {Date::AddMonths(inProgressStart, 15), Date::AddMonths(today, 24), Date::AddMonths(today, 36),
                                       Date::AddMonths(today, 48), Date::AddMonths(today, 60)};
    const CurrencyFixture_ domestic =
        MakeCurrency(today, pair.domestic_, knots, maturities, {0.015, 0.016, 0.017, 0.018, 0.019}, {0.024, 0.025, 0.026, 0.027, 0.028});
    const CurrencyFixture_ foreign =
        MakeCurrency(today, pair.foreign_, knots, maturities, {0.010, 0.011, 0.012, 0.013, 0.014}, {0.019, 0.020, 0.021, 0.022, 0.023});
    const CrossCurrencySwapConfig_ mtmConfig = Config(pair, domestic.forwardIndex_, foreign.forwardIndex_, domesticRateFixing, foreignRateFixing);
    const Vector_<> basisParameters = {0.0010, 0.0014, 0.0018, 0.0022, 0.0026};
    const XccyBasisCurveDeclaration_ basis =
        MakeBasisDeclaration(today, inProgressStart, knots, maturities, mtmConfig, domestic, foreign, fixings, basisParameters);
    const JointXccyCalibrationSpec_ spec = JointSpec(valuationTime, pair, domestic.declaration_, foreign.declaration_, basis, fixings);
    const JointXccyCalibrationResult_ result = CalibrateJointXccyMarket(spec);
    return PrintAndValidate(result, domestic.parameters_, foreign.parameters_, basisParameters) ? 0 : 1;
}
