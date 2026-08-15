//
// Created by dal-implementer on 2026/7/28.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <algorithm>
#include <cmath>
#include <type_traits>

#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/jointrate.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/time/dateincrement.hpp>

namespace Dal {
#include <dal/auto/MG_RateInstrumentType_enum.inc>

    namespace {
        static_assert(std::variant_size_v<RateTradeTerms_> == 7);

        struct CouponPeriod_ {
            SchedulePeriod_ schedule_;
            AccrualPeriod_ accrual_;
        };

        bool ValidFixingIdentity(const FixingIdentity_& identity) {
            return !identity.indexName_.empty() && identity.fixingHour_ >= 0 && identity.fixingHour_ < 24 && identity.fixingMinute_ >= 0 &&
                   identity.fixingMinute_ < 60;
        }

        Date_ FixingDate(const Date_& accrualStart, const RateIndexConvention_& index) {
            if (index.fixingLag_ == 0)
                return accrualStart;
            REQUIRE(index.fixingLag_ > 0, "Rate pricing requires a non-negative fixing lag");
            return Date::NBusDays(index.fixingLag_, index.fixingHolidays_)->BackFrom(accrualStart);
        }

        DateTime_ FixingTime(const Date_& accrualStart, const RateIndexConvention_& index, const FixingIdentity_& identity) {
            REQUIRE(ValidFixingIdentity(identity), "Floating rate pricing requires an explicit valid fixing identity");
            return DateTime_(FixingDate(accrualStart, index), identity.fixingHour_, identity.fixingMinute_);
        }

        const DiscountCurve_& Curve(const RatePricingMarket_& market, const String_& key) {
            const auto found = market.curveComponents_.find(key);
            REQUIRE(found != market.curveComponents_.end() && found->second, "Rate pricing market is missing curve component " + key);
            return *found->second;
        }

        double Discount(const DiscountCurve_& curve, const DateTime_& valuationTime, const Date_& paymentDate) {
            return Tape::DiscountFromValuation(curve, valuationTime, paymentDate, "Rate pricing requires positive finite discount factors");
        }

        void AddUnique(const String_& value, Vector_<String_>* values) {
            if (std::find(values->begin(), values->end(), value) == values->end())
                values->push_back(value);
        }

        bool SameFixing(const FixingRequest_& lhs, const FixingRequest_& rhs) {
            return lhs.indexName_ == rhs.indexName_ && lhs.fixingTime_ == rhs.fixingTime_;
        }

        void AddUnique(const FixingRequest_& value, Vector_<FixingRequest_>* values) {
            if (std::find_if(values->begin(), values->end(), [&](const auto& item) { return SameFixing(item, value); }) == values->end())
                values->push_back(value);
        }

        double ForwardRate(
            const DiscountCurve_& forecast, const Date_& start, const Date_& maturity, const DayBasis_& basis, const DayBasis::Context_* context) {
            const double accrual = basis(start, maturity, context);
            const double df = forecast(start, maturity);
            REQUIRE(std::isfinite(accrual) && accrual > 0.0 && std::isfinite(df) && df > 0.0,
                    "Floating rate pricing requires positive finite accrual and forecast discount factor");
            return Tape::ForwardRateFromDf(df, accrual);
        }

        double ResolveRate(const SchedulePeriod_& period,
                           const RateIndexConvention_& index,
                           const FixingIdentity_& identity,
                           const DiscountCurve_& forecast,
                           const RatePricingMarket_& market,
                           RatePricingTradeResult_* result) {
            const DateTime_ fixingTime = FixingTime(period.accrualStart_, index, identity);
            const FixingRequest_ request{identity.indexName_, fixingTime};
            const auto projected = [&]() {
                return ForwardRate(forecast, period.accrualStart_, period.accrualEnd_, index.dayBasis_, period.dayCountContext_.get());
            };
            if (fixingTime > market.valuationTime_)
                return projected();
            const auto supplied = market.fixings_ ? market.fixings_->Find(identity.indexName_, fixingTime) : std::optional<double>();
            if (fixingTime == market.valuationTime_ && !supplied)
                return projected();
            AddUnique(request, &result->requiredHistoricalFixings_);
            if (!supplied) {
                AddUnique(request, &result->missingHistoricalFixings_);
                THROW("Missing historical fixing " + identity.indexName_);
            }
            REQUIRE(std::isfinite(*supplied), "Historical rate fixing must be finite");
            return *supplied;
        }

