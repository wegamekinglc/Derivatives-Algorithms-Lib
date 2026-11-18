#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <rapidjson/document.h>
#include <string>
#include <type_traits>
#include <variant>

#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/model/ivs.hpp>
#include <dal/platform/platform.hpp>

#include <dal-cpp/benchmarks/rate_risk_perf/quoteriskbenchfixtures.hpp>
#include <dal-public/src/calibrationrisk.hpp>
#include <dal-public/src/calibrationriskrequest.hpp>

#include "jointxccyquoteriskfixtures.hpp"

namespace {
    class FlatIVS_ final : public Dal::AAD::IVS_ {
        double vol_;

    public:
        explicit FlatIVS_(double vol = 0.2) : IVS_(100.0, 0.05, 0.02), vol_(vol) {}
        [[nodiscard]] double ImpliedVol(double, double) const override { return vol_; }
    };

    Dal::DupireRiskInputs_ Inputs() {
        return {{75.0, 105.0, 135.0}, {0.4, 1.2}, Dal::Matrix_<>(3, 2, 0.0), {60.0, 100.0, 140.0}, 10.0, {0.5, 1.0}, 0.5};
    }

    void AssertSameMatrix(const Dal::Matrix_<>& actual, const Dal::Matrix_<>& expected) {
        ASSERT_EQ(actual.Rows(), expected.Rows());
        ASSERT_EQ(actual.Cols(), expected.Cols());
        for (int row = 0; row < actual.Rows(); ++row)
            for (int column = 0; column < actual.Cols(); ++column)
                ASSERT_EQ(actual(row, column), expected(row, column));
    }

