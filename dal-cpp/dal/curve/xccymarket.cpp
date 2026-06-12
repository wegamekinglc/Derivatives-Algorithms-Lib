//
// Created by GitHub Copilot on 2026/6/6.
//

#include <algorithm>
#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/xccymarket.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/optimization/underdeterminedutils.hpp>
#include <dal/protocol/accrualperiod.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/datetime.hpp>
#include <dal/time/schedules.hpp>
#include <dal/utilities/dictionary.hpp>

namespace Dal {
    namespace {
        struct XccyCouponPeriod_ {
            SchedulePeriod_ schedule_;
            AccrualPeriod_ accrual_;
        };

        AccrualPeriod_ MakeAccrualPeriod(const SchedulePeriod_& period, const DayBasis_& basis) {
            return AccrualPeriod_(period.accrualStart_, period.accrualEnd_, 1.0, basis, period.dayCountContext_, period.isStub_);
        }

        Vector_<XccyCouponPeriod_> BuildLegPeriods(const Date_& start,
                                                   const Date_& maturity,
                                                   const RateLegConvention_& legConvention,
                                                   int fixingLag,
                                                   const Holidays_& fixingHolidays) {
            Vector_<XccyCouponPeriod_> retval;
            for (const auto& period : MakeSchedulePeriods(start,
                                                          maturity,
                                                          legConvention.paymentFrequency_,
                                                          legConvention.accrualHolidays_,
                                                          fixingLag,
                                                          fixingHolidays,
                                                          legConvention.paymentLag_,
                                                          legConvention.paymentHolidays_,
                                                          DateGeneration_("Forward"),
                                                          legConvention.businessDayConvention_,
                                                          legConvention.paymentConvention_,
                                                          legConvention.endOfMonth_)) {
                retval.push_back({period, MakeAccrualPeriod(period, legConvention.dayBasis_)});
            }
            return retval;
        }

        double ForwardRate(const DiscountCurve_& forecast,
                           const XccyCouponPeriod_& period,
                           const DayBasis_& basis) {
            const double df = forecast(period.schedule_.accrualStart_, period.schedule_.accrualEnd_);
            REQUIRE(df > 0.0, "Cross-currency forward-rate calculation requires positive discount factors");
            return (1.0 / df - 1.0) / basis(period.schedule_.accrualStart_,
                                            period.schedule_.accrualEnd_,
                                            period.schedule_.dayCountContext_.get());
        }

        double CouponPv(const Vector_<XccyCouponPeriod_>& periods,
                        const DiscountCurve_& discount,
                        const DiscountCurve_& forecast,
                        const Date_& valueDate,
                        double notional,
                        const RateIndexConvention_& indexConvention,
                        double spread) {
            double retval = 0.0;
            for (const auto& period : periods) {
                const double fixing = ForwardRate(forecast, period, indexConvention.dayBasis_) + spread;
                retval += notional * fixing * period.accrual_.dcf_ * discount(valueDate, period.schedule_.paymentDate_);
            }
            return retval;
        }

        double ConvertedCouponPv(const Vector_<XccyCouponPeriod_>& periods,
                                 const CrossCurrencyMarket_& market,
                                 const DiscountCurve_& discount,
                                 const DiscountCurve_& forecast,
                                 const Date_& valueDate,
                                 double notional,
                                 const RateIndexConvention_& indexConvention,
                                 double spread) {
            double retval = 0.0;
            for (const auto& period : periods) {
                const double fixing = ForwardRate(forecast, period, indexConvention.dayBasis_) + spread;
                const double discountFactor = discount(valueDate, period.schedule_.paymentDate_)
                                              / market.BasisDiscountFactor(valueDate, period.schedule_.paymentDate_);
                retval += notional * fixing * period.accrual_.dcf_ * discountFactor * market.FxSpot();
            }
            return retval;
        }

        double ConvertedAnnuity(const Vector_<XccyCouponPeriod_>& periods,
                                const CrossCurrencyMarket_& market,
                                const DiscountCurve_& discount,
                                const Date_& valueDate,
                                double notional) {
            double retval = 0.0;
            for (const auto& period : periods) {
                const double discountFactor = discount(valueDate, period.schedule_.paymentDate_)
                                              / market.BasisDiscountFactor(valueDate, period.schedule_.paymentDate_);
                retval += notional * period.accrual_.dcf_ * discountFactor * market.FxSpot();
            }
            return retval;
        }

        double DomesticAnnuity(const Vector_<XccyCouponPeriod_>& periods,
                               const DiscountCurve_& discount,
                               const Date_& valueDate) {
            double retval = 0.0;
            for (const auto& period : periods) {
                retval += period.accrual_.dcf_ * discount(valueDate, period.schedule_.paymentDate_);
            }
            return retval;
        }

