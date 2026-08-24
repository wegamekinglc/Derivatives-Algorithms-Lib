//
// Created by dal-implementer on 2026/7/28.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
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

        Date_ FixingDate(const Date_& accrualStart, const RateIndexConvention_& index) {
            if (index.fixingLag_ == 0)
                return accrualStart;
            REQUIRE(index.fixingLag_ > 0, "Rate pricing requires a non-negative fixing lag");
            return Date::NBusDays(index.fixingLag_, index.fixingHolidays_)->BackFrom(accrualStart);
        }

        DateTime_ FixingTime(const Date_& accrualStart, const RateIndexConvention_& index, const FixingIdentity_& identity) {
            REQUIRE(ValidFixingIdentity(identity), "Floating rate pricing requires an explicit valid fixing identity");
            return FixingDateTime(FixingDate(accrualStart, index), identity);
        }

        // Curve reference for the pricing kernels: the target component as an active
        // DiscountCurve_<T_>, every other component as its passive double curve, so mixed
        // active/passive arithmetic type-checks without registering passive parameters
        // (frozen P0 contract 1 in docs/experimental/aad-node-risk-portfolio-aggregation-design.md).
        // T_ = double keeps every reference passive.
        template <class T_> struct CurveRef_ {
            const Tape::DiscountCurve_<T_>* active_ = nullptr;
            const Tape::DiscountCurve_<double>* passive_ = nullptr;

            T_ operator()(const Date_& from, const Date_& to) const { return active_ ? (*active_)(from, to) : T_((*passive_)(from, to)); }
        };

        template <class T_> struct RateMarketView_ {
            const RatePricingMarket_* market_ = nullptr;
            const String_* activeKey_ = nullptr;
            const Tape::DiscountCurve_<T_>* activeCurve_ = nullptr;
        };

        template <class T_> CurveRef_<T_> Curve(const RateMarketView_<T_>& view, const String_& key) {
            if (view.activeKey_ && key == *view.activeKey_) {
                REQUIRE(view.activeCurve_, "Rate pricing active curve component is missing for " + key);
                return {view.activeCurve_, nullptr};
            }
            const auto found = view.market_->curveComponents_.find(key);
            REQUIRE(found != view.market_->curveComponents_.end() && found->second, "Rate pricing market is missing curve component " + key);
            return {nullptr, &*found->second};
        }

        template <class T_> T_ Discount(const CurveRef_<T_>& curve, const DateTime_& valuationTime, const Date_& paymentDate) {
            static constexpr const char* POSITIVE_DISCOUNT_FACTORS = "Rate pricing requires positive finite discount factors";
            if (curve.active_)
                return Tape::DiscountFromValuation(*curve.active_, valuationTime, paymentDate, POSITIVE_DISCOUNT_FACTORS);
            return T_(Tape::DiscountFromValuation(*curve.passive_, valuationTime, paymentDate, POSITIVE_DISCOUNT_FACTORS));
        }

        void AddUnique(const String_& value, Vector_<String_>* values) {
            if (std::find(values->begin(), values->end(), value) == values->end())
                values->push_back(value);
        }

        void AddUnique(const FixingRequest_& value, Vector_<FixingRequest_>* values) {
            if (std::find_if(values->begin(), values->end(), [&](const auto& item) { return SameFixingRequest(item, value); }) == values->end())
                values->push_back(value);
        }

        template <class T_>
        T_ ForwardRate(
            const CurveRef_<T_>& forecast, const Date_& start, const Date_& maturity, const DayBasis_& basis, const DayBasis::Context_* context) {
            const double accrual = basis(start, maturity, context);
            const T_ df = forecast(start, maturity);
            // Dal::AAD::Value extracts the primal on every backend; static_cast<double> would
            // only work on native and CoDiPack (XAD/Adept have no conversion operator).
            const double dfValue = AAD::Value(df);
            REQUIRE(std::isfinite(accrual) && accrual > 0.0 && std::isfinite(dfValue) && dfValue > 0.0,
                    "Floating rate pricing requires positive finite accrual and forecast discount factor");
            return Tape::ForwardRateFromDf(df, accrual);
        }

        template <class T_>
        T_ ResolveRate(const SchedulePeriod_& period,
                       const RateIndexConvention_& index,
                       const FixingIdentity_& identity,
                       const CurveRef_<T_>& forecast,
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
            return T_(*supplied);
        }

        SchedulePeriod_ SinglePeriod(const RateTradeDefinition_& trade, const RateIndexConvention_& index) {
            auto result = BuildSinglePeriodSchedule(trade.startDate_, trade.maturityDate_, index,
                                                    SinglePeriodCouponMonths(index, trade.startDate_, trade.maturityDate_));
            result.fixingDate_ = FixingDate(result.accrualStart_, index);
            result.paymentDate_ = result.accrualEnd_;
            return result;
        }

        void AddFloatingPlan(const RateTradeDefinition_& trade,
                             const DateTime_& valuationTime,
                             const RateLegConvention_& leg,
                             const RateIndexConvention_& index,
                             const FixingIdentity_& identity,
                             bool dailyObservations,
                             RateCashflowPlan_* result) {
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
        }

        // #lizard forgives -- explicit instrument-family validation is the public contract boundary.
        bool TermsMatchFamily(const RateTradeDefinition_& trade) {
            return (trade.instrumentType_ == RateInstrumentType_::Value_::DEPOSIT && std::holds_alternative<DepositTradeTerms_>(trade.terms_)) ||
                   (trade.instrumentType_ == RateInstrumentType_::Value_::FRA && std::holds_alternative<FraTradeTerms_>(trade.terms_)) ||
                   (trade.instrumentType_ == RateInstrumentType_::Value_::FUTURE && std::holds_alternative<FutureTradeTerms_>(trade.terms_)) ||
                   (trade.instrumentType_ == RateInstrumentType_::Value_::OIS && std::holds_alternative<OisTradeTerms_>(trade.terms_)) ||
                   (trade.instrumentType_ == RateInstrumentType_::Value_::IRS && std::holds_alternative<IrsTradeTerms_>(trade.terms_)) ||
                   (trade.instrumentType_ == RateInstrumentType_::Value_::BASIS_SWAP && std::holds_alternative<BasisTradeTerms_>(trade.terms_)) ||
                   (trade.instrumentType_ == RateInstrumentType_::Value_::XCCY && std::holds_alternative<XccyTradeTerms_>(trade.terms_));
        }

        void ValidateTermMatch(const RateTradeDefinition_& trade) {
            REQUIRE(TermsMatchFamily(trade), "Rate instrument type does not match its immutable terms alternative");
        }

        bool IsFamilyAadEnabled(const RateTradeDefinition_& trade) {
            const auto enabled = RateCashflowPricingInternal::AadEnabledRateFamilies();
            return std::find(enabled.begin(), enabled.end(), trade.instrumentType_) != enabled.end() && TermsMatchFamily(trade);
        }

        // Curve components the trade's pricing actually reads, in deterministic terms order — the
        // single enumeration shared by the cashflow plan and the AAD stage's dependency gate.
        void AppendDependencyKeys(const RateTradeTerms_& terms, Vector_<String_>* result) {
            std::visit(
                [&](const auto& value) {
                    using terms_t = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<terms_t, DepositTradeTerms_>) {
                        AddUnique(value.discountComponentKey_, result);
                    } else if constexpr (std::is_same_v<terms_t, FraTradeTerms_>) {
                        AddUnique(value.forecastComponentKey_, result);
                        AddUnique(value.discountComponentKey_, result);
                    } else if constexpr (std::is_same_v<terms_t, FutureTradeTerms_>) {
                        AddUnique(value.forecastComponentKey_, result);
                    } else if constexpr (std::is_same_v<terms_t, OisTradeTerms_> || std::is_same_v<terms_t, IrsTradeTerms_>) {
                        AddUnique(value.value_.forecastComponentKey_, result);
                        AddUnique(value.value_.discountComponentKey_, result);
                    } else if constexpr (std::is_same_v<terms_t, BasisTradeTerms_>) {
                        AddUnique(value.spreadForecastComponentKey_, result);
                        AddUnique(value.referenceForecastComponentKey_, result);
                        AddUnique(value.discountComponentKey_, result);
                    }
                },
                terms);
        }

        Vector_<String_> AadDependencyKeys(const RateTradeDefinition_& trade) {
            Vector_<String_> result;
            AppendDependencyKeys(trade.terms_, &result);
            return result;
        }

        // Each family's actual PV denomination: the trade currency, or the domestic currency a
        // XCCY swap's PV takes from covered-interest parity inside PriceXccyContract. The
        // passthrough RatePricingTradeResult_.currency_ (market.resultCurrency_) is never it.
        Ccy_ ActualPvCurrency(const RateTradeDefinition_& trade) {
            if (const auto* terms = std::get_if<XccyTradeTerms_>(&trade.terms_))
                return terms->config_.pair_.domestic_;
            return trade.currencyOrPair_;
        }

        template <class T_>
        T_ PriceFixedFloat(const RateTradeDefinition_& trade,
                           const FixedFloatTradeTerms_& terms,
                           bool overnight,
                           const RateMarketView_<T_>& view,
                           RatePricingTradeResult_* result) {
            REQUIRE(std::isfinite(terms.notional_) && terms.notional_ > 0.0, "Swap notional must be positive and finite");
            REQUIRE(std::isfinite(terms.contractRate_), "Swap contract rate must be finite");
            const auto& market = *view.market_;
            const auto discount = Curve(view, terms.discountComponentKey_);
            const auto forecast = Curve(view, terms.forecastComponentKey_);
            T_ fixedPv(0.0);
            for (const auto& period : BuildLegPeriods<CouponPeriod_>(trade.startDate_, trade.maturityDate_, terms.fixedLeg_, 0, Holidays::None())) {
                const T_ df = Discount(discount, market.valuationTime_, period.schedule_.paymentDate_);
                fixedPv += terms.notional_ * terms.contractRate_ * period.accrual_.dcf_ * df;
            }
            T_ floatPv(0.0);
            for (const auto& period : BuildLegPeriods<CouponPeriod_>(trade.startDate_, trade.maturityDate_, terms.floatLeg_,
                                                                     terms.floatIndex_.fixingLag_, terms.floatIndex_.fixingHolidays_)) {
                const T_ df = Discount(discount, market.valuationTime_, period.schedule_.paymentDate_);
                if (AAD::Value(df) == 0.0)
                    continue;
                T_ couponRate(0.0);
                if (!overnight) {
                    couponRate = ResolveRate(period.schedule_, terms.floatIndex_, terms.fixingIdentity_, forecast, market, result);
                } else {
                    T_ compound(1.0);
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
            const T_ payFixedPv = floatPv - fixedPv;
            if (terms.payFixed_)
                return payFixedPv;
            return -payFixedPv;
        }

        template <class T_>
        T_ PriceBasis(const RateTradeDefinition_& trade,
                      const BasisTradeTerms_& terms,
                      const RateMarketView_<T_>& view,
                      RatePricingTradeResult_* result) {
            REQUIRE(std::isfinite(terms.notional_) && terms.notional_ > 0.0, "Basis swap notional must be positive and finite");
            REQUIRE(std::isfinite(terms.contractSpread_), "Basis swap contract spread must be finite");
            const auto& market = *view.market_;
            const auto discount = Curve(view, terms.discountComponentKey_);
            const auto spreadForecast = Curve(view, terms.spreadForecastComponentKey_);
            const auto referenceForecast = Curve(view, terms.referenceForecastComponentKey_);
            T_ spreadPv(0.0);
            for (const auto& period : BuildLegPeriods<CouponPeriod_>(trade.startDate_, trade.maturityDate_, terms.spreadLeg_,
                                                                     terms.spreadIndex_.fixingLag_, terms.spreadIndex_.fixingHolidays_)) {
                const T_ df = Discount(discount, market.valuationTime_, period.schedule_.paymentDate_);
                if (AAD::Value(df) == 0.0)
                    continue;
                const T_ rate = ResolveRate(period.schedule_, terms.spreadIndex_, terms.spreadFixingIdentity_, spreadForecast, market, result);
                spreadPv += terms.notional_ * (rate + terms.contractSpread_) * period.accrual_.dcf_ * df;
            }
            T_ referencePv(0.0);
            for (const auto& period : BuildLegPeriods<CouponPeriod_>(trade.startDate_, trade.maturityDate_, terms.referenceLeg_,
                                                                     terms.referenceIndex_.fixingLag_, terms.referenceIndex_.fixingHolidays_)) {
                const T_ df = Discount(discount, market.valuationTime_, period.schedule_.paymentDate_);
                if (AAD::Value(df) == 0.0)
                    continue;
                const T_ rate =
                    ResolveRate(period.schedule_, terms.referenceIndex_, terms.referenceFixingIdentity_, referenceForecast, market, result);
                referencePv += terms.notional_ * rate * period.accrual_.dcf_ * df;
            }
            const T_ receiveReference = referencePv - spreadPv;
            if (terms.receiveReferencePaySpread_)
                return receiveReference;
            return -receiveReference;
        }

        Tape::JointCurveBlock_<double> JointBlock(const CurveBlock_& block) {
            Tape::JointCurveBlock_<double> result;
            for (const auto& [key, curve] : block.DiscountCurves())
                result.discountCurves[key] = curve.get();
            for (const auto& [key, curve] : block.ForwardCurves())
                result.forwardCurves[key] = curve.get();
            return result;
        }

        const MarketFixingSnapshot_& XccyFixings(const RatePricingMarket_& market, const CrossCurrencyMarket_& nativeMarket) {
            static const MarketFixingSnapshot_ empty;
            const Handle_<MarketFixingSnapshot_>& fixings = market.fixings_ ? market.fixings_ : nativeMarket.Fixings();
            return fixings ? *fixings : empty;
        }

        // Same accounting contract as ResolveRate: every strictly-past observation the plan needs is
        // recorded as required, and the first missing one fails validation on the stable token.
        void AccountXccyHistoricalFixings(const XccyCashflowPlan_& plan,
                                          const DateTime_& valuationTime,
                                          const MarketFixingSnapshot_& fixings,
                                          RatePricingTradeResult_* result) {
            for (const auto& request : RequiredHistoricalFixings(plan, valuationTime)) {
                AddUnique(request, &result->requiredHistoricalFixings_);
                if (!fixings.Find(request.indexName_, request.fixingTime_)) {
                    AddUnique(request, &result->missingHistoricalFixings_);
                    THROW("Missing historical fixing " + request.indexName_);
                }
            }
        }

        double PriceXccy(const RateTradeDefinition_& trade,
                         const XccyTradeTerms_& terms,
                         const RatePricingMarket_& market,
                         RatePricingTradeResult_* result) {
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
            const auto plan = BuildXccyCashflowPlan(trade.startDate_, trade.maturityDate_, terms.config_);
            const MarketFixingSnapshot_& fixings = XccyFixings(market, nativeMarket);
            AccountXccyHistoricalFixings(plan, market.valuationTime_, fixings, result);
            return terms.positionCount_ *
                   PriceXccyContract(plan, view, fixings, terms.contractSpread_, terms.spreadOnForeignLeg_, terms.receiveNonSpreadPaySpread_);
        }

        // #lizard forgives -- family-specific pricing branches preserve the audited formula mapping.
        template <class T_> T_ Price(const RateTradeDefinition_& trade, const RateMarketView_<T_>& view, RatePricingTradeResult_* result) {
            const auto& market = *view.market_;
            if (const auto* terms = std::get_if<DepositTradeTerms_>(&trade.terms_)) {
                REQUIRE(std::isfinite(terms->notional_) && terms->notional_ > 0.0, "Deposit notional must be positive and finite");
                REQUIRE(std::isfinite(terms->contractRate_), "Deposit contract rate must be finite");
                const auto discount = Curve(view, terms->discountComponentKey_);
                const double accrual = terms->index_.dayBasis_(trade.startDate_, trade.maturityDate_, nullptr);
                REQUIRE(std::isfinite(accrual) && accrual > 0.0, "Deposit accrual must be positive and finite");
                const T_ start = trade.startDate_ < market.valuationTime_.Date()
                                     ? T_(0.0)
                                     : T_(-terms->notional_ * Discount(discount, market.valuationTime_, trade.startDate_));
                const T_ maturity = trade.maturityDate_ < market.valuationTime_.Date()
                                        ? T_(0.0)
                                        : T_(terms->notional_ * (1.0 + terms->contractRate_ * accrual) *
                                             Discount(discount, market.valuationTime_, trade.maturityDate_));
                if (terms->lend_)
                    return start + maturity;
                return -(start + maturity);
            }
            if (const auto* terms = std::get_if<FraTradeTerms_>(&trade.terms_)) {
                REQUIRE(std::isfinite(terms->notional_) && terms->notional_ > 0.0, "FRA notional must be positive and finite");
                REQUIRE(std::isfinite(terms->contractRate_), "FRA contract rate must be finite");
                auto period = SinglePeriod(trade, terms->index_);
                const Date_ payment = terms->settleAtStart_ ? period.accrualStart_ : period.accrualEnd_;
                if (payment < market.valuationTime_.Date())
                    return T_(0.0);
                const auto forecast = Curve(view, terms->forecastComponentKey_);
                const auto discount = Curve(view, terms->discountComponentKey_);
                const T_ rate = ResolveRate(period, terms->index_, terms->fixingIdentity_, forecast, market, result);
                const double accrual = terms->index_.dayBasis_(period.accrualStart_, period.accrualEnd_, period.dayCountContext_.get());
                T_ payoff = terms->notional_ * accrual * (rate - terms->contractRate_);
                if (terms->settleAtStart_) {
                    const T_ denominator = 1.0 + accrual * rate;
                    REQUIRE(AAD::Value(denominator) > 0.0, "Start-settled FRA denominator must be positive");
                    payoff /= denominator;
                }
                payoff *= Discount(discount, market.valuationTime_, payment);
                if (terms->receiveFloating_)
                    return payoff;
                return -payoff;
            }
            if (const auto* terms = std::get_if<FutureTradeTerms_>(&trade.terms_)) {
                REQUIRE(std::isfinite(terms->contractCount_) && terms->contractCount_ > 0.0, "Future contract count must be positive and finite");
                REQUIRE(std::isfinite(terms->referencePrice_) && std::isfinite(terms->contractValuePerPricePoint_) &&
                            terms->contractValuePerPricePoint_ > 0.0 && std::isfinite(terms->convexityAdjustment_),
                        "Future pricing terms must be finite and point value must be positive");
                if (trade.maturityDate_ < market.valuationTime_.Date())
                    return T_(0.0);
                auto period = SinglePeriod(trade, terms->index_);
                const auto forecast = Curve(view, terms->forecastComponentKey_);
                const T_ forward = ResolveRate(period, terms->index_, terms->fixingIdentity_, forecast, market, result);
                const T_ modelPrice = 100.0 * (1.0 - forward + terms->convexityAdjustment_);
                const T_ pv = terms->contractCount_ * terms->contractValuePerPricePoint_ * (modelPrice - terms->referencePrice_);
                if (terms->long_)
                    return pv;
                return -pv;
            }
            if (const auto* terms = std::get_if<OisTradeTerms_>(&trade.terms_))
                return PriceFixedFloat(trade, terms->value_, true, view, result);
            if (const auto* terms = std::get_if<IrsTradeTerms_>(&trade.terms_))
                return PriceFixedFloat(trade, terms->value_, false, view, result);
            if (const auto* terms = std::get_if<BasisTradeTerms_>(&trade.terms_))
                return PriceBasis(trade, *terms, view, result);
            if (const auto* terms = std::get_if<XccyTradeTerms_>(&trade.terms_))
                return T_(PriceXccy(trade, *terms, market, result));
            THROW("Rate trade terms alternative is unsupported");
        }

        // Passive entry: every CurveRef_ resolves from the market's double curves.
        double Price(const RateTradeDefinition_& trade, const RatePricingMarket_& market, RatePricingTradeResult_* result) {
            const RateMarketView_<double> view{&market, nullptr, nullptr};
            return Price(trade, view, result);
        }

        // Explicit instantiations: every kernel compiles for the passive double path and the AAD path.
        template double Discount(const CurveRef_<double>&, const DateTime_&, const Date_&);
        template AAD::Number_ Discount(const CurveRef_<AAD::Number_>&, const DateTime_&, const Date_&);
        template double ForwardRate(const CurveRef_<double>&, const Date_&, const Date_&, const DayBasis_&, const DayBasis::Context_*);
        template AAD::Number_ ForwardRate(const CurveRef_<AAD::Number_>&, const Date_&, const Date_&, const DayBasis_&, const DayBasis::Context_*);
        template double ResolveRate(const SchedulePeriod_&,
                                    const RateIndexConvention_&,
                                    const FixingIdentity_&,
                                    const CurveRef_<double>&,
                                    const RatePricingMarket_&,
                                    RatePricingTradeResult_*);
        template AAD::Number_ ResolveRate(const SchedulePeriod_&,
                                          const RateIndexConvention_&,
                                          const FixingIdentity_&,
                                          const CurveRef_<AAD::Number_>&,
                                          const RatePricingMarket_&,
                                          RatePricingTradeResult_*);
        template double
        PriceFixedFloat(const RateTradeDefinition_&, const FixedFloatTradeTerms_&, bool, const RateMarketView_<double>&, RatePricingTradeResult_*);
        template AAD::Number_ PriceFixedFloat(
            const RateTradeDefinition_&, const FixedFloatTradeTerms_&, bool, const RateMarketView_<AAD::Number_>&, RatePricingTradeResult_*);
        template double PriceBasis(const RateTradeDefinition_&, const BasisTradeTerms_&, const RateMarketView_<double>&, RatePricingTradeResult_*);
        template AAD::Number_
        PriceBasis(const RateTradeDefinition_&, const BasisTradeTerms_&, const RateMarketView_<AAD::Number_>&, RatePricingTradeResult_*);
        template double Price(const RateTradeDefinition_&, const RateMarketView_<double>&, RatePricingTradeResult_*);
        template AAD::Number_ Price(const RateTradeDefinition_&, const RateMarketView_<AAD::Number_>&, RatePricingTradeResult_*);

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
                                LogDfScheme_::Value_::LOG_LINEAR, taggedCurve->KnotDates(), valuationDate, DayBasis::Act365F());
                            result.passiveParameters_ = taggedCurve->FRight();
                        } else if constexpr (std::is_same_v<curve_t, const Tape::DiscountPWLF_<double>*>) {
                            result.definition_ = MakeCurveDefinition(
                                taggedCurve->Name(), taggedCurve->ccy_.String(), CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD,
                                LogDfScheme_::Value_::LOG_LINEAR, taggedCurve->KnotDates(), valuationDate, DayBasis::Act365F());
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

        void AddUniqueCurve(const DiscountCurve_* curve, Vector_<const DiscountCurve_*>* curves) {
            if (curve && std::find(curves->begin(), curves->end(), curve) == curves->end())
                curves->push_back(curve);
        }

        // XCCY addressing (design decision point 1): the curves a trade consumes are the
        // collateral/tenor-selected slots of the domestic/foreign blocks plus the basis curve —
        // never mere block membership. Slots are located by routing the same JointCurveBlock_
        // logic the pricing kernel uses.
        Vector_<const DiscountCurve_*> ConsumedXccyCurves(const XccyTradeTerms_& terms, const CrossCurrencyMarket_& nativeMarket) {
            const auto domestic = JointBlock(nativeMarket.DomesticBlock());
            const auto foreign = JointBlock(nativeMarket.ForeignBlock());
            const auto& convention = terms.config_.convention_;
            Vector_<const DiscountCurve_*> result;
            AddUniqueCurve(&domestic.Discount(convention.domesticIndex_.collateral_), &result);
            AddUniqueCurve(&Tape::ForecastCurve(domestic, convention.domesticIndex_), &result);
            AddUniqueCurve(&foreign.Discount(convention.foreignIndex_.collateral_), &result);
            AddUniqueCurve(&Tape::ForecastCurve(foreign, convention.foreignIndex_), &result);
            AddUniqueCurve(nativeMarket.BasisCurve(), &result);
            return result;
        }

        // An addressed curve must also be registered under a stable key in curveComponents_; the
        // shared-ownership Handles make pointer comparison well-defined with no false positives.
        // Without an XCCY market — or against a block the config cannot route — no key can be
        // established, so the dependency gate reports every request as not consumed.
        struct XccyConsumedCurves_ {
            bool resolved_ = false;
            Vector_<const DiscountCurve_*> curves_;
        };

        // One routing walk shared by dependency-key emission and stage addressing: the consumed
        // curves in deterministic order (domestic discount, domestic forecast, foreign discount,
        // foreign forecast, basis). Unresolvable requests report no consumed curve.
        XccyConsumedCurves_ ResolveXccyConsumedCurves(const XccyTradeTerms_& terms, const RatePricingMarket_& market) {
            XccyConsumedCurves_ result;
            if (!market.xccyMarket_)
                return result;
            try {
                result.curves_ = ConsumedXccyCurves(terms, *market.xccyMarket_);
            } catch (const std::exception&) {
                return XccyConsumedCurves_();
            }
            result.resolved_ = true;
            return result;
        }

        Vector_<String_> DependencyKeysForConsumedCurves(const Vector_<const DiscountCurve_*>& curves, const RatePricingMarket_& market) {
            Vector_<String_> result;
            for (const auto* curve : curves)
                for (const auto& [key, handle] : market.curveComponents_)
                    if (handle && handle.get() == curve)
                        AddUnique(key, &result);
            return result;
        }

        Vector_<AAD::Number_> ConstantActiveParameters(const Vector_<>& parameters) {
            Vector_<AAD::Number_> result(parameters.size());
            for (int i = 0; i < static_cast<int>(parameters.size()); ++i)
                result[i] = AAD::Number_(parameters[i]);
            return result;
        }

        // The AAD sweep of one single-currency (trade, component) cell: register the target's
        // parameters, rebuild only that component active, price on the heterogeneous view, and
        // harvest the gradient. Shared verbatim by the single-trade entry point and the batch.
        RateTradeNodeSensitivityResult_ RunSingleNodeSensitivityStage(const RateTradeDefinition_& trade,
                                                                      const RatePricingMarket_& market,
                                                                      const String_& componentKey,
                                                                      const NodeSensitivityPreparation_& target) {
            using namespace RateCashflowPricingInternal;
            return RunNodeSensitivityAADStage(target.expectedParameterCount_, [&]() {
                Vector_<AAD::Number_> parameters = RegisterCurveParameters(target.passiveParameters_);
                AAD::NewRecording(*AAD::Tape());
                const auto activeCurve = BuildDiscountCurveUniqueT<AAD::Number_>(target.definition_, parameters, target.passiveBase_);
                // Fixing failures were already gated by the passive price; this diagnostics result is stage scratch.
                RatePricingTradeResult_ activeDiagnostics;
                const RateMarketView_<AAD::Number_> view{&market, &componentKey, activeCurve.get()};
                AAD::Number_ pv = Price(trade, view, &activeDiagnostics);
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

        // Per-trade XCCY state hoisted out of the (trade, component) inner loop: the consumed-curve
        // walk, its classification, the passive validation, and every preparation are computed once
        // per trade and replayed per cell in the single-trade gate order.
        struct XccyNodeSensitivityHoist_ {
            XccyConsumedCurves_ consumed_;
            Vector_<String_> dependencyKeys_;
            const DiscountCurve_* classificationFailureCurve_ = nullptr;
            std::map<const DiscountCurve_*, RateCashflowPricingInternal::NodeSensitivityCurve_> classified_;
            std::optional<bool> passiveOk_;
            bool preparationAttempted_ = false;
            bool preparationOk_ = true;
            std::map<const DiscountCurve_*, NodeSensitivityPreparation_> prepared_;
            std::optional<XccyCashflowPlan_> plan_;
            bool expired_ = false;
        };

        // The AAD sweep of one XCCY (trade, component) cell, on the hoisted per-trade state.
        // Active-typed assembly (frozen P0 contract 8): the uniformly typed XCCY view cannot hold
        // double curves, so every consumed curve is rebuilt as AAD::Number_ — only the addressed
        // component's parameters are registered, the others stay constant-typed, and fxSpot_ is an
        // unregistered constant scalar (P0 ships rate axes only, no FX delta).
        // Active-typed assembly (frozen P0 contract 8): every consumed curve is rebuilt as
        // AAD::Number_ -- only the addressed component's parameters registered, the others
        // constant-typed -- and the uniformly typed domestic/foreign blocks are filled by pointer
        // identity against the consumed-curve preparations. fxSpot_ is an unregistered constant
        // scalar (P0 ships rate axes only, no FX delta).
        struct XccyActiveAssembly_ {
            std::map<const DiscountCurve_*, std::shared_ptr<Tape::DiscountCurve_<AAD::Number_>>> active_;
            Tape::JointCurveBlock_<AAD::Number_> domestic_, foreign_;
            XccyMarketView_<AAD::Number_> view_;
        };

        // Fills a caller-owned assembly: view_ points into the caller's domestic_/foreign_
        // members, so the struct must not be moved or copied after assembly.
        void AssembleXccyActiveView(const XccyTradeTerms_& terms,
                                    const RatePricingMarket_& market,
                                    const CrossCurrencyMarket_& nativeMarket,
                                    const DiscountCurve_* targetCurve,
                                    const XccyNodeSensitivityHoist_& hoist,
                                    const Vector_<AAD::Number_>& targetParameters,
                                    XccyActiveAssembly_* assembly) {
            for (const auto& [curve, preparation] : hoist.prepared_)
                assembly->active_.emplace(curve,
                                          BuildDiscountCurveUniqueT<AAD::Number_>(
                                              preparation.definition_,
                                              curve == targetCurve ? targetParameters : ConstantActiveParameters(preparation.passiveParameters_),
                                              preparation.passiveBase_));

            const auto fillTypedBlock = [](const std::map<const DiscountCurve_*, std::shared_ptr<Tape::DiscountCurve_<AAD::Number_>>>& activeCurves,
                                           const CurveBlock_& source, Tape::JointCurveBlock_<AAD::Number_>* typed) {
                for (const auto& [collateral, handle] : source.DiscountCurves()) {
                    const auto found = activeCurves.find(handle.get());
                    if (found != activeCurves.end())
                        typed->discountCurves[collateral] = found->second.get();
                }
                for (const auto& [tenor, handle] : source.ForwardCurves()) {
                    const auto found = activeCurves.find(handle.get());
                    if (found != activeCurves.end())
                        typed->forwardCurves[tenor] = found->second.get();
                }
            };

            fillTypedBlock(assembly->active_, nativeMarket.DomesticBlock(), &assembly->domestic_);
            fillTypedBlock(assembly->active_, nativeMarket.ForeignBlock(), &assembly->foreign_);
            assembly->view_.valuationTime_ = market.valuationTime_;
            assembly->view_.pair_ = terms.config_.pair_;
            assembly->view_.collateralCurrency_ = nativeMarket.CollateralCurrency();
            assembly->view_.fxSpot_ = AAD::Number_(nativeMarket.FxSpot());
            assembly->view_.domestic_ = &assembly->domestic_;
            assembly->view_.foreign_ = &assembly->foreign_;
            if (const DiscountCurve_* basisCurve = nativeMarket.BasisCurve())
                assembly->view_.basis_ = assembly->active_.at(basisCurve).get();
        }

        RateTradeNodeSensitivityResult_ RunXccyNodeSensitivityStage(const XccyTradeTerms_& terms,
                                                                    const RatePricingMarket_& market,
                                                                    const DiscountCurve_* targetCurve,
                                                                    const XccyNodeSensitivityHoist_& hoist) {
            using namespace RateCashflowPricingInternal;
            const auto& nativeMarket = *market.xccyMarket_;
            return RunNodeSensitivityAADStage(hoist.prepared_.at(targetCurve).expectedParameterCount_, [&]() {
                const Vector_<AAD::Number_> targetParameters = RegisterCurveParameters(hoist.prepared_.at(targetCurve).passiveParameters_);
                AAD::NewRecording(*AAD::Tape());
                NodeSensitivityCandidate_ candidate;
                if (hoist.expired_) {
                    candidate.pv_ = 0.0;
                    candidate.gradient_ = Vector_<>(targetParameters.size(), 0.0);
                    return candidate;
                }
                XccyActiveAssembly_ assembly;
                AssembleXccyActiveView(terms, market, nativeMarket, targetCurve, hoist, targetParameters, &assembly);
                const MarketFixingSnapshot_& fixings = XccyFixings(market, nativeMarket);
                AAD::Number_ pv = terms.positionCount_ * PriceXccyContract(*hoist.plan_, assembly.view_, fixings, terms.contractSpread_,
                                                                           terms.spreadOnForeignLeg_, terms.receiveNonSpreadPaySpread_);
                AAD::Adjoint(pv) = 1.0;
                AAD::PropagateToStart(*AAD::Tape());
                candidate.pv_ = AAD::Value(pv);
                candidate.gradient_ = Vector_<>(targetParameters.size());
                for (int i = 0; i < static_cast<int>(targetParameters.size()); ++i)
                    candidate.gradient_[i] = AAD::AdjointValue(targetParameters[i]);
                return candidate;
            });
        }

        // The six-gate sweep engine shared by the single-trade entry point and the batch: per-key
        // classification gates and per-curve preparations are memoized across every cell of one
        // sweeper instance, and per-trade passive validation is memoized per trade object, so a
        // T x K batch costs one passive PV per trade and one preparation per curve (frozen P0
        // contract 2; P0 stays serial — nothing here dispatches onto the thread pool).
        class NodeSensitivitySweeper_ {
        public:
            explicit NodeSensitivitySweeper_(const RatePricingMarket_& market) : market_(market) {}

            RateTradeNodeSensitivityResult_ Sweep(const RateTradeDefinition_& trade, const String_& componentKey) {
                using namespace RateCashflowPricingInternal;
                if (!IsFamilyAadEnabled(trade))
                    return NodeSensitivityFailure("TRADE_FAMILY_NOT_AAD_ENABLED");
                if (std::holds_alternative<XccyTradeTerms_>(trade.terms_))
                    return SweepXccy(trade, *std::get_if<XccyTradeTerms_>(&trade.terms_), componentKey);
                return SweepSingleCurrency(trade, componentKey);
            }

            // Axis source for aggregation: the preparation of a component key, or nullptr when the
            // key is unavailable, unrepresentable, or failed preparation (meta-table-only then).
            const NodeSensitivityPreparation_* PreparationForKey(const String_& key) {
                const auto& gate = GateForKey(key);
                if (!gate.available_ || !gate.representable_)
                    return nullptr;
                return PreparationFor(gate.curve_, gate.classification_);
            }

        private:
            struct ComponentGate_ {
                bool available_ = false;
                bool representable_ = false;
                const DiscountCurve_* curve_ = nullptr;
                RateCashflowPricingInternal::NodeSensitivityCurve_ classification_;
            };

            const ComponentGate_& GateForKey(const String_& key) {
                const auto found = gates_.find(key);
                if (found != gates_.end())
                    return found->second;
                ComponentGate_ gate;
                const auto component = market_.curveComponents_.find(key);
                gate.available_ = component != market_.curveComponents_.end() && component->second;
                if (gate.available_) {
                    gate.curve_ = component->second.get();
                    gate.classification_ = RateCashflowPricingInternal::ClassifyNodeSensitivityCurve(*component->second);
                    gate.representable_ = !std::holds_alternative<std::monostate>(gate.classification_);
                }
                return gates_.emplace(key, gate).first->second;
            }

            const NodeSensitivityPreparation_* PreparationFor(const DiscountCurve_* curve,
                                                              const RateCashflowPricingInternal::NodeSensitivityCurve_& classification) {
                if (preparationFailures_.count(curve))
                    return nullptr;
                const auto found = prepared_.find(curve);
                if (found != prepared_.end())
                    return &found->second;
                try {
                    NodeSensitivityPreparation_ preparation = PrepareNodeSensitivityCurve(classification, market_.valuationTime_.Date());
                    ++RateCashflowPricingInternal::g_nodeSensitivityPreparationCount;
                    return &prepared_.emplace(curve, std::move(preparation)).first->second;
                } catch (const std::exception&) {
                    preparationFailures_.insert(curve);
                    return nullptr;
                }
            }

            // One passive PV per trade object: the TRADE_VALIDATION_FAILED gate is hoisted out of
            // the (trade, component) inner loop. Lazily priced on first need so trades whose every
            // cell fails an earlier gate are never priced at all.
            bool PassiveOk(const RateTradeDefinition_& trade) {
                const auto found = passiveOk_.find(&trade);
                if (found != passiveOk_.end())
                    return found->second;
                ++RateCashflowPricingInternal::g_nodeSensitivityPassivePriceCount;
                const auto passive = PriceRateTrade(trade, market_);
                const bool ok = passive.succeeded_ && std::isfinite(passive.pv_);
                passiveOk_.emplace(&trade, ok);
                return ok;
            }

            RateTradeNodeSensitivityResult_ SweepSingleCurrency(const RateTradeDefinition_& trade, const String_& componentKey) {
                using namespace RateCashflowPricingInternal;
                const Vector_<String_>& dependencyKeys = DependencyKeysFor(trade);
                if (std::find(dependencyKeys.begin(), dependencyKeys.end(), componentKey) == dependencyKeys.end())
                    return NodeSensitivityFailure("TRADE_DOES_NOT_DEPEND_ON_COMPONENT");

                // Availability and representation are gated for every component the trade depends
                // on, in dependency order; only the requested target is prepared and registered.
                for (const auto& key : dependencyKeys) {
                    const auto& gate = GateForKey(key);
                    if (!gate.available_)
                        return NodeSensitivityFailure("CURVE_COMPONENT_UNAVAILABLE");
                    if (!gate.representable_)
                        return NodeSensitivityFailure("CURVE_REPRESENTATION_NOT_AAD_ENABLED");
                }
                if (!PassiveOk(trade))
                    return NodeSensitivityFailure("TRADE_VALIDATION_FAILED");
                const NodeSensitivityPreparation_* target = PreparationForKey(componentKey);
                if (!target)
                    return NodeSensitivityFailure("AAD_EVALUATION_FAILED");
                return RunSingleNodeSensitivityStage(trade, market_, componentKey, *target);
            }

            const XccyNodeSensitivityHoist_& HoistXccy(const RateTradeDefinition_& trade, const XccyTradeTerms_& terms) {
                const auto found = xccyHoists_.find(&trade);
                if (found != xccyHoists_.end())
                    return found->second;
                XccyNodeSensitivityHoist_ hoist;
                hoist.consumed_ = ResolveXccyConsumedCurves(terms, market_);
                hoist.dependencyKeys_ = DependencyKeysForConsumedCurves(hoist.consumed_.curves_, market_);
                // Classification walk over the consumed curves only — unused in-block members are
                // never classified. The addressed component keeps the representation token; any
                // other consumed curve that cannot be classified is an assembly failure instead
                // (frozen P0 contract 7: that token stays a representation failure).
                for (const auto* curve : hoist.consumed_.curves_) {
                    auto classification = RateCashflowPricingInternal::ClassifyNodeSensitivityCurve(*curve);
                    if (std::holds_alternative<std::monostate>(classification)) {
                        hoist.classificationFailureCurve_ = curve;
                        break;
                    }
                    hoist.classified_[curve] = classification;
                }
                if (!hoist.classificationFailureCurve_) {
                    for (const auto& [curve, classification] : hoist.classified_) {
                        if (const NodeSensitivityPreparation_* preparation = PreparationFor(curve, classification))
                            hoist.prepared_[curve] = *preparation;
                        else {
                            hoist.preparationOk_ = false;
                            break;
                        }
                    }
                    hoist.preparationAttempted_ = true;
                }
                try {
                    hoist.plan_ = BuildXccyCashflowPlan(trade.startDate_, trade.maturityDate_, terms.config_);
                } catch (const std::exception&) {
                    hoist.plan_.reset();
                }
                hoist.expired_ = trade.maturityDate_ < market_.valuationTime_.Date();
                return xccyHoists_.emplace(&trade, std::move(hoist)).first->second;
            }

            RateTradeNodeSensitivityResult_ SweepXccy(const RateTradeDefinition_& trade, const XccyTradeTerms_& terms, const String_& componentKey) {
                using namespace RateCashflowPricingInternal;
                const XccyNodeSensitivityHoist_& hoist = HoistXccy(trade, terms);
                if (std::find(hoist.dependencyKeys_.begin(), hoist.dependencyKeys_.end(), componentKey) == hoist.dependencyKeys_.end())
                    return NodeSensitivityFailure("TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
                REQUIRE(market_.xccyMarket_, "XCCY node sensitivity requires an immutable cross-currency market");
                const DiscountCurve_* targetCurve = market_.curveComponents_.at(componentKey).get();
                if (hoist.classificationFailureCurve_)
                    return NodeSensitivityFailure(hoist.classificationFailureCurve_ == targetCurve ? "CURVE_REPRESENTATION_NOT_AAD_ENABLED"
                                                                                                   : "AAD_EVALUATION_FAILED");
                if (!PassiveOk(trade))
                    return NodeSensitivityFailure("TRADE_VALIDATION_FAILED");
                if (!hoist.preparationAttempted_ || !hoist.preparationOk_ || !hoist.plan_)
                    return NodeSensitivityFailure("AAD_EVALUATION_FAILED");
                return RunXccyNodeSensitivityStage(terms, market_, targetCurve, hoist);
            }

            const Vector_<String_>& DependencyKeysFor(const RateTradeDefinition_& trade) {
                const auto found = dependencyKeys_.find(&trade);
                if (found != dependencyKeys_.end())
                    return found->second;
                return dependencyKeys_.emplace(&trade, AadDependencyKeys(trade)).first->second;
            }

            const RatePricingMarket_& market_;
            std::map<String_, ComponentGate_> gates_;
            std::map<const DiscountCurve_*, NodeSensitivityPreparation_> prepared_;
            std::set<const DiscountCurve_*> preparationFailures_;
            std::map<const RateTradeDefinition_*, bool> passiveOk_;
            std::map<const RateTradeDefinition_*, Vector_<String_>> dependencyKeys_;
            std::map<const RateTradeDefinition_*, XccyNodeSensitivityHoist_> xccyHoists_;
        };
    } // namespace

    // #lizard forgives -- one plan builder keeps family-specific cashflow admission atomic.
    RateCashflowPlan_ BuildRateCashflowPlan(const RateTradeDefinition_& trade, const DateTime_& valuationTime) {
        ValidateTermMatch(trade);
        REQUIRE(trade.tradeDate_.IsValid() && trade.startDate_.IsValid() && trade.maturityDate_.IsValid() && trade.startDate_ < trade.maturityDate_,
                "Rate trade dates must be valid and start before maturity");
        REQUIRE(valuationTime.IsValid(), "Rate cashflow plan requires a valid valuation time");
        RateCashflowPlan_ result;
        result.trade_ = trade;
        AppendDependencyKeys(trade.terms_, &result.dependencyComponentKeys_);
        std::visit(
            [&](const auto& terms) {
                using terms_t = std::decay_t<decltype(terms)>;
                if constexpr (std::is_same_v<terms_t, DepositTradeTerms_>) {
                    // No fixing requests: the single accrual span is pure discounting.
                } else if constexpr (std::is_same_v<terms_t, FraTradeTerms_>) {
                    const auto period = SinglePeriod(trade, terms.index_);
                    const Date_ payment = terms.settleAtStart_ ? period.accrualStart_ : period.accrualEnd_;
                    const DateTime_ fixingTime = FixingTime(period.accrualStart_, terms.index_, terms.fixingIdentity_);
                    if (payment >= valuationTime.Date() && fixingTime < valuationTime)
                        AddUnique({terms.fixingIdentity_.indexName_, fixingTime}, &result.requiredHistoricalFixings_);
                } else if constexpr (std::is_same_v<terms_t, FutureTradeTerms_>) {
                    const auto period = SinglePeriod(trade, terms.index_);
                    const DateTime_ fixingTime = FixingTime(period.accrualStart_, terms.index_, terms.fixingIdentity_);
                    if (trade.maturityDate_ >= valuationTime.Date() && fixingTime < valuationTime)
                        AddUnique({terms.fixingIdentity_.indexName_, fixingTime}, &result.requiredHistoricalFixings_);
                } else if constexpr (std::is_same_v<terms_t, OisTradeTerms_> || std::is_same_v<terms_t, IrsTradeTerms_>) {
                    AddFloatingPlan(trade, valuationTime, terms.value_.floatLeg_, terms.value_.floatIndex_, terms.value_.fixingIdentity_,
                                    std::is_same_v<terms_t, OisTradeTerms_>, &result);
                } else if constexpr (std::is_same_v<terms_t, BasisTradeTerms_>) {
                    AddFloatingPlan(trade, valuationTime, terms.spreadLeg_, terms.spreadIndex_, terms.spreadFixingIdentity_, false, &result);
                    AddFloatingPlan(trade, valuationTime, terms.referenceLeg_, terms.referenceIndex_, terms.referenceFixingIdentity_, false, &result);
                } else if constexpr (std::is_same_v<terms_t, XccyTradeTerms_>) {
                    const auto plan = BuildXccyCashflowPlan(trade.startDate_, trade.maturityDate_, terms.config_);
                    result.requiredHistoricalFixings_ = RequiredHistoricalFixings(plan, valuationTime);
                }
            },
            trade.terms_);
        return result;
    }

    RateCashflowPlan_ BuildRateCashflowPlan(const RateTradeDefinition_& trade, const RatePricingMarket_& market) {
        RateCashflowPlan_ result = BuildRateCashflowPlan(trade, market.valuationTime_);
        if (const auto* terms = std::get_if<XccyTradeTerms_>(&trade.terms_)) {
            const auto consumed = ResolveXccyConsumedCurves(*terms, market);
            for (const auto& key : DependencyKeysForConsumedCurves(consumed.curves_, market))
                AddUnique(key, &result.dependencyComponentKeys_);
        }
        return result;
    }

    RatePricingTradeResult_ PriceRateTrade(const RateTradeDefinition_& trade, const RatePricingMarket_& market) {
        RatePricingTradeResult_ result;
        result.instrumentId_ = trade.instrumentId_;
        result.instrumentType_ = trade.instrumentType_;
        result.currency_ = market.resultCurrency_;
        try {
            const auto plan = BuildRateCashflowPlan(trade, market);
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
        NodeSensitivitySweeper_ sweeper(market);
        return sweeper.Sweep(trade, componentKey);
    }

    namespace {
        // Deterministic serial order (frozen P0 contract 6): trade-major, then the shared key list.
        Vector_<RateTradeNodeSensitivityCell_>
        SweepBatchCells(NodeSensitivitySweeper_& sweeper, const Vector_<RateTradeDefinition_>& trades, const Vector_<String_>& componentKeys) {
            Vector_<RateTradeNodeSensitivityCell_> cells;
            cells.reserve(trades.size() * componentKeys.size());
            for (const auto& trade : trades)
                for (const auto& key : componentKeys)
                    cells.push_back(RateTradeNodeSensitivityCell_{trade.instrumentId_, key, sweeper.Sweep(trade, key)});
            return cells;
        }

        // Meta rows plus the per-currency PV totals and per-component gradient sums. Duplicate
        // keys in componentKeys still produce one meta row per cell (the meta table stays parallel
        // to the batch cells) but contribute their PV and gradient once per trade, at each key's
        // first occurrence -- never once per duplicate.
        struct AggregationAccumulator_ {
            Vector_<RatePortfolioNodeRiskMetaEntry_> meta_;
            std::map<String_, double> pvByActualPvCcy_;
            std::map<String_, Vector_<>> gradientSums_;
        };

        void AccumulateTradeCells(const RateTradeDefinition_& trade,
                                  const Vector_<RateTradeNodeSensitivityCell_>& cells,
                                  int firstCell,
                                  int cellCount,
                                  AggregationAccumulator_* accumulator) {
            REQUIRE(firstCell >= 0 && cellCount >= 0 && firstCell + cellCount <= static_cast<int>(cells.size()),
                    "Aggregation cell window is out of range");
            const Ccy_ actualPvCcy = ActualPvCurrency(trade);
            bool pvCounted = false;
            std::set<String_> gradientCounted;
            for (int index = firstCell; index < firstCell + cellCount; ++index) {
                const auto& cell = cells[index];
                RatePortfolioNodeRiskMetaEntry_ meta;
                meta.instrumentId_ = cell.instrumentId_;
                meta.componentKey_ = cell.componentKey_;
                meta.eligible_ = cell.result_.eligible_;
                meta.reason_ = cell.result_.reason_;
                meta.actualPvCcy_ = actualPvCcy;
                meta.pv_ = cell.result_.pv_;
                accumulator->meta_.push_back(meta);
                if (!cell.result_.eligible_)
                    continue;
                if (!pvCounted) {
                    accumulator->pvByActualPvCcy_[actualPvCcy.String()] += cell.result_.pv_;
                    pvCounted = true;
                }
                if (!gradientCounted.insert(cell.componentKey_).second)
                    continue;
                Vector_<>& sum = accumulator->gradientSums_[cell.componentKey_];
                if (sum.empty()) {
                    sum.Resize(cell.result_.gradient_.size());
                    std::fill(sum.begin(), sum.end(), 0.0);
                }
                for (int node = 0; node < static_cast<int>(cell.result_.gradient_.size()); ++node)
                    sum[node] += cell.result_.gradient_[node];
            }
        }

        // One dense Report_ over the component's node axis: count and order from
        // BuildCurveParameterLayout, header rows from DescribeCurveFreeParameters, values the
        // summed gradient (all zeros when no eligible cell contributed).
        RatePortfolioNodeRiskComponent_
        AggregateComponentTensor(const String_& key, const NodeSensitivityPreparation_& preparation, const Vector_<>& summedGradient) {
            const Vector_<CurveFreeParameter_> freeParameters = DescribeCurveFreeParameters(preparation.definition_);
            REQUIRE(static_cast<int>(freeParameters.size()) == preparation.expectedParameterCount_,
                    "Aggregation node axis disagrees with BuildCurveParameterLayout for " + key);
            REQUIRE(static_cast<int>(summedGradient.size()) == preparation.expectedParameterCount_,
                    "Aggregation gradient width disagrees with BuildCurveParameterLayout for " + key);
            Report::Axis_ axis;
            axis.name_ = "node";
            axis.size_ = preparation.expectedParameterCount_;
            axis.labels_ = {"date", "component"};
            auto values = std::make_shared<Report_>(key, Vector_<Report::Axis_>{axis});
            for (int node = 0; node < preparation.expectedParameterCount_; ++node)
                values->AddHeaderRow("node", node, {Cell_(freeParameters[node].date_), Cell_(freeParameters[node].component_.String())});
            values->SetAll(summedGradient);
            RatePortfolioNodeRiskComponent_ component;
            component.componentKey_ = key;
            component.values_ = std::move(values);
            return component;
        }

        Vector_<String_> UniqueKeysInOrder(const Vector_<String_>& componentKeys) {
            Vector_<String_> ordered;
            std::set<String_> seen;
            for (const auto& key : componentKeys)
                if (seen.insert(key).second)
                    ordered.push_back(key);
            return ordered;
        }
    } // namespace

    Vector_<RateTradeNodeSensitivityCell_> RateTradeNodeSensitivitiesBatch(const Vector_<RateTradeDefinition_>& trades,
                                                                           const RatePricingMarket_& market,
                                                                           const Vector_<String_>& componentKeys) {
        NodeSensitivitySweeper_ sweeper(market);
        return SweepBatchCells(sweeper, trades, componentKeys);
    }

    RatePortfolioNodeRisk_ AggregateRatePortfolioNodeRisk(const Vector_<RateTradeDefinition_>& trades,
                                                          const RatePricingMarket_& market,
                                                          const Vector_<String_>& componentKeys) {
        RatePortfolioNodeRisk_ result;
        NodeSensitivitySweeper_ sweeper(market);
        const Vector_<RateTradeNodeSensitivityCell_> cells = SweepBatchCells(sweeper, trades, componentKeys);

        AggregationAccumulator_ accumulator;
        int cellIndex = 0;
        for (const auto& trade : trades) {
            AccumulateTradeCells(trade, cells, cellIndex, static_cast<int>(componentKeys.size()), &accumulator);
            cellIndex += static_cast<int>(componentKeys.size());
        }
        result.meta_ = std::move(accumulator.meta_);
        result.pvByActualPvCcy_ = std::move(accumulator.pvByActualPvCcy_);

        // Components whose classification or preparation fail carry no tensor -- their tokens
        // live in the meta table only, and no padding convention is introduced.
        for (const auto& key : UniqueKeysInOrder(componentKeys)) {
            const auto* preparation = sweeper.PreparationForKey(key);
            if (!preparation)
                continue;
            const auto found = accumulator.gradientSums_.find(key);
            const Vector_<> zeros(preparation->expectedParameterCount_, 0.0);
            result.components_.push_back(
                AggregateComponentTensor(key, *preparation, found != accumulator.gradientSums_.end() ? found->second : zeros));
        }
        return result;
    }

    Vector_<String_> RateNodeSensitivityAxisLabels(const RatePricingMarket_& market, const String_& componentKey) {
        NodeSensitivitySweeper_ sweeper(market);
        const auto* preparation = sweeper.PreparationForKey(componentKey);
        if (!preparation)
            return {};
        Vector_<String_> result;
        for (const auto& parameter : DescribeCurveFreeParameters(preparation->definition_))
            result.push_back(Date::ToString(parameter.date_) + ":" + parameter.component_.String());
        return result;
    }
} // namespace Dal
