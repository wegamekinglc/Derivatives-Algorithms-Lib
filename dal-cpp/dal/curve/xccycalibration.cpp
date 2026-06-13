//
// Created by GitHub Copilot on 2026/6/6.
//

#include <algorithm>
#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/optimization/underdeterminedutils.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/dictionary.hpp>

namespace Dal {
    namespace {
        CrossCurrencyCalibrationDiagnostics_ BuildDiagnostics(const Vector_<Handle_<CrossCurrencySwap_>>& instruments,
                                                              const CrossCurrencyMarket_& market,
                                                              bool usedApproximateFit = false,
                                                              const Matrix_<>* effJacobianInverse = nullptr) {
            CrossCurrencyCalibrationDiagnostics_ retval;
            retval.usedApproximateFit_ = usedApproximateFit;
            double maxResidual = 0.0;
            double sqResidual = 0.0;
            for (const auto& instrument : instruments) {
                const double modelRate = (*instrument->Precompute())(market);
                const double marketRate = instrument->MarketRate();
                const double residual = modelRate - marketRate;
                retval.instrumentNames_.push_back(instrument->Name());
                retval.marketRates_.push_back(marketRate);
                retval.modelRates_.push_back(modelRate);
                retval.residuals_.push_back(residual);
                maxResidual = std::max(maxResidual, std::fabs(residual));
                sqResidual += residual * residual;
            }
            retval.maxAbsResidual_ = maxResidual;
            retval.rmsResidual_ = instruments.empty() ? 0.0 : std::sqrt(sqResidual / instruments.size());
            if (effJacobianInverse)
                retval.effJacobianInverse_ = *effJacobianInverse;
            return retval;
        }

        Handle_<DiscountCurve_> BuildBasisCurve(const String_& domesticCcy,
                                                const Vector_<Date_>& knotDates,
                                                const Vector_<>& rates) {
            REQUIRE(rates.size() == knotDates.size(), "Basis curve rates and knot dates must have the same size");
            return Handle_<DiscountCurve_>(
                NewDiscountPWC(String_("xccy_basis_") + domesticCcy,
                               domesticCcy,
                               PiecewiseConstant_(knotDates, rates)));
        }

        void ValidateCalibrationSpec(const CrossCurrencyCalibrationSpec_& spec) {
            REQUIRE(spec.domesticCurveBlock_, "Cross-currency calibration requires a domestic curve block");
            REQUIRE(spec.foreignCurveBlock_, "Cross-currency calibration requires a foreign curve block");
            REQUIRE(spec.fxSpot_ > 0.0, "Cross-currency calibration requires a positive FX spot");
            REQUIRE(spec.basisPair_.domestic_ == spec.domesticCurveBlock_->ccy_,
                    "Cross-currency calibration basis pair domestic currency must match the domestic curve block");
            REQUIRE(spec.basisPair_.foreign_ == spec.foreignCurveBlock_->ccy_,
                    "Cross-currency calibration basis pair foreign currency must match the foreign curve block");
        }

        CrossCurrencyFxForwardCurve_ BuildFxForwardCurve(const CurrencyPair_& pair,
                                                         const Vector_<Date_>& dates,
                                                         const CrossCurrencyMarket_& market,
                                                         const CollateralType_& collateral) {
            CrossCurrencyFxForwardCurve_ retval;
            retval.pair_ = pair;
            retval.dates_ = dates;
            for (const auto& date : dates) {
                retval.forwards_.push_back(market.FxForward(market.Today(), date, collateral));
            }
            return retval;
        }

        Handle_<DiscountCurve_> BuildBasisCurveInternal(const String_& domesticCcy,
                                                        const Vector_<Date_>& knotDates,
                                                        const Vector_<>& rates) {
            return BuildBasisCurve(domesticCcy, knotDates, rates);
        }
    } // namespace

    class XccyCalibrationFunc_ : public Underdetermined::Function_ {
        Handle_<CurveBlock_> domesticBlock_;
        Handle_<CurveBlock_> foreignBlock_;
        double fxSpot_;
        String_ domesticCcy_;
        Vector_<Handle_<CrossCurrencySwap_>> instruments_;
        Vector_<Handle_<CrossCurrencySwap_::Rate_>> rates_;
        Vector_<> marketRates_;
        Vector_<Date_> knotDates_;