        class CrossCurrencySwapRate_ : public CrossCurrencySwap_::Rate_ {
            Date_ maturity_;
            CurrencyPair_ pair_;
            double domesticNotional_;
            double foreignNotional_;
            Vector_<XccyCouponPeriod_> domesticPeriods_;
            Vector_<XccyCouponPeriod_> foreignPeriods_;
            RateIndexConvention_ domesticIndexConvention_;
            RateIndexConvention_ foreignIndexConvention_;
            CrossCurrencyConvention_ convention_;

        public:
            CrossCurrencySwapRate_(const Date_& maturity,
                                   const CurrencyPair_& pair,
                                   double domesticNotional,
                                   double foreignNotional,
                                   const Vector_<XccyCouponPeriod_>& domesticPeriods,
                                   const Vector_<XccyCouponPeriod_>& foreignPeriods,
                                   const RateIndexConvention_& domesticIndexConvention,
                                   const RateIndexConvention_& foreignIndexConvention,
                                   const CrossCurrencyConvention_& convention)
                : maturity_(maturity),
                  pair_(pair),
                  domesticNotional_(domesticNotional),
                  foreignNotional_(foreignNotional),
                  domesticPeriods_(domesticPeriods),
                  foreignPeriods_(foreignPeriods),
                  domesticIndexConvention_(domesticIndexConvention),
                  foreignIndexConvention_(foreignIndexConvention),
                  convention_(convention) {}

            double operator()(const CrossCurrencyMarket_& market) const override {
                REQUIRE(pair_.domestic_ == market.DomesticCcy() && pair_.foreign_ == market.ForeignCcy(),
                        "Cross-currency swap currency pair does not match the pricing market orientation");
                const Date_ valueDate = market.Today();
                REQUIRE(!domesticPeriods_.empty() && !foreignPeriods_.empty(), "Cross-currency swap requires scheduled periods on both legs");
                REQUIRE(valueDate <= domesticPeriods_.front().schedule_.accrualStart_ &&
                            valueDate <= foreignPeriods_.front().schedule_.accrualStart_,
                        "Cross-currency swap pricing of in-progress swaps (evaluation date after swap start) is not supported");
                const DiscountCurve_& domesticDiscount = market.DomesticDiscountCurve(domesticIndexConvention_.collateral_);
                const DiscountCurve_& domesticForecast = market.DomesticForwardCurve(domesticIndexConvention_.forecastTenor_,
                                                                                     domesticIndexConvention_.collateral_);
                const DiscountCurve_& foreignForecast = market.ForeignForwardCurve(foreignIndexConvention_.forecastTenor_,
                                                                                   foreignIndexConvention_.collateral_);
                const DiscountCurve_& foreignDiscount = market.ForeignDiscountCurve(foreignIndexConvention_.collateral_);

                const double domesticBase = CouponPv(domesticPeriods_,
                                                     domesticDiscount,
                                                     domesticForecast,
                                                     valueDate,
                                                     domesticNotional_,
                                                     domesticIndexConvention_,
                                                     0.0);
                const double domesticSpreadAnnuity = domesticNotional_ * DomesticAnnuity(domesticPeriods_, domesticDiscount, valueDate);
                const double foreignBase = ConvertedCouponPv(foreignPeriods_,
                                                             market,
                                                             foreignDiscount,
                                                             foreignForecast,
                                                             valueDate,
                                                             foreignNotional_,
                                                             foreignIndexConvention_,
                                                             0.0);
                const double foreignSpreadAnnuity = ConvertedAnnuity(foreignPeriods_,
                                                                     market,
                                                                     foreignDiscount,
                                                                     valueDate,
                                                                     foreignNotional_);

                double domesticPv = domesticBase;
                double foreignPv = foreignBase;
                if (convention_.initialNotionalExchange_) {
                    domesticPv -= domesticNotional_;
                    foreignPv -= foreignNotional_ * market.FxSpot();
                }
                if (convention_.finalNotionalExchange_) {
                    domesticPv += domesticNotional_ * domesticDiscount(valueDate, maturity_);
                    foreignPv += foreignNotional_ * foreignDiscount(valueDate, maturity_)
                                 / market.BasisDiscountFactor(valueDate, maturity_) * market.FxSpot();
                }

                if (convention_.spreadOnForeignLeg_) {
                    REQUIRE(foreignSpreadAnnuity > 0.0, "Cross-currency swap requires positive foreign spread annuity");
                    return (domesticPv - foreignPv) / foreignSpreadAnnuity;
                }
                REQUIRE(domesticSpreadAnnuity > 0.0, "Cross-currency swap requires positive domestic spread annuity");
                return (foreignPv - domesticPv) / domesticSpreadAnnuity;
            }
        };

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