        SchedulePeriod_ SinglePeriod(const RateTradeDefinition_& trade, const RateIndexConvention_& index) {
            auto result = BuildSinglePeriodSchedule(trade.startDate_, trade.maturityDate_, index,
                                                    SinglePeriodCouponMonths(index, trade.startDate_, trade.maturityDate_));
            result.fixingDate_ = FixingDate(result.accrualStart_, index);
            result.paymentDate_ = result.accrualEnd_;
            return result;
        }

        template <class Terms_>
        void AddFloatingPlan(const RateTradeDefinition_& trade,
                             const Terms_& terms,
                             const DateTime_& valuationTime,
                             const RateLegConvention_& leg,
                             const RateIndexConvention_& index,
                             const FixingIdentity_& identity,
                             const String_& forecastKey,
                             const String_& discountKey,
                             bool dailyObservations,
                             RateCashflowPlan_* result) {
            AddUnique(forecastKey, &result->dependencyComponentKeys_);
            AddUnique(discountKey, &result->dependencyComponentKeys_);
            for (const auto& period :
                 BuildLegPeriods<CouponPeriod_>(trade.startDate_, trade.maturityDate_, leg, index.fixingLag_, index.fixingHolidays_)) {
                if (period.schedule_.paymentDate_ < valuationTime.Date())
                    continue;
                const auto addObservation = [&](const Date_& accrualStart) {
                    const DateTime_ time = FixingTime(accrualStart, index, identity);
                    if (time < valuationTime)
                        AddUnique({identity.indexName_, time}, &result->requiredHistoricalFixings_);
                };
                if (dailyObservations) {
                    for (Date_ day = period.schedule_.accrualStart_; day < period.schedule_.accrualEnd_; ++day)
                        addObservation(day);
                } else {
                    addObservation(period.schedule_.accrualStart_);
                }
            }
            static_cast<void>(terms);
        }

        // #lizard forgives -- explicit instrument-family validation is the public contract boundary.
        void ValidateTermMatch(const RateTradeDefinition_& trade) {
            const bool matches =
                (trade.instrumentType_ == RateInstrumentType_::Value_::DEPOSIT && std::holds_alternative<DepositTradeTerms_>(trade.terms_)) ||
                (trade.instrumentType_ == RateInstrumentType_::Value_::FRA && std::holds_alternative<FraTradeTerms_>(trade.terms_)) ||
                (trade.instrumentType_ == RateInstrumentType_::Value_::FUTURE && std::holds_alternative<FutureTradeTerms_>(trade.terms_)) ||
                (trade.instrumentType_ == RateInstrumentType_::Value_::OIS && std::holds_alternative<OisTradeTerms_>(trade.terms_)) ||
                (trade.instrumentType_ == RateInstrumentType_::Value_::IRS && std::holds_alternative<IrsTradeTerms_>(trade.terms_)) ||
                (trade.instrumentType_ == RateInstrumentType_::Value_::BASIS_SWAP && std::holds_alternative<BasisTradeTerms_>(trade.terms_)) ||
                (trade.instrumentType_ == RateInstrumentType_::Value_::XCCY && std::holds_alternative<XccyTradeTerms_>(trade.terms_));
            REQUIRE(matches, "Rate instrument type does not match its immutable terms alternative");
        }