    public:
        XccyCalibrationFunc_(const Handle_<CurveBlock_>& domesticBlock,
                             const Handle_<CurveBlock_>& foreignBlock,
                             double fxSpot,
                             const Vector_<Handle_<CrossCurrencySwap_>>& instruments,
                             const Vector_<Date_>& knotDates)
            : domesticBlock_(domesticBlock),
              foreignBlock_(foreignBlock),
              fxSpot_(fxSpot),
              domesticCcy_(domesticBlock->ccy_.String()),
              instruments_(instruments),
              knotDates_(knotDates) {
            rates_.reserve(instruments_.size());
            marketRates_.reserve(instruments_.size());
            for (const auto& inst : instruments_) {
                rates_.push_back(inst->Precompute());
                marketRates_.push_back(inst->MarketRate());
            }
        }

        [[nodiscard]] Vector_<> F(const Vector_<>& x) const override {
            CrossCurrencyMarket_ market(domesticBlock_, foreignBlock_, fxSpot_);
            market.SetBasisCurve(BuildBasisCurveInternal(domesticCcy_, knotDates_, x));

            Vector_<> result(instruments_.size());
            for (int i = 0; i < static_cast<int>(instruments_.size()); ++i)
                result[i] = (*rates_[i])(market) - marketRates_[i];
            return result;
        }
    };

    CrossCurrencyMarket_::CrossCurrencyMarket_(const Handle_<CurveBlock_>& domesticBlock,
                                               const Handle_<CurveBlock_>& foreignBlock,
                                               double fxSpot)
        : domesticBlock_(domesticBlock), foreignBlock_(foreignBlock), fxSpot_(fxSpot) {
        REQUIRE(domesticBlock_, "CrossCurrencyMarket_ requires a non-empty domestic curve block");
        REQUIRE(foreignBlock_, "CrossCurrencyMarket_ requires a non-empty foreign curve block");
        REQUIRE(fxSpot_ > 0.0, "CrossCurrencyMarket_ requires a positive FX spot");
        domesticCcy_ = domesticBlock_->ccy_;
        foreignCcy_ = foreignBlock_->ccy_;
        REQUIRE(!(domesticCcy_ == foreignCcy_), "CrossCurrencyMarket_ requires distinct domestic and foreign currencies");
    }

    Date_ CrossCurrencyMarket_::Today() const { return Global::Dates_::EvaluationDate(); }

    const DiscountCurve_& CrossCurrencyMarket_::DomesticDiscountCurve(const CollateralType_& collateral) const {
        return domesticBlock_->Discount(collateral);
    }

    const DiscountCurve_& CrossCurrencyMarket_::ForeignDiscountCurve(const CollateralType_& collateral) const {
        return foreignBlock_->Discount(collateral);
    }

    const DiscountCurve_& CrossCurrencyMarket_::DomesticForwardCurve(const PeriodLength_& tenor,
                                                                     const CollateralType_& collateral) const {
        return domesticBlock_->Forward(tenor, collateral);
    }

    const DiscountCurve_& CrossCurrencyMarket_::ForeignForwardCurve(const PeriodLength_& tenor,
                                                                    const CollateralType_& collateral) const {
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
        REQUIRE(basisCurve->ccy_ == domesticCcy_,
                "Cross-currency basis curve currency must match the market's domestic currency");
        basisCurve_ = basisCurve;
    }