    template <class C_> void AssertFailure(C_ operation, const std::string& reason) {
        try {
            operation();
            FAIL() << "Expected " << reason;
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(reason), std::string::npos) << error.what();
        }
    }

    Dal::RateQuoteRiskProvenance_ Captured(const Dal::JointMultiCurveCalibrationSpec_& spec,
                                           const Dal::JointMultiCurveCalibrationResult_& calibrated,
                                           const Dal::JointMultiCurveCalibrationOptions_& options,
                                           const Dal::RatePricingMarket_& market,
                                           Dal::RateQuoteRiskProvenanceConfig_ config) {
        config.retainCalibrationRecord_ = true;
        return Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, config);
    }

    Dal::Matrix_<> PricedGradient(const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                                  const Dal::RatePricingMarket_& market,
                                  const Dal::RateQuoteRiskProvenance_& provenance) {
        Dal::Vector_<Dal::String_> keys;
        const auto& ranges = provenance.Axis().parameterRanges_;
        for (const auto& range : ranges)
            keys.push_back(provenance.ComponentKeyByParameterBlock().at(range.blockKey_));
        const auto cells = provenance.Kind() == "JOINT_MULTI_CURVE"
                               ? Dal::RateCashflowPricingInternal::JointNodeSensitivitiesBatch(trades, market, keys)
                               : Dal::RateTradeNodeSensitivitiesBatch(trades, market, keys);
        REQUIRE(cells.size() == trades.size() * ranges.size(), "Incomplete independent joint gradients");
        Dal::Matrix_<> gradient(static_cast<int>(provenance.Axis().parameters_.size()), 1, 0.0);
        for (size_t index = 0; index < cells.size(); ++index) {
            const auto block = index % ranges.size();
            const auto& range = ranges[block];
            const auto& cell = cells[index];
            REQUIRE(cell.componentKey_ == keys[block], "Independent gradient component order differs");
            if (!cell.result_.eligible_) {
                REQUIRE(cell.result_.reason_ == "TRADE_DOES_NOT_DEPEND_ON_COMPONENT", "Independent gradient failed: " + cell.result_.reason_);
                continue;
            }
            REQUIRE(cell.result_.gradient_.size() == static_cast<size_t>(range.size_), "Independent joint gradient width differs");
            for (int ordinal = 0; ordinal < range.size_; ++ordinal)
                gradient(range.offset_ + ordinal, 0) += cell.result_.gradient_[ordinal];
        }
        return gradient;
    }

    void AssertLegacyMapping(const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                             const Dal::RatePricingMarket_& market,
                             const Dal::RateQuoteRiskProvenance_& provenance) {
        const auto boundary = Dal::NewCalibrationPullback(provenance);
        const auto parameters = Dal::NewCalibrationParameterAdjoints(boundary, PricedGradient(trades, market, provenance));
        const auto result = Dal::PullbackCalibration(boundary, parameters);
        const auto legacy = Dal::AggregateRatePortfolioQuoteRisk(trades, market, {provenance});
        ASSERT_TRUE(legacy.provenanceFailures_.empty());
        ASSERT_EQ(legacy.buckets_.size(), provenance.Axis().quotes_.size());
        ASSERT_EQ(boundary.Domain(), "RATE_CURVE");
        ASSERT_EQ(result.Method(), "RetainedCurveEffectiveInverse");
        ASSERT_EQ(result.Unit(), "DECIMAL_QUOTE");
        ASSERT_EQ(result.Boundary(), "FrozenCalibrationEffectiveInverse");
        ASSERT_EQ(result.TotalAdjoints().Rows(), static_cast<int>(legacy.buckets_.size()));
        ASSERT_EQ(result.TotalAdjoints().Cols(), 1);
        for (int row = 0; row < result.TotalAdjoints().Rows(); ++row) {
            ASSERT_EQ(result.TotalAdjoints()(row, 0), legacy.buckets_[row].dPvDDecimalQuote_);
            ASSERT_EQ(result.TotalAdjoints()(row, 0) * 1e-4, legacy.buckets_[row].dv01_);
            ASSERT_EQ(result.CalibrationAdjoints()(row, 0), result.TotalAdjoints()(row, 0));
            ASSERT_EQ(result.DirectAdjoints()(row, 0), 0.0);
        }
    }

    auto SmallCurve() {
        const auto spec = JointQuoteRiskFixtures::Spec();
        const auto options = JointXccyQuoteRiskFixtures::Options();
        const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
        const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
        return Dal::NewCalibrationPullback(Captured(spec, calibrated, options, market, JointQuoteRiskFixtures::Config(2)));
    }

    template <class M_, class B_>
    void AssertNativeProvider(const M_& materials, const Dal::RateRiskPerf::QuoteRiskBenchmarkCase_& example, B_ build) {
        auto config = materials.config_;
        config.retainCalibrationRecord_ = true;
        const auto provenance = build(materials.spec_, *materials.calibration_, materials.options_, materials.market_, config);
        ASSERT_TRUE(provenance.Available()) << provenance.Reason();
        ASSERT_NO_FATAL_FAILURE(AssertLegacyMapping(example.trades_, materials.market_, provenance));
        ASSERT_EQ(Dal::NewCalibrationPullback(provenance).ParameterCols(), 1);
    }

    void AssertCurveMismatch(const Dal::CalibrationPullback_& target, const Dal::CalibrationPullback_& changed) {
        ASSERT_FALSE(target.Matches(changed));
        const auto seeds = Dal::NewCalibrationParameterAdjoints(changed, Dal::Matrix_<>(changed.ParameterRows(), changed.ParameterCols(), 0.0));
        ASSERT_THROW(Dal::PullbackCalibration(target, seeds), Dal::Exception_);
        const auto good = Dal::NewCalibrationParameterAdjoints(target, Dal::Matrix_<>(target.ParameterRows(), target.ParameterCols(), 0.0));
        const auto direct = Dal::NewCalibrationDirectQuoteAdjoints(changed, Dal::Matrix_<>(changed.QuoteRows(), changed.QuoteCols(), 0.0));
        ASSERT_THROW(Dal::PullbackCalibration(target, good, direct), Dal::Exception_);
    }

    template <class F_>
    void AssertSubsecondConsumers(
        const Dal::RatePricingMarket_& original, const Dal::Vector_<Dal::RateTradeDefinition_>& trades, F_ freeze, bool fixing, const char* scheme) {
        using namespace Dal;
        auto before = original;
        const auto day = original.valuationTime_.Date();
        before.valuationTime_ = DateTime_(day, .1 / 86400.);
        before.fixings_.reset(new MarketFixingSnapshot_({{"EQ[UNUSED]", {{DateTime_(day.AddDays(-1), .1 / 86400.), 99.}}}}));
        auto after = before;
        if (fixing)
            after.fixings_.reset(new MarketFixingSnapshot_({{"EQ[UNUSED]", {{DateTime_(day.AddDays(-1), .2 / 86400.), 99.}}}}));
        else
            after.valuationTime_ = DateTime_(day, .2 / 86400.);
        for (bool retain : {false, true}) {
            const auto oldSource = freeze(before, retain);
            const auto newSource = freeze(after, retain);
            ASSERT_NE(oldSource.State().fingerprint_, newSource.State().fingerprint_);
            ASSERT_NE(oldSource.State().components_.front().fingerprint_, newSource.State().components_.front().fingerprint_);
            ASSERT_EQ(oldSource.State().scheme_, scheme);
            const auto rejected = AggregateRatePortfolioQuoteRisk(trades, after, {oldSource});
            ASSERT_TRUE(rejected.buckets_.empty());
            for (const auto& meta : rejected.meta_) {
                ASSERT_FALSE(meta.eligible_);
                ASSERT_FALSE(meta.structuralZero_);
            }
            ASSERT_EQ(rejected.provenanceFailures_.size(), 1);
            ASSERT_EQ(rejected.provenanceFailures_.front().reason_, "QUOTE_RISK_CALIBRATION_STATE_MISMATCH");
            const auto accepted = AggregateRatePortfolioQuoteRisk(trades, after, {newSource});
            ASSERT_TRUE(accepted.provenanceFailures_.empty());
            ASSERT_FALSE(accepted.buckets_.empty());
            if (!retain) {
                ASSERT_TRUE(newSource.CalibrationRecord().empty());
                ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { NewCalibrationPullback(newSource); }, "QUOTE_RISK_CALIBRATION_RECORD_NOT_RETAINED"));
                continue;
            }
            ASSERT_NE(oldSource.CalibrationRecord(), newSource.CalibrationRecord());
            rapidjson::Document record;
            record.Parse<rapidjson::kParseFullPrecisionFlag>(newSource.CalibrationRecord().c_str());
            ASSERT_FALSE(record.HasParseError());
            ASSERT_TRUE(record["market"]["valuationTime"].IsObject());
            ASSERT_EQ(record["market"]["valuationTime"]["fraction"].GetDouble(), after.valuationTime_.Frac());
            ASSERT_EQ(record["market"]["fixings"][0]["fixingTime"]["fraction"].GetDouble(),
                      after.fixings_->Values().begin()->second.begin()->first.Frac());
            const auto oldBoundary = NewCalibrationPullback(oldSource);
            const auto boundary = NewCalibrationPullback(newSource);
            ASSERT_FALSE(boundary.Matches(oldBoundary));
            const Matrix_<> input(boundary.ParameterRows(), 1, .01);
            const auto good = NewCalibrationParameterAdjoints(boundary, input);
            const auto stale = NewCalibrationParameterAdjoints(oldBoundary, input);
            const auto direct = NewCalibrationDirectQuoteAdjoints(oldBoundary, Matrix_<>(boundary.QuoteRows(), 1, .02));
            const auto reference = PullbackCalibration(boundary, good);
            ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { PullbackCalibration(boundary, stale); }, "CalibrationSnapshotMismatch"));
            ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { PullbackCalibration(boundary, good, direct); }, "CalibrationSnapshotMismatch"));
            ASSERT_NO_FATAL_FAILURE(AssertSameMatrix(PullbackCalibration(boundary, good).TotalAdjoints(), reference.TotalAdjoints()));
            auto copy = after;
            copy.fixings_.reset(new MarketFixingSnapshot_(after.fixings_->Values()));
            ASSERT_TRUE(boundary.Matches(NewCalibrationPullback(freeze(copy, true))));
        }
        auto adjacent = before;
        if (fixing)
            adjacent.fixings_.reset(
                new MarketFixingSnapshot_({{"EQ[UNUSED]", {{DateTime_(day.AddDays(-1), std::nextafter(.1 / 86400., 1.)), 99.}}}}));
        else
            adjacent.valuationTime_ = DateTime_(day, std::nextafter(.1 / 86400., 1.));
        ASSERT_NE(freeze(before, true).State().fingerprint_, freeze(adjacent, true).State().fingerprint_);
        auto zero = before;
        auto negativeZero = before;
        if (fixing) {
            zero.fixings_.reset(new MarketFixingSnapshot_({{"EQ[UNUSED]", {{DateTime_(day.AddDays(-1), 0.), 99.}}}}));
            negativeZero.fixings_.reset(new MarketFixingSnapshot_({{"EQ[UNUSED]", {{DateTime_(day.AddDays(-1), -0.), 99.}}}}));
        } else {
            zero.valuationTime_ = DateTime_(day, 0.);
            negativeZero.valuationTime_ = DateTime_(day, -0.);
        }
        ASSERT_EQ(freeze(zero, true).State().fingerprint_, freeze(negativeZero, true).State().fingerprint_);
        auto changedValue = before;
        changedValue.fixings_.reset(new MarketFixingSnapshot_({{"EQ[UNUSED]", {{DateTime_(day.AddDays(-1), .1 / 86400.), 100.}}}}));
        ASSERT_NE(freeze(before, true).State().fingerprint_, freeze(changedValue, true).State().fingerprint_);
        auto extra = before;
        extra.fixings_.reset(new MarketFixingSnapshot_(
            {{"EQ[UNUSED]", {{DateTime_(day.AddDays(-1), .1 / 86400.), 99.}}}, {"EQ[EXTRA]", {{DateTime_(day.AddDays(-2), .1 / 86400.), 98.}}}}));
        ASSERT_NE(freeze(before, true).State().fingerprint_, freeze(extra, true).State().fingerprint_);
        auto wholeSecond = before;
        wholeSecond.valuationTime_ = DateTime_(day, 1.1 / 86400.);
        ASSERT_NE(freeze(before, true).State().fingerprint_, freeze(wholeSecond, true).State().fingerprint_);
    }

    void AssertSingleSubseconds(bool fixing) {
        using namespace Dal;
        const auto materials = RateRiskPerf::MakeSingleCurveProvenanceMaterials(4, CurveJacobianMode_::Value_::ANALYTIC);
        const auto example = RateRiskPerf::MakeSingleCurveQuoteRiskCase(4, CurveJacobianMode_::Value_::ANALYTIC, 1);
        AssertSubsecondConsumers(
            materials.market_, example.trades_,
            [&](const RatePricingMarket_& market, bool retain) {
                auto config = materials.config_;
                config.retainCalibrationRecord_ = retain;
                return BuildSingleCurveQuoteRiskProvenance(materials.spec_, *materials.calibration_, materials.options_, market, config);
            },
            fixing, "dal.quote-risk-state/3+jcs+sha256");
    }

    void AssertJointSubseconds(bool fixing) {
        using namespace Dal;
        const auto spec = JointQuoteRiskFixtures::Spec(4, 2);
        const auto options = JointXccyQuoteRiskFixtures::Options();
        const auto result = CalibrateJointMultiCurve(spec, options);
        const auto market = JointQuoteRiskFixtures::Market(spec, result);
        AssertSubsecondConsumers(
            market, {JointQuoteRiskFixtures::Irs(spec)},
            [&](const RatePricingMarket_& current, bool retain) {
                auto config = JointQuoteRiskFixtures::Config(2);
                config.retainCalibrationRecord_ = retain;
                return BuildJointMultiCurveQuoteRiskProvenance(spec, result, options, current, config);
            },
            fixing, "dal.quote-risk-state/4+jcs+sha256");
    }

    template <class M_> M_ RecalibrateXccy(M_ materials, double valuationFraction, double fixingFraction) {
        using namespace Dal;
        const auto old = materials.calibration_;
        const auto day = materials.spec_.valuationTime_.Date();
        materials.spec_.valuationTime_ = DateTime_(day, valuationFraction);
        materials.spec_.fixings_.reset(new MarketFixingSnapshot_({{"EQ[UNUSED]", {{DateTime_(day.AddDays(-1), fixingFraction), 99.}}}}));
        std::map<const DiscountCurve_*, Handle_<DiscountCurve_>> replacements;
        if constexpr (std::is_same_v<std::decay_t<decltype(*old)>, JointXccyCalibrationResult_>) {
            materials.calibration_ = std::make_shared<JointXccyCalibrationResult_>(CalibrateJointXccyMarket(materials.spec_, materials.options_));
            const auto& current = *materials.calibration_;
            const auto replaceCurves = [&](const auto& before, const auto& after) {
                for (const auto& [route, curve] : before)
                    replacements[curve.get()] = after.at(route);
            };
            for (const auto& blocks : {std::make_pair(old->domesticCurveBlock_, current.domesticCurveBlock_),
                                       std::make_pair(old->foreignCurveBlock_, current.foreignCurveBlock_)}) {
                replaceCurves(blocks.first->DiscountCurves(), blocks.second->DiscountCurves());
                replaceCurves(blocks.first->ForwardCurves(), blocks.second->ForwardCurves());
            }
            replacements[old->basisCurve_.get()] = current.basisCurve_;
            auto xccy = std::make_shared<CrossCurrencyMarket_>(current.domesticCurveBlock_, current.foreignCurveBlock_, materials.spec_.fxSpot_,
                                                               materials.spec_.valuationTime_, materials.spec_.collateralCurrency_, current.fixings_);
            xccy->SetBasisCurve(current.basisCurve_);
            materials.market_.xccyMarket_ = xccy;
            materials.market_.fixings_ = current.fixings_;
        } else {
            materials.calibration_ =
                std::make_shared<CrossCurrencyCalibrationResult_>(CalibrateCrossCurrencyMarket(materials.spec_, materials.options_));
            replacements[old->basisCurves_.at(materials.spec_.basisPair_).get()] =
                materials.calibration_->basisCurves_.at(materials.spec_.basisPair_);
            materials.market_.xccyMarket_ = std::make_shared<CrossCurrencyMarket_>(materials.calibration_->market_);
            materials.market_.fixings_ = materials.calibration_->market_.Fixings();
        }
        for (auto& [key, curve] : materials.market_.curveComponents_)
            curve = replacements.at(curve.get());
        materials.market_.valuationTime_ = materials.spec_.valuationTime_;
        materials.config_.retainCalibrationRecord_ = true;
        return materials;
    }

    std::shared_ptr<Dal::CrossCurrencyMarket_>
    XccyWithTime(const Dal::RatePricingMarket_& market, const Dal::DateTime_& time, const Dal::Handle_<Dal::MarketFixingSnapshot_>& fixings) {
        const auto& base = *market.xccyMarket_;
        const auto copyBlock = [](const Dal::CurveBlock_& block, const Dal::Ccy_& currency) {
            return Dal::Handle_<Dal::CurveBlock_>(
                new Dal::CurveBlock_(block.Name(), currency.String(), block.DiscountCurves(), block.ForwardCurves(), block.LiborBasis()));
        };
        const auto domestic = copyBlock(base.DomesticBlock(), base.DomesticCcy());
        const auto foreign = copyBlock(base.ForeignBlock(), base.ForeignCcy());
        auto result = std::make_shared<Dal::CrossCurrencyMarket_>(domestic, foreign, base.FxSpot(), time, base.CollateralCurrency(), fixings);
        result->SetBasisCurve(Dal::Handle_<Dal::DiscountCurve_>(std::shared_ptr<const Dal::DiscountCurve_>(market.xccyMarket_, base.BasisCurve())));
        return result;
    }

    void AssertXccyTimeRecord(const Dal::RateQuoteRiskProvenance_& source) {
        rapidjson::Document record;
        record.Parse<rapidjson::kParseFullPrecisionFlag>(source.CalibrationRecord().c_str());
        ASSERT_FALSE(record.HasParseError());
        ASSERT_TRUE(record["market"].HasMember("xccy"));
        for (const auto* time : {&record["market"]["valuationTime"], &record["market"]["xccy"]["valuationTime"],
                                 &record["market"]["fixings"][0]["fixingTime"], &record["market"]["xccy"]["fixings"][0]["fixingTime"],
                                 &record["spec"]["valuationTime"], &record["spec"]["fixings"][0]["fixingTime"]}) {
            ASSERT_TRUE(time->IsObject());
            ASSERT_EQ((*time)["fraction"].GetDouble(), .1 / 86400.);
        }
        if (source.Kind() == "JOINT_XCCY") {
            ASSERT_TRUE(record["result"].HasMember("fixings"));
            ASSERT_EQ(record["result"]["fixings"][0]["fixingTime"]["fraction"].GetDouble(), .1 / 86400.);
        } else {
            ASSERT_EQ(record["result"]["market"]["valuationTime"]["fraction"].GetDouble(), .1 / 86400.);
            ASSERT_EQ(record["result"]["market"]["fixings"][0]["fixingTime"]["fraction"].GetDouble(), .1 / 86400.);
        }
    }

    void AssertXccyLiveTimeChanges(const Dal::RatePricingMarket_& market,
                                   const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                                   const Dal::RateQuoteRiskProvenance_& source) {
        using namespace Dal;
        const auto day = market.valuationTime_.Date();
        const DateTime_ next(day, .2 / 86400.);
        const Handle_<MarketFixingSnapshot_> nextFixings(
            new MarketFixingSnapshot_({{"EQ[UNUSED]", {{DateTime_(day.AddDays(-1), .2 / 86400.), 99.}}}}));
        for (int field = 0; field < 4; ++field) {
            auto live = market;
            if (field == 0)
                live.valuationTime_ = next;
            else if (field == 1)
                live.fixings_ = nextFixings;
            else
                live.xccyMarket_ =
                    XccyWithTime(live, field == 2 ? next : live.xccyMarket_->ValuationTime(), field == 3 ? nextFixings : live.xccyMarket_->Fixings());
            const int sweeps = RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load();
            const auto stale = AggregateRatePortfolioQuoteRisk(trades, live, {source});
            ASSERT_EQ(stale.provenanceFailures_.size(), 1) << field;
            ASSERT_EQ(stale.provenanceFailures_.front().reason_, "QUOTE_RISK_CALIBRATION_STATE_MISMATCH");
            ASSERT_TRUE(stale.buckets_.empty());
            ASSERT_EQ(RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), sweeps);
        }
    }

    template <class M_, class F_>
    void AssertXccyExactConsumers(const M_& prototype, const Dal::RateRiskPerf::QuoteRiskBenchmarkCase_& example, F_ build) {
        using namespace Dal;
        const auto before = RecalibrateXccy(prototype, .1 / 86400., .1 / 86400.);
        const auto source = build(before.spec_, *before.calibration_, before.options_, before.market_, before.config_);
        ASSERT_EQ(source.State().scheme_, "dal.quote-risk-state/3+jcs+sha256");
        ASSERT_NO_FATAL_FAILURE(AssertXccyTimeRecord(source));
        std::cout << "ExactTimeState," << source.Kind() << ",before," << source.CalibrationRecord() << '\n';
        ASSERT_NO_FATAL_FAILURE(AssertLegacyMapping(example.trades_, before.market_, source));
        const auto original = NewCalibrationPullback(source);
        for (bool fixing : {false, true}) {
            const auto after = RecalibrateXccy(prototype, (fixing ? .1 : .2) / 86400., (fixing ? .2 : .1) / 86400.);
            const auto changed = build(after.spec_, *after.calibration_, after.options_, after.market_, after.config_);
            std::cout << "ExactTimeState," << changed.Kind() << (fixing ? ",fixing," : ",valuation,") << changed.CalibrationRecord() << '\n';
            ASSERT_NE(source.State().fingerprint_, changed.State().fingerprint_);
            ASSERT_NE(source.CalibrationRecord(), changed.CalibrationRecord());
            ASSERT_NO_FATAL_FAILURE(AssertCurveMismatch(original, NewCalibrationPullback(changed)));
            ASSERT_NO_FATAL_FAILURE(AssertLegacyMapping(example.trades_, after.market_, changed));
            if (fixing) {
                ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { build(before.spec_, *after.calibration_, after.options_, after.market_, after.config_); },
                                                      "QUOTE_RISK_SPEC_RESULT_MISMATCH"));
                ASSERT_NO_FATAL_FAILURE(
                    AssertFailure([&] { build(before.spec_, *before.calibration_, before.options_, after.market_, before.config_); },
                                  "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH"));
            }
        }
        ASSERT_NO_FATAL_FAILURE(AssertXccyLiveTimeChanges(before.market_, example.trades_, source));
        ASSERT_NO_FATAL_FAILURE(AssertLegacyMapping(example.trades_, before.market_, source));
    }

    double CurveObjective(const Dal::JointMultiCurveCalibrationSpec_& spec,
                          const Dal::JointMultiCurveCalibrationOptions_& options,
                          const Dal::Matrix_<>& seeds,
                          const Dal::Vector_<>& direction,
                          double step) {
        auto shifted = spec;
        int quote = 0;
        for (int block = 0; block < static_cast<int>(shifted.curves_.size()); ++block)
            for (auto& instrument : shifted.curves_[block].instruments_) {
                instrument = JointQuoteRiskFixtures::Instrument(spec.today_, instrument->TimeSpan().second, block,
                                                                instrument->MarketRate() + step * direction[quote++]);
            }
        const auto calibrated = Dal::CalibrateJointMultiCurve(shifted, options);
        const auto market = JointQuoteRiskFixtures::Market(shifted, calibrated);
        double value = 0.0;
        int parameter = 0;
        for (int block = 0; block < static_cast<int>(shifted.curves_.size()); ++block) {
            const auto inspected = Dal::InspectCurveParameters(*market.curveComponents_.at(JointQuoteRiskFixtures::BlockKey(block)), spec.today_);
            for (double coordinate : inspected.passiveParameters_)
                value += coordinate * seeds(parameter++, 0);
        }
        REQUIRE(parameter == seeds.Rows(), "Independent calibration coordinate count differs");
        return value;
    }

    void AssertDirectionalCalibration(const Dal::JointMultiCurveCalibrationSpec_& spec, const Dal::JointMultiCurveCalibrationOptions_& options) {
        const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
        const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
        const auto boundary = Dal::NewCalibrationPullback(Captured(spec, calibrated, options, market, JointQuoteRiskFixtures::Config(2)));
        ASSERT_EQ(boundary.ParameterRows(), boundary.QuoteRows());
        Dal::Matrix_<> seeds(boundary.ParameterRows(), 1);
        Dal::Vector_<> direction(boundary.QuoteRows());
        for (int row = 0; row < seeds.Rows(); ++row) {
            seeds(row, 0) = std::sin(0.3 + 0.7 * row);
            direction[row] = std::cos(0.4 + 0.6 * row);
        }
        const auto result = Dal::PullbackCalibration(boundary, Dal::NewCalibrationParameterAdjoints(boundary, seeds));
        double adjoint = 0.0;
        for (int row = 0; row < result.TotalAdjoints().Rows(); ++row)
            adjoint += result.TotalAdjoints()(row, 0) * direction[row];
        bool previousPass = false;
        bool adjacentPass = false;
        for (double step : {2e-6, 1e-6, 5e-7}) {
            const double oracle =
                (CurveObjective(spec, options, seeds, direction, step) - CurveObjective(spec, options, seeds, direction, -step)) / (2.0 * step);
            const double error = std::abs(adjoint - oracle);
            const bool pass = error <= 1e-6 || error <= 1e-6 * std::max(std::abs(adjoint), std::abs(oracle));
            adjacentPass = adjacentPass || (previousPass && pass);
            previousPass = pass;
            std::cout << "CommonCurveCalibrationOracle," << options.jacobianMode_.String() << ',' << spec.curves_[1].baseLayeredOverDiscount_ << ','
                      << std::setprecision(17) << step << ',' << adjoint << ',' << oracle << ',' << error << ',' << pass << '\n';
        }
        ASSERT_TRUE(adjacentPass);
    }
} // namespace

