//
// Created by Codex on 2026/9/8.
//

#pragma once

#include <dal/platform/platform.hpp>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/jointcalibration_internal.hpp>
#include <dal/curve/quoteriskaggregation.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/time/holidays.hpp>

namespace JointQuoteRiskFixtures {
    inline Dal::RateIndexConvention_ Index(int block) {
        Dal::RateIndexConvention_ result;
        result.forecastTenor_ = Dal::PeriodLength_(block == 2 ? "6M" : "3M");
        result.dayBasis_ = Dal::DayBasis::Act365F();
        result.businessDayConvention_ = Dal::BizDayConvention_("Unadjusted");
        result.fixingHolidays_ = Dal::Holidays::None();
        result.accrualHolidays_ = Dal::Holidays::None();
        result.useProjectionCurve_ = block != 0;
        result.collateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        return result;
    }

    inline Dal::RateLegConvention_ Leg(int months) {
        Dal::RateLegConvention_ result;
        result.paymentFrequency_ = Dal::PeriodLength_(Dal::String::FromInt(months) + "M");
        result.dayBasis_ = Dal::DayBasis::Act365F();
        result.businessDayConvention_ = Dal::BizDayConvention_("Unadjusted");
        result.accrualHolidays_ = Dal::Holidays::None();
        result.paymentHolidays_ = Dal::Holidays::None();
        return result;
    }

