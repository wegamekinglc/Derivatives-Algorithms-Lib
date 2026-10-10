//
// Created by Codex on 2026/10/10.
//

#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <limits>
#include <string>

#include <dal-public/src/ratecurvature.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/platform/platform.hpp>
#include <dal/time/holidays.hpp>

#include "jointquoteriskfixtures.hpp"
#include "ratejacobianfixtures.hpp"
#include "ratetradecurvaturefixtures.hpp"
#include "ratexccycurvaturefixtures.hpp"

namespace {
    Dal::CurveCalibrationSpec_ TradeSingleSpec() {
        Dal::CurveCalibrationSpec_ spec;
        spec.today_ = Dal::Date_(2025, 1, 2);
        spec.ccy_ = "USD";
        spec.curveName_ = "trade_curve";
        spec.parameterization_ = Dal::CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotPolicy_ = Dal::CurveKnotPolicy_::Value_::INPUT;
        spec.tolerance_ = 1.0e-14;
        spec.initialGuess_ = 0.025;
        spec.knotDates_.push_back(spec.today_);
        Dal::RateIndexConvention_ index;
        index.dayBasis_ = Dal::DayBasis::Act365F();
        index.businessDayConvention_ = Dal::BizDayConvention_("Unadjusted");
        index.accrualHolidays_ = Dal::Holidays::None();
        for (int years = 1; years <= 2; ++years) {
            const auto maturity = Dal::Date::AddMonths(spec.today_, 12 * years);
            spec.knotDates_.push_back(maturity);
            spec.instruments_.push_back(
                Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(spec.today_, spec.today_, maturity, 0.02 + 0.005 * years, index)));
        }
        return spec;
    }

    Dal::RateTradeDefinition_ DepositTrade(const Dal::CurveCalibrationSpec_& spec) {
        Dal::RateTradeDefinition_ trade;
        trade.instrumentId_ = "off-knot-deposit";
        trade.instrumentType_ = Dal::RateInstrumentType_::Value_::DEPOSIT;
        trade.tradeDate_ = spec.today_;
        trade.startDate_ = spec.today_;
        trade.maturityDate_ = Dal::Date::AddMonths(spec.today_, 18);
        trade.currencyOrPair_ = Dal::Ccy_(spec.ccy_);
        Dal::DepositTradeTerms_ terms;
        terms.notional_ = 1.0;
        terms.contractRate_ = 0.028;
        terms.discountComponentKey_ = spec.curveName_;
        terms.index_ = static_cast<const Dal::Deposit_&>(*spec.instruments_.front()).FloatConvention();
        trade.terms_ = terms;
        return trade;
    }

    Dal::RatePricingMarket_
    PassiveSingleMarket(Dal::CurveCalibrationSpec_ spec, const Dal::Vector_<>& quotes, const Dal::Handle_<Dal::MarketFixingSnapshot_>& fixings = {}) {
        for (size_t i = 0; i < quotes.size(); ++i) {
            const auto& old = static_cast<const Dal::Deposit_&>(*spec.instruments_[i]);
            const auto span = old.TimeSpan();
            spec.instruments_[i] =
                Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(old.TradeDate(), span.first, span.second, quotes[i], old.FloatConvention()));
        }
        auto calibrated = Dal::CalibrateYieldCurve(spec, Dal::CurveCalibrationOptions_{});
        Dal::RatePricingMarket_ market;
        market.valuationTime_ = Dal::DateTime_(spec.today_);
        market.resultCurrency_ = Dal::Ccy_(spec.ccy_);
        market.curveComponents_[spec.curveName_] = Dal::Handle_<Dal::DiscountCurve_>(std::move(calibrated.curve_));
        market.fixings_ = fixings;
        return market;
    }

    double PassivePortfolioPrice(const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                                 const Dal::RatePricingMarket_& market,
                                 const Dal::Vector_<>& weights = {}) {
        const auto prices = Dal::PriceRateTrades(trades, market);
        double value = 0.0;
        for (size_t row = 0; row < prices.size(); ++row) {
            REQUIRE(prices[row].succeeded_, prices[row].error_);
            value += (weights.empty() ? 1.0 : weights[row]) * prices[row].pv_;
        }
        return value;
    }

    double PassiveSinglePrice(const Dal::CurveCalibrationSpec_& spec,
                              const Dal::Vector_<>& quotes,
                              const Dal::RateTradeDefinition_& trade,
                              const Dal::Handle_<Dal::MarketFixingSnapshot_>& fixings = {}) {
        return PassivePortfolioPrice({trade}, PassiveSingleMarket(spec, quotes, fixings));
    }

    Dal::AAD::BumpOverAADRequest_ TradeDirection(int count) {
        Dal::AAD::BumpOverAADRequest_ request;
        request.directions_ = Dal::Matrix_<>(1, count, 0.0);
        request.directions_(0, 0) = 1.0;
        request.steps_ = {2.0e-4};
        return request;
    }
    using RateTradeCurvatureFixtures::SingleFamilyTrades;

    using PassivePrice_ = std::function<double(const Dal::Vector_<>&)>;

    double PriceGradient(const PassivePrice_& price, const Dal::Vector_<>& point, int coordinate, double step) {
        auto plus = point, minus = point;
        plus[coordinate] += step;
        minus[coordinate] -= step;
        return (price(plus) - price(minus)) / (2.0 * step);
    }

    double PriceCurvature(const PassivePrice_& price,
                          const Dal::Vector_<>& point,
                          const Dal::Matrix_<>& directions,
                          int row,
                          int coordinate,
                          double outer,
                          double inner) {
        double numerator = 0.0;
        for (int directionSign : {-1, 1})
            for (int coordinateSign : {-1, 1}) {
                auto bumped = point;
                for (int column = 0; column < directions.Cols(); ++column)
                    bumped[column] += directionSign * outer * directions(row, column);
                bumped[coordinate] += coordinateSign * inner;
                numerator += directionSign * coordinateSign * price(bumped);
            }
        return numerator / (4.0 * outer * inner);
    }

    void AssertTradeFinancial(const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                              const Dal::RateCalibrationSnapshot_& snapshot,
                              const PassivePrice_& price,
                              const Dal::RateTradeQuoteCurvatureSettings_& settings = {}) {
        const auto& point = snapshot.Point();
        const int count = static_cast<int>(point.size());
        Dal::Vector_<> gradient(count);
        for (int column = 0; column < count; ++column)
            gradient[column] = (4.0 * PriceGradient(price, point, column, 5.0e-6) - PriceGradient(price, point, column, 1.0e-5)) / 3.0;
        for (double outer : {4.0e-4, 2.0e-4, 1.0e-4}) {
            Dal::AAD::BumpOverAADRequest_ request;
            request.directions_ = Dal::Matrix_<>(2, count, 0.0);
            request.directions_(0, 0) = 1.0;
            for (int column = 0; column < count; ++column)
                request.directions_(1, column) = (column % 2 == 0 ? 1.0 : -0.6) / (column + 1);
            request.steps_ = {outer, outer};
            const auto result = Dal::EvaluateRateTradeQuoteCurvature(trades, snapshot, request, settings);
            const auto& risk = result.Curvature();
            ASSERT_NEAR(risk.Value(), price(point), 1.0e-9);
            ASSERT_EQ(risk.Execution().calibrations_, 5U);
            ASSERT_EQ(risk.Execution().objectiveReverseSweeps_, 5U);
            ASSERT_EQ(risk.BaseCalibration().Provenance().Axis().fingerprint_, snapshot.Provenance().Axis().fingerprint_);
            for (int column = 0; column < count; ++column) {
                ASSERT_NEAR(risk.Gradient()[column], gradient[column], 1.0e-6) << column;
                for (int row = 0; row < 2; ++row) {
                    const double fine = PriceCurvature(price, point, request.directions_, row, column, outer, 5.0e-5);
                    const double coarse = PriceCurvature(price, point, request.directions_, row, column, outer, 1.0e-4);
                    ASSERT_NEAR(risk.HessianProducts()(row, column), (4.0 * fine - coarse) / 3.0, 2.0e-3) << row << "," << column;
                }
            }
        }
    }

    Dal::RateTradeDefinition_ XccyTrade(const Dal::CrossCurrencyCalibrationSpec_& spec) {
        auto trade = RateJacobianFixtures::Xccy();
        trade.tradeDate_ = spec.today_;
        trade.startDate_ = Dal::Date::AddMonths(spec.today_, 1);
        trade.maturityDate_ = Dal::Date::AddMonths(spec.today_, 31);
        auto& terms = std::get<Dal::XccyTradeTerms_>(trade.terms_);
        terms.positionCount_ = 0.01;
        terms.config_ = spec.instruments_.front()->Config();
        return trade;
    }

    Dal::RatePricingMarket_ PassiveStagedMarket(Dal::CrossCurrencyCalibrationSpec_ spec, const Dal::Vector_<>& quotes) {
        for (size_t index = 0; index < quotes.size(); ++index)
            spec.instruments_[index] = RateXccyCurvatureFixtures::QuoteXccy(spec.instruments_[index], quotes[index]);
        const auto result = Dal::CalibrateCrossCurrencyMarket(spec);
        Dal::RatePricingMarket_ market;
        market.valuationTime_ = spec.valuationTime_;
        market.resultCurrency_ = spec.basisPair_.domestic_;
        market.xccyMarket_ = std::make_shared<Dal::CrossCurrencyMarket_>(result.market_);
        market.fixings_ = spec.fixings_;
        return market;
    }

    Dal::RatePricingMarket_ PassiveJointXccyMarket(Dal::JointXccyCalibrationSpec_ spec, const Dal::Vector_<>& quotes) {
        size_t offset = 0;
        spec.domestic_ = RateXccyCurvatureFixtures::QuoteCurrency(spec.domestic_, quotes, &offset);
        spec.foreign_ = RateXccyCurvatureFixtures::QuoteCurrency(spec.foreign_, quotes, &offset);
        for (auto& instrument : spec.basis_.instruments_)
            instrument = RateXccyCurvatureFixtures::QuoteXccy(instrument, quotes[offset++]);
        const auto result = Dal::CalibrateJointXccyMarket(spec);
        auto native = std::make_shared<Dal::CrossCurrencyMarket_>(result.domesticCurveBlock_, result.foreignCurveBlock_, spec.fxSpot_,
                                                                  spec.valuationTime_, spec.collateralCurrency_, result.fixings_);
        native->SetBasisCurve(result.basisCurve_);
        Dal::RatePricingMarket_ market;
        market.valuationTime_ = spec.valuationTime_;
        market.resultCurrency_ = spec.pair_.domestic_;
        market.xccyMarket_ = native;
        market.fixings_ = result.fixings_;
        return market;
    }
} // namespace