TEST(CalibrationRiskTest, TestSingleValuationSubsecondsRejectStaleConsumers) { AssertSingleSubseconds(false); }
TEST(CalibrationRiskTest, TestSingleFixingSubsecondsRejectStaleConsumers) { AssertSingleSubseconds(true); }
TEST(CalibrationRiskTest, TestJointValuationSubsecondsRejectStaleConsumers) { AssertJointSubseconds(false); }
TEST(CalibrationRiskTest, TestJointFixingSubsecondsRejectStaleConsumers) { AssertJointSubseconds(true); }

TEST(CalibrationRiskTest, TestJointXccyExactTimesReachFactoryAggregateAndPullback) {
    using namespace Dal;
    const auto mode = CurveJacobianMode_::Value_::ANALYTIC;
    AssertXccyExactConsumers(RateRiskPerf::MakeJointXccyProvenanceMaterials(2, mode), RateRiskPerf::MakeJointXccyQuoteRiskCase(2, mode, 1),
                             [](const auto& spec, const auto& result, const auto& options, const auto& market, const auto& config) {
                                 return BuildJointXccyQuoteRiskProvenance(spec, result, options, market, config);
                             });
}

TEST(CalibrationRiskTest, TestStagedXccyExactTimesReachFactoryAggregateAndPullback) {
    using namespace Dal;
    const auto mode = CurveJacobianMode_::Value_::ANALYTIC;
    AssertXccyExactConsumers(RateRiskPerf::MakeStagedXccyProvenanceMaterials(2, mode), RateRiskPerf::MakeStagedXccyBasisQuoteRiskCase(2, mode, 1),
                             [](const auto& spec, const auto& result, const auto& options, const auto& market, const auto& config) {
                                 return BuildStagedXccyBasisQuoteRiskProvenance(spec, result, options, market, config);
                             });
}