    inline Dal::Handle_<Dal::YCInstrument_> Instrument(const Dal::Date_& today, const Dal::Date_& maturity, int block, double quote) {
        if (block == 0)
            return Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(today, today, maturity, quote, Index(block)));
        return Dal::Handle_<Dal::YCInstrument_>(new Dal::Swap_(today, today, maturity, quote, Leg(6), Index(block), Leg(block == 2 ? 6 : 3)));
    }

    inline Dal::JointCurveDeclaration_
    Declaration(const Dal::Date_& today, int block, int count, Dal::CurveParameterization_ parameterization, bool layered) {
        Dal::JointCurveDeclaration_ declaration;
        declaration.curveName_ = "repeated_name";
        declaration.calibrateDiscountCurve_ = block == 0;
        declaration.targetTenor_ = block == 0 ? Dal::PeriodLength_() : Index(block).forecastTenor_;
        declaration.baseLayeredOverDiscount_ = layered && block != 0;
        declaration.parameterization_ = parameterization;
        for (int ordinal = 0; ordinal < count; ++ordinal) {
            const auto maturity = Dal::Date::AddMonths(today, 12 * (ordinal + 1));
            declaration.knotDates_.push_back(Dal::Date::AddMonths(maturity, -6));
            declaration.instruments_.push_back(Instrument(today, maturity, block, 0.0));
        }
        return declaration;
    }

    inline Dal::Vector_<> KnownParameters(const Dal::JointMultiCurveCalibrationSpec_& spec,
                                          const std::vector<Dal::JointCalibrationInternal::CurveSlot_>& slots,
                                          bool layered) {
        Dal::Vector_<> known;
        for (const auto& slot : slots) {
            const double rate = slot.curveIndex_ == 0 ? 0.02 : (layered ? 0.014 : 0.034) + 0.004 * (slot.curveIndex_ - 1);
            const auto parameters = Dal::JointCalibrationInternal::BuildGuessSlice(spec.curves_[slot.curveIndex_], slot.definition_, rate, "fixture");
            for (double parameter : parameters)
                known.push_back(parameter);
        }
        return known;
    }

    inline Dal::JointMultiCurveCalibrationSpec_
    Spec(int quotes = 5,
         int blocks = 2,
         Dal::CurveParameterization_ parameterization = Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
         bool layered = false) {
        Dal::JointMultiCurveCalibrationSpec_ result;
        result.today_ = Dal::Date_(2025, 1, 2);
        result.ccy_ = "USD";
        result.liborBasis_ = Dal::DayBasis::Act365F();
        result.tolerance_ = 1.0e-11;
        result.fitTolerance_ = 1.0e-9;
        result.maxEvaluations_ = 1000;
        result.maxRestarts_ = 100;
        result.initialGuess_ = 0.025;
        for (int block = 0; block < blocks; ++block) {
            const int count = quotes / blocks + (block < quotes % blocks ? 1 : 0);
            result.curves_.push_back(Declaration(result.today_, block, count, parameterization, layered));
        }
        Dal::JointCalibrationInternal::CurveCollectionSpec_ collection;
        collection.today_ = result.today_;
        collection.ccy_ = result.ccy_;
        collection.liborBasis_ = result.liborBasis_;
        collection.curves_ = &result.curves_;
        const auto slots = Dal::JointCalibrationInternal::ValidateAndBuildSlots(collection);
        const auto market = Dal::JointCalibrationInternal::BuildCurveBlock(collection, slots, KnownParameters(result, slots, layered));
        for (int block = 0; block < blocks; ++block)
            for (auto& instrument : result.curves_[block].instruments_) {
                const double quote = (*instrument->Precompute(Dal::Handle_<Dal::YieldCurve_>()))(*market);
                instrument = Instrument(result.today_, instrument->TimeSpan().second, block, quote);
            }
        return result;
    }

    inline Dal::String_ BlockKey(int index) { return "curve:" + Dal::String::FromInt(index); }

    inline Dal::RateQuoteRiskProvenanceConfig_ Config(int blocks) {
        Dal::RateQuoteRiskProvenanceConfig_ result;
        result.calibrationId_ = "generic-joint";
        for (int block = 0; block < blocks; ++block)
            result.componentKeyByParameterBlock_[BlockKey(block)] = BlockKey(block);
        return result;
    }

    inline Dal::RatePricingMarket_ Market(const Dal::JointMultiCurveCalibrationSpec_& spec, const Dal::JointMultiCurveCalibrationResult_& result) {
        Dal::RatePricingMarket_ market;
        market.valuationTime_ = Dal::DateTime_(spec.today_);
        market.resultCurrency_ = Dal::Ccy_("USD");
        market.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
        for (int block = 0; block < static_cast<int>(spec.curves_.size()); ++block) {
            const auto& declaration = spec.curves_[block];
            market.curveComponents_[BlockKey(block)] = declaration.calibrateDiscountCurve_ ? result.discountCurves_.at(declaration.targetCollateral_)
                                                                                           : result.forwardCurves_.at(declaration.targetTenor_);
        }
        return market;
    }

    inline Dal::RateTradeDefinition_ Irs(const Dal::JointMultiCurveCalibrationSpec_& spec, int block = 1, double notional = 1000000.0) {
        Dal::RateTradeDefinition_ result;
        result.instrumentId_ = "irs:" + Dal::String::FromInt(block);
        result.instrumentType_ = Dal::RateInstrumentType_::Value_::IRS;
        result.tradeDate_ = spec.today_;
        result.startDate_ = spec.today_;
        result.maturityDate_ = spec.curves_[block].instruments_.back()->TimeSpan().second;
        result.currencyOrPair_ = Dal::Ccy_(spec.ccy_);
        Dal::IrsTradeTerms_ terms;
        terms.value_.notional_ = std::abs(notional);
        terms.value_.payFixed_ = notional >= 0.0;
        terms.value_.contractRate_ = 0.03;
        terms.value_.fixedLeg_ = Leg(6);
        terms.value_.floatLeg_ = Leg(block == 2 ? 6 : 3);
        terms.value_.floatIndex_ = Index(block);
        terms.value_.fixingIdentity_ = {"USD-LIBOR", 11, 0};
        terms.value_.forecastComponentKey_ = BlockKey(block);
        terms.value_.discountComponentKey_ = BlockKey(0);
        result.terms_ = terms;
        return result;
    }

    inline Dal::Vector_<Dal::RateTradeDefinition_> Trades(const Dal::JointMultiCurveCalibrationSpec_& spec) {
        Dal::Vector_<Dal::RateTradeDefinition_> result{Irs(spec), Irs(spec, 1, -250000.0)};
        result[1].instrumentId_ = "short-irs";
        if (spec.curves_.size() == 3) {
            auto basis = Irs(spec, 2);
            basis.instrumentId_ = "basis-3m-6m";
            basis.instrumentType_ = Dal::RateInstrumentType_::Value_::BASIS_SWAP;
            Dal::BasisTradeTerms_ terms;
            terms.notional_ = 750000.0;
            terms.contractSpread_ = 0.001;
            terms.spreadLeg_ = Leg(3);
            terms.referenceLeg_ = Leg(6);
            terms.spreadIndex_ = Index(1);
            terms.referenceIndex_ = Index(2);
            terms.spreadFixingIdentity_ = {"USD-3M", 11, 0};
            terms.referenceFixingIdentity_ = {"USD-6M", 11, 0};
            terms.spreadForecastComponentKey_ = BlockKey(1);
            terms.referenceForecastComponentKey_ = BlockKey(2);
            terms.discountComponentKey_ = BlockKey(0);
            basis.terms_ = terms;
            result.push_back(basis);
        }
        return result;
    }
} // namespace JointQuoteRiskFixtures
