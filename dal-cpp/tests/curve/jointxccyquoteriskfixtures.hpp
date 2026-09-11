//
// Created by dal-implementer on 2026/9/11.
//

#pragma once

#include <functional>

#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/ycconst.hpp>

#include "jointquoteriskfixtures.hpp"

namespace JointXccyQuoteRiskFixtures {
    using CurveTransform_ = std::function<Dal::Handle_<Dal::DiscountCurve_>(const Dal::Handle_<Dal::DiscountCurve_>&)>;

    inline Dal::JointMultiCurveCalibrationOptions_ Options(Dal::CurveJacobianMode_ mode = Dal::CurveJacobianMode_::Value_::ANALYTIC) {
        Dal::JointMultiCurveCalibrationOptions_ result;
        result.computeEffJacobianInverse_ = true;
        result.jacobianMode_ = mode;
        return result;
    }

    inline Dal::Handle_<Dal::DiscountCurve_> Flat(const Dal::JointMultiCurveCalibrationSpec_& spec,
                                                  const Dal::String_& name,
                                                  const Dal::String_& currency,
                                                  double rate,
                                                  const Dal::Handle_<Dal::DiscountCurve_>& base = {}) {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC(
            name, currency, Dal::PiecewiseConstant_(spec.curves_.front().knotDates_, Dal::Vector_<>(spec.curves_.front().knotDates_.size(), rate)),
            base));
    }

    inline Dal::RatePricingMarket_ Market(const Dal::JointMultiCurveCalibrationSpec_& spec,
                                          const Dal::JointMultiCurveCalibrationResult_& calibrated,
                                          bool registerExtraForward = false,
                                          const CurveTransform_& transformBase = {},
                                          const CurveTransform_& transformEuro = {}) {
        using namespace Dal;
        auto result = JointQuoteRiskFixtures::Market(spec, calibrated);
        const auto euroDiscount = Flat(spec, "eur-ois", "EUR", 0.017);
        const auto euroForward = transformEuro ? transformEuro(Flat(spec, "eur-3m", "EUR", 0.019)) : Flat(spec, "eur-3m", "EUR", 0.019);
        const auto basis = Flat(spec, "eur-usd-basis", "EUR", 0.001);
        const auto base = calibrated.discountCurves_.at(CollateralType_("OIS"));
        const auto extraForward = transformBase ? transformBase(base) : Flat(spec, "usd-6m", "USD", 0.015, base);
        result.curveComponents_["eur-discount"] = euroDiscount;
        result.curveComponents_["eur-forward"] = euroForward;
        result.curveComponents_["basis"] = basis;
        if (registerExtraForward)
            result.curveComponents_["usd-6m"] = extraForward;
        const Handle_<CurveBlock_> domestic(
            new CurveBlock_("EUR", "EUR", {{CollateralType_("OIS"), euroDiscount}}, {{PeriodLength_("3M"), euroForward}}, DayBasis::Act365F()));
        auto forwards = calibrated.forwardCurves_;
        forwards[PeriodLength_("6M")] = extraForward;
        const Handle_<CurveBlock_> foreign(new CurveBlock_("USD", "USD", calibrated.discountCurves_, forwards, spec.liborBasis_));
        auto xccy = std::make_shared<CrossCurrencyMarket_>(domestic, foreign, 0.9, result.valuationTime_, Ccy_("EUR"), result.fixings_);
        xccy->SetBasisCurve(basis);
        result.xccyMarket_ = xccy;
        return result;
    }

    inline Dal::RateTradeDefinition_ Trade(const Dal::JointMultiCurveCalibrationSpec_& spec) {
        using namespace Dal;
        XccyTradeTerms_ terms;
        terms.positionCount_ = 1.0;
        terms.contractSpread_ = 0.0015;
        terms.spreadOnForeignLeg_ = true;
        terms.receiveNonSpreadPaySpread_ = true;
        terms.config_.pair_ = CurrencyPair_(Ccy_("EUR"), Ccy_("USD"));
        terms.config_.domesticNotional_ = 900000.0;
        terms.config_.foreignNotional_ = 1000000.0;
        terms.config_.convention_.domesticLeg_ = JointQuoteRiskFixtures::Leg(3);
        terms.config_.convention_.foreignLeg_ = JointQuoteRiskFixtures::Leg(6);
        terms.config_.convention_.domesticIndex_ = JointQuoteRiskFixtures::Index(1);
        terms.config_.convention_.foreignIndex_ = JointQuoteRiskFixtures::Index(2);
        terms.config_.domesticRateFixing_ = {"EUR-3M", 11, 0};
        terms.config_.foreignRateFixing_ = {"USD-6M", 11, 0};
        return {"euro-xccy", RateInstrumentType_::Value_::XCCY, spec.today_, spec.today_, Date::AddMonths(spec.today_, 24), Ccy_("EUR"), terms};
    }

    inline double Reprice(const Dal::JointMultiCurveCalibrationSpec_& spec,
                          const Dal::JointMultiCurveCalibrationOptions_& options,
                          int block,
                          int ordinal,
                          double bump,
                          const CurveTransform_& transformBase = {}) {
        auto shifted = spec;
        const auto& instrument = spec.curves_[block].instruments_[ordinal];
        shifted.curves_[block].instruments_[ordinal] =
            JointQuoteRiskFixtures::Instrument(spec.today_, instrument->TimeSpan().second, block, instrument->MarketRate() + bump);
        const auto solved = Dal::CalibrateJointMultiCurve(shifted, options);
        REQUIRE(solved.converged_, "Joint quote oracle calibration failed");
        const auto pv = Dal::PriceRateTrade(Trade(spec), Market(shifted, solved, false, transformBase));
        REQUIRE(pv.succeeded_ && std::isfinite(pv.pv_), "Joint quote oracle repricing failed: " + pv.error_);
        return pv.pv_;
    }
} // namespace JointXccyQuoteRiskFixtures