        double PriceFixedFloat(const RateTradeDefinition_& trade,
                               const FixedFloatTradeTerms_& terms,
                               bool overnight,
                               const RatePricingMarket_& market,
                               RatePricingTradeResult_* result) {
            REQUIRE(std::isfinite(terms.notional_) && terms.notional_ > 0.0, "Swap notional must be positive and finite");
            REQUIRE(std::isfinite(terms.contractRate_), "Swap contract rate must be finite");
            const auto& discount = Curve(market, terms.discountComponentKey_);
            const auto& forecast = Curve(market, terms.forecastComponentKey_);
            double fixedPv = 0.0;
            for (const auto& period : BuildLegPeriods<CouponPeriod_>(trade.startDate_, trade.maturityDate_, terms.fixedLeg_, 0, Holidays::None())) {
                const double df = Discount(discount, market.valuationTime_, period.schedule_.paymentDate_);
                fixedPv += terms.notional_ * terms.contractRate_ * period.accrual_.dcf_ * df;
            }
            double floatPv = 0.0;
            for (const auto& period : BuildLegPeriods<CouponPeriod_>(trade.startDate_, trade.maturityDate_, terms.floatLeg_,
                                                                     terms.floatIndex_.fixingLag_, terms.floatIndex_.fixingHolidays_)) {
                const double df = Discount(discount, market.valuationTime_, period.schedule_.paymentDate_);
                if (df == 0.0)
                    continue;
                double couponRate = 0.0;
                if (!overnight) {
                    couponRate = ResolveRate(period.schedule_, terms.floatIndex_, terms.fixingIdentity_, forecast, market, result);
                } else {
                    double compound = 1.0;
                    for (Date_ day = period.schedule_.accrualStart_; day < period.schedule_.accrualEnd_; ++day) {
                        SchedulePeriod_ daily;
                        daily.accrualStart_ = day;
                        daily.accrualEnd_ = day.AddDays(1);
                        daily.fixingDate_ = FixingDate(day, terms.floatIndex_);
                        daily.paymentDate_ = period.schedule_.paymentDate_;
                        const double accrual = terms.floatIndex_.dayBasis_(day, day.AddDays(1), nullptr);
                        compound *= 1.0 + ResolveRate(daily, terms.floatIndex_, terms.fixingIdentity_, forecast, market, result) * accrual;
                    }
                    couponRate = (compound - 1.0) / period.accrual_.dcf_;
                }
                floatPv += terms.notional_ * couponRate * period.accrual_.dcf_ * df;
            }
            const double payFixedPv = floatPv - fixedPv;
            return terms.payFixed_ ? payFixedPv : -payFixedPv;
        }

        double PriceBasis(const RateTradeDefinition_& trade,
                          const BasisTradeTerms_& terms,
                          const RatePricingMarket_& market,
                          RatePricingTradeResult_* result) {
            REQUIRE(std::isfinite(terms.notional_) && terms.notional_ > 0.0, "Basis swap notional must be positive and finite");
            REQUIRE(std::isfinite(terms.contractSpread_), "Basis swap contract spread must be finite");
            const auto& discount = Curve(market, terms.discountComponentKey_);
            const auto& spreadForecast = Curve(market, terms.spreadForecastComponentKey_);
            const auto& referenceForecast = Curve(market, terms.referenceForecastComponentKey_);
            double spreadPv = 0.0;
            for (const auto& period : BuildLegPeriods<CouponPeriod_>(trade.startDate_, trade.maturityDate_, terms.spreadLeg_,
                                                                     terms.spreadIndex_.fixingLag_, terms.spreadIndex_.fixingHolidays_)) {
                const double df = Discount(discount, market.valuationTime_, period.schedule_.paymentDate_);
                if (df == 0.0)
                    continue;
                const double rate = ResolveRate(period.schedule_, terms.spreadIndex_, terms.spreadFixingIdentity_, spreadForecast, market, result);
                spreadPv += terms.notional_ * (rate + terms.contractSpread_) * period.accrual_.dcf_ * df;
            }
            double referencePv = 0.0;
            for (const auto& period : BuildLegPeriods<CouponPeriod_>(trade.startDate_, trade.maturityDate_, terms.referenceLeg_,
                                                                     terms.referenceIndex_.fixingLag_, terms.referenceIndex_.fixingHolidays_)) {
                const double df = Discount(discount, market.valuationTime_, period.schedule_.paymentDate_);
                if (df == 0.0)
                    continue;
                const double rate =
                    ResolveRate(period.schedule_, terms.referenceIndex_, terms.referenceFixingIdentity_, referenceForecast, market, result);
                referencePv += terms.notional_ * rate * period.accrual_.dcf_ * df;
            }
            const double receiveReference = referencePv - spreadPv;
            return terms.receiveReferencePaySpread_ ? receiveReference : -receiveReference;
        }