TEST(CalibrationRiskTest, TestLiveSchemeAdmissionChecksEverySourceAndAllowsCorrectRetry) {
    using namespace Dal;
    const auto spec = JointQuoteRiskFixtures::Spec(4, 2);
    const auto options = JointXccyQuoteRiskFixtures::Options();
    const auto result = CalibrateJointMultiCurve(spec, options);
    const auto market = JointQuoteRiskFixtures::Market(spec, result);
    const auto freeze = [&] { return Captured(spec, result, options, market, JointQuoteRiskFixtures::Config(2)); };
    for (const String_& rejected : Vector_<String_>{"dal.quote-risk-state/1+jcs+sha256", "dal.quote-risk-state/2+jcs+sha256",
                                                    "dal.quote-risk-state/3+jcs+sha256", "unknown/99", "DAL.quote-risk-state/4+jcs+sha256"})
        for (int role = 0; role < 3; ++role) {
            SCOPED_TRACE(rejected);
            SCOPED_TRACE(role);
            const Vector_<RateQuoteRiskProvenance_> sources{freeze(), freeze(), freeze()};
            const auto target = NewCalibrationPullback(sources[0]);
            const auto parameterSource = NewCalibrationPullback(sources[1]);
            const auto directSource = NewCalibrationPullback(sources[2]);
            const Matrix_<> matrix(target.ParameterRows(), 1, .01);
            const auto parameters = NewCalibrationParameterAdjoints(parameterSource, matrix);
            const Matrix_<> directMatrix(target.QuoteRows(), 1, .02);
            const auto direct = NewCalibrationDirectQuoteAdjoints(directSource, directMatrix);
            const auto reference = PullbackCalibration(target, parameters, direct);
            const auto plan = PlanCalibrationRiskRequest(target, {});
            // Simulate a historical in-memory handle through the existing native fixture.
            auto& state = const_cast<RateQuoteRiskState_&>(sources[role].State());
            const auto original = state.scheme_;
            state.scheme_ = rejected;
            const auto& changed = role == 0 ? target : role == 1 ? parameterSource : directSource;
            ASSERT_FALSE(changed.Matches(changed));
            ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { NewCalibrationPullback(sources[role]); }, "QUOTE_RISK_STATE_SCHEME_NOT_SUPPORTED"));
            ASSERT_NO_FATAL_FAILURE(
                AssertFailure([&] { NewCalibrationParameterAdjoints(changed, matrix); }, "QUOTE_RISK_STATE_SCHEME_NOT_SUPPORTED"));
            ASSERT_NO_FATAL_FAILURE(
                AssertFailure([&] { NewCalibrationDirectQuoteAdjoints(changed, directMatrix); }, "QUOTE_RISK_STATE_SCHEME_NOT_SUPPORTED"));
            ASSERT_NO_FATAL_FAILURE(
                AssertFailure([&] { static_cast<void>(PlanCalibrationRiskRequest(changed, {})); }, "QUOTE_RISK_STATE_SCHEME_NOT_SUPPORTED"));
            const std::string field = role == 0 ? "calibration.source" : role == 1 ? "parameters.source" : "direct.source";
            ASSERT_NO_FATAL_FAILURE(AssertFailure(
                [&] { PullbackCalibration(target, parameters, direct); },
                "field=" + field + "; expected=dal.quote-risk-state/4+jcs+sha256; actual=" + std::string(rejected.data(), rejected.size())));
            ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { static_cast<void>(PullbackCalibrationWithRisk(plan, parameters, direct)); },
                                                  "QUOTE_RISK_STATE_SCHEME_NOT_SUPPORTED"));
            if (role == 0) {
                const auto rejectedRisk = AggregateRatePortfolioQuoteRisk({JointQuoteRiskFixtures::Irs(spec)}, market, {sources[0]});
                ASSERT_TRUE(rejectedRisk.buckets_.empty());
                ASSERT_EQ(rejectedRisk.provenanceFailures_.size(), 1);
                const auto& failure = rejectedRisk.provenanceFailures_.front();
                ASSERT_EQ(failure.reason_, "QUOTE_RISK_STATE_SCHEME_NOT_SUPPORTED");
                ASSERT_TRUE(failure.componentKey_.empty());
                ASSERT_EQ(failure.expectedStateFingerprint_, sources[0].State().fingerprint_);
                ASSERT_EQ(failure.actualStateFingerprint_, "UNSUPPORTED_SCHEME:" + rejected);
            }
            state.scheme_ = original;
            ASSERT_TRUE(changed.Matches(changed));
            ASSERT_NO_FATAL_FAILURE(AssertSameMatrix(PullbackCalibration(target, parameters, direct).TotalAdjoints(), reference.TotalAdjoints()));
        }
    auto historical = freeze();
    const_cast<RateQuoteRiskState_&>(historical.State()).scheme_ = "dal.quote-risk-state/2+jcs+sha256";
    auto config = JointQuoteRiskFixtures::Config(2);
    config.calibrationId_ = "fresh";
    const auto fresh = Captured(spec, result, options, market, config);
    const auto mixed = AggregateRatePortfolioQuoteRisk({JointQuoteRiskFixtures::Irs(spec)}, market, {historical, fresh});
    ASSERT_EQ(mixed.provenanceFailures_.size(), 1);
    ASSERT_EQ(mixed.buckets_.size(), fresh.Axis().quotes_.size());
    for (const auto& bucket : mixed.buckets_)
        ASSERT_EQ(bucket.calibrationId_, "fresh");
}