TEST(RateQuoteCurvatureTest, TestTradeDepositFinancialRequest) {
    const auto spec = TradeSingleSpec();
    const auto trade = DepositTrade(spec);
    const auto snapshot = Dal::NewRateCalibration(spec);
    Dal::AAD::BumpOverAADRequest_ request;
    request.directions_ = Dal::Matrix_<>(1, 2);
    request.directions_(0, 0) = 1.0;
    request.directions_(0, 1) = 0.3;
    request.steps_ = {2.0e-4};
    const auto result = Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, request);
    ASSERT_EQ(result.Currency(), Dal::Ccy_("USD"));
    ASSERT_NEAR(result.Curvature().Value(), PassiveSinglePrice(spec, snapshot.Point(), trade), 1.0e-10);
    ASSERT_EQ(result.Curvature().Execution().objectiveReverseSweeps_, 3U);
    for (double derivative : result.Curvature().Gradient())
        ASSERT_TRUE(std::isfinite(derivative));
    ASSERT_GT(std::abs(result.Curvature().HessianProducts()(0, 0)), 1.0e-4);
    AssertTradeFinancial({trade}, snapshot, [=](const auto& quotes) { return PassiveSinglePrice(spec, quotes, trade); });
}

TEST(RateQuoteCurvatureTest, TestTradeHistoricalExplicitFixing) {
    const auto spec = TradeSingleSpec();
    auto trade = DepositTrade(spec);
    trade.instrumentId_ = "historical-fra";
    trade.instrumentType_ = Dal::RateInstrumentType_::Value_::FRA;
    trade.startDate_ = Dal::Date::AddMonths(spec.today_, -6);
    trade.tradeDate_ = trade.startDate_;
    trade.maturityDate_ = Dal::Date::AddMonths(spec.today_, 6);
    Dal::FraTradeTerms_ terms;
    terms.notional_ = 1.0;
    terms.contractRate_ = 0.028;
    terms.settleAtStart_ = false;
    terms.index_ = static_cast<const Dal::Deposit_&>(*spec.instruments_.front()).FloatConvention();
    terms.index_.forecastTenor_ = Dal::PeriodLength_("12M");
    terms.fixingIdentity_ = {"USD-AAD-TRADE-CURVATURE", 10, 0};
    terms.forecastComponentKey_ = spec.curveName_;
    terms.discountComponentKey_ = spec.curveName_;
    trade.terms_ = terms;
    const auto plan = Dal::BuildRateCashflowPlan(trade, Dal::DateTime_(spec.today_));
    ASSERT_EQ(plan.requiredHistoricalFixings_.size(), 1U);
    Dal::MarketFixingSnapshot_::values_t values;
    const auto& required = plan.requiredHistoricalFixings_.front();
    values[required.indexName_][required.fixingTime_] = 0.031;
    Dal::RateTradeQuoteCurvatureSettings_ settings;
    settings.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(values));
    const auto snapshot = Dal::NewRateCalibration(spec);
    const auto result = Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, TradeDirection(2), settings);
    ASSERT_NEAR(result.Curvature().Value(), PassiveSinglePrice(spec, snapshot.Point(), trade, settings.fixings_), 1.0e-10);
}