        Tape::JointCurveBlock_<double> JointBlock(const CurveBlock_& block) {
            Tape::JointCurveBlock_<double> result;
            for (const auto& [key, curve] : block.DiscountCurves())
                result.discountCurves[key] = curve.get();
            for (const auto& [key, curve] : block.ForwardCurves())
                result.forwardCurves[key] = curve.get();
            return result;
        }

        double PriceXccy(const RateTradeDefinition_& trade, const XccyTradeTerms_& terms, const RatePricingMarket_& market) {
            REQUIRE(std::isfinite(terms.positionCount_) && terms.positionCount_ > 0.0, "XCCY position count must be positive and finite");
            if (trade.maturityDate_ < market.valuationTime_.Date())
                return 0.0;
            REQUIRE(terms.spreadOnForeignLeg_ == terms.config_.convention_.spreadOnForeignLeg_,
                    "XCCY spread-leg pricing terms must match the cashflow convention");
            REQUIRE(market.xccyMarket_, "XCCY pricing requires an immutable cross-currency market");
            const auto& nativeMarket = *market.xccyMarket_;
            REQUIRE(nativeMarket.ValuationTime() == market.valuationTime_, "XCCY market valuation time does not match the pricing request");
            REQUIRE(nativeMarket.DomesticCcy() == terms.config_.pair_.domestic_ && nativeMarket.ForeignCcy() == terms.config_.pair_.foreign_,
                    "XCCY market currencies do not match the trade");
            const auto domestic = JointBlock(nativeMarket.DomesticBlock());
            const auto foreign = JointBlock(nativeMarket.ForeignBlock());
            XccyMarketView_<double> view;
            view.valuationTime_ = market.valuationTime_;
            view.pair_ = terms.config_.pair_;
            view.collateralCurrency_ = nativeMarket.CollateralCurrency();
            view.fxSpot_ = nativeMarket.FxSpot();
            view.domestic_ = &domestic;
            view.foreign_ = &foreign;
            view.basis_ = nativeMarket.BasisCurve();
            const Handle_<MarketFixingSnapshot_> fixings = market.fixings_ ? market.fixings_ : nativeMarket.Fixings();
            const MarketFixingSnapshot_ empty;
            const auto plan = BuildXccyCashflowPlan(trade.startDate_, trade.maturityDate_, terms.config_);
            return terms.positionCount_ * PriceXccyContract(plan, view, fixings ? *fixings : empty, terms.contractSpread_, terms.spreadOnForeignLeg_,
                                                            terms.receiveNonSpreadPaySpread_);
        }