TEST(CalibrationRiskTest, TestSingleSchemeAndKindRequireExactBytesBeforeSameSourceShortcut) {
    using namespace Dal;
    const auto materials = RateRiskPerf::MakeSingleCurveProvenanceMaterials(4, CurveJacobianMode_::Value_::ANALYTIC);
    const auto example = RateRiskPerf::MakeSingleCurveQuoteRiskCase(4, CurveJacobianMode_::Value_::ANALYTIC, 1);
    auto config = materials.config_;
    config.retainCalibrationRecord_ = true;
    const auto source = BuildSingleCurveQuoteRiskProvenance(materials.spec_, *materials.calibration_, materials.options_, materials.market_, config);
    const auto boundary = NewCalibrationPullback(source);
    const auto parameters = NewCalibrationParameterAdjoints(boundary, Matrix_<>(boundary.ParameterRows(), 1, .01));
    const auto reference = PullbackCalibration(boundary, parameters);
    for (const bool changeKind : {false, true}) {
        // Exercise the existing native metadata fixture without a persistence API.
        auto& field = changeKind ? const_cast<String_&>(source.Kind()) : const_cast<RateQuoteRiskState_&>(source.State()).scheme_;
        const auto original = field;
        field = changeKind ? "single_curve" : "dal.quote-risk-state/1+jcs+sha256";
        ASSERT_FALSE(boundary.Matches(boundary));
        ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { NewCalibrationPullback(source); }, "QUOTE_RISK_STATE_SCHEME_NOT_SUPPORTED"));
        ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { PullbackCalibration(boundary, parameters); }, "QUOTE_RISK_STATE_SCHEME_NOT_SUPPORTED"));
        const auto rejected = AggregateRatePortfolioQuoteRisk(example.trades_, materials.market_, {source});
        ASSERT_TRUE(rejected.buckets_.empty());
        ASSERT_EQ(rejected.provenanceFailures_.size(), 1);
        ASSERT_EQ(rejected.provenanceFailures_.front().reason_, "QUOTE_RISK_STATE_SCHEME_NOT_SUPPORTED");
        field = original;
        ASSERT_NO_FATAL_FAILURE(AssertSameMatrix(PullbackCalibration(boundary, parameters).TotalAdjoints(), reference.TotalAdjoints()));
    }
}