    CurrencyPair_::CurrencyPair_() : domestic_(Ccy_::Value_::USD), foreign_(Ccy_::Value_::EUR) {}

    CurrencyPair_::CurrencyPair_(const Ccy_& domestic, const Ccy_& foreign) : domestic_(domestic), foreign_(foreign) {
        REQUIRE(!(domestic_ == foreign_), "CurrencyPair_ requires two distinct currencies");
    }

    CurrencyPair_ CurrencyPair_::Reversed() const { return CurrencyPair_(foreign_, domestic_); }

    bool CurrencyPair_::operator<(const CurrencyPair_& rhs) const {
        if (domestic_ < rhs.domestic_ || rhs.domestic_ < domestic_)
            return domestic_ < rhs.domestic_;
        return foreign_ < rhs.foreign_;
    }

    bool CurrencyPair_::operator==(const CurrencyPair_& rhs) const {
        return domestic_ == rhs.domestic_ && foreign_ == rhs.foreign_;
    }

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
        basisCurve_ = basisCurve;
    }

    CrossCurrencySwap_::CrossCurrencySwap_(const Date_& tradeDate,
                                           const Date_& start,
                                           const Date_& maturity,
                                           double marketRate,
                                           const CurrencyPair_& pair,
                                           double domesticNotional,
                                           double foreignNotional,
                                           const CrossCurrencyConvention_& convention)
        : tradeDate_(tradeDate),
          start_(start),
          maturity_(maturity),
          marketRate_(marketRate),
          pair_(pair),
          domesticNotional_(domesticNotional),
          foreignNotional_(foreignNotional),
          convention_(convention) {
        REQUIRE(domesticNotional_ > 0.0 && foreignNotional_ > 0.0, "CrossCurrencySwap_ requires positive notionals");
        REQUIRE(maturity_ > start_, "CrossCurrencySwap_ requires maturity after start");
    }

    String_ CrossCurrencySwap_::Name() const { return "CrossCurrencySwap"; }

    pair<Date_, Date_> CrossCurrencySwap_::TimeSpan() const { return {start_, maturity_}; }

    Handle_<CrossCurrencySwap_::Rate_> CrossCurrencySwap_::Precompute() const {
        REQUIRE(!convention_.resettableNotional_, "Resettable cross-currency notionals are not implemented");
        REQUIRE(!convention_.markToMarketNotional_, "Mark-to-market cross-currency notionals are not implemented");
        const auto domesticPeriods = BuildLegPeriods(start_,
                                                     maturity_,
                                                     convention_.domesticLeg_,
                                                     convention_.domesticIndex_.fixingLag_,
                                                     convention_.domesticIndex_.fixingHolidays_);
        const auto foreignPeriods = BuildLegPeriods(start_,
                                                    maturity_,
                                                    convention_.foreignLeg_,
                                                    convention_.foreignIndex_.fixingLag_,
                                                    convention_.foreignIndex_.fixingHolidays_);
        return Handle_<Rate_>(new CrossCurrencySwapRate_(maturity_,
                                                         pair_,
                                                         domesticNotional_,
                                                         foreignNotional_,
                                                         domesticPeriods,
                                                         foreignPeriods,
                                                         convention_.domesticIndex_,
                                                         convention_.foreignIndex_,
                                                         convention_));
    }

    CrossCurrencyCalibrationResult_ CalibrateCrossCurrencyMarket(const CrossCurrencyCalibrationSpec_& spec) {
        REQUIRE(!spec.instruments_.empty(), "Cross-currency calibration requires at least one instrument");
        REQUIRE(!spec.knotDates_.empty(), "Cross-currency calibration requires at least one basis knot date");
        REQUIRE(spec.smoothingWeight_ > 0.0, "Cross-currency calibration smoothing weight must be positive");
        REQUIRE(spec.tolerance_ > 0.0, "Cross-currency calibration tolerance must be positive");
        REQUIRE(spec.fitTolerance_ > 0.0, "Cross-currency calibration fit tolerance must be positive");
        REQUIRE(spec.maxEvaluations_ > 0, "Cross-currency calibration max evaluations must be positive");
        REQUIRE(spec.maxRestarts_ > 0, "Cross-currency calibration max restarts must be positive");
        REQUIRE(std::isfinite(spec.initialGuess_), "Cross-currency calibration initial guess must be finite");
        ValidateCalibrationSpec(spec);

        const Date_ evalDate = Global::Dates_::EvaluationDate();
        REQUIRE(spec.knotDates_.front() > evalDate, "Cross-currency calibration basis knot dates must be after the evaluation date");
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