        // #lizard forgives -- family-specific pricing branches preserve the audited formula mapping.
        double Price(const RateTradeDefinition_& trade, const RatePricingMarket_& market, RatePricingTradeResult_* result) {
            if (const auto* terms = std::get_if<DepositTradeTerms_>(&trade.terms_)) {
                REQUIRE(std::isfinite(terms->notional_) && terms->notional_ > 0.0, "Deposit notional must be positive and finite");
                REQUIRE(std::isfinite(terms->contractRate_), "Deposit contract rate must be finite");
                const auto& discount = Curve(market, terms->discountComponentKey_);
                const double accrual = terms->index_.dayBasis_(trade.startDate_, trade.maturityDate_, nullptr);
                REQUIRE(std::isfinite(accrual) && accrual > 0.0, "Deposit accrual must be positive and finite");
                const double start = trade.startDate_ < market.valuationTime_.Date()
                                         ? 0.0
                                         : -terms->notional_ * Discount(discount, market.valuationTime_, trade.startDate_);
                const double maturity =
                    trade.maturityDate_ < market.valuationTime_.Date()
                        ? 0.0
                        : terms->notional_ * (1.0 + terms->contractRate_ * accrual) * Discount(discount, market.valuationTime_, trade.maturityDate_);
                return terms->lend_ ? start + maturity : -(start + maturity);
            }
            if (const auto* terms = std::get_if<FraTradeTerms_>(&trade.terms_)) {
                REQUIRE(std::isfinite(terms->notional_) && terms->notional_ > 0.0, "FRA notional must be positive and finite");
                REQUIRE(std::isfinite(terms->contractRate_), "FRA contract rate must be finite");
                auto period = SinglePeriod(trade, terms->index_);
                const Date_ payment = terms->settleAtStart_ ? period.accrualStart_ : period.accrualEnd_;
                if (payment < market.valuationTime_.Date())
                    return 0.0;
                const auto& forecast = Curve(market, terms->forecastComponentKey_);
                const auto& discount = Curve(market, terms->discountComponentKey_);
                const double rate = ResolveRate(period, terms->index_, terms->fixingIdentity_, forecast, market, result);
                const double accrual = terms->index_.dayBasis_(period.accrualStart_, period.accrualEnd_, period.dayCountContext_.get());
                double payoff = terms->notional_ * accrual * (rate - terms->contractRate_);
                if (terms->settleAtStart_) {
                    REQUIRE(1.0 + accrual * rate > 0.0, "Start-settled FRA denominator must be positive");
                    payoff /= 1.0 + accrual * rate;
                }
                payoff *= Discount(discount, market.valuationTime_, payment);
                return terms->receiveFloating_ ? payoff : -payoff;
            }
            if (const auto* terms = std::get_if<FutureTradeTerms_>(&trade.terms_)) {
                REQUIRE(std::isfinite(terms->contractCount_) && terms->contractCount_ > 0.0, "Future contract count must be positive and finite");
                REQUIRE(std::isfinite(terms->referencePrice_) && std::isfinite(terms->contractValuePerPricePoint_) &&
                            terms->contractValuePerPricePoint_ > 0.0 && std::isfinite(terms->convexityAdjustment_),
                        "Future pricing terms must be finite and point value must be positive");
                if (trade.maturityDate_ < market.valuationTime_.Date())
                    return 0.0;
                auto period = SinglePeriod(trade, terms->index_);
                const auto& forecast = Curve(market, terms->forecastComponentKey_);
                const double forward = ResolveRate(period, terms->index_, terms->fixingIdentity_, forecast, market, result);
                const double modelPrice = 100.0 * (1.0 - forward + terms->convexityAdjustment_);
                const double pv = terms->contractCount_ * terms->contractValuePerPricePoint_ * (modelPrice - terms->referencePrice_);
                return terms->long_ ? pv : -pv;
            }
            if (const auto* terms = std::get_if<OisTradeTerms_>(&trade.terms_))
                return PriceFixedFloat(trade, terms->value_, true, market, result);
            if (const auto* terms = std::get_if<IrsTradeTerms_>(&trade.terms_))
                return PriceFixedFloat(trade, terms->value_, false, market, result);
            if (const auto* terms = std::get_if<BasisTradeTerms_>(&trade.terms_))
                return PriceBasis(trade, *terms, market, result);
            if (const auto* terms = std::get_if<XccyTradeTerms_>(&trade.terms_))
                return PriceXccy(trade, *terms, market);
            THROW("Rate trade terms alternative is unsupported");
        }

