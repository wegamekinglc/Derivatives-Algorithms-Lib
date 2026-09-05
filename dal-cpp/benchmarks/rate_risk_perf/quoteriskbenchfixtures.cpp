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

    QuoteRiskBenchmarkCase_ MakeSingleCurveQuoteRiskCase() { return MakeSingleCurveQuoteRiskCase(2, CurveJacobianMode_::Value_::ANALYTIC, 1); }

    SingleCurveProvenanceMaterials_ MakeSingleCurveProvenanceMaterials(int quoteCount, CurveJacobianMode_ mode) {
        REQUIRE(quoteCount > 0, "Single quote-risk benchmark quote count must be positive");
        SingleCurveProvenanceMaterials_ materials;
        CurveCalibrationSpec_& spec = materials.spec_;
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
        Vector_<> knownForwards;
        for (int i = 0; i < quoteCount; ++i) {
            spec.knotDates_.push_back(Date::AddMonths(spec.today_, 6 * (i + 1)));
            knownForwards.push_back(0.02 + 0.0005 * i);
        }
        const auto known = Pwc("single_quote_bench_known", Ccy_(spec.ccy_), spec.knotDates_, knownForwards);
        const CurveBlock_ knownBlock(known, spec.liborBasis_);
        for (const auto& maturity : spec.knotDates_)
            spec.instruments_.push_back(DepositInstrument(spec.today_, maturity, Index(false), knownBlock));

        materials.options_.jacobianMode_ = mode;
        materials.options_.computeForwardJacobian_ = false;
        materials.calibration_ = std::make_shared<CurveCalibrationResult_>(CalibrateYieldCurve(spec, materials.options_));
        materials.market_.valuationTime_ = DateTime_(spec.today_, 9, 0);
        materials.market_.resultCurrency_ = Ccy_(spec.ccy_);
        materials.market_.curveComponents_["discount"] =
            Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), materials.calibration_->curve_.get()));
        materials.market_.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());
        materials.config_.calibrationId_ = "single-quote-bench";
        materials.config_.componentKeyByParameterBlock_[spec.curveName_] = "discount";
        return materials;
    }

    QuoteRiskBenchmarkCase_ MakeSingleCurveQuoteRiskCase(int quoteCount, CurveJacobianMode_ mode, int tradeCount) {
        REQUIRE(quoteCount > 0 && tradeCount > 0, "Single quote-risk benchmark dimensions must be positive");
        auto materials = std::make_shared<SingleCurveProvenanceMaterials_>(MakeSingleCurveProvenanceMaterials(quoteCount, mode));
        QuoteRiskBenchmarkCase_ result;
        result.market_ = materials->market_;
        result.provenances_.push_back(
            BuildSingleCurveQuoteRiskProvenance(materials->spec_, *materials->calibration_, materials->options_, materials->market_, materials->config_));
        for (int i = 0; i < tradeCount; ++i)
            result.trades_.push_back(DepositTrade(materials->spec_.today_, materials->spec_.knotDates_.back(), Ccy_(materials->spec_.ccy_), "discount",
                                                  "single-quote-bench-trade-" + String::FromInt(i)));
        result.calibrationLifetime_ = materials;
        result.expectedPassivePriceCount_ = tradeCount;
        result.expectedPreparationCount_ = 1;
        result.expectedSweepCount_ = tradeCount;
        return result;
    }

    QuoteRiskBenchmarkCase_ MakeJointXccyQuoteRiskCase() { return MakeJointXccyQuoteRiskCase(2, CurveJacobianMode_::Value_::ANALYTIC, 1); }

    JointXccyProvenanceMaterials_ MakeJointXccyProvenanceMaterials(int quoteCount, CurveJacobianMode_ mode) {
        REQUIRE(quoteCount > 0, "Joint quote-risk benchmark quote count must be positive");
        JointXccyProvenanceMaterials_ materials;
        JointXccyCalibrationSpec_& spec = materials.spec_;
        const Date_ today(2025, 1, 16);
        Vector_<Date_> maturities, knots, basisMaturities, basisKnots;
        Vector_<> domesticDiscount, domesticForward, foreignDiscount, foreignForward, basisParameters;
        for (int i = 0; i < quoteCount; ++i) {
            maturities.push_back(Date::AddMonths(today, 12 * (i + 1)));
            knots.push_back(Date::AddMonths(today, 6 + 12 * i));
            basisMaturities.push_back(Date::AddMonths(today, 12 * (i + 1) + 6));
            basisKnots.push_back(Date::AddMonths(today, 12 * (i + 1)));
            domesticDiscount.push_back(0.015 + 0.0005 * i);
            domesticForward.push_back(0.024 + 0.0005 * i);
            foreignDiscount.push_back(0.010 + 0.0004 * i);
            foreignForward.push_back(0.019 + 0.0004 * i);
            basisParameters.push_back(0.0010 + 0.0002 * i);
        }
        const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));
        const auto domestic = Block("usd_true_quote_bench", pair.domestic_, knots, domesticDiscount, domesticForward);
        const auto foreign = Block("eur_true_quote_bench", pair.foreign_, knots, foreignDiscount, foreignForward);
        const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_());
        CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, DateTime_(today, 9, 0), pair.domestic_, fixings);
        quoteMarket.SetBasisCurve(Pwc("basis_true_quote_bench", pair.domestic_, basisKnots, basisParameters));

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

        materials.options_.jacobianMode_ = mode;
        materials.options_.computeForwardJacobian_ = false;
        materials.calibration_ = std::make_shared<JointXccyCalibrationResult_>(CalibrateJointXccyMarket(spec, materials.options_));
        materials.config_.calibrationId_ = "joint-quote-bench";
        for (int i = 0; i < static_cast<int>(materials.calibration_->parameterRanges_.size()); ++i)
            materials.config_.componentKeyByParameterBlock_[materials.calibration_->parameterRanges_[i].name_] = "joint-quote-bench-" + String::FromInt(i);
        materials.market_ = JointMarket(spec, *materials.calibration_, materials.config_);
        return materials;
    }

    QuoteRiskBenchmarkCase_ MakeJointXccyQuoteRiskCase(int quoteCount, CurveJacobianMode_ mode, int tradeCount) {
        REQUIRE(quoteCount > 0 && tradeCount > 0, "Joint quote-risk benchmark dimensions must be positive");
        auto materials = std::make_shared<JointXccyProvenanceMaterials_>(MakeJointXccyProvenanceMaterials(quoteCount, mode));
        QuoteRiskBenchmarkCase_ result;
        result.market_ = materials->market_;
        result.provenances_.push_back(
            BuildJointXccyQuoteRiskProvenance(materials->spec_, *materials->calibration_, materials->options_, materials->market_, materials->config_));
        Vector_<Date_> basisKnots;
        for (int i = 0; i < quoteCount; ++i)
            basisKnots.push_back(Date::AddMonths(materials->spec_.valuationTime_.Date(), 12 * (i + 1)));
        for (int i = 0; i < tradeCount; ++i)
            result.trades_.push_back(
                XccyTrade(materials->spec_.valuationTime_.Date(), basisKnots.back(), materials->spec_.pair_, "joint-quote-bench-trade-" + String::FromInt(i)));
        result.calibrationLifetime_ = materials;
        result.expectedPassivePriceCount_ = tradeCount;
        result.expectedPreparationCount_ = 5;
        result.expectedSweepCount_ = 5 * tradeCount;
        return result;
    }

    QuoteRiskBenchmarkCase_ MakeStagedXccyBasisQuoteRiskCase() {
        return MakeStagedXccyBasisQuoteRiskCase(2, CurveJacobianMode_::Value_::ANALYTIC, 1);
    }

    StagedXccyProvenanceMaterials_ MakeStagedXccyProvenanceMaterials(int quoteCount, CurveJacobianMode_ mode) {
        REQUIRE(quoteCount > 0, "Staged quote-risk benchmark quote count must be positive");
        StagedXccyProvenanceMaterials_ materials;
        CrossCurrencyCalibrationSpec_& spec = materials.spec_;
        const Date_ today(2025, 1, 16);
        Vector_<Date_> knots;
        Vector_<> domesticDiscount, domesticForward, foreignDiscount, foreignForward, basisParameters;
        for (int i = 0; i < quoteCount; ++i) {
            knots.push_back(Date::AddMonths(today, 12 * (i + 1)));
            domesticDiscount.push_back(0.015 + 0.0003 * i);
            domesticForward.push_back(0.024 + 0.0003 * i);
            foreignDiscount.push_back(0.010 + 0.0002 * i);
            foreignForward.push_back(0.019 + 0.0002 * i);
            basisParameters.push_back(0.0010 + 0.0001 * i);
        }
        const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));
        const auto domestic = Block("usd_staged_quote_bench", pair.domestic_, knots, domesticDiscount, domesticForward);
        const auto foreign = Block("eur_staged_quote_bench", pair.foreign_, knots, foreignDiscount, foreignForward);
        const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_());
        const auto xccyConfig = XccyConfig(pair);
        CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, DateTime_(today, 9, 0), pair.domestic_, fixings);
        quoteMarket.SetBasisCurve(Pwc("known_staged_quote_bench", pair.domestic_, knots, basisParameters));

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

        materials.options_.jacobianMode_ = mode;
        materials.options_.computeForwardJacobian_ = false;
        materials.calibration_ = std::make_shared<CrossCurrencyCalibrationResult_>(CalibrateCrossCurrencyMarket(spec, materials.options_));
        const auto basis = materials.calibration_->basisCurves_.at(pair);
        materials.market_.valuationTime_ = spec.valuationTime_;
        materials.market_.resultCurrency_ = pair.domestic_;
        materials.market_.curveComponents_["staged-basis-quote-bench"] = basis;
        materials.market_.fixings_ = materials.calibration_->market_.Fixings();
        materials.market_.xccyMarket_ = std::make_shared<CrossCurrencyMarket_>(materials.calibration_->market_);
        materials.config_.calibrationId_ = "staged-quote-bench";
        materials.config_.componentKeyByParameterBlock_["basis:xccy_basis_USD"] = "staged-basis-quote-bench";
        return materials;
    }

    QuoteRiskBenchmarkCase_ MakeStagedXccyBasisQuoteRiskCase(int quoteCount, CurveJacobianMode_ mode, int tradeCount) {
        REQUIRE(quoteCount > 0 && tradeCount > 0, "Staged quote-risk benchmark dimensions must be positive");
        auto materials = std::make_shared<StagedXccyProvenanceMaterials_>(MakeStagedXccyProvenanceMaterials(quoteCount, mode));
        QuoteRiskBenchmarkCase_ result;
        result.market_ = materials->market_;
        result.provenances_.push_back(
            BuildStagedXccyBasisQuoteRiskProvenance(materials->spec_, *materials->calibration_, materials->options_, materials->market_, materials->config_));
        for (int i = 0; i < tradeCount; ++i)
            result.trades_.push_back(XccyTrade(materials->spec_.today_, Date::AddMonths(materials->spec_.knotDates_.back(), 6), materials->spec_.basisPair_,
                                               "staged-quote-bench-trade-" + String::FromInt(i)));
        result.calibrationLifetime_ = materials;
        result.expectedPassivePriceCount_ = tradeCount;
        result.expectedPreparationCount_ = 5;
        result.expectedSweepCount_ = tradeCount;
        return result;
    }
} // namespace Dal::RateRiskPerf