    CrossCurrencyCalibrationResult_ CalibrateCrossCurrencyMarket(const CrossCurrencyCalibrationSpec_& spec) {
        REQUIRE(spec.today_.IsValid(), "Cross-currency calibration requires a valid today date");
        auto evalDateScope = XGLOBAL::SetEvaluationDateInScope(spec.today_);
        REQUIRE(!spec.instruments_.empty(), "Cross-currency calibration requires at least one instrument");
        REQUIRE(!spec.knotDates_.empty(), "Cross-currency calibration requires at least one basis knot date");
        REQUIRE(spec.smoothingWeight_ > 0.0, "Cross-currency calibration smoothing weight must be positive");
        REQUIRE(spec.tolerance_ > 0.0, "Cross-currency calibration tolerance must be positive");
        REQUIRE(spec.fitTolerance_ > 0.0, "Cross-currency calibration fit tolerance must be positive");
        REQUIRE(spec.maxEvaluations_ > 0, "Cross-currency calibration max evaluations must be positive");
        REQUIRE(spec.maxRestarts_ > 0, "Cross-currency calibration max restarts must be positive");
        REQUIRE(std::isfinite(spec.initialGuess_), "Cross-currency calibration initial guess must be finite");
        ValidateCalibrationSpec(spec);

        REQUIRE(spec.knotDates_.front() > spec.today_, "Cross-currency calibration basis knot dates must be after the evaluation date");
        for (int i = 1; i < static_cast<int>(spec.knotDates_.size()); ++i)
            REQUIRE(spec.knotDates_[i] > spec.knotDates_[i - 1], "Cross-currency calibration basis knot dates must be strictly increasing");

        const int nKnots = static_cast<int>(spec.knotDates_.size());
        const int nInstruments = static_cast<int>(spec.instruments_.size());

        Vector_<> guess(nKnots, spec.initialGuess_);
        Vector_<> tol(nInstruments, spec.tolerance_);

        Vector_<DateTime_> knotDateTimes;
        knotDateTimes.reserve(nKnots);
        for (const auto& d : spec.knotDates_)
            knotDateTimes.push_back(DateTime_(d));
        std::unique_ptr<Sparse::TriDiagonal_> weights(Underdetermined::WeightsPWC(knotDateTimes, spec.smoothingWeight_));

        constexpr const char* KEY_MAX_EVALUATIONS = "MAXEVALUATIONS";
        constexpr const char* KEY_MAX_RESTARTS = "MAXRESTARTS";
        Dictionary_ ctrlDict;
        ctrlDict.Insert(KEY_MAX_EVALUATIONS, Cell_(static_cast<double>(spec.maxEvaluations_)));
        ctrlDict.Insert(KEY_MAX_RESTARTS, Cell_(static_cast<double>(spec.maxRestarts_)));
        UnderdeterminedControls_ controls(ctrlDict);

        XccyCalibrationFunc_ func(spec.domesticCurveBlock_, spec.foreignCurveBlock_, spec.fxSpot_, spec.instruments_, spec.knotDates_);

        Vector_<> result;
        Matrix_<> effJacobianInverse;
        const bool useApproximate = spec.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE;
        if (!useApproximate) {
            std::unique_ptr<Sparse::SymmetricDecomposition_> wDecomp(weights->DecomposeSymmetric());
            result = Underdetermined::Find(func, guess, tol, *wDecomp, controls, &effJacobianInverse);
        } else {
            result = Underdetermined::Approximate(func, guess, tol, spec.fitTolerance_, *weights, controls);
        }

        auto basisCurve = BuildBasisCurve(spec.domesticCurveBlock_->ccy_.String(), spec.knotDates_, result);

        CrossCurrencyMarket_ market(spec.domesticCurveBlock_, spec.foreignCurveBlock_, spec.fxSpot_);
        market.SetBasisCurve(basisCurve);

        std::map<CurrencyPair_, Handle_<DiscountCurve_>> basisCurves;
        basisCurves[spec.basisPair_] = basisCurve;

        CrossCurrencyFxForwardCurve_ fxForwardCurve = BuildFxForwardCurve(spec.basisPair_, spec.knotDates_, market, spec.fxForwardCollateral_);
        CrossCurrencyCalibrationDiagnostics_ diagnostics = BuildDiagnostics(
            spec.instruments_, market, useApproximate, useApproximate ? nullptr : &effJacobianInverse);

        return CrossCurrencyCalibrationResult_(market, basisCurves, fxForwardCurve, diagnostics);
    }
} // namespace Dal