        struct NodeSensitivityPreparation_ {
            CurveDefinition_ definition_;
            Vector_<> passiveParameters_;
            Handle_<DiscountCurve_> passiveBase_;
            int expectedParameterCount_ = 0;
        };

        NodeSensitivityPreparation_ PrepareNodeSensitivityCurve(const RateCashflowPricingInternal::NodeSensitivityCurve_& classifiedCurve,
                                                                const Date_& valuationDate) {
            NodeSensitivityPreparation_ result;
            std::visit(
                [&](const auto& taggedCurve) {
                    using curve_t = std::decay_t<decltype(taggedCurve)>;
                    if constexpr (std::is_same_v<curve_t, std::monostate>) {
                        REQUIRE(false, "Node sensitivity curve representation is unsupported");
                    } else {
                        REQUIRE(taggedCurve, "Node sensitivity curve classification is empty");
                        if constexpr (std::is_same_v<curve_t, const Tape::DiscountPWC_<double>*>) {
                            result.definition_ = MakeCurveDefinition(
                                taggedCurve->Name(), taggedCurve->ccy_.String(), CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
                                LogDfScheme_::Value_::LOG_LINEAR, taggedCurve->KnotDates(), valuationDate, DayBasis_("ACT_365F"));
                            result.passiveParameters_ = taggedCurve->FRight();
                        } else if constexpr (std::is_same_v<curve_t, const Tape::DiscountPWLF_<double>*>) {
                            result.definition_ = MakeCurveDefinition(
                                taggedCurve->Name(), taggedCurve->ccy_.String(), CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD,
                                LogDfScheme_::Value_::LOG_LINEAR, taggedCurve->KnotDates(), valuationDate, DayBasis_("ACT_365F"));
                            const Vector_<> left = taggedCurve->FLeft();
                            const Vector_<> right = taggedCurve->FRight();
                            result.passiveParameters_ = Vector_<>(2 * left.size());
                            for (int i = 0; i < static_cast<int>(left.size()); ++i) {
                                result.passiveParameters_[2 * i] = left[i];
                                result.passiveParameters_[2 * i + 1] = right[i];
                            }
                        } else if constexpr (std::is_same_v<curve_t, const Tape::DiscountLogDF_<double>*>) {
                            result.definition_ = MakeCurveDefinition(
                                taggedCurve->Name(), taggedCurve->ccy_.String(), CurveParameterization_::Value_::LOG_DISCOUNT, taggedCurve->Scheme(),
                                taggedCurve->NodeDates(), taggedCurve->NodeDates().front(), taggedCurve->DayCount());
                            const Vector_<> stored = taggedCurve->NodeLogDF();
                            result.passiveParameters_ = Vector_<>(stored.begin() + 1, stored.end());
                        } else if constexpr (std::is_same_v<curve_t, const Tape::DiscountZeroRate_<double>*>) {
                            result.definition_ = MakeCurveDefinition(taggedCurve->Name(), taggedCurve->ccy_.String(),
                                                                     CurveParameterization_::Value_::ZERO_RATE, taggedCurve->Scheme(),
                                                                     taggedCurve->NodeDates(), taggedCurve->AnchorDate(), taggedCurve->DayCount());
                            result.passiveParameters_ = taggedCurve->NodeZeroRates();
                        }
                        result.passiveBase_ = taggedCurve->Base();
                    }
                },
                classifiedCurve);
            result.expectedParameterCount_ = BuildCurveParameterLayout(result.definition_).parameterCount_;
            return result;
        }
    } // namespace