TEST(CalibrationRiskTest, TestCommonDupireResultMatchesTypedPullbackAndOwnsSeeds) {
    const FlatIVS_ ivs;
    const auto snapshot = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto boundary = Dal::NewCalibrationPullback(snapshot);
    Dal::Matrix_<> parameterMatrix(snapshot.Surface()->vols_.Rows(), snapshot.Surface()->vols_.Cols(), -0.25);
    Dal::Matrix_<> directMatrix(3, 2, 0.125);
    const auto reference = Dal::PullbackDupireCalibration(snapshot, Dal::DupireParameterAdjoints_{snapshot, parameterMatrix},
                                                          Dal::DupireDirectQuoteAdjoints_{snapshot, directMatrix});
    const auto parameters = Dal::NewCalibrationParameterAdjoints(boundary, parameterMatrix);
    const auto direct = Dal::NewCalibrationDirectQuoteAdjoints(boundary, directMatrix);
    parameterMatrix.Fill(100.0);
    directMatrix.Fill(200.0);
    const auto result = Dal::PullbackCalibration(boundary, parameters, direct);
    ASSERT_EQ(boundary.Domain(), "DUPIRE");
    ASSERT_EQ(boundary.ParameterRows(), snapshot.Surface()->vols_.Rows());
    ASSERT_EQ(boundary.ParameterCols(), snapshot.Surface()->vols_.Cols());
    ASSERT_EQ(boundary.QuoteRows(), 3);
    ASSERT_EQ(boundary.QuoteCols(), 2);
    ASSERT_TRUE(std::get<Dal::DupireCalibrationSnapshot_>(boundary.Source()).Matches(snapshot));
    ASSERT_TRUE(result.Calibration().Matches(boundary));
    ASSERT_EQ(result.Method(), reference.Method());
    ASSERT_EQ(result.Unit(), reference.Unit());
    ASSERT_EQ(result.Boundary(), reference.Boundary());
    AssertSameMatrix(result.CalibrationAdjoints(), reference.CalibrationAdjoints());
    AssertSameMatrix(result.DirectAdjoints(), reference.DirectAdjoints());
    AssertSameMatrix(result.TotalAdjoints(), reference.TotalAdjoints());
    static_assert(std::is_const_v<std::remove_reference_t<decltype(result.TotalAdjoints())>>);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(parameters.Adjoints())>>);
}

TEST(CalibrationRiskTest, TestDupireDirectQuotesPermitEqualQuotesUnderDifferentBases) {
    const FlatIVS_ original, changed(0.25);
    const auto snapshot = Dal::CalibrateDupireWithRisk(original, Inputs());
    const auto other = Dal::CalibrateDupireWithRisk(changed, Inputs());
    const auto boundary = Dal::NewCalibrationPullback(snapshot);
    const auto different = Dal::NewCalibrationPullback(other);
    ASSERT_FALSE(boundary.Matches(different));
    const Dal::Matrix_<> nodes(boundary.ParameterRows(), boundary.ParameterCols(), 0.25);
    const Dal::Matrix_<> quotes(3, 2, -0.125);
    const auto reference = Dal::PullbackDupireCalibration(snapshot, {snapshot, nodes}, Dal::DupireDirectQuoteAdjoints_{other, quotes});
    const auto result = Dal::PullbackCalibration(boundary, Dal::NewCalibrationParameterAdjoints(boundary, nodes),
                                                 Dal::NewCalibrationDirectQuoteAdjoints(different, quotes));
    AssertSameMatrix(result.CalibrationAdjoints(), reference.CalibrationAdjoints());
    AssertSameMatrix(result.DirectAdjoints(), reference.DirectAdjoints());
    AssertSameMatrix(result.TotalAdjoints(), reference.TotalAdjoints());
    ASSERT_THROW(Dal::PullbackCalibration(boundary, Dal::NewCalibrationParameterAdjoints(different, nodes)), Dal::Exception_);
}

TEST(CalibrationRiskTest, TestCurveCommonResultMatchesLegacyAcrossModesAndRepresentations) {
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED})
        for (const auto parameterization :
             {Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, Dal::CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD,
              Dal::CurveParameterization_::Value_::ZERO_RATE, Dal::CurveParameterization_::Value_::LOG_DISCOUNT})
            for (bool layered : {false, true}) {
                const auto spec = JointQuoteRiskFixtures::Spec(5, 2, parameterization, layered);
                const auto options = JointXccyQuoteRiskFixtures::Options(mode);
                const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
                const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
                const auto provenance = Captured(spec, calibrated, options, market, JointQuoteRiskFixtures::Config(2));
                ASSERT_NO_FATAL_FAILURE(
                    AssertLegacyMapping({JointQuoteRiskFixtures::Irs(spec), JointQuoteRiskFixtures::Irs(spec, 1, -250000.0)}, market, provenance));
            }
}