TEST(RateQuoteCurvatureTest, TestTradeRejectsCurveCurrencyMismatch) {
    const auto spec = TradeSingleSpec();
    auto trade = DepositTrade(spec);
    trade.currencyOrPair_ = Dal::Ccy_("EUR");
    const auto snapshot = Dal::NewRateCalibration(spec);
    ASSERT_THROW((void)Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, TradeDirection(2)), Dal::Exception_);
}

TEST(RateQuoteCurvatureTest, TestTradeSixSingleCurrencyFamilies) {
    const auto spec = TradeSingleSpec();
    const auto trades = SingleFamilyTrades(spec);
    const auto snapshot = Dal::NewRateCalibration(spec);
    for (const auto& trade : trades) {
        SCOPED_TRACE(trade.instrumentId_);
        AssertTradeFinancial({trade}, snapshot, [=](const auto& quotes) { return PassiveSinglePrice(spec, quotes, trade); });
    }
    Dal::RateTradeQuoteCurvatureSettings_ settings;
    settings.weights_ = {1.0, -0.5, 0.25, 0.0, 0.75, -0.3};
    AssertTradeFinancial(
        trades, snapshot, [=](const auto& quotes) { return PassivePortfolioPrice(trades, PassiveSingleMarket(spec, quotes), settings.weights_); },
        settings);
}

