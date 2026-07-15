//
// Created by GitHub Copilot on 2026/6/6.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <cmath>
#include <dal/curve/xccycalibration.hpp>
#include <dal/storage/globals.hpp>

namespace Dal {
    CrossCurrencyMarket_::CrossCurrencyMarket_(const Handle_<CurveBlock_>& domesticBlock, const Handle_<CurveBlock_>& foreignBlock, double fxSpot)
        : CrossCurrencyMarket_(
              domesticBlock, foreignBlock, fxSpot, DateTime_(Global::Dates_::EvaluationDate()), domesticBlock ? domesticBlock->ccy_ : Ccy_()) {}

    CrossCurrencyMarket_::CrossCurrencyMarket_(const Handle_<CurveBlock_>& domesticBlock,
                                               const Handle_<CurveBlock_>& foreignBlock,
                                               double fxSpot,
                                               const DateTime_& valuationTime,
                                               const Ccy_& collateralCurrency,
                                               const Handle_<MarketFixingSnapshot_>& fixings)
        : domesticBlock_(domesticBlock), foreignBlock_(foreignBlock), fxSpot_(fxSpot), valuationTime_(valuationTime),
          collateralCurrency_(collateralCurrency), fixings_(fixings) {
        REQUIRE(domesticBlock_, "CrossCurrencyMarket_ requires a non-empty domestic curve block");
        REQUIRE(foreignBlock_, "CrossCurrencyMarket_ requires a non-empty foreign curve block");
        REQUIRE(std::isfinite(fxSpot_) && fxSpot_ > 0.0, "CrossCurrencyMarket_ requires a positive finite FX spot");
        REQUIRE(valuationTime_.IsValid(), "CrossCurrencyMarket_ requires a valid valuation time");
        domesticCcy_ = domesticBlock_->ccy_;
        foreignCcy_ = foreignBlock_->ccy_;
        REQUIRE(!(domesticCcy_ == foreignCcy_), "CrossCurrencyMarket_ requires distinct domestic and foreign currencies");
        REQUIRE(collateralCurrency_ == domesticCcy_, "CrossCurrencyMarket_ supports domestic-currency collateral only");
    }

    const DiscountCurve_& CrossCurrencyMarket_::DomesticDiscountCurve(const CollateralType_& collateral) const {
        return domesticBlock_->Discount(collateral);
    }

    const DiscountCurve_& CrossCurrencyMarket_::ForeignDiscountCurve(const CollateralType_& collateral) const {
        return foreignBlock_->Discount(collateral);
    }

    const DiscountCurve_& CrossCurrencyMarket_::DomesticForwardCurve(const PeriodLength_& tenor, const CollateralType_& collateral) const {
        return domesticBlock_->Forward(tenor, collateral);
    }

    const DiscountCurve_& CrossCurrencyMarket_::ForeignForwardCurve(const PeriodLength_& tenor, const CollateralType_& collateral) const {
        return foreignBlock_->Forward(tenor, collateral);
    }

    double CrossCurrencyMarket_::BasisDiscountFactor(const Date_& from, const Date_& to) const {
        if (!basisCurve_)
            return 1.0;
        const double retval = (*basisCurve_)(from, to);
        REQUIRE(retval > 0.0, "Cross-currency basis discount factors must be positive");
        return retval;
    }

    double CrossCurrencyMarket_::FxForward(const Date_& maturity) const {
        return FxForward(Today(), maturity, CollateralType_(CollateralType_::Value_::OIS));
    }

    double CrossCurrencyMarket_::FxForward(const Date_& from, const Date_& maturity, const CollateralType_& collateral) const {
        const double domesticDf = DomesticDiscountCurve(collateral)(from, maturity);
        const double foreignDf = ForeignDiscountCurve(collateral)(from, maturity);
        const double basisDf = BasisDiscountFactor(from, maturity);
        REQUIRE(domesticDf > 0.0 && foreignDf > 0.0, "FX forward parity requires positive discount factors");
        return fxSpot_ * foreignDf / (domesticDf * basisDf);
    }

    void CrossCurrencyMarket_::SetBasisCurve(const Handle_<DiscountCurve_>& basisCurve) {
        REQUIRE(basisCurve, "CrossCurrencyMarket_ requires a non-empty basis curve handle");
        REQUIRE(basisCurve->ccy_ == domesticCcy_, "Cross-currency basis curve currency must match the market's domestic currency");
        basisCurve_ = basisCurve;
    }

    CrossCurrencyCalibrationResult_ CalibrateCrossCurrencyMarket(const CrossCurrencyCalibrationSpec_& spec) {
        return CalibrateCrossCurrencyMarket(spec, CrossCurrencyCalibrationOptions_());
    }
} // namespace Dal
