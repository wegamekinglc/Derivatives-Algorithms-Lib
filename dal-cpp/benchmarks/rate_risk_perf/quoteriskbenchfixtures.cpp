//
// Created by dal-implementer on 2026/9/2.
//

#include <memory>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/curve/xccyjointcalibration.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/indice/fixingsnapshot.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/holidays.hpp>

#include "quoteriskbenchfixtures.hpp"

namespace Dal::RateRiskPerf {
    namespace {
        RateIndexConvention_ Index(bool projection) {
            RateIndexConvention_ result;
            result.forecastTenor_ = PeriodLength_(projection ? "3M" : "6M");
            result.dayBasis_ = DayBasis_("ACT_365F");
            result.businessDayConvention_ = BizDayConvention_("Unadjusted");
            result.fixingHolidays_ = Holidays::None();
            result.accrualHolidays_ = Holidays::None();
            result.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
            result.useProjectionCurve_ = projection;
            result.fixingLag_ = 0;
            return result;
        }

        Handle_<DiscountCurve_> Pwc(const String_& name, const Ccy_& ccy, const Vector_<Date_>& knots, const Vector_<>& parameters) {
            return Handle_<DiscountCurve_>(NewDiscountPWC(name, ccy.String(), PiecewiseConstant_(knots, parameters)));
        }

        Handle_<CurveBlock_> Block(const String_& name,
                                   const Ccy_& ccy,
                                   const Vector_<Date_>& knots,
                                   const Vector_<>& discountParameters,
                                   const Vector_<>& forwardParameters) {
            return Handle_<CurveBlock_>(new CurveBlock_(
                name, ccy.String(), {{CollateralType_(CollateralType_::Value_::OIS), Pwc(name + "_ois", ccy, knots, discountParameters)}},
                {{PeriodLength_("3M"), Pwc(name + "_3m", ccy, knots, forwardParameters)}}, DayBasis_("ACT_365F")));
        }

        Handle_<YCInstrument_>
        DepositInstrument(const Date_& today, const Date_& maturity, const RateIndexConvention_& index, const CurveBlock_& market) {
            const Handle_<YCInstrument_> prototype(new Deposit_(today, today, maturity, 0.0, index));
            const double quote = (*prototype->Precompute(Handle_<YieldCurve_>()))(market);
            return Handle_<YCInstrument_>(new Deposit_(today, today, maturity, quote, index));
        }

        RateTradeDefinition_
        DepositTrade(const Date_& today, const Date_& maturity, const Ccy_& currency, const String_& componentKey, const String_& instrumentId) {
            DepositTradeTerms_ terms;
            terms.notional_ = 1'000'000.0;
            terms.contractRate_ = 0.02;
            terms.lend_ = true;
            terms.index_ = Index(false);
            terms.discountComponentKey_ = componentKey;
            return {instrumentId, RateInstrumentType_::Value_::DEPOSIT, today, today, maturity, currency, terms};
        }

        CrossCurrencySwapConfig_ XccyConfig(const CurrencyPair_& pair) {
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
            result.notionalMode_ = XccyNotionalMode_::Value_::FIXED;
            return result;
        }

        RateTradeDefinition_ XccyTrade(const Date_& today, const Date_& maturity, const CurrencyPair_& pair, const String_& instrumentId) {
            XccyTradeTerms_ terms;
            terms.positionCount_ = 10'000.0;
            terms.contractSpread_ = 0.0015;
            terms.spreadOnForeignLeg_ = true;
            terms.receiveNonSpreadPaySpread_ = true;
            terms.config_ = XccyConfig(pair);
            return {instrumentId, RateInstrumentType_::Value_::XCCY, today, today, maturity, pair.domestic_, terms};
        }

        JointCurrencyCurveSpec_ CurrencySpec(
            const Date_& today, const Ccy_& ccy, const Vector_<Date_>& knots, const Vector_<Date_>& maturities, const Handle_<CurveBlock_>& market) {
            JointCurveDeclaration_ discount;
            discount.curveName_ = String_(ccy.String()) + "_ois_quote_bench";
            discount.knotDates_ = knots;
            discount.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
            discount.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);

            JointCurveDeclaration_ forward;
            forward.curveName_ = String_(ccy.String()) + "_3m_quote_bench";
            forward.knotDates_ = knots;
            forward.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
            forward.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
            forward.targetTenor_ = PeriodLength_("3M");
            forward.calibrateDiscountCurve_ = false;
            for (const auto& maturity : maturities) {
                discount.instruments_.push_back(DepositInstrument(today, maturity, Index(false), *market));
                forward.instruments_.push_back(DepositInstrument(today, maturity, Index(true), *market));
            }

            JointCurrencyCurveSpec_ result;
            result.ccy_ = ccy;
            result.curves_ = {discount, forward};
            return result;
        }