TEST(CalibrationRiskTest, TestSingleJointXccyAndStagedSourcesMatchActualLegacyPortfolios) {
    using namespace Dal::RateRiskPerf;
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED})
        for (int quotes : {8, 16}) {
            ASSERT_NO_FATAL_FAILURE(
                AssertNativeProvider(MakeSingleCurveProvenanceMaterials(quotes, mode), MakeSingleCurveQuoteRiskCase(quotes, mode, 3),
                                     [](const auto& spec, const auto& result, const auto& options, const auto& market, const auto& config) {
                                         return Dal::BuildSingleCurveQuoteRiskProvenance(spec, result, options, market, config);
                                     }));
            ASSERT_NO_FATAL_FAILURE(
                AssertNativeProvider(MakeJointXccyProvenanceMaterials(quotes, mode), MakeJointXccyQuoteRiskCase(quotes, mode, 3),
                                     [](const auto& spec, const auto& result, const auto& options, const auto& market, const auto& config) {
                                         return Dal::BuildJointXccyQuoteRiskProvenance(spec, result, options, market, config);
                                     }));
            ASSERT_NO_FATAL_FAILURE(
                AssertNativeProvider(MakeStagedXccyProvenanceMaterials(quotes, mode), MakeStagedXccyBasisQuoteRiskCase(quotes, mode, 3),
                                     [](const auto& spec, const auto& result, const auto& options, const auto& market, const auto& config) {
                                         return Dal::BuildStagedXccyBasisQuoteRiskProvenance(spec, result, options, market, config);
                                     }));
        }
}

TEST(CalibrationRiskTest, TestCurveMixedRepresentationsAndActualPvCurrenciesRemainSeparate) {
    auto spec = JointQuoteRiskFixtures::Spec(8, 3);
    spec.curves_[1].parameterization_ = Dal::CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
    spec.curves_[2].parameterization_ = Dal::CurveParameterization_::Value_::LOG_DISCOUNT;
    const auto options = JointXccyQuoteRiskFixtures::Options();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
    const auto provenance = Captured(spec, calibrated, options, market, JointQuoteRiskFixtures::Config(3));
    ASSERT_NO_FATAL_FAILURE(AssertLegacyMapping({JointQuoteRiskFixtures::Irs(spec, 1), JointQuoteRiskFixtures::Irs(spec, 2)}, market, provenance));
    const auto currencySpec = JointQuoteRiskFixtures::Spec(5, 2, Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true);
    const auto currencyCalibrated = Dal::CalibrateJointMultiCurve(currencySpec, options);
    const auto currencyMarket = JointXccyQuoteRiskFixtures::Market(currencySpec, currencyCalibrated, true);
    const auto currencyProvenance = Captured(currencySpec, currencyCalibrated, options, currencyMarket, JointQuoteRiskFixtures::Config(2));
    ASSERT_NO_FATAL_FAILURE(AssertLegacyMapping({JointQuoteRiskFixtures::Irs(currencySpec, 1)}, currencyMarket, currencyProvenance));
    ASSERT_NO_FATAL_FAILURE(AssertLegacyMapping({JointXccyQuoteRiskFixtures::Trade(currencySpec)}, currencyMarket, currencyProvenance));
    const auto combined = Dal::AggregateRatePortfolioQuoteRisk(
        {JointQuoteRiskFixtures::Irs(currencySpec, 1), JointXccyQuoteRiskFixtures::Trade(currencySpec)}, currencyMarket, {currencyProvenance});
    ASSERT_EQ(combined.pvByActualPvCcy_.size(), 2);
    ASSERT_EQ(combined.buckets_.size(), 2 * currencyProvenance.Axis().quotes_.size());
}

TEST(CalibrationRiskTest, TestCurveSourceIdentityIncludesIdCaseBindingsQuotesAndSettings) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto options = JointXccyQuoteRiskFixtures::Options();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
    auto config = JointQuoteRiskFixtures::Config(2);
    const auto original = Captured(spec, calibrated, options, market, config);
    const auto boundary = Dal::NewCalibrationPullback(original);
    const auto equivalent = Dal::NewCalibrationPullback(Captured(spec, calibrated, options, market, config));
    ASSERT_TRUE(boundary.Matches(equivalent));
    config.calibrationId_ = "GENERIC-JOINT";
    const auto caseId = Captured(spec, calibrated, options, market, config);
    ASSERT_EQ(caseId.State().fingerprint_, original.State().fingerprint_);
    ASSERT_EQ(caseId.CalibrationRecord(), original.CalibrationRecord());
    ASSERT_NO_FATAL_FAILURE(AssertCurveMismatch(boundary, Dal::NewCalibrationPullback(caseId)));
    config = JointQuoteRiskFixtures::Config(2);
    config.componentKeyByParameterBlock_["curve:0"] = "renamed";
    market.curveComponents_["renamed"] = market.curveComponents_.at("curve:0");
    ASSERT_NO_FATAL_FAILURE(AssertCurveMismatch(boundary, Dal::NewCalibrationPullback(Captured(spec, calibrated, options, market, config))));
    auto renamedSpec = spec;
    renamedSpec.curves_[0].curveName_ = "Repeated_Name";
    const auto renamed = Dal::CalibrateJointMultiCurve(renamedSpec, options);
    ASSERT_NO_FATAL_FAILURE(AssertCurveMismatch(
        boundary, Dal::NewCalibrationPullback(Captured(renamedSpec, renamed, options, JointQuoteRiskFixtures::Market(renamedSpec, renamed),
                                                       JointQuoteRiskFixtures::Config(2)))));
    auto changedSpec = spec;
    const auto instrument = spec.curves_[0].instruments_[0];
    changedSpec.curves_[0].instruments_[0] =
        JointQuoteRiskFixtures::Instrument(spec.today_, instrument->TimeSpan().second, 0, instrument->MarketRate() + 1e-5);
    const auto changed = Dal::CalibrateJointMultiCurve(changedSpec, options);
    ASSERT_NO_FATAL_FAILURE(AssertCurveMismatch(
        boundary, Dal::NewCalibrationPullback(Captured(changedSpec, changed, options, JointQuoteRiskFixtures::Market(changedSpec, changed),
                                                       JointQuoteRiskFixtures::Config(2)))));
    auto changedOptions = options;
    changedOptions.jacobianMode_ = Dal::CurveJacobianMode_::Value_::BUMPED;
    const auto bumped = Dal::CalibrateJointMultiCurve(spec, changedOptions);
    ASSERT_NO_FATAL_FAILURE(
        AssertCurveMismatch(boundary, Dal::NewCalibrationPullback(Captured(spec, bumped, changedOptions, JointQuoteRiskFixtures::Market(spec, bumped),
                                                                           JointQuoteRiskFixtures::Config(2)))));
}

TEST(CalibrationRiskTest, TestCurveMappingRequiresAvailableCapturedContent) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    auto options = JointXccyQuoteRiskFixtures::Options();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
    const auto defaultSource = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, JointQuoteRiskFixtures::Config(2));
    ASSERT_TRUE(defaultSource.Available());
    ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { Dal::NewCalibrationPullback(defaultSource); }, "QUOTE_RISK_CALIBRATION_RECORD_NOT_RETAINED"));
    options.computeEffJacobianInverse_ = false;
    const auto unavailable = Dal::CalibrateJointMultiCurve(spec, options);
    const auto source = Captured(spec, unavailable, options, JointQuoteRiskFixtures::Market(spec, unavailable), JointQuoteRiskFixtures::Config(2));
    ASSERT_FALSE(source.Available());
    ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { Dal::NewCalibrationPullback(source); }, std::string(source.Reason().c_str())));
}