    // #lizard forgives -- one plan builder keeps family-specific cashflow admission atomic.
    RateCashflowPlan_ BuildRateCashflowPlan(const RateTradeDefinition_& trade, const DateTime_& valuationTime) {
        ValidateTermMatch(trade);
        REQUIRE(trade.tradeDate_.IsValid() && trade.startDate_.IsValid() && trade.maturityDate_.IsValid() && trade.startDate_ < trade.maturityDate_,
                "Rate trade dates must be valid and start before maturity");
        REQUIRE(valuationTime.IsValid(), "Rate cashflow plan requires a valid valuation time");
        RateCashflowPlan_ result;
        result.trade_ = trade;
        std::visit(
            [&](const auto& terms) {
                using terms_t = std::decay_t<decltype(terms)>;
                if constexpr (std::is_same_v<terms_t, DepositTradeTerms_>) {
                    AddUnique(terms.discountComponentKey_, &result.dependencyComponentKeys_);
                } else if constexpr (std::is_same_v<terms_t, FraTradeTerms_>) {
                    AddUnique(terms.forecastComponentKey_, &result.dependencyComponentKeys_);
                    AddUnique(terms.discountComponentKey_, &result.dependencyComponentKeys_);
                    const auto period = SinglePeriod(trade, terms.index_);
                    const Date_ payment = terms.settleAtStart_ ? period.accrualStart_ : period.accrualEnd_;
                    const DateTime_ fixingTime = FixingTime(period.accrualStart_, terms.index_, terms.fixingIdentity_);
                    if (payment >= valuationTime.Date() && fixingTime < valuationTime)
                        AddUnique({terms.fixingIdentity_.indexName_, fixingTime}, &result.requiredHistoricalFixings_);
                } else if constexpr (std::is_same_v<terms_t, FutureTradeTerms_>) {
                    AddUnique(terms.forecastComponentKey_, &result.dependencyComponentKeys_);
                    const auto period = SinglePeriod(trade, terms.index_);
                    const DateTime_ fixingTime = FixingTime(period.accrualStart_, terms.index_, terms.fixingIdentity_);
                    if (trade.maturityDate_ >= valuationTime.Date() && fixingTime < valuationTime)
                        AddUnique({terms.fixingIdentity_.indexName_, fixingTime}, &result.requiredHistoricalFixings_);
                } else if constexpr (std::is_same_v<terms_t, OisTradeTerms_> || std::is_same_v<terms_t, IrsTradeTerms_>) {
                    AddFloatingPlan(trade, terms, valuationTime, terms.value_.floatLeg_, terms.value_.floatIndex_, terms.value_.fixingIdentity_,
                                    terms.value_.forecastComponentKey_, terms.value_.discountComponentKey_, std::is_same_v<terms_t, OisTradeTerms_>,
                                    &result);
                } else if constexpr (std::is_same_v<terms_t, BasisTradeTerms_>) {
                    AddFloatingPlan(trade, terms, valuationTime, terms.spreadLeg_, terms.spreadIndex_, terms.spreadFixingIdentity_,
                                    terms.spreadForecastComponentKey_, terms.discountComponentKey_, false, &result);
                    AddFloatingPlan(trade, terms, valuationTime, terms.referenceLeg_, terms.referenceIndex_, terms.referenceFixingIdentity_,
                                    terms.referenceForecastComponentKey_, terms.discountComponentKey_, false, &result);
                } else if constexpr (std::is_same_v<terms_t, XccyTradeTerms_>) {
                    const auto plan = BuildXccyCashflowPlan(trade.startDate_, trade.maturityDate_, terms.config_);
                    result.requiredHistoricalFixings_ = RequiredHistoricalFixings(plan, valuationTime);
                }
            },
            trade.terms_);
        return result;
    }

    RatePricingTradeResult_ PriceRateTrade(const RateTradeDefinition_& trade, const RatePricingMarket_& market) {
        RatePricingTradeResult_ result;
        result.instrumentId_ = trade.instrumentId_;
        result.instrumentType_ = trade.instrumentType_;
        result.currency_ = market.resultCurrency_;
        try {
            const auto plan = BuildRateCashflowPlan(trade, market.valuationTime_);
            result.requiredHistoricalFixings_ = plan.requiredHistoricalFixings_;
            result.dependencyComponentKeys_ = plan.dependencyComponentKeys_;
            result.pv_ = Price(trade, market, &result);
            REQUIRE(std::isfinite(result.pv_), "Rate trade PV must be finite");
            result.succeeded_ = true;
        } catch (const std::exception& exc) {
            result.error_ = exc.what();
        }
        return result;
    }