TEST(RateQuoteCurvatureTest, TestTradeStagedXccyFinancialRequest) {
    for (auto mode :
         {Dal::XccyNotionalMode_::Value_::FIXED, Dal::XccyNotionalMode_::Value_::RESETTABLE, Dal::XccyNotionalMode_::Value_::MARK_TO_MARKET}) {
        const auto spec = RateXccyCurvatureFixtures::StagedSpec(mode);
        const auto trade = XccyTrade(spec);
        const auto snapshot = Dal::NewRateCalibration(spec);
        AssertTradeFinancial({trade}, snapshot,
                             [=](const auto& quotes) { return PassivePortfolioPrice({trade}, PassiveStagedMarket(spec, quotes)); });
    }
}

TEST(RateQuoteCurvatureTest, TestTradeJointXccyFinancialRequest) {
    for (bool layered : {false, true}) {
        const auto spec = RateXccyCurvatureFixtures::JointSpec(layered);
        const auto trade = XccyTrade(RateXccyCurvatureFixtures::StagedSpec());
        const auto snapshot = Dal::NewRateCalibration(spec);
        AssertTradeFinancial({trade}, snapshot,
                             [=](const auto& quotes) { return PassivePortfolioPrice({trade}, PassiveJointXccyMarket(spec, quotes)); });
    }
}