TEST(CalibrationRiskTest, TestSeedShapeNonFiniteAndMixedDomainFailuresRecover) {
    const auto curve = SmallCurve();
    const FlatIVS_ ivs;
    const auto dupire = Dal::NewCalibrationPullback(Dal::CalibrateDupireWithRisk(ivs, Inputs()));
    ASSERT_FALSE(curve.Matches(dupire));
    for (const auto& boundary : {curve, dupire}) {
        const auto good = Dal::NewCalibrationParameterAdjoints(boundary, Dal::Matrix_<>(boundary.ParameterRows(), boundary.ParameterCols(), 0.25));
        const auto before = Dal::PullbackCalibration(boundary, good);
        for (const auto& invalid : {Dal::Matrix_<>(), Dal::Matrix_<>(0, std::numeric_limits<int>::max()),
                                    Dal::Matrix_<>(boundary.ParameterRows(), boundary.ParameterCols() + 1)})
            ASSERT_THROW(Dal::NewCalibrationParameterAdjoints(boundary, invalid), Dal::Exception_);
        for (double invalid :
             {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
            Dal::Matrix_<> parameters(boundary.ParameterRows(), boundary.ParameterCols(), 0.0);
            Dal::Matrix_<> direct(boundary.QuoteRows(), boundary.QuoteCols(), 0.0);
            parameters(0, 0) = direct(0, 0) = invalid;
            ASSERT_THROW(Dal::NewCalibrationParameterAdjoints(boundary, parameters), Dal::Exception_);
            ASSERT_THROW(Dal::NewCalibrationDirectQuoteAdjoints(boundary, direct), Dal::Exception_);
        }
        const auto other = boundary.Matches(curve) ? dupire : curve;
        const auto mixed = Dal::NewCalibrationDirectQuoteAdjoints(other, Dal::Matrix_<>(other.QuoteRows(), other.QuoteCols(), 0.0));
        ASSERT_THROW(Dal::PullbackCalibration(boundary, good, mixed), Dal::Exception_);
        ASSERT_NO_FATAL_FAILURE(AssertSameMatrix(Dal::PullbackCalibration(boundary, good).TotalAdjoints(), before.TotalAdjoints()));
    }
}

TEST(CalibrationRiskTest, TestCurveDirectContributionsZeroSeedsAndOverflowRecover) {
    const auto boundary = SmallCurve();
    const auto zero = Dal::NewCalibrationParameterAdjoints(boundary, Dal::Matrix_<>(boundary.ParameterRows(), 1, 0.0));
    Dal::Matrix_<> direct(boundary.QuoteRows(), 1, -0.125);
    const auto directOnly = Dal::PullbackCalibration(boundary, zero, Dal::NewCalibrationDirectQuoteAdjoints(boundary, direct));
    ASSERT_NO_FATAL_FAILURE(AssertSameMatrix(directOnly.TotalAdjoints(), direct));
    for (double value : directOnly.CalibrationAdjoints())
        ASSERT_EQ(value, 0.0);
    const auto seeds = Dal::NewCalibrationParameterAdjoints(boundary, Dal::Matrix_<>(boundary.ParameterRows(), 1, -0.25));
    const auto calibration = Dal::PullbackCalibration(boundary, seeds);
    const auto combined = Dal::PullbackCalibration(boundary, seeds, Dal::NewCalibrationDirectQuoteAdjoints(boundary, direct));
    for (int row = 0; row < direct.Rows(); ++row) {
        ASSERT_EQ(combined.CalibrationAdjoints()(row, 0), calibration.TotalAdjoints()(row, 0));
        ASSERT_EQ(combined.DirectAdjoints()(row, 0), direct(row, 0));
        ASSERT_EQ(combined.TotalAdjoints()(row, 0), calibration.TotalAdjoints()(row, 0) + direct(row, 0));
    }
    const auto extreme = Dal::NewCalibrationParameterAdjoints(boundary, Dal::Matrix_<>(boundary.ParameterRows(), 1, 1e300));
    const auto finite = Dal::PullbackCalibration(boundary, extreme);
    for (int row = 0; row < direct.Rows(); ++row)
        direct(row, 0) = std::copysign(std::numeric_limits<double>::max(), finite.TotalAdjoints()(row, 0));
    ASSERT_THROW(Dal::PullbackCalibration(boundary, extreme, Dal::NewCalibrationDirectQuoteAdjoints(boundary, direct)), Dal::Exception_);
    ASSERT_NO_FATAL_FAILURE(AssertSameMatrix(Dal::PullbackCalibration(boundary, seeds).TotalAdjoints(), calibration.TotalAdjoints()));
}

TEST(CalibrationRiskTest, TestPassiveCurveMappingPreservesUnrelatedActiveRecording) {
    const auto boundary = SmallCurve();
    const auto seeds = Dal::NewCalibrationParameterAdjoints(boundary, Dal::Matrix_<>(boundary.ParameterRows(), 1, -0.25));
    const auto reference = Dal::PullbackCalibration(boundary, seeds);
    Dal::AAD::RecordingScope_ recording;
    Dal::AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    Dal::AAD::Number_ output = input * input;
    const auto nodes = Dal::AAD::Tape()->nodes_.Size();
    const auto result = Dal::PullbackCalibration(boundary, seeds);
    ASSERT_NO_FATAL_FAILURE(AssertSameMatrix(result.TotalAdjoints(), reference.TotalAdjoints()));
    ASSERT_TRUE(result.Calibration().Matches(boundary));
    ASSERT_EQ(result.Unit(), "DECIMAL_QUOTE");
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), nodes);
    recording.FinishRecording();
    Dal::AAD::NativeOperations_::AddSeed(output, 1.0);
    recording.Reverse();
    ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
    recording.Close();
}

TEST(CalibrationRiskTest, TestDupireNestedRejectionPreservesRecordingAndRecovers) {
    const FlatIVS_ ivs;
    const auto boundary = Dal::NewCalibrationPullback(Dal::CalibrateDupireWithRisk(ivs, Inputs()));
    const auto seeds = Dal::NewCalibrationParameterAdjoints(boundary, Dal::Matrix_<>(boundary.ParameterRows(), boundary.ParameterCols(), 0.25));
    const auto reference = Dal::PullbackCalibration(boundary, seeds);
    {
        Dal::AAD::RecordingScope_ recording;
        Dal::AAD::Number_ input;
        recording.RegisterInput(input, 2.0);
        recording.StartRecording();
        Dal::AAD::Number_ output = input * input;
        recording.FinishRecording();
        const auto nodes = Dal::AAD::Tape()->nodes_.Size();
        ASSERT_THROW(Dal::PullbackCalibration(boundary, seeds), Dal::Exception_);
        ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), nodes);
        Dal::AAD::NativeOperations_::AddSeed(output, 1.0);
        recording.Reverse();
        ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
        recording.Close();
    }
    ASSERT_NO_FATAL_FAILURE(AssertSameMatrix(Dal::PullbackCalibration(boundary, seeds).TotalAdjoints(), reference.TotalAdjoints()));
}

TEST(CalibrationRiskTest, TestSmoothSquareCurveDirectionsMatchIndependentRecalibration) {
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED})
        for (bool layered : {false, true})
            ASSERT_NO_FATAL_FAILURE(
                AssertDirectionalCalibration(JointQuoteRiskFixtures::Spec(5, 2, Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, layered),
                                             JointXccyQuoteRiskFixtures::Options(mode)));
}