    Vector_<RatePricingTradeResult_> PriceRateTrades(const Vector_<RateTradeDefinition_>& trades, const RatePricingMarket_& market) {
        Vector_<RatePricingTradeResult_> result;
        result.reserve(trades.size());
        for (const auto& trade : trades)
            result.push_back(PriceRateTrade(trade, market));
        return result;
    }

    // #lizard forgives -- backend eligibility and diagnostics must remain aligned in one boundary.
    RateTradeNodeSensitivityResult_
    RateTradeNodeSensitivities(const RateTradeDefinition_& trade, const RatePricingMarket_& market, const String_& componentKey) {
        using namespace RateCashflowPricingInternal;
        const auto* terms = std::get_if<DepositTradeTerms_>(&trade.terms_);
        if (!terms || trade.instrumentType_ != RateInstrumentType_::Value_::DEPOSIT)
            return NodeSensitivityFailure("TRADE_FAMILY_NOT_AAD_ENABLED");
        if (terms->discountComponentKey_ != componentKey)
            return NodeSensitivityFailure("TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
        const auto found = market.curveComponents_.find(componentKey);
        if (found == market.curveComponents_.end() || !found->second)
            return NodeSensitivityFailure("CURVE_COMPONENT_UNAVAILABLE");
        const auto classifiedCurve = ClassifyNodeSensitivityCurve(*found->second);
        if (std::holds_alternative<std::monostate>(classifiedCurve))
            return NodeSensitivityFailure("CURVE_REPRESENTATION_NOT_AAD_ENABLED");

        const auto passive = PriceRateTrade(trade, market);
        if (!passive.succeeded_ || !std::isfinite(passive.pv_))
            return NodeSensitivityFailure("TRADE_VALIDATION_FAILED");

        NodeSensitivityPreparation_ preparation;
        try {
            preparation = PrepareNodeSensitivityCurve(classifiedCurve, market.valuationTime_.Date());
        } catch (const std::exception&) {
            return NodeSensitivityFailure("AAD_EVALUATION_FAILED");
        }

        return RunNodeSensitivityAADStage(preparation.expectedParameterCount_, [&]() {
            Vector_<AAD::Number_> parameters = RegisterCurveParameters(preparation.passiveParameters_);
            AAD::NewRecording(*AAD::Tape());
            const auto activeCurve = BuildDiscountCurveUniqueT<AAD::Number_>(preparation.definition_, parameters, preparation.passiveBase_);
            const double accrual = terms->index_.dayBasis_(trade.startDate_, trade.maturityDate_, nullptr);
            const AAD::Number_ start = trade.startDate_ < market.valuationTime_.Date()
                                           ? AAD::Number_(0.0)
                                           : -terms->notional_ * (*activeCurve)(market.valuationTime_.Date(), trade.startDate_);
            const AAD::Number_ maturity =
                trade.maturityDate_ < market.valuationTime_.Date()
                    ? AAD::Number_(0.0)
                    : terms->notional_ * (1.0 + terms->contractRate_ * accrual) * (*activeCurve)(market.valuationTime_.Date(), trade.maturityDate_);
            AAD::Number_ pv = start + maturity;
            if (!terms->lend_)
                pv = -pv;
            AAD::Adjoint(pv) = 1.0;
            AAD::PropagateToStart(*AAD::Tape());
            NodeSensitivityCandidate_ candidate;
            candidate.pv_ = AAD::Value(pv);
            candidate.gradient_ = Vector_<>(parameters.size());
            for (int i = 0; i < static_cast<int>(parameters.size()); ++i)
                candidate.gradient_[i] = AAD::AdjointValue(parameters[i]);
            return candidate;
        });
    }
} // namespace Dal