TEST(RateQuoteCurvatureTest, TestTradeSameCurrencyJointFinancialRequest) {
    for (bool layered : {false, true}) {
        auto spec = JointQuoteRiskFixtures::Spec(4, 2, Dal::CurveParameterization_::Value_::LOG_DISCOUNT, layered);
        spec.tolerance_ = 1.0e-14;
        const auto trade = JointQuoteRiskFixtures::Irs(spec, 1, 1.0);
        const auto snapshot = Dal::NewRateCalibration(spec);
        AssertTradeFinancial({trade}, snapshot, [=](const auto& quotes) {
            auto bumped = spec;
            size_t offset = 0;
            for (auto& curve : bumped.curves_) {
                curve.instruments_ = Dal::OrderInstruments(curve.instruments_);
                for (auto& instrument : curve.instruments_)
                    instrument = RateXccyCurvatureFixtures::QuoteYc(instrument, quotes[offset++]);
            }
            const auto result = Dal::CalibrateJointMultiCurve(bumped);
            return PassivePortfolioPrice({trade}, JointQuoteRiskFixtures::Market(bumped, result));
        });
    }
}

TEST(RateQuoteCurvatureTest, TestTradeWeightsDuplicateIdsAndLinearity) {
    const auto spec = TradeSingleSpec();
    auto first = DepositTrade(spec), second = first;
    std::get<Dal::DepositTradeTerms_>(second.terms_).contractRate_ = 0.035;
    const auto snapshot = Dal::NewRateCalibration(spec);
    const auto request = TradeDirection(2);
    const auto a = Dal::EvaluateRateTradeQuoteCurvature({first}, snapshot, request);
    const auto b = Dal::EvaluateRateTradeQuoteCurvature({second}, snapshot, request);
    Dal::RateTradeQuoteCurvatureSettings_ settings;
    settings.weights_ = {1.0, -0.5};
    const auto both = Dal::EvaluateRateTradeQuoteCurvature({first, second}, snapshot, request, settings);
    ASSERT_NEAR(both.Curvature().Value(), a.Curvature().Value() - 0.5 * b.Curvature().Value(), 1.0e-10);
    ASSERT_EQ(both.Curvature().Execution().objectiveReverseSweeps_, 3U);
    for (int column = 0; column < 2; ++column) {
        ASSERT_NEAR(both.Curvature().Gradient()[column], a.Curvature().Gradient()[column] - 0.5 * b.Curvature().Gradient()[column], 1.0e-10);
        ASSERT_NEAR(both.Curvature().HessianProducts()(0, column),
                    a.Curvature().HessianProducts()(0, column) - 0.5 * b.Curvature().HessianProducts()(0, column), 1.0e-8);
    }
    settings.weights_ = {0.0, 0.0};
    const auto zero = Dal::EvaluateRateTradeQuoteCurvature({first, second}, snapshot, request, settings);
    ASSERT_EQ(zero.Curvature().Value(), 0.0);
    for (double derivative : zero.Curvature().Gradient())
        ASSERT_EQ(derivative, 0.0);
}

