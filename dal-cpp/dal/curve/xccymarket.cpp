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
#include <dal/protocol/accrualperiod.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/schedules.hpp>

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
                        const Date_& tradeDate,
                        double notional,
                        const RateIndexConvention_& indexConvention,
                        double spread) {
            double retval = 0.0;
            for (const auto& period : periods) {
                const double fixing = ForwardRate(forecast, period, indexConvention.dayBasis_) + spread;
                retval += notional * fixing * period.accrual_.dcf_ * discount(tradeDate, period.schedule_.paymentDate_);
            }
            return retval;
        }

        double ConvertedCouponPv(const Vector_<XccyCouponPeriod_>& periods,
                                 const CrossCurrencyMarket_& market,
                                 const CurrencyPair_& pair,
                                 const DiscountCurve_& discount,
                                 const DiscountCurve_& forecast,
                                 const Date_& tradeDate,
                                 double notional,
                                 const RateIndexConvention_& indexConvention,
                                 double spread) {
            double retval = 0.0;
            for (const auto& period : periods) {
                const double fixing = ForwardRate(forecast, period, indexConvention.dayBasis_) + spread;
                const double discountFactor = discount(tradeDate, period.schedule_.paymentDate_)
                                              / market.BasisDiscountFactor(pair, tradeDate, period.schedule_.paymentDate_);
                retval += notional * fixing * period.accrual_.dcf_ * discountFactor * market.FxSpot(pair);
            }
            return retval;
        }

        double ConvertedAnnuity(const Vector_<XccyCouponPeriod_>& periods,
                                const CrossCurrencyMarket_& market,
                                const CurrencyPair_& pair,
                                const DiscountCurve_& discount,
                                const Date_& tradeDate,
                                double notional) {
            double retval = 0.0;
            for (const auto& period : periods) {
                const double discountFactor = discount(tradeDate, period.schedule_.paymentDate_)
                                              / market.BasisDiscountFactor(pair, tradeDate, period.schedule_.paymentDate_);
                retval += notional * period.accrual_.dcf_ * discountFactor * market.FxSpot(pair);
            }
            return retval;
        }

        double DomesticAnnuity(const Vector_<XccyCouponPeriod_>& periods,
                               const DiscountCurve_& discount,
                               const Date_& tradeDate) {
            double retval = 0.0;
            for (const auto& period : periods) {
                retval += period.accrual_.dcf_ * discount(tradeDate, period.schedule_.paymentDate_);
            }
            return retval;
        }

        class CrossCurrencySwapRate_ : public CrossCurrencySwap_::Rate_ {
            Date_ tradeDate_;
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
            CrossCurrencySwapRate_(const Date_& tradeDate,
                                   const Date_& maturity,
                                   const CurrencyPair_& pair,
                                   double domesticNotional,
                                   double foreignNotional,
                                   const Vector_<XccyCouponPeriod_>& domesticPeriods,
                                   const Vector_<XccyCouponPeriod_>& foreignPeriods,
                                   const RateIndexConvention_& domesticIndexConvention,
                                   const RateIndexConvention_& foreignIndexConvention,
                                   const CrossCurrencyConvention_& convention)
                : tradeDate_(tradeDate),
                  maturity_(maturity),
                  pair_(pair),
                  domesticNotional_(domesticNotional),
                  foreignNotional_(foreignNotional),
                  domesticPeriods_(domesticPeriods),
                  foreignPeriods_(foreignPeriods),
                  domesticIndexConvention_(domesticIndexConvention),
                  foreignIndexConvention_(foreignIndexConvention),
                  convention_(convention) {}

            double operator()(const CrossCurrencyMarket_& market) const override {
                const DiscountCurve_& domesticDiscount = market.DomesticDiscountCurve(pair_, domesticIndexConvention_.collateral_);
                const DiscountCurve_& domesticForecast = market.ForwardCurve(pair_.domestic_,
                                                                             domesticIndexConvention_.forecastTenor_,
                                                                             domesticIndexConvention_.collateral_);
                const DiscountCurve_& foreignForecast = market.ForwardCurve(pair_.foreign_,
                                                                            foreignIndexConvention_.forecastTenor_,
                                                                            foreignIndexConvention_.collateral_);
                const DiscountCurve_& foreignDiscount = market.ForeignDiscountCurve(pair_, foreignIndexConvention_.collateral_);

                const double domesticBase = CouponPv(domesticPeriods_,
                                                     domesticDiscount,
                                                     domesticForecast,
                                                     tradeDate_,
                                                     domesticNotional_,
                                                     domesticIndexConvention_,
                                                     0.0);
                const double domesticSpreadAnnuity = domesticNotional_ * DomesticAnnuity(domesticPeriods_, domesticDiscount, tradeDate_);
                const double foreignBase = ConvertedCouponPv(foreignPeriods_,
                                                            market,
                                                            pair_,
                                                            foreignDiscount,
                                                            foreignForecast,
                                                            tradeDate_,
                                                            foreignNotional_,
                                                            foreignIndexConvention_,
                                                            0.0);
                const double foreignSpreadAnnuity = ConvertedAnnuity(foreignPeriods_,
                                                                    market,
                                                                    pair_,
                                                                    foreignDiscount,
                                                                    tradeDate_,
                                                                    foreignNotional_);

                double domesticPv = domesticBase;
                double foreignPv = foreignBase;
                if (convention_.initialNotionalExchange_) {
                    domesticPv -= domesticNotional_;
                    foreignPv -= foreignNotional_ * market.FxSpot(pair_);
                }
                if (convention_.finalNotionalExchange_) {
                    domesticPv += domesticNotional_ * domesticDiscount(tradeDate_, maturity_);
                    foreignPv += foreignNotional_ * foreignDiscount(tradeDate_, maturity_)
                                 / market.BasisDiscountFactor(pair_, tradeDate_, maturity_) * market.FxSpot(pair_);
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
                                                             const CrossCurrencyMarket_& market) {
            CrossCurrencyCalibrationDiagnostics_ retval;
            double maxResidual = 0.0;
            for (const auto& instrument : instruments) {
                const double modelRate = (*instrument->Precompute())(market);
                const double marketRate = instrument->MarketRate();
                const double residual = modelRate - marketRate;
                retval.instrumentNames_.push_back(instrument->Name());
                retval.marketRates_.push_back(marketRate);
                retval.modelRates_.push_back(modelRate);
                retval.residuals_.push_back(residual);
                maxResidual = std::max(maxResidual, std::fabs(residual));
            }
            retval.maxAbsResidual_ = maxResidual;
            return retval;
        }

        Handle_<DiscountCurve_> FlatBasisCurve(const CurrencyPair_& pair,
                                               const Vector_<Date_>& knotDates,
                                               double rate) {
            Vector_<> vals(knotDates.size(), rate);
            return Handle_<DiscountCurve_>(
                NewDiscountPWC(String_("xccy_basis_") + pair.domestic_.String() + "_" + pair.foreign_.String(),
                               pair.domestic_.String(),
                               PiecewiseConstant_(knotDates, vals)));
        }

        CrossCurrencyMarket_ MakeCalibrationMarket(const CrossCurrencyCalibrationSpec_& spec) {
            REQUIRE(spec.domesticCurveBlock_, "Cross-currency calibration requires a domestic curve block");
            REQUIRE(spec.foreignCurveBlock_, "Cross-currency calibration requires a foreign curve block");
            REQUIRE(spec.fxSpot_ > 0.0, "Cross-currency calibration requires a positive FX spot");
            CrossCurrencyMarket_ retval(spec.today_);
            retval.SetCurveBlock(spec.basisPair_.domestic_, spec.domesticCurveBlock_);
            retval.SetCurveBlock(spec.basisPair_.foreign_, spec.foreignCurveBlock_);
            retval.SetFxSpot(spec.basisPair_, spec.fxSpot_);
            return retval;
        }

        CrossCurrencyFxForwardCurve_ BuildFxForwardCurve(const CurrencyPair_& pair,
                                                         const Vector_<Date_>& dates,
                                                         const CrossCurrencyMarket_& market,
                                                         const CollateralType_& collateral) {
            CrossCurrencyFxForwardCurve_ retval;
            retval.pair_ = pair;
            retval.dates_ = dates;
            for (const auto& date : dates) {
                retval.forwards_.push_back(market.FxForward(pair, market.Today(), date, collateral));
            }
            return retval;
        }
    } // namespace

    CurrencyPair_::CurrencyPair_() : domestic_(Ccy_::Value_::USD), foreign_(Ccy_::Value_::EUR) {}

    CurrencyPair_::CurrencyPair_(const Ccy_& domestic, const Ccy_& foreign) : domestic_(domestic), foreign_(foreign) {
        REQUIRE(!(domestic_ == foreign_), "CurrencyPair_ requires two distinct currencies");
    }

    CurrencyPair_ CurrencyPair_::Reversed() const { return CurrencyPair_(foreign_, domestic_); }

    bool CurrencyPair_::operator<(const CurrencyPair_& rhs) const {
        if (domestic_.String() != rhs.domestic_.String())
            return domestic_.String() < rhs.domestic_.String();
        return foreign_.String() < rhs.foreign_.String();
    }

    bool CurrencyPair_::operator==(const CurrencyPair_& rhs) const {
        return domestic_ == rhs.domestic_ && foreign_ == rhs.foreign_;
    }

    CrossCurrencyMarket_::CrossCurrencyMarket_(const Date_& today) : today_(today) {}

    void CrossCurrencyMarket_::SetCurveBlock(const Ccy_& ccy, const Handle_<CurveBlock_>& curveBlock) {
        REQUIRE(curveBlock, "CrossCurrencyMarket_ requires non-empty curve-block handles");
        curveBlocks_[ccy] = curveBlock;
    }

    const CurveBlock_& CrossCurrencyMarket_::CurveBlock(const Ccy_& ccy) const {
        const auto found = curveBlocks_.find(ccy);
        REQUIRE(found != curveBlocks_.end(), "CrossCurrencyMarket_ does not contain the requested currency curve block");
        return *found->second;
    }

    const DiscountCurve_& CrossCurrencyMarket_::DomesticDiscountCurve(const CurrencyPair_& pair,
                                                                      const CollateralType_& collateral) const {
        return CurveBlock(pair.domestic_).Discount(collateral);
    }

    const DiscountCurve_& CrossCurrencyMarket_::ForeignDiscountCurve(const CurrencyPair_& pair,
                                                                     const CollateralType_& collateral) const {
        return CurveBlock(pair.foreign_).Discount(collateral);
    }

    const DiscountCurve_& CrossCurrencyMarket_::ForwardCurve(const Ccy_& ccy,
                                                             const PeriodLength_& tenor,
                                                             const CollateralType_& collateral) const {
        return CurveBlock(ccy).Forward(tenor, collateral);
    }

    void CrossCurrencyMarket_::SetFxSpot(const CurrencyPair_& pair, double spot) {
        REQUIRE(spot > 0.0, "CrossCurrencyMarket_ requires positive FX spots");
        fxSpots_[pair] = spot;
    }

    void CrossCurrencyMarket_::SetBasisCurve(const CurrencyPair_& pair, const Handle_<DiscountCurve_>& basisCurve) {
        REQUIRE(basisCurve, "CrossCurrencyMarket_ requires non-empty basis-curve handles");
        basisCurves_[pair] = basisCurve;
    }

    double CrossCurrencyMarket_::FxSpot(const CurrencyPair_& pair) const {
        const auto found = fxSpots_.find(pair);
        if (found != fxSpots_.end())
            return found->second;
        const auto reverse = fxSpots_.find(pair.Reversed());
        REQUIRE(reverse != fxSpots_.end(), "CrossCurrencyMarket_ does not contain the requested FX spot");
        return 1.0 / reverse->second;
    }

    double CrossCurrencyMarket_::BasisDiscountFactor(const CurrencyPair_& pair, const Date_& from, const Date_& to) const {
        const auto basis = basisCurves_.find(pair);
        if (basis != basisCurves_.end()) {
            const double retval = (*basis->second)(from, to);
            REQUIRE(retval > 0.0, "Cross-currency basis discount factors must be positive");
            return retval;
        }
        const auto reverseBasis = basisCurves_.find(pair.Reversed());
        if (reverseBasis != basisCurves_.end()) {
            const double retval = (*reverseBasis->second)(from, to);
            REQUIRE(retval > 0.0, "Cross-currency basis discount factors must be positive");
            return 1.0 / retval;
        }
        return 1.0;
    }

    double CrossCurrencyMarket_::FxForward(const CurrencyPair_& pair, const Date_& maturity) const {
        return FxForward(pair, today_, maturity, CollateralType_(CollateralType_::Value_::OIS));
    }

    double CrossCurrencyMarket_::FxForward(const CurrencyPair_& pair,
                                           const Date_& from,
                                           const Date_& maturity,
                                           const CollateralType_& collateral) const {
        const double domesticDf = DomesticDiscountCurve(pair, collateral)(from, maturity);
        const double foreignDf = ForeignDiscountCurve(pair, collateral)(from, maturity);
        const double basisDf = BasisDiscountFactor(pair, from, maturity);
        REQUIRE(domesticDf > 0.0 && foreignDf > 0.0, "FX forward parity requires positive discount factors");
        return FxSpot(pair) * foreignDf / (domesticDf * basisDf);
    }

    CrossCurrencySwap_::CrossCurrencySwap_(const Date_& tradeDate,
                                           const Date_& start,
                                           const Date_& maturity,
                                           double marketRate,
                                           const CurrencyPair_& pair,
                                           double domesticNotional,
                                           double foreignNotional,
                                           const RateIndexConvention_& domesticIndexConvention,
                                           const RateLegConvention_& domesticLegConvention,
                                           const RateIndexConvention_& foreignIndexConvention,
                                           const RateLegConvention_& foreignLegConvention,
                                           const CrossCurrencyConvention_& convention)
        : tradeDate_(tradeDate),
          start_(start),
          maturity_(maturity),
          marketRate_(marketRate),
          pair_(pair),
          domesticNotional_(domesticNotional),
          foreignNotional_(foreignNotional),
          domesticIndexConvention_(domesticIndexConvention),
          domesticLegConvention_(domesticLegConvention),
          foreignIndexConvention_(foreignIndexConvention),
          foreignLegConvention_(foreignLegConvention),
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
                                                     domesticLegConvention_,
                                                     domesticIndexConvention_.fixingLag_,
                                                     domesticIndexConvention_.fixingHolidays_);
        const auto foreignPeriods = BuildLegPeriods(start_,
                                                    maturity_,
                                                    foreignLegConvention_,
                                                    foreignIndexConvention_.fixingLag_,
                                                    foreignIndexConvention_.fixingHolidays_);
        return Handle_<Rate_>(new CrossCurrencySwapRate_(tradeDate_,
                                                         maturity_,
                                                         pair_,
                                                         domesticNotional_,
                                                         foreignNotional_,
                                                         domesticPeriods,
                                                         foreignPeriods,
                                                         domesticIndexConvention_,
                                                         foreignIndexConvention_,
                                                         convention_));
    }

    CrossCurrencyCalibrationResult_ CalibrateCrossCurrencyMarket(const CrossCurrencyCalibrationSpec_& spec) {
        REQUIRE(!spec.instruments_.empty(), "Cross-currency calibration requires at least one instrument");
        REQUIRE(spec.instruments_.size() == 1,
                "Cross-currency calibration solves a single flat basis spread and supports exactly one instrument");
        REQUIRE(!spec.knotDates_.empty(), "Cross-currency calibration requires at least one basis knot date");

        CrossCurrencyCalibrationResult_ retval;
        retval.market_ = MakeCalibrationMarket(spec);

        int evaluationsUsed = 0;
        auto residualAt = [&](double rate) {
            ++evaluationsUsed;
            retval.market_.SetBasisCurve(spec.basisPair_, FlatBasisCurve(spec.basisPair_, spec.knotDates_, rate));
            const auto diagnostics = BuildDiagnostics(spec.instruments_, retval.market_);
            return diagnostics.residuals_.front();
        };

        constexpr double INITIAL_BRACKET_LO = -0.20;
        constexpr double INITIAL_BRACKET_HI = 0.20;
        constexpr int MAX_BRACKET_EXPANSIONS = 50;

        double lo = INITIAL_BRACKET_LO;
        double hi = INITIAL_BRACKET_HI;
        double fLo = residualAt(lo);
        double fHi = residualAt(hi);
        for (int i = 0; i < MAX_BRACKET_EXPANSIONS && evaluationsUsed < spec.maxEvaluations_ && fLo * fHi > 0.0; ++i) {
            if (std::isnan(fLo) || std::isinf(fLo) || std::isnan(fHi) || std::isinf(fHi))
                THROW("Cross-currency calibration encountered NaN/Inf during bracket expansion");
            lo *= 2.0;
            hi *= 2.0;
            fLo = residualAt(lo);
            fHi = residualAt(hi);
        }
        REQUIRE(fLo * fHi <= 0.0, "Cross-currency calibration could not bracket the basis spread");
        REQUIRE(evaluationsUsed < spec.maxEvaluations_,
                "Cross-currency calibration exhausted its evaluation budget before bisection");

        double mid = 0.0;
        bool converged = false;
        while (evaluationsUsed < spec.maxEvaluations_) {
            mid = 0.5 * (lo + hi);
            const double fMid = residualAt(mid);
            if (std::isnan(fMid) || std::isinf(fMid))
                THROW("Cross-currency calibration encountered NaN/Inf during bisection");
            if (std::fabs(fMid) <= spec.tolerance_) {
                converged = true;
                break;
            }
            if (fLo * fMid <= 0.0) {
                hi = mid;
                fHi = fMid;
            } else {
                lo = mid;
                fLo = fMid;
            }
        }
        REQUIRE(converged, "Cross-currency calibration did not converge within the evaluation budget");

        auto basisCurve = FlatBasisCurve(spec.basisPair_, spec.knotDates_, mid);
        retval.market_.SetBasisCurve(spec.basisPair_, basisCurve);
        retval.basisCurves_[spec.basisPair_] = basisCurve;
        retval.fxForwardCurve_ = BuildFxForwardCurve(spec.basisPair_, spec.knotDates_, retval.market_, spec.fxForwardCollateral_);
        retval.diagnostics_ = BuildDiagnostics(spec.instruments_, retval.market_);
        return retval;
    }
} // namespace Dal