        Handle_<DiscountCurve_> AliasCurve(const DiscountCurve_& curve) {
            return Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), &curve));
        }

        RatePricingMarket_
        JointMarket(const JointXccyCalibrationSpec_& spec, const JointXccyCalibrationResult_& result, const RateQuoteRiskProvenanceConfig_& config) {
            const CollateralType_ collateral(CollateralType_::Value_::OIS);
            const Vector_<Handle_<DiscountCurve_>> curves = {
                AliasCurve(result.domesticCurveBlock_->Discount(collateral)),
                AliasCurve(result.domesticCurveBlock_->Forward(PeriodLength_("3M"), collateral)),
                AliasCurve(result.foreignCurveBlock_->Discount(collateral)),
                AliasCurve(result.foreignCurveBlock_->Forward(PeriodLength_("3M"), collateral)),
                result.basisCurve_,
            };
            REQUIRE(curves.size() == result.parameterRanges_.size(), "Unexpected joint quote-risk benchmark range count");
            RatePricingMarket_ market;
            for (int i = 0; i < static_cast<int>(result.parameterRanges_.size()); ++i)
                market.curveComponents_[config.componentKeyByParameterBlock_.at(result.parameterRanges_[i].name_)] = curves[i];
            market.valuationTime_ = spec.valuationTime_;
            market.resultCurrency_ = spec.pair_.domestic_;
            market.fixings_ = result.fixings_;
            auto xccy = std::make_shared<CrossCurrencyMarket_>(result.domesticCurveBlock_, result.foreignCurveBlock_, spec.fxSpot_,
                                                               spec.valuationTime_, spec.collateralCurrency_, result.fixings_);
            xccy->SetBasisCurve(result.basisCurve_);
            market.xccyMarket_ = xccy;
            return market;
        }
    } // namespace

    QuoteRiskBenchmarkCase_ MakeSingleCurveQuoteRiskCase() {
        QuoteRiskBenchmarkCase_ result;
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2025, 1, 2);
        spec.ccy_ = "USD";
        spec.curveName_ = "single_quote_bench";
        spec.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        spec.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-10;
        spec.initialGuess_ = 0.02;
        spec.knotDates_ = {Date::AddMonths(spec.today_, 6), Date::AddMonths(spec.today_, 12)};
        const auto known = Pwc("single_quote_bench_known", Ccy_(spec.ccy_), spec.knotDates_, {0.02, 0.025});
        const CurveBlock_ knownBlock(known, spec.liborBasis_);
        for (const auto& maturity : spec.knotDates_)
            spec.instruments_.push_back(DepositInstrument(spec.today_, maturity, Index(false), knownBlock));

        CurveCalibrationOptions_ options;
        options.computeForwardJacobian_ = false;
        auto calibration = std::make_shared<CurveCalibrationResult_>(CalibrateYieldCurve(spec, options));
        result.market_.valuationTime_ = DateTime_(spec.today_, 9, 0);
        result.market_.resultCurrency_ = Ccy_(spec.ccy_);
        result.market_.curveComponents_["discount"] =
            Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), calibration->curve_.get()));
        result.market_.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());
        RateQuoteRiskProvenanceConfig_ config;
        config.calibrationId_ = "single-quote-bench";
        config.componentKeyByParameterBlock_[spec.curveName_] = "discount";
        result.provenances_.push_back(BuildSingleCurveQuoteRiskProvenance(spec, *calibration, options, result.market_, config));
        result.trades_.push_back(DepositTrade(spec.today_, spec.knotDates_.back(), Ccy_(spec.ccy_), "discount", "single-quote-bench-trade"));
        result.calibrationLifetime_ = calibration;
        return result;
    }

    QuoteRiskBenchmarkCase_ MakeJointXccyQuoteRiskCase() {
        QuoteRiskBenchmarkCase_ result;
        const Date_ today(2025, 1, 16);
        const Vector_<Date_> maturities{Date::AddMonths(today, 12), Date::AddMonths(today, 24)};
        const Vector_<Date_> knots{Date::AddMonths(today, 6), Date::AddMonths(today, 18)};
        const Vector_<Date_> basisMaturities{Date::AddMonths(today, 18), Date::AddMonths(today, 30)};
        const Vector_<Date_> basisKnots{Date::AddMonths(today, 12), Date::AddMonths(today, 24)};
        const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));
        const auto domestic = Block("usd_true_quote_bench", pair.domestic_, knots, {0.015, 0.0155}, {0.024, 0.0245});
        const auto foreign = Block("eur_true_quote_bench", pair.foreign_, knots, {0.010, 0.0104}, {0.019, 0.0194});
        const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_());
        CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, DateTime_(today, 9, 0), pair.domestic_, fixings);
        quoteMarket.SetBasisCurve(Pwc("basis_true_quote_bench", pair.domestic_, basisKnots, {0.0010, 0.0012}));

        JointXccyCalibrationSpec_ spec;
        spec.valuationTime_ = DateTime_(today, 9, 0);
        spec.pair_ = pair;
        spec.collateralCurrency_ = pair.domestic_;
        spec.fxSpot_ = 1.10;
        spec.domestic_ = CurrencySpec(today, pair.domestic_, knots, maturities, domestic);
        spec.foreign_ = CurrencySpec(today, pair.foreign_, knots, maturities, foreign);
        spec.basis_.curveName_ = "usd_eur_basis_quote_bench";
        spec.basis_.knotDates_ = basisKnots;
        spec.fixings_ = fixings;
        spec.initialGuess_ = 0.005;
        spec.tolerance_ = 1.0e-10;
        spec.fitTolerance_ = 1.0e-8;
        const auto xccyConfig = XccyConfig(pair);
        for (const auto& maturity : basisMaturities) {
            const CrossCurrencySwap_ prototype(today, today, maturity, 0.0, xccyConfig);
            spec.basis_.instruments_.push_back(
                Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(today, today, maturity, (*prototype.Precompute())(quoteMarket), xccyConfig)));
        }

        JointXccyCalibrationOptions_ options;
        options.computeForwardJacobian_ = false;
        auto calibration = std::make_shared<JointXccyCalibrationResult_>(CalibrateJointXccyMarket(spec, options));
        RateQuoteRiskProvenanceConfig_ config;
        config.calibrationId_ = "joint-quote-bench";
        for (int i = 0; i < static_cast<int>(calibration->parameterRanges_.size()); ++i)
            config.componentKeyByParameterBlock_[calibration->parameterRanges_[i].name_] = "joint-quote-bench-" + String::FromInt(i);
        result.market_ = JointMarket(spec, *calibration, config);
        result.provenances_.push_back(BuildJointXccyQuoteRiskProvenance(spec, *calibration, options, result.market_, config));
        result.trades_.push_back(XccyTrade(today, basisKnots.back(), pair, "joint-quote-bench-trade"));
        result.calibrationLifetime_ = calibration;
        return result;
    }

    QuoteRiskBenchmarkCase_ MakeStagedXccyBasisQuoteRiskCase() {
        QuoteRiskBenchmarkCase_ result;
        const Date_ today(2025, 1, 16);
        const Vector_<Date_> knots{Date::AddMonths(today, 12), Date::AddMonths(today, 24)};
        const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));
        const auto domestic = Block("usd_staged_quote_bench", pair.domestic_, knots, {0.015, 0.0153}, {0.024, 0.0243});
        const auto foreign = Block("eur_staged_quote_bench", pair.foreign_, knots, {0.010, 0.0102}, {0.019, 0.0192});
        const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_());
        const auto xccyConfig = XccyConfig(pair);
        CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, DateTime_(today, 9, 0), pair.domestic_, fixings);
        quoteMarket.SetBasisCurve(Pwc("known_staged_quote_bench", pair.domestic_, knots, {0.0010, 0.0011}));

        CrossCurrencyCalibrationSpec_ spec;
        spec.today_ = today;
        spec.valuationTime_ = DateTime_(today, 9, 0);
        spec.collateralCurrency_ = pair.domestic_;
        spec.fixings_ = fixings;
        spec.basisPair_ = pair;
        spec.domesticCurveBlock_ = domestic;
        spec.foreignCurveBlock_ = foreign;
        spec.fxSpot_ = 1.10;
        spec.knotDates_ = knots;
        spec.initialGuess_ = 0.001;
        spec.tolerance_ = 1.0e-10;
        for (const auto& knot : knots) {
            const Date_ maturity = Date::AddMonths(knot, 6);
            const CrossCurrencySwap_ prototype(today, today, maturity, 0.0, xccyConfig);
            spec.instruments_.push_back(
                Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(today, today, maturity, (*prototype.Precompute())(quoteMarket), xccyConfig)));
        }

        CrossCurrencyCalibrationOptions_ options;
        options.computeForwardJacobian_ = false;
        auto calibration = std::make_shared<CrossCurrencyCalibrationResult_>(CalibrateCrossCurrencyMarket(spec, options));
        const auto basis = calibration->basisCurves_.at(pair);
        result.market_.valuationTime_ = spec.valuationTime_;
        result.market_.resultCurrency_ = pair.domestic_;
        result.market_.curveComponents_["staged-basis-quote-bench"] = basis;
        result.market_.fixings_ = calibration->market_.Fixings();
        result.market_.xccyMarket_ = std::make_shared<CrossCurrencyMarket_>(calibration->market_);
        RateQuoteRiskProvenanceConfig_ config;
        config.calibrationId_ = "staged-quote-bench";
        config.componentKeyByParameterBlock_["basis:xccy_basis_USD"] = "staged-basis-quote-bench";
        result.provenances_.push_back(BuildStagedXccyBasisQuoteRiskProvenance(spec, *calibration, options, result.market_, config));
        result.trades_.push_back(XccyTrade(today, Date::AddMonths(knots.back(), 6), pair, "staged-quote-bench-trade"));
        result.calibrationLifetime_ = calibration;
        return result;
    }
} // namespace Dal::RateRiskPerf