TEST(RateQuoteCurvatureTest, TestTradeAdmissionAndZeroWeightValidation) {
    const auto spec = TradeSingleSpec();
    const auto trade = DepositTrade(spec);
    const auto snapshot = Dal::NewRateCalibration(spec);
    const auto request = TradeDirection(2);
    ASSERT_THROW((void)Dal::EvaluateRateTradeQuoteCurvature({}, snapshot, request), Dal::Exception_);
    Dal::RateTradeQuoteCurvatureSettings_ settings;
    for (const Dal::Vector_<>& weights : {Dal::Vector_<>{1.0, 2.0}, Dal::Vector_<>{std::numeric_limits<double>::quiet_NaN()},
                                          Dal::Vector_<>{std::numeric_limits<double>::infinity()}}) {
        settings.weights_ = weights;
        ASSERT_THROW((void)Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, request, settings), Dal::Exception_);
    }
    settings.weights_ = {0.0};
    for (int failure = 0; failure < 5; ++failure) {
        auto bad = trade;
        if (failure == 0)
            bad.instrumentType_ = Dal::RateInstrumentType_::Value_::IRS;
        else if (failure == 1)
            std::get<Dal::DepositTradeTerms_>(bad.terms_).discountComponentKey_ = "unavailable";
        else if (failure == 2)
            std::get<Dal::DepositTradeTerms_>(bad.terms_).notional_ = -1.0;
        else if (failure == 3)
            bad.currencyOrPair_ = Dal::Ccy_();
        else
            bad.maturityDate_ = bad.startDate_;
        ASSERT_THROW((void)Dal::EvaluateRateTradeQuoteCurvature({bad}, snapshot, request, settings), Dal::Exception_);
    }
    auto foreign = trade;
    foreign.currencyOrPair_ = Dal::Ccy_("EUR");
    settings.weights_ = {1.0, 0.0};
    ASSERT_THROW((void)Dal::EvaluateRateTradeQuoteCurvature({trade, foreign}, snapshot, request, settings), Dal::Exception_);
}

TEST(RateQuoteCurvatureTest, TestTradeSavedFixingConflict) {
    auto spec = RateXccyCurvatureFixtures::StagedSpec();
    Dal::MarketFixingSnapshot_::values_t values;
    values["AAD-SEALED-CALIBRATION-FIXING"][Dal::DateTime_(Dal::Date::AddMonths(spec.today_, -1))] = 0.03;
    spec.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(values));
    const auto snapshot = Dal::NewRateCalibration(spec);
    const auto trade = XccyTrade(spec);
    Dal::RateTradeQuoteCurvatureSettings_ settings;
    settings.fixings_ = spec.fixings_;
    const auto before = Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, TradeDirection(2), settings);
    values.begin()->second.begin()->second = 0.04;
    settings.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(values));
    ASSERT_THROW((void)Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, TradeDirection(2), settings), Dal::Exception_);
    const auto after = Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, TradeDirection(2));
    ASSERT_EQ(before.Curvature().Gradient(), after.Curvature().Gradient());
}

TEST(RateQuoteCurvatureTest, TestTradeModesBudgetsAndRecovery) {
    const auto spec = TradeSingleSpec();
    const auto trade = DepositTrade(spec);
    const auto snapshot = Dal::NewRateCalibration(spec);
    const auto mode = Dal::AAD::SetNumResultsForAAD(true, 3);
    auto request = TradeDirection(2);
    const auto accepted = Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, request);
    for (int failure = 0; failure < 4; ++failure) {
        auto bad = request;
        if (failure == 0)
            bad.numericPayloadBudgetBytes_ = 0;
        else if (failure == 1)
            bad.recordingCapacityBudgetBytes_ = 0;
        else if (failure == 2)
            bad.steps_ = {0.0};
        else
            bad.directions_ = Dal::Matrix_<>(1, 1, 0.0);
        ASSERT_THROW((void)Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, bad), Dal::Exception_);
        ASSERT_TRUE(Dal::AAD::Tape()->multi_);
        ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 3U);
    }
    request.directions_ = Dal::Matrix_<>(0, 2);
    request.steps_.clear();
    request.numericPayloadBudgetBytes_ = Dal::AAD::BumpOverAADPayloadBytes(2, 0);
    const auto recovered = Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, request);
    ASSERT_EQ(recovered.Curvature().Gradient(), accepted.Curvature().Gradient());
    ASSERT_EQ(recovered.Curvature().Execution().calibrations_, 1U);
    ASSERT_EQ(recovered.Curvature().Execution().numericPayloadBytes_, *request.numericPayloadBudgetBytes_);
    ASSERT_TRUE(Dal::AAD::Tape()->multi_);
    ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 3U);
}

