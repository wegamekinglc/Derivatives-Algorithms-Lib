//
// Created by dal-implementer on 2026/8/25.
//

#if defined(_WIN32) || defined(DAL_EXCEL_API_TESTS_PORTABLE)

#include <gtest/gtest.h>

#include <map>
#include <set>
#include <string>

#include <dal-excel/src/__curve_storable.hpp>
#include <dal-excel/src/__curvepricing_test_api.hpp>
#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curveprotocol.hpp>
#include <dal-public/src/curvespec.hpp>

namespace {
    using namespace Dal;

    Date_ Today() { return Date_(2026, 1, 15); }

    Handle_<StorableRateIndexConvention_> QuarterlyIndex() {
        return Handle_<StorableRateIndexConvention_>(
            new StorableRateIndexConvention_(RateIndexConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_365F"), CollateralType_OIS())));
    }

    Handle_<StorableRateLegConvention_> AnnualLeg() {
        return Handle_<StorableRateLegConvention_>(
            new StorableRateLegConvention_(RateLegConvention_New(PeriodLength_New("12M"), DayBasis_New("ACT_365F"))));
    }

    Handle_<StorableFixingIdentity_> SofrFixing() {
        Handle_<StorableFixingIdentity_> identity;
        RateFixingIdentity_New("USD-SOFR", 11, 0, &identity);
        return identity;
    }

    Handle_<Storable_> CurveComponent(const String_& name, const Date_& today, const Date_& maturity) {
        const Vector_<Date_> knots{Date_(2026, 4, 15), Date_(2026, 7, 15), Date_(2026, 10, 15), maturity};
        const Vector_<> rates{0.03, 0.031, 0.032, 0.033};
        return Handle_<Storable_>(new StorableDiscountCurve_(DiscountPWLFNew(name, "USD", knots, rates)));
    }

    Handle_<StorableRatePricingMarket_> Market(const Date_& today, const Date_& maturity) {
        Handle_<StorableRatePricingMarket_> market;
        RatePricingMarket_New(Cell_(DateTime_(today, 10, 30)), "USD", {"discount", "forecast"},
                              {CurveComponent("discount", today, maturity), CurveComponent("forecast", today, maturity)},
                              Handle_<StorableMarketFixingSnapshot_>(), Handle_<StorableCurveBlock_>(), Handle_<StorableCurveBlock_>(), 0.0,
                              String_(), Handle_<StorableDiscountCurve_>(), &market);
        return market;
    }

    Handle_<StorableRateTradeDefinition_> DepositTrade(const Date_& today, const Date_& maturity, double notional) {
        Handle_<StorableRateTradeDefinition_> header;
        RateTradeHeader_New("deposit-1", today, today, maturity, "USD", &header);
        Handle_<StorableRateTradeDefinition_> trade;
        RateDepositTrade_New(header, notional, 0.05, true, QuarterlyIndex(), "discount", &trade);
        return trade;
    }

    Handle_<StorableRateTradeDefinition_> IrsTrade(const Date_& today, const Date_& start, const Date_& maturity) {
        Handle_<StorableRateTradeDefinition_> header;
        RateTradeHeader_New("irs-1", today, start, maturity, "USD", &header);
        Handle_<StorableRateTradeDefinition_> trade;
        RateFixedFloatTrade_New(header, "IRS", 1'000'000.0, 0.03, true, AnnualLeg(), AnnualLeg(), QuarterlyIndex(), SofrFixing(), "forecast",
                                "discount", &trade);
        return trade;
    }

    struct SingleQuoteRiskFixture_ {
        Handle_<StorableCurveCalibrationResult_> result_;
        Handle_<StorableRatePricingMarket_> market_;
        Handle_<StorableRateTradeDefinition_> trade_;
    };

    SingleQuoteRiskFixture_ SingleQuoteRiskFixture(const CurveCalibrationOptions_& options = CurveCalibrationOptions_()) {
        CurveCalibrationSpecBuilder_ builder;
        builder.today_ = Today();
        builder.ccy_ = "USD";
        builder.curveName_ = "single_quote_risk";
        builder.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        builder.knotDates_ = {Date::AddMonths(builder.today_, 6), Date::AddMonths(builder.today_, 12)};
        const auto known = DiscountPWCNew(builder.curveName_, builder.ccy_, builder.knotDates_, {0.02, 0.025});
        const CurveBlock_ knownBlock(known, DayBasis_New("ACT_365F"));
        const RateIndexConvention_ index = QuarterlyIndex()->val_;
        for (const auto& maturity : builder.knotDates_) {
            const auto prototype = DepositNew(builder.today_, builder.today_, maturity, 0.0, index);
            const double quote = (*prototype->Precompute(Handle_<YieldCurve_>()))(knownBlock);
            builder.instruments_.push_back(DepositNew(builder.today_, builder.today_, maturity, quote, index));
        }

        const auto spec = builder.Build();
        const auto calibrated = CalibrateSingleCurve(spec, options);
        SingleQuoteRiskFixture_ result;
        result.result_ = Handle_<StorableCurveCalibrationResult_>(new StorableCurveCalibrationResult_(calibrated, spec, options));

        RatePricingMarket_ market;
        market.valuationTime_ = DateTime_(builder.today_, 10, 30);
        market.resultCurrency_ = Ccy_("USD");
        market.curveComponents_["discount"] = calibrated.curve_;
        market.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());
        result.market_ = Handle_<StorableRatePricingMarket_>(new StorableRatePricingMarket_(market));
        result.trade_ = DepositTrade(builder.today_, builder.knotDates_.back(), 1'000'000.0);
        return result;
    }

    Vector_<Handle_<Storable_>> TradeHandles(const Vector_<Handle_<StorableRateTradeDefinition_>>& trades) {
        Vector_<Handle_<Storable_>> result;
        for (const auto& trade : trades)
            result.push_back(Handle_<Storable_>(new StorableRateTradeDefinition_(trade->val_)));
        return result;
    }

    std::string StdString(const String_& value) { return std::string(value.c_str()); }

    bool IsNodeLabel(const Cell_& cell) {
        if (!Cell::IsString(cell))
            return false;
        const std::string label = StdString(Cell::ToString(cell));
        const auto separator = label.rfind(':');
        if (separator == std::string::npos || separator == 0 || separator + 1 >= label.size())
            return false;
        const std::string component = label.substr(separator + 1);
        return component == "RIGHT_FORWARD" || component == "LEFT_FORWARD" || component == "ZERO_RATE" || component == "LOG_DISCOUNT_FACTOR";
    }
} // namespace

TEST(ExcelCurvePricingTest, TestBatchSpillLongFormContract) {
    const Date_ today = Today();
    const Date_ maturity(2027, 1, 15);
    const auto market = Market(today, maturity);
    const auto trades = TradeHandles({DepositTrade(today, maturity, 100.0)});

    Matrix_<Cell_> spill;
    RateTradeNodeSensitivitiesBatch_Spill(trades, {"discount", "forecast"}, market, &spill);

    // Six columns; one node row for the eligible (deposit x discount) cell plus one reason row
    // for the dependency failure (deposit x forecast). The deposit curve has 2x4 PWLF knots.
    ASSERT_EQ(spill.Cols(), 6);
    ASSERT_EQ(spill.Rows(), 9);

    const auto& single = RateTradeNodeSensitivities(DepositTrade(today, maturity, 100.0)->val_, market->val_, "discount");
    ASSERT_TRUE(single.eligible_);
    ASSERT_EQ(static_cast<int>(single.gradient_.size()), 8);
    for (int node = 0; node < 8; ++node) {
        ASSERT_EQ(StdString(Cell::ToString(spill(node, 0))), "deposit-1");
        ASSERT_EQ(StdString(Cell::ToString(spill(node, 1))), "discount");
        ASSERT_TRUE(Cell::IsEmpty(spill(node, 2))) << "eligible rows carry no reason";
        ASSERT_DOUBLE_EQ(Cell::ToDouble(spill(node, 3)), single.pv_);
        ASSERT_TRUE(IsNodeLabel(spill(node, 4))) << "node labels must be <date>:<free-parameter component>, got "
                                                 << StdString(Cell::ToString(spill(node, 4)));
        ASSERT_DOUBLE_EQ(Cell::ToDouble(spill(node, 5)), single.gradient_[node]);
    }
    ASSERT_EQ(StdString(Cell::ToString(spill(8, 0))), "deposit-1");
    ASSERT_EQ(StdString(Cell::ToString(spill(8, 1))), "forecast");
    ASSERT_EQ(StdString(Cell::ToString(spill(8, 2))), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    ASSERT_DOUBLE_EQ(Cell::ToDouble(spill(8, 3)), 0.0);
    ASSERT_TRUE(Cell::IsEmpty(spill(8, 4)));
    ASSERT_TRUE(Cell::IsEmpty(spill(8, 5)));
}

TEST(ExcelCurvePricingTest, TestPortfolioSpillLongFormContractWithCurrencyOnAggregateRows) {
    const Date_ today = Today();
    const Date_ start(2026, 4, 15);
    const Date_ maturity(2029, 1, 15);
    const auto market = Market(today, maturity);
    const auto trades = TradeHandles({DepositTrade(today, maturity, 100.0), IrsTrade(today, start, maturity)});

    Matrix_<Cell_> spill;
    RatePortfolioNodeRisk_Spill(trades, {"discount", "forecast"}, market, &spill);

    // Seven columns. Rows: discount tensor (8 nodes) + forecast tensor (8 nodes) + one aggregate
    // PV row per actual PV currency (both trades are USD, PV counted once per trade) + one
    // failure row for the deposit x forecast dependency failure.
    ASSERT_EQ(spill.Cols(), 7);
    ASSERT_EQ(spill.Rows(), 18);

    const auto deposit = RateTradeNodeSensitivities(DepositTrade(today, maturity, 100.0)->val_, market->val_, "discount");
    const auto irs = RateTradeNodeSensitivities(IrsTrade(today, start, maturity)->val_, market->val_, "discount");
    for (int node = 0; node < 8; ++node) {
        ASSERT_EQ(StdString(Cell::ToString(spill(node, 1))), "discount");
        ASSERT_TRUE(IsNodeLabel(spill(node, 4)));
        ASSERT_NEAR(Cell::ToDouble(spill(node, 5)), deposit.gradient_[node] + irs.gradient_[node], 1.0e-12);
        ASSERT_TRUE(Cell::IsEmpty(spill(node, 6))) << "node rows carry no currency";
    }
    for (int node = 0; node < 8; ++node) {
        ASSERT_EQ(StdString(Cell::ToString(spill(8 + node, 1))), "forecast");
        ASSERT_TRUE(IsNodeLabel(spill(8 + node, 4)));
    }

    const Cell_& aggregate = spill(16, 2);
    ASSERT_EQ(StdString(Cell::ToString(aggregate)), "UnconvertedByActualPvCcy");
    ASSERT_NEAR(Cell::ToDouble(spill(16, 3)), deposit.pv_ + irs.pv_, 1.0e-10 * std::max(1.0, std::abs(deposit.pv_ + irs.pv_)));
    ASSERT_EQ(StdString(Cell::ToString(spill(16, 6))), "USD");

    ASSERT_EQ(StdString(Cell::ToString(spill(17, 0))), "deposit-1");
    ASSERT_EQ(StdString(Cell::ToString(spill(17, 1))), "forecast");
    ASSERT_EQ(StdString(Cell::ToString(spill(17, 2))), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    ASSERT_TRUE(Cell::IsEmpty(spill(17, 5)));
}

TEST(ExcelCurvePricingTest, TestSpillsCarryNoInternalObjectStructure) {
    const Date_ today = Today();
    const Date_ maturity(2027, 1, 15);
    const auto market = Market(today, maturity);
    const auto trades = TradeHandles({DepositTrade(today, maturity, 100.0)});

    Matrix_<Cell_> batchSpill;
    RateTradeNodeSensitivitiesBatch_Spill(trades, {"discount", "missing"}, market, &batchSpill);
    Matrix_<Cell_> aggregateSpill;
    RatePortfolioNodeRisk_Spill(trades, {"discount", "missing"}, market, &aggregateSpill);

    // Every emitted cell is an empty, string, number, or date -- never a handle, pointer, or any
    // internal object structure; failure reasons are the stable six-token set only.
    const std::map<std::string, int> tokens = {{"TRADE_FAMILY_NOT_AAD_ENABLED", 1}, {"TRADE_DOES_NOT_DEPEND_ON_COMPONENT", 2},
                                               {"CURVE_COMPONENT_UNAVAILABLE", 3},  {"CURVE_REPRESENTATION_NOT_AAD_ENABLED", 4},
                                               {"TRADE_VALIDATION_FAILED", 5},      {"AAD_EVALUATION_FAILED", 6}};
    for (const auto& spill : {batchSpill, aggregateSpill}) {
        for (int row = 0; row < spill.Rows(); ++row)
            for (int col = 0; col < spill.Cols(); ++col) {
                const Cell_& cell = spill(row, col);
                ASSERT_TRUE(Cell::IsEmpty(cell) || Cell::IsString(cell) || Cell::IsDouble(cell) || Cell::IsDate(cell))
                    << "spill leaked a non-scalar cell at (" << row << ", " << col << ")";
                if (col == 2 && Cell::IsString(cell) && !Cell::IsEmpty(cell)) {
                    const std::string reason = StdString(Cell::ToString(cell));
                    ASSERT_TRUE(reason == "UnconvertedByActualPvCcy" || tokens.count(reason)) << "unexpected reason token: " << reason;
                }
            }
    }
}

TEST(ExcelCurvePricingTest, TestQuoteRiskEmptySpillKeepsFixedColumnShape) {
    const Date_ today = Today();
    const Date_ maturity(2027, 1, 15);
    Matrix_<Cell_> spill;

    RatePortfolioQuoteRisk_Spill({}, Market(today, maturity), {}, &spill);

    ASSERT_EQ(spill.Rows(), 0);
    ASSERT_EQ(spill.Cols(), 10);
}

TEST(ExcelCurvePricingTest, TestSingleCurveQuoteRiskProvenanceRejectsMissingHandles) {
    Handle_<StorableRateQuoteRiskProvenance_> provenance;
    ASSERT_THROW(SingleCurveQuoteRiskProvenance_New({}, "single", {"curve"}, {"discount"}, {}, &provenance), Exception_);
}

TEST(ExcelCurvePricingTest, TestExcludedCalibrationDomainsSpillFrozenUnavailableReasons) {
    const auto market = Market(Today(), Date_(2027, 1, 15));
    const Vector_<Handle_<Storable_>> results = {
        Handle_<Storable_>(new StorableMultiCurveCalibrationResult_(MultiCurveCalibrationResult_())),
        Handle_<Storable_>(new StorableJointMultiCurveCalibrationResult_(JointMultiCurveCalibrationResult_())),
    };
    Vector_<Handle_<Storable_>> provenances;
    for (int index = 0; index < static_cast<int>(results.size()); ++index) {
        Handle_<StorableRateQuoteRiskProvenance_> provenance;
        RateQuoteRiskProvenance_New(results[index], String_("excluded-") + String::FromInt(index), {}, {}, market, &provenance);
        provenances.push_back(Handle_<Storable_>(provenance));
    }

    Matrix_<Cell_> spill;
    RatePortfolioQuoteRisk_Spill({}, market, provenances, &spill);

    ASSERT_EQ(spill.Rows(), 2);
    ASSERT_EQ(spill.Cols(), 10);
    ASSERT_EQ(Cell::ToString(spill(0, 0)), String_("excluded-0"));
    ASSERT_EQ(Cell::ToString(spill(0, 8)), String_("unavailable"));
    ASSERT_EQ(Cell::ToString(spill(0, 9)), String_("QUOTE_RISK_NOT_AVAILABLE_FOR_STAGED_CHAIN_RULE"));
    ASSERT_EQ(Cell::ToString(spill(1, 0)), String_("excluded-1"));
    ASSERT_EQ(Cell::ToString(spill(1, 8)), String_("unavailable"));
    ASSERT_EQ(Cell::ToString(spill(1, 9)), String_("QUOTE_RISK_EFFECTIVE_INVERSE_UNAVAILABLE"));
    for (int row = 0; row < spill.Rows(); ++row) {
        ASSERT_TRUE(Cell::IsEmpty(spill(row, 6)));
        ASSERT_TRUE(Cell::IsEmpty(spill(row, 7)));
    }
}

TEST(ExcelCurvePricingTest, TestUnavailableSingleProvenanceSpillsReasonWithoutBuckets) {
    CurveCalibrationOptions_ options;
    options.computeEffJacobianInverse_ = false;
    const auto fixture = SingleQuoteRiskFixture(options);
    Handle_<StorableRateQuoteRiskProvenance_> provenance;
    SingleCurveQuoteRiskProvenance_New(fixture.result_, "single", {fixture.result_->spec_.curveName_}, {"discount"}, fixture.market_, &provenance);
    ASSERT_FALSE(provenance->val_->Available());

    Matrix_<Cell_> spill;
    RatePortfolioQuoteRisk_Spill(TradeHandles({fixture.trade_}), fixture.market_, {Handle_<Storable_>(provenance)}, &spill);

    ASSERT_EQ(spill.Rows(), 1);
    ASSERT_EQ(spill.Cols(), 10);
    ASSERT_EQ(Cell::ToString(spill(0, 0)), String_("single"));
    ASSERT_EQ(Cell::ToString(spill(0, 1)), provenance->val_->Axis().fingerprint_);
    ASSERT_TRUE(Cell::IsEmpty(spill(0, 6)));
    ASSERT_TRUE(Cell::IsEmpty(spill(0, 7)));
    ASSERT_EQ(Cell::ToString(spill(0, 8)), String_("unavailable"));
    ASSERT_EQ(Cell::ToString(spill(0, 9)), String_("QUOTE_RISK_INVERSE_NOT_REQUESTED"));
}

TEST(ExcelCurvePricingTest, TestStaleProvenanceSpillsOneDeterministicFailureRow) {
    const auto fixture = SingleQuoteRiskFixture();
    Handle_<StorableRateQuoteRiskProvenance_> provenance;
    SingleCurveQuoteRiskProvenance_New(fixture.result_, "single", {fixture.result_->spec_.curveName_}, {"discount"}, fixture.market_, &provenance);
    RatePricingMarket_ stale = fixture.market_->val_;
    stale.valuationTime_ = DateTime_(Today(), 11, 0);
    const Handle_<StorableRatePricingMarket_> staleMarket(new StorableRatePricingMarket_(stale));

    Matrix_<Cell_> spill;
    RatePortfolioQuoteRisk_Spill(TradeHandles({fixture.trade_}), staleMarket, {Handle_<Storable_>(provenance)}, &spill);

    ASSERT_EQ(spill.Rows(), 1);
    ASSERT_EQ(spill.Cols(), 10);
    ASSERT_EQ(Cell::ToString(spill(0, 0)), String_("single"));
    ASSERT_EQ(Cell::ToString(spill(0, 1)), provenance->val_->Axis().fingerprint_);
    ASSERT_EQ(Cell::ToString(spill(0, 4)), String_("discount"));
    ASSERT_TRUE(Cell::IsEmpty(spill(0, 6)));
    ASSERT_TRUE(Cell::IsEmpty(spill(0, 7)));
    ASSERT_EQ(Cell::ToString(spill(0, 8)), String_("unavailable"));
    ASSERT_EQ(Cell::ToString(spill(0, 9)), String_("QUOTE_RISK_CALIBRATION_STATE_MISMATCH"));
}

TEST(ExcelCurvePricingTest, TestIncompleteTradeSpillsOneFailureAndOnlyScalarCells) {
    const auto fixture = SingleQuoteRiskFixture();
    Handle_<StorableRateQuoteRiskProvenance_> provenance;
    SingleCurveQuoteRiskProvenance_New(fixture.result_, "single", {fixture.result_->spec_.curveName_}, {"discount"}, fixture.market_, &provenance);
    auto invalid = fixture.trade_->val_;
    std::get<DepositTradeTerms_>(invalid.terms_).notional_ = 0.0;
    const Vector_<Handle_<Storable_>> trades{Handle_<Storable_>(new StorableRateTradeDefinition_(invalid))};

    Matrix_<Cell_> spill;
    RatePortfolioQuoteRisk_Spill(trades, fixture.market_, {Handle_<Storable_>(provenance)}, &spill);

    ASSERT_EQ(spill.Rows(), 1);
    ASSERT_EQ(spill.Cols(), 10);
    ASSERT_EQ(Cell::ToString(spill(0, 2)), invalid.instrumentId_);
    ASSERT_EQ(Cell::ToString(spill(0, 3)), String_("TRADE_VALIDATION_FAILED"));
    ASSERT_EQ(Cell::ToString(spill(0, 8)), String_("unavailable"));
    ASSERT_EQ(Cell::ToString(spill(0, 9)), String_("QUOTE_RISK_TRADE_PROVENANCE_INCOMPLETE"));
    for (int col = 0; col < spill.Cols(); ++col)
        ASSERT_TRUE(Cell::IsEmpty(spill(0, col)) || Cell::IsString(spill(0, col)) || Cell::IsDouble(spill(0, col)))
            << "non-scalar quote-risk spill cell in column " << col;
}

TEST(ExcelCurvePricingTest, TestDuplicateQuoteRiskCalibrationIdsFailClosed) {
    const auto fixture = SingleQuoteRiskFixture();
    Handle_<StorableRateQuoteRiskProvenance_> provenance;
    SingleCurveQuoteRiskProvenance_New(fixture.result_, "single", {fixture.result_->spec_.curveName_}, {"discount"}, fixture.market_, &provenance);
    try {
        Matrix_<Cell_> spill;
        RatePortfolioQuoteRisk_Spill(TradeHandles({fixture.trade_}), fixture.market_,
                                     {Handle_<Storable_>(provenance), Handle_<Storable_>(provenance)}, &spill);
        FAIL() << "Expected duplicate calibration ids to fail";
    } catch (const Exception_& exception) {
        ASSERT_NE(std::string(exception.what()).find("QUOTE_RISK_DUPLICATE_CALIBRATION_ID"), std::string::npos) << exception.what();
    }
}

TEST(ExcelCurvePricingTest, TestQuoteRiskMixedCurrenciesRemainSeparateAndOrderedLikePublicAggregate) {
    const auto fixture = SingleQuoteRiskFixture();
    Handle_<StorableRateQuoteRiskProvenance_> provenance;
    SingleCurveQuoteRiskProvenance_New(fixture.result_, "single", {fixture.result_->spec_.curveName_}, {"discount"}, fixture.market_, &provenance);
    auto eurTrade = fixture.trade_->val_;
    eurTrade.instrumentId_ = "eur-deposit";
    eurTrade.currencyOrPair_ = Ccy_("EUR");
    const Handle_<StorableRateTradeDefinition_> eur(new StorableRateTradeDefinition_(eurTrade));

    Matrix_<Cell_> spill;
    RatePortfolioQuoteRisk_Spill(TradeHandles({fixture.trade_, eur}), fixture.market_, {Handle_<Storable_>(provenance)}, &spill);
    const auto native = AggregateRatePortfolioQuoteRisk({fixture.trade_->val_, eurTrade}, fixture.market_->val_, {*provenance->val_});

    ASSERT_EQ(spill.Rows(), static_cast<int>(native.buckets_.size()));
    ASSERT_EQ(spill.Cols(), 10);
    std::set<String_> currencies;
    for (int row = 0; row < spill.Rows(); ++row) {
        ASSERT_EQ(Cell::ToString(spill(row, 2)), native.buckets_[row].quoteKey_);
        ASSERT_EQ(Cell::ToString(spill(row, 5)), native.buckets_[row].actualPvCcy_.String());
        ASSERT_DOUBLE_EQ(Cell::ToDouble(spill(row, 6)), native.buckets_[row].dPvDDecimalQuote_);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(spill(row, 7)), native.buckets_[row].dv01_);
        currencies.insert(Cell::ToString(spill(row, 5)));
    }
    ASSERT_EQ(currencies, (std::set<String_>{"EUR", "USD"}));
}

TEST(ExcelCurvePricingTest, TestSingleCurveQuoteRiskSpillMatchesPublicAggregate) {
    const auto fixture = SingleQuoteRiskFixture();
    Handle_<StorableRateQuoteRiskProvenance_> provenance;
    SingleCurveQuoteRiskProvenance_New(fixture.result_, "single", {fixture.result_->spec_.curveName_}, {"discount"}, fixture.market_, &provenance);

    Matrix_<Cell_> spill;
    RatePortfolioQuoteRisk_Spill(TradeHandles({fixture.trade_}), fixture.market_, {Handle_<Storable_>(provenance)}, &spill);

    const auto native = AggregateRatePortfolioQuoteRisk({fixture.trade_->val_}, fixture.market_->val_, {*provenance->val_});
    ASSERT_EQ(spill.Rows(), static_cast<int>(native.buckets_.size()));
    ASSERT_EQ(spill.Cols(), 10);
    for (int row = 0; row < spill.Rows(); ++row) {
        const auto& bucket = native.buckets_[row];
        ASSERT_EQ(Cell::ToString(spill(row, 0)), bucket.calibrationId_);
        ASSERT_EQ(Cell::ToString(spill(row, 1)), bucket.axisFingerprint_);
        ASSERT_EQ(Cell::ToString(spill(row, 2)), bucket.quoteKey_);
        ASSERT_EQ(Cell::ToString(spill(row, 3)), bucket.quoteName_);
        ASSERT_EQ(Cell::ToString(spill(row, 4)), bucket.residualBlock_);
        ASSERT_EQ(Cell::ToString(spill(row, 5)), bucket.actualPvCcy_.String());
        ASSERT_DOUBLE_EQ(Cell::ToDouble(spill(row, 6)), bucket.dPvDDecimalQuote_);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(spill(row, 7)), bucket.dv01_);
        ASSERT_EQ(Cell::ToString(spill(row, 8)), String_("available"));
        ASSERT_TRUE(Cell::IsEmpty(spill(row, 9)));
    }
}
#endif