TEST(RateQuoteCurvatureTest, TestTradePreservesActiveOuterRecording) {
    const auto spec = TradeSingleSpec();
    const auto trade = DepositTrade(spec);
    const auto snapshot = Dal::NewRateCalibration(spec);
    Dal::AAD::RecordingScope_ recording;
    Dal::AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    ASSERT_THROW((void)Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, TradeDirection(2)), Dal::Exception_);
    Dal::AAD::Number_ root = input * input;
    recording.FinishRecording();
    recording.ClearAdjoints();
    Dal::AAD::NativeOperations_::SetSeed(root, 1.0);
    recording.Reverse();
    ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
    recording.Close();
}

TEST(RateQuoteCurvatureTest, TestTradeInvalidNumericRequestPrecedesFixingReads) {
    struct Reads_ : Dal::Detail::FixingReadObserver_ {
        int count_ = 0;
        void BeforeHistory(const Dal::String_&) override { ++count_; }
    } reads;
    const auto spec = TradeSingleSpec();
    auto trade = SingleFamilyTrades(spec)[1];
    trade.startDate_ = Dal::Date::AddMonths(spec.today_, -6);
    trade.tradeDate_ = trade.startDate_;
    trade.maturityDate_ = Dal::Date::AddMonths(spec.today_, 6);
    const auto snapshot = Dal::NewRateCalibration(spec);
    Dal::Detail::ScopedFixingReadObserver_ observation(&reads);
    for (int failure = 0; failure < 3; ++failure) {
        auto request = TradeDirection(2);
        if (failure == 0)
            request.numericPayloadBudgetBytes_ = 0;
        else if (failure == 1)
            request.steps_ = {0.0};
        else
            request.directions_ = Dal::Matrix_<>(1, 1, 0.0);
        ASSERT_THROW((void)Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, request), Dal::Exception_);
        ASSERT_EQ(reads.count_, 0);
    }
}

TEST(RateQuoteCurvatureTest, TestTradeFailureIdentifiesRowAndInstrument) {
    const auto spec = TradeSingleSpec();
    const auto first = DepositTrade(spec);
    const auto snapshot = Dal::NewRateCalibration(spec);
    for (int failure = 0; failure < 2; ++failure) {
        auto second = first;
        if (failure == 0)
            second.instrumentType_ = Dal::RateInstrumentType_::Value_::IRS;
        else
            std::get<Dal::DepositTradeTerms_>(second.terms_).discountComponentKey_ = "missing";
        try {
            (void)Dal::EvaluateRateTradeQuoteCurvature({first, second}, snapshot, TradeDirection(2));
            FAIL() << "Expected invalid second trade";
        } catch (const Dal::Exception_& error) {
            const std::string text(error.what());
            ASSERT_NE(text.find("RateTradeQuoteCurvature"), std::string::npos) << text;
            ASSERT_NE(text.find("trade[1]"), std::string::npos) << text;
            ASSERT_NE(text.find(first.instrumentId_.c_str()), std::string::npos) << text;
        }
    }
}

TEST(RateQuoteCurvatureTest, TestTradeXccyGeometryFailureIdentifiesRow) {
    const auto spec = RateXccyCurvatureFixtures::StagedSpec();
    const auto first = XccyTrade(spec);
    auto second = first;
    second.startDate_ = second.maturityDate_;
    const auto snapshot = Dal::NewRateCalibration(spec);
    try {
        (void)Dal::EvaluateRateTradeQuoteCurvature({first, second}, snapshot, TradeDirection(2));
        FAIL() << "Expected invalid second XCCY geometry";
    } catch (const Dal::Exception_& error) {
        const std::string text(error.what());
        ASSERT_NE(text.find("trade[1]"), std::string::npos) << text;
        ASSERT_NE(text.find(first.instrumentId_.c_str()), std::string::npos) << text;
    }
}

TEST(RateQuoteCurvatureTest, TestTradeAdditionalHistoricalFxAndRateFixings) {
    const auto spec = RateXccyCurvatureFixtures::StagedSpec(Dal::XccyNotionalMode_::Value_::MARK_TO_MARKET);
    auto trade = XccyTrade(spec);
    trade.startDate_ = Dal::Date::AddMonths(spec.today_, -10);
    trade.tradeDate_ = trade.startDate_;
    trade.maturityDate_ = Dal::Date::AddMonths(spec.today_, 20);
    const auto plan = Dal::BuildRateCashflowPlan(trade, spec.valuationTime_);
    ASSERT_FALSE(plan.requiredHistoricalFixings_.empty());
    Dal::MarketFixingSnapshot_::values_t values;
    int fxObservations = 0;
    for (const auto& request : plan.requiredHistoricalFixings_) {
        double fixing = 0.025;
        if (request.indexName_ == Dal::FxIndexName(spec.basisPair_)) {
            fixing = 1.1;
            ++fxObservations;
        } else if (request.indexName_ == Dal::ReverseFxIndexName(spec.basisPair_)) {
            fixing = 1.0 / 1.1;
            ++fxObservations;
        }
        values[request.indexName_][request.fixingTime_] = fixing;
    }
    ASSERT_GT(fxObservations, 0);
    const auto snapshot = Dal::NewRateCalibration(spec);
    Dal::RateTradeQuoteCurvatureSettings_ settings;
    settings.fixings_ = spec.fixings_;
    ASSERT_THROW((void)Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, TradeDirection(2), settings), Dal::Exception_);
    settings.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(values));
    AssertTradeFinancial(
        {trade}, snapshot,
        [=](const auto& quotes) {
            auto market = PassiveStagedMarket(spec, quotes);
            market.fixings_ = settings.fixings_;
            return PassivePortfolioPrice({trade}, market);
        },
        settings);
    const auto result = Dal::EvaluateRateTradeQuoteCurvature({trade}, snapshot, TradeDirection(2), settings);
    ASSERT_EQ(result.Curvature().BaseCalibration().Provenance().State().fingerprint_, snapshot.Provenance().State().fingerprint_);
}

TEST(RateQuoteCurvatureTest, TestTradeExpiredXccyTermsValidateBeforeZeroPv) {
    const auto spec = RateXccyCurvatureFixtures::StagedSpec();
    auto expired = XccyTrade(spec);
    expired.startDate_ = Dal::Date::AddMonths(spec.today_, -24);
    expired.tradeDate_ = expired.startDate_;
    expired.maturityDate_ = Dal::Date::AddMonths(spec.today_, -1);
    const auto snapshot = Dal::NewRateCalibration(spec);
    const auto request = TradeDirection(2);
    const auto valid = Dal::EvaluateRateTradeQuoteCurvature({expired}, snapshot, request);
    ASSERT_EQ(valid.Currency(), Dal::Ccy_("USD"));
    ASSERT_EQ(valid.Curvature().Value(), 0.0);
    for (double derivative : valid.Curvature().Gradient())
        ASSERT_EQ(derivative, 0.0);
    for (double product : valid.Curvature().HessianProducts())
        ASSERT_EQ(product, 0.0);
    const std::vector<std::function<void(Dal::XccyTradeTerms_&)>> mutations = {
        [](auto& terms) { terms.positionCount_ = 0.0; }, [](auto& terms) { terms.contractSpread_ = std::numeric_limits<double>::quiet_NaN(); },
        [](auto& terms) { terms.spreadOnForeignLeg_ = !terms.config_.convention_.spreadOnForeignLeg_; },
        [](auto& terms) { terms.config_.pair_ = Dal::CurrencyPair_(Dal::Ccy_("USD"), Dal::Ccy_("GBP")); }};
    for (double weight : {0.0, 1.0}) {
        Dal::RateTradeQuoteCurvatureSettings_ settings;
        settings.weights_ = {1.0, weight};
        for (const auto& mutate : mutations) {
            auto invalid = expired;
            mutate(std::get<Dal::XccyTradeTerms_>(invalid.terms_));
            try {
                (void)Dal::EvaluateRateTradeQuoteCurvature({expired, invalid}, snapshot, request, settings);
                FAIL() << "Expected invalid expired XCCY row";
            } catch (const Dal::Exception_& error) {
                const std::string text(error.what());
                ASSERT_NE(text.find("trade[1]"), std::string::npos) << text;
                ASSERT_NE(text.find(expired.instrumentId_.c_str()), std::string::npos) << text;
            }
        }
    }
}
