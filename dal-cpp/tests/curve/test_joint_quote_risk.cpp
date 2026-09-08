//
// Created by Codex on 2026/9/8.
//

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>

#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/platform/platform.hpp>

#include "jointquoteriskfixtures.hpp"

namespace {
    Dal::JointMultiCurveCalibrationOptions_ InverseOptions(Dal::CurveJacobianMode_ mode = Dal::CurveJacobianMode_::Value_::ANALYTIC) {
        Dal::JointMultiCurveCalibrationOptions_ result;
        result.computeEffJacobianInverse_ = true;
        result.jacobianMode_ = mode;
        return result;
    }

    struct Observation_ {
        double pv_ = 0.0;
        double gross_ = 0.0;
    };

    Observation_ PriceTrades(const Dal::Vector_<Dal::RateTradeDefinition_>& trades, const Dal::RatePricingMarket_& market) {
        Observation_ result;
        for (const auto& trade : trades) {
            const auto price = Dal::PriceRateTrade(trade, market);
            REQUIRE(price.succeeded_ && std::isfinite(price.pv_), "Joint quote oracle lost an eligible trade: " + trade.instrumentId_ + price.error_);
            result.pv_ += price.pv_;
            result.gross_ += std::abs(price.pv_);
        }
        REQUIRE(std::isfinite(result.pv_) && std::isfinite(result.gross_), "Joint quote oracle portfolio is not finite");
        return result;
    }

    Observation_ RecalibrateQuote(const Dal::JointMultiCurveCalibrationSpec_& spec,
                                  const Dal::JointMultiCurveCalibrationOptions_& options,
                                  const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                                  int block,
                                  int originalOrdinal,
                                  double bump) {
        auto shifted = spec;
        const auto& instrument = spec.curves_[block].instruments_[originalOrdinal];
        const int instrumentBlock = spec.curves_[block].calibrateDiscountCurve_                    ? 0
                                    : spec.curves_[block].targetTenor_ == Dal::PeriodLength_("6M") ? 2
                                                                                                   : 1;
        shifted.curves_[block].instruments_[originalOrdinal] =
            JointQuoteRiskFixtures::Instrument(spec.today_, instrument->TimeSpan().second, instrumentBlock, instrument->MarketRate() + bump);
        const auto calibration = Dal::CalibrateJointMultiCurve(shifted, options);
        REQUIRE(calibration.converged_, "Joint quote oracle calibration did not converge");
        return PriceTrades(trades, JointQuoteRiskFixtures::Market(shifted, calibration));
    }

    double RelativeError(double error, double first, double second) {
        const double denominator = std::max(std::abs(first), std::abs(second));
        return denominator == 0.0 ? 0.0 : error / denominator;
    }

    Dal::RatePricingMarket_ XccyPricingMarket(const Dal::JointMultiCurveCalibrationSpec_& spec,
                                              const Dal::JointMultiCurveCalibrationResult_& calibrated) {
        using namespace Dal;
        auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
        const auto flat = [&](const String_& name, const String_& currency, double rate) {
            return Handle_<DiscountCurve_>(NewDiscountPWC(
                name, currency, PiecewiseConstant_(spec.curves_.front().knotDates_, Vector_<>(spec.curves_.front().knotDates_.size(), rate))));
        };
        const auto euroDiscount = flat("eur-ois", "EUR", 0.017);
        const auto euroForward = flat("eur-3m", "EUR", 0.019);
        const auto basis = flat("eur-usd-basis", "EUR", 0.001);
        market.curveComponents_["eur-discount"] = euroDiscount;
        market.curveComponents_["eur-forward"] = euroForward;
        market.curveComponents_["basis"] = basis;
        const Handle_<CurveBlock_> domestic(
            new CurveBlock_("EUR", "EUR", {{CollateralType_("OIS"), euroDiscount}}, {{PeriodLength_("3M"), euroForward}}, DayBasis::Act365F()));
        const Handle_<CurveBlock_> foreign(new CurveBlock_("USD", "USD", calibrated.discountCurves_, calibrated.forwardCurves_, spec.liborBasis_));
        auto xccy = std::make_shared<CrossCurrencyMarket_>(domestic, foreign, 0.9, market.valuationTime_, Ccy_("EUR"), market.fixings_);
        xccy->SetBasisCurve(basis);
        market.xccyMarket_ = xccy;
        return market;
    }

    Dal::RateTradeDefinition_ EuroXccyTrade(const Dal::JointMultiCurveCalibrationSpec_& spec) {
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
        terms.config_.convention_.foreignLeg_ = JointQuoteRiskFixtures::Leg(3);
        terms.config_.convention_.domesticIndex_ = JointQuoteRiskFixtures::Index(1);
        terms.config_.convention_.foreignIndex_ = JointQuoteRiskFixtures::Index(1);
        terms.config_.domesticRateFixing_ = {"EUR-3M", 11, 0};
        terms.config_.foreignRateFixing_ = {"USD-3M", 11, 0};
        return {"euro-xccy", RateInstrumentType_::Value_::XCCY, spec.today_, spec.today_, Date::AddMonths(spec.today_, 24), Ccy_("EUR"), terms};
    }

    void AssertOracleBucket(const Dal::RateQuoteRiskBucket_& bucket,
                            const std::array<Observation_, 5>& prices,
                            const Dal::JointMultiCurveCalibrationSpec_& spec,
                            const Dal::JointMultiCurveCalibrationOptions_& options,
                            int width,
                            bool writeEvidence = true) {
        constexpr double h = 1.0e-6, b = 1.0e-4;
        const double target = width <= 5 ? 5.0e-6 : width <= 10 ? 1.0e-4 : 1.0e-3;
        double scale = 1.0;
        for (const auto& price : prices)
            scale = std::max(scale, price.gross_);
        const double derivative = (prices[1].pv_ - prices[2].pv_) / (2.0 * h);
        const double dv01 = (prices[3].pv_ - prices[4].pv_) / 2.0;
        const double derivativeError = std::abs(bucket.dPvDDecimalQuote_ - derivative);
        const double derivativeRelative = RelativeError(derivativeError, bucket.dPvDDecimalQuote_, derivative);
        const double dv01Error = std::abs(bucket.dv01_ - dv01);
        const double dv01Relative = RelativeError(dv01Error, bucket.dv01_, dv01);
        const double unitError = std::abs(bucket.dv01_ - b * bucket.dPvDDecimalQuote_);
        const double unitThreshold =
            64.0 * std::numeric_limits<double>::epsilon() * std::max({scale * b, std::abs(bucket.dv01_), b * std::abs(bucket.dPvDDecimalQuote_)});
        const auto parameterization = spec.curves_.front().parameterization_;
        const bool layered = spec.curves_[1].baseLayeredOverDiscount_;
        const Dal::String_ fixture = "b" + Dal::String::FromInt(static_cast<int>(spec.curves_.size())) + "-n" + Dal::String::FromInt(width) + "-" +
                                     options.jacobianMode_.String() + "-" + parameterization.String() + (layered ? "-layered" : "-unlayered");
        SCOPED_TRACE(std::string(fixture.data(), fixture.size()));
        SCOPED_TRACE(std::string(bucket.quoteKey_.data(), bucket.quoteKey_.size()));
        const char* path = writeEvidence ? std::getenv("DAL_JOINT_QUOTE_RISK_EVIDENCE_FILE") : nullptr;
        if (path) {
            std::ofstream output(path, std::ios::app);
            REQUIRE(output.is_open(), "Cannot open joint quote oracle evidence file");
            if (output.tellp() == 0)
                output << "fixture_id,blocks,width,mode,parameterization,layered,currency,quote_key,axis_fingerprint,price_scale,api_derivative,"
                          "oracle_derivative,derivative_abs_error,derivative_rel_error,derivative_abs_threshold,derivative_rel_threshold,api_dv01,"
                          "oracle_dv01,dv01_abs_error,dv01_rel_error,dv01_abs_threshold,dv01_rel_threshold,unit_error,unit_threshold,"
                          "pv_base,pv_plus_h,pv_minus_h,pv_plus_b,pv_minus_b,gross_base,gross_plus_h,gross_minus_h,gross_plus_b,gross_minus_b\n";
            output << std::setprecision(17) << fixture << ',' << spec.curves_.size() << ',' << width << ',' << options.jacobianMode_.String() << ','
                   << parameterization.String() << ',' << layered << ',' << bucket.actualPvCcy_.String() << ',' << bucket.quoteKey_ << ','
                   << bucket.axisFingerprint_ << ',' << scale << ',' << bucket.dPvDDecimalQuote_ << ',' << derivative << ',' << derivativeError << ','
                   << derivativeRelative << ',' << target * scale << ',' << target << ',' << bucket.dv01_ << ',' << dv01 << ',' << dv01Error << ','
                   << dv01Relative << ',' << target * scale * b << ',' << target << ',' << unitError << ',' << unitThreshold;
            for (const auto& price : prices)
                output << ',' << price.pv_;
            for (const auto& price : prices)
                output << ',' << price.gross_;
            output << '\n';
            REQUIRE(output.good(), "Cannot write joint quote oracle evidence file");
        }
        ASSERT_TRUE(std::isfinite(bucket.dPvDDecimalQuote_) && std::isfinite(bucket.dv01_));
        ASSERT_LE(unitError, unitThreshold);
        ASSERT_TRUE(derivativeError <= target * scale || derivativeRelative <= target)
            << "API=" << bucket.dPvDDecimalQuote_ << " oracle=" << derivative << " abs=" << derivativeError << " rel=" << derivativeRelative;
        ASSERT_TRUE(dv01Error <= target * scale * b || dv01Relative <= target)
            << "API=" << bucket.dv01_ << " oracle=" << dv01 << " abs=" << dv01Error << " rel=" << dv01Relative;
    }

    void AssertSpecOracle(const Dal::JointMultiCurveCalibrationSpec_& spec,
                          const Dal::JointMultiCurveCalibrationOptions_& options,
                          bool writeEvidence = true) {
        using namespace JointQuoteRiskFixtures;
        const int blocks = static_cast<int>(spec.curves_.size());
        const auto calibration = Dal::CalibrateJointMultiCurve(spec, options);
        const int width = calibration.effJacobianInverse_.Cols();
        ASSERT_EQ(calibration.jacobianModeUsed_, options.jacobianMode_.String());
        const auto market = Market(spec, calibration);
        const auto trades = Trades(spec);
        const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibration, options, market, Config(blocks));
        ASSERT_TRUE(provenance.Available()) << provenance.Reason();
        const auto risk = Dal::AggregateRatePortfolioQuoteRisk(trades, market, {provenance});
        ASSERT_EQ(risk.buckets_.size(), width);
        ASSERT_EQ(risk.meta_.size(), trades.size());
        for (const auto& meta : risk.meta_)
            ASSERT_TRUE(meta.eligible_) << meta.originalNodeRiskReason_;
        const auto base = PriceTrades(trades, market);
        for (int block = 0; block < blocks; ++block) {
            const auto& range = calibration.residualRanges_[block];
            for (int ordinal = 0; ordinal < range.size_; ++ordinal) {
                const int global = range.offset_ + ordinal;
                const int original = calibration.residualInstrumentOrdinals_[global];
                const std::array<Observation_, 5> observations{base, RecalibrateQuote(spec, options, trades, block, original, 1.0e-6),
                                                               RecalibrateQuote(spec, options, trades, block, original, -1.0e-6),
                                                               RecalibrateQuote(spec, options, trades, block, original, 1.0e-4),
                                                               RecalibrateQuote(spec, options, trades, block, original, -1.0e-4)};
                AssertOracleBucket(risk.buckets_[global], observations, spec, options, width, writeEvidence);
            }
        }
    }
} // namespace

TEST(JointQuoteRiskTest, TestV2ProvenanceKeepsDeclarationCoordinatesAndOwnsItsInverse) {
    using namespace JointQuoteRiskFixtures;
    for (bool layered : {false, true})
        for (auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED}) {
            const auto spec = Spec(5, 3, Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, layered);
            const auto options = InverseOptions(mode);
            auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
            const auto market = Market(spec, calibrated);
            const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, Config(3));
            ASSERT_TRUE(provenance.Available()) << provenance.Reason();
            ASSERT_EQ(provenance.Kind(), "JOINT_MULTI_CURVE");
            ASSERT_EQ(provenance.Axis().scheme_, "dal.quote-risk-axis/2+jcs+sha256");
            ASSERT_EQ(provenance.State().scheme_, "dal.quote-risk-state/2+jcs+sha256");
            ASSERT_EQ(provenance.Axis().quotes_.size(), 5);
            ASSERT_EQ(provenance.Axis().parameters_.size(), 5);
            ASSERT_EQ(provenance.Axis().parameterRanges_.size(), 3);
            for (int block = 0; block < 3; ++block)
                ASSERT_EQ(provenance.Axis().parameterRanges_[block].blockKey_, BlockKey(block));
            const double first = provenance.EffectiveInverse()(0, 0);
            calibrated.effJacobianInverse_(0, 0) = 123.0;
            ASSERT_DOUBLE_EQ(provenance.EffectiveInverse()(0, 0), first);
            ASSERT_EQ(Dal::RateQuoteRiskAxisFingerprintScheme(), "dal.quote-risk-axis/1+jcs+sha256");
        }
}

TEST(JointQuoteRiskTest, TestUnlayeredTradeProducesEveryResidualQuoteWithoutRecalibration) {
    using namespace JointQuoteRiskFixtures;
    const auto spec = Spec();
    const auto options = InverseOptions();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    const auto market = Market(spec, calibrated);
    const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, Config(2));
    const int count = Dal::CurveCalibrationInvocationCount();
    const auto result = Dal::AggregateRatePortfolioQuoteRisk({Irs(spec)}, market, {provenance});
    ASSERT_EQ(Dal::CurveCalibrationInvocationCount(), count);
    ASSERT_TRUE(result.provenanceFailures_.empty());
    ASSERT_EQ(result.meta_.size(), 1);
    ASSERT_TRUE(result.meta_[0].eligible_) << result.meta_[0].originalNodeRiskReason_;
    ASSERT_EQ(result.buckets_.size(), 5);
    for (const auto& bucket : result.buckets_) {
        ASSERT_EQ(bucket.actualPvCcy_, Dal::Ccy_("USD"));
        ASSERT_DOUBLE_EQ(bucket.dv01_, bucket.dPvDDecimalQuote_ * 1.0e-4);
    }
}

TEST(JointQuoteRiskTest, TestLayeredBaseRiskMatchesFullJointRecalibration) {
    using namespace JointQuoteRiskFixtures;
    const auto spec = Spec(5, 2, Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true);
    const auto options = InverseOptions();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    const auto market = Market(spec, calibrated);
    const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, Config(2));
    const auto trade = Irs(spec);
    const auto base = Dal::PriceRateTrade(trade, market);
    ASSERT_TRUE(base.succeeded_) << base.error_;
    const auto risk = Dal::AggregateRatePortfolioQuoteRisk({trade}, market, {provenance});
    ASSERT_EQ(risk.buckets_.size(), 5);
    const auto independentNodes = Dal::RateTradeNodeSensitivitiesBatch({trade}, market, {BlockKey(0), BlockKey(1)});
    Dal::Vector_<> fixedBaseGradient;
    for (const auto& cell : independentNodes) {
        ASSERT_TRUE(cell.result_.eligible_);
        for (double parameter : cell.result_.gradient_)
            fixedBaseGradient.push_back(parameter);
    }
    Dal::Vector_<> independentResponse;
    Dal::Matrix::Multiply(fixedBaseGradient, calibrated.effJacobianInverse_, &independentResponse);
    ASSERT_GT(std::abs(independentResponse.front() / spec.tolerance_ - risk.buckets_.front().dPvDDecimalQuote_), 1.0)
        << "Independent fixed-base sweeps must not reproduce a coupled joint gradient";
    for (int block = 0; block < 2; ++block)
        for (int ordinal = 0; ordinal < static_cast<int>(spec.curves_[block].instruments_.size()); ++ordinal) {
            auto shifted = [&](double bump) {
                auto input = spec;
                const auto& original = spec.curves_[block].instruments_[ordinal];
                input.curves_[block].instruments_[ordinal] =
                    Instrument(spec.today_, original->TimeSpan().second, block, original->MarketRate() + bump);
                const auto result = Dal::CalibrateJointMultiCurve(input, options);
                return Dal::PriceRateTrade(trade, Market(input, result));
            };
            const auto plus = shifted(1.0e-6);
            const auto minus = shifted(-1.0e-6);
            ASSERT_TRUE(plus.succeeded_);
            ASSERT_TRUE(minus.succeeded_);
            const double finiteDifference = (plus.pv_ - minus.pv_) / 2.0e-6;
            const int global = calibrated.residualRanges_[block].offset_ + ordinal;
            const double actual = risk.buckets_[global].dPvDDecimalQuote_;
            const double scale = std::max({1.0, std::abs(base.pv_), std::abs(plus.pv_), std::abs(minus.pv_)});
            ASSERT_TRUE(std::abs(actual - finiteDifference) <= 5.0e-6 * scale ||
                        std::abs(actual - finiteDifference) <= 5.0e-6 * std::max(std::abs(actual), std::abs(finiteDifference)))
                << "block=" << block << " quote=" << ordinal << " API=" << actual << " oracle=" << finiteDifference;
        }
}

TEST(JointQuoteRiskTest, TestFullRecalibrationOracleAtRequiredWidthsModesLayoutsAndLayers) {
    for (int width : {5, 10, 16})
        for (int blocks : {2, 3})
            for (auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED})
                for (auto parameterization :
                     {Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, Dal::CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD,
                      Dal::CurveParameterization_::Value_::LOG_DISCOUNT, Dal::CurveParameterization_::Value_::ZERO_RATE})
                    for (bool layered : {false, true})
                        AssertSpecOracle(JointQuoteRiskFixtures::Spec(width, blocks, parameterization, layered), InverseOptions(mode));
}

TEST(JointQuoteRiskTest, TestMixedParameterizationsAndReversedQuoteDeclarations) {
    using namespace JointQuoteRiskFixtures;
    for (auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED}) {
        auto spec = Spec(10, 3, Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true);
        spec.curves_[1].parameterization_ = Dal::CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
        spec.curves_[2].parameterization_ = Dal::CurveParameterization_::Value_::LOG_DISCOUNT;
        for (auto& declaration : spec.curves_)
            std::reverse(declaration.instruments_.begin(), declaration.instruments_.end());
        AssertSpecOracle(spec, InverseOptions(mode), false);
    }
}

TEST(JointQuoteRiskTest, TestUnderdeterminedInverseDifferentiatesEveryNativeParameterWithFixedGuesses) {
    using namespace JointQuoteRiskFixtures;
    for (bool layered : {false, true})
        for (auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED}) {
            const auto spec = Spec(5, 3, Dal::CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD, layered);
            const auto options = InverseOptions(mode);
            const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
            ASSERT_EQ(calibrated.effJacobianInverseMapping_, "initial_jacobian_chart");
            const auto parameters = [&](const Dal::JointMultiCurveCalibrationResult_& result) {
                Dal::Vector_<> values;
                const auto market = Market(spec, result);
                for (int block = 0; block < 3; ++block) {
                    const auto& curve = dynamic_cast<const Dal::Tape::DiscountPWLF_<double>&>(*market.curveComponents_.at(BlockKey(block)));
                    const auto left = curve.FLeft(), right = curve.FRight();
                    for (int index = 0; index < static_cast<int>(left.size()); ++index) {
                        values.push_back(left[index]);
                        values.push_back(right[index]);
                    }
                }
                return values;
            };
            for (int block = 0; block < 3; ++block)
                for (int quote = 0; quote < calibrated.residualRanges_[block].size_; ++quote) {
                    const auto shifted = [&](double bump) {
                        auto input = spec;
                        const auto& original = spec.curves_[block].instruments_[quote];
                        input.curves_[block].instruments_[quote] =
                            Instrument(spec.today_, original->TimeSpan().second, block, original->MarketRate() + bump);
                        return parameters(Dal::CalibrateJointMultiCurve(input, options));
                    };
                    const auto plus = shifted(1.0e-6), minus = shifted(-1.0e-6);
                    const int column = calibrated.residualRanges_[block].offset_ + quote;
                    ASSERT_EQ(plus.size(), calibrated.effJacobianInverse_.Rows());
                    for (int row = 0; row < static_cast<int>(plus.size()); ++row) {
                        const double oracle = (plus[row] - minus[row]) / 2.0e-6;
                        const double inverse = calibrated.effJacobianInverse_(row, column) / spec.tolerance_;
                        ASSERT_TRUE(std::isfinite(oracle) && std::isfinite(inverse));
                        ASSERT_LE(std::abs(oracle - inverse), 1.0e-5 * std::max({1.0, std::abs(oracle), std::abs(inverse)}))
                            << "row=" << row << " quote=" << column << " mode=" << options.jacobianMode_.String();
                    }
                }
        }
}

TEST(JointQuoteRiskTest, TestMalformedProvenanceInputsFailClosed) {
    using namespace JointQuoteRiskFixtures;
    const auto spec = Spec();
    const auto options = InverseOptions();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    const auto market = Market(spec, calibrated);
    const std::vector<std::function<void(Dal::JointMultiCurveCalibrationResult_*)>> corruptions{
        [](auto* value) { value->parameterRanges_[1].offset_ = 0; },
        [](auto* value) { value->residualRanges_.pop_back(); },
        [](auto* value) { value->residualInstrumentOrdinals_[1] = value->residualInstrumentOrdinals_[0]; },
        [](auto* value) { value->effJacobianInverseScaling_ = "unscaled"; },
        [](auto* value) { value->effJacobianInverseMapping_ = "unknown"; },
        [](auto* value) { value->effJacobianInverseAvailability_ = "not_requested"; },
        [](auto* value) { value->effJacobianInverse_(0, 0) = std::numeric_limits<double>::infinity(); },
        [](auto* value) { value->effJacobianInverse_.Resize(1, 1); },
        [](auto* value) { value->jacobianAtSolution_.Resize(1, 1); },
        [](auto* value) { value->diagnostics_[0].marketRates_[0] += 0.01; },
        [](auto* value) { value->converged_ = false; },
    };
    for (const auto& corrupt : corruptions) {
        auto value = calibrated;
        corrupt(&value);
        ASSERT_THROW(Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, value, options, market, Config(2)), Dal::Exception_);
    }
    auto config = Config(2);
    config.calibrationId_.clear();
    ASSERT_THROW(Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, config), Dal::Exception_);
    config = Config(2);
    config.componentKeyByParameterBlock_[BlockKey(1)] = BlockKey(0);
    ASSERT_THROW(Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, config), Dal::Exception_);
    auto invalidSpec = spec;
    invalidSpec.initialGuess_ = std::numeric_limits<double>::quiet_NaN();
    ASSERT_THROW(Dal::BuildJointMultiCurveQuoteRiskProvenance(invalidSpec, calibrated, options, market, Config(2)), Dal::Exception_);
}

TEST(JointQuoteRiskTest, TestStaleValuationAndCloneBaseRoutingRejectBeforeSweeping) {
    using namespace JointQuoteRiskFixtures;
    const auto spec = Spec(5, 2, Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true);
    const auto options = InverseOptions();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    const auto market = Market(spec, calibrated);
    const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, Config(2));
    const auto& discount = dynamic_cast<const Dal::Tape::DiscountPWC_<double>&>(*market.curveComponents_.at(BlockKey(0)));
    auto cloned = market;
    cloned.curveComponents_[BlockKey(0)] = Dal::Handle_<Dal::DiscountCurve_>(
        Dal::NewDiscountPWC(discount.Name(), "USD", Dal::PiecewiseConstant_(discount.KnotDates(), discount.FRight())));
    ASSERT_THROW(Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, cloned, Config(2)), Dal::Exception_);
    auto stale = market;
    stale.valuationTime_ = Dal::DateTime_(spec.today_, 1, 0);
    for (const auto& changed : {cloned, stale}) {
        const int sweeps = Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load();
        const int preparations = Dal::RateCashflowPricingInternal::g_nodeSensitivityPreparationCount.load();
        const auto result = Dal::AggregateRatePortfolioQuoteRisk({Irs(spec)}, changed, {provenance});
        ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), sweeps);
        ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivityPreparationCount.load(), preparations);
        ASSERT_EQ(result.provenanceFailures_.size(), 1);
        ASSERT_EQ(result.provenanceFailures_[0].reason_, "QUOTE_RISK_CALIBRATION_STATE_MISMATCH");
        ASSERT_TRUE(result.buckets_.empty());
    }
}

TEST(JointQuoteRiskTest, TestConsumedBlockFailureDiscardsSiblingGradientAndPreservesSiblingTrade) {
    using namespace JointQuoteRiskFixtures;
    const auto spec = Spec(5, 2, Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true);
    const auto options = InverseOptions();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    const auto market = Market(spec, calibrated);
    const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, Config(2));
    auto deposit = Irs(spec);
    deposit.instrumentId_ = "surviving-deposit";
    deposit.instrumentType_ = Dal::RateInstrumentType_::Value_::DEPOSIT;
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = 1000000.0;
    terms.contractRate_ = 0.03;
    terms.index_ = Index(0);
    terms.discountComponentKey_ = BlockKey(0);
    deposit.terms_ = terms;
    const auto expected = Dal::AggregateRatePortfolioQuoteRisk({deposit}, market, {provenance});
    struct FailureScope_ {
        const Dal::String_ key_ = BlockKey(1);
        FailureScope_() { Dal::RateCashflowPricingInternal::g_quoteRiskForcedSweepFailureComponent.store(&key_); }
        ~FailureScope_() { Dal::RateCashflowPricingInternal::g_quoteRiskForcedSweepFailureComponent.store(nullptr); }
    } failure;
    const auto actual = Dal::AggregateRatePortfolioQuoteRisk({Irs(spec), deposit}, market, {provenance});
    ASSERT_EQ(actual.meta_.size(), 2);
    ASSERT_FALSE(actual.meta_[0].eligible_);
    ASSERT_EQ(actual.meta_[0].failingComponentKey_, BlockKey(1));
    ASSERT_TRUE(actual.meta_[1].eligible_);
    ASSERT_EQ(actual.buckets_.size(), expected.buckets_.size());
    for (int quote = 0; quote < static_cast<int>(actual.buckets_.size()); ++quote)
        ASSERT_DOUBLE_EQ(actual.buckets_[quote].dPvDDecimalQuote_, expected.buckets_[quote].dPvDDecimalQuote_);
}

TEST(JointQuoteRiskTest, TestUnusedBlocksAndEmptyInputsDoNotSweep) {
    using namespace JointQuoteRiskFixtures;
    const auto spec = Spec(5, 3);
    const auto options = InverseOptions();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    const auto market = Market(spec, calibrated);
    const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, Config(3));
    const int before = Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load();
    const auto result = Dal::AggregateRatePortfolioQuoteRisk({Irs(spec)}, market, {provenance});
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load() - before, 2);
    ASSERT_EQ(result.buckets_.size(), 5);
    // A dense solver inverse may carry roundoff into a quote bucket; the unconsumed
    // native block itself is exactly zero and the counter proves it was not swept.
    ASSERT_NEAR(result.buckets_.back().dPvDDecimalQuote_, 0.0, 1.0e-12);
    const int after = Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load();
    ASSERT_TRUE(Dal::AggregateRatePortfolioQuoteRisk({}, market, {provenance}).buckets_.empty());
    ASSERT_TRUE(Dal::AggregateRatePortfolioQuoteRisk({Irs(spec)}, market, {}).buckets_.empty());
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), after);
}

TEST(JointQuoteRiskTest, TestCyclicNativeBaseRejectsBeforeSerializationAndSweeps) {
    using namespace JointQuoteRiskFixtures;
    struct CyclicPwc_ : Dal::Tape::DiscountPWC_<double> {
        explicit CyclicPwc_(const Dal::Tape::DiscountPWC_<double>& source)
            : Dal::Tape::DiscountPWC_<double>(source.Name(), "USD", source.KnotDates(), source.FRight()) {}
        void SetBase(const Dal::Handle_<Dal::DiscountCurve_>& base) { base_ = base; }
    };
    const auto spec = Spec();
    const auto options = InverseOptions();
    auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    auto market = Market(spec, calibrated);
    const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, Config(2));
    auto cycle = std::make_shared<CyclicPwc_>(dynamic_cast<const Dal::Tape::DiscountPWC_<double>&>(*market.curveComponents_.at(BlockKey(0))));
    const Dal::Handle_<Dal::DiscountCurve_> handle(std::static_pointer_cast<const Dal::DiscountCurve_>(cycle));
    struct ClearCycle_ {
        CyclicPwc_& curve_;
        ~ClearCycle_() { curve_.SetBase({}); }
    } cleanup{*cycle};
    cycle->SetBase(handle);
    calibrated.discountCurves_.begin()->second = handle;
    market.curveComponents_[BlockKey(0)] = handle;
    ASSERT_THROW(Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, Config(2)), Dal::Exception_);
    const int before = Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load();
    const auto result = Dal::AggregateRatePortfolioQuoteRisk({Irs(spec)}, market, {provenance});
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), before);
    ASSERT_TRUE(result.buckets_.empty());
    ASSERT_EQ(result.provenanceFailures_.size(), 1);
}

TEST(JointQuoteRiskTest, TestJointXccyPvBaseCouplingAndActualCurrencies) {
    using namespace JointQuoteRiskFixtures;
    for (bool layered : {false, true}) {
        const auto spec = Spec(5, 2, Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, layered);
        const auto options = InverseOptions();
        const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
        const auto market = XccyPricingMarket(spec, calibrated);
        const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, Config(2));
        const auto euro = EuroXccyTrade(spec);
        const auto risk = Dal::AggregateRatePortfolioQuoteRisk({euro, Irs(spec)}, market, {provenance});
        ASSERT_EQ(risk.meta_.size(), 2);
        for (const auto& meta : risk.meta_)
            ASSERT_TRUE(meta.eligible_) << meta.originalNodeRiskReason_;
        ASSERT_EQ(risk.buckets_.size(), 10);
        ASSERT_EQ(risk.pvByActualPvCcy_.size(), 2);
        const auto base = PriceTrades({euro}, market);
        for (int block = 0; block < 2; ++block)
            for (int quote = 0; quote < calibrated.residualRanges_[block].size_; ++quote) {
                const auto shifted = [&](double bump) {
                    auto input = spec;
                    const auto& original = spec.curves_[block].instruments_[quote];
                    input.curves_[block].instruments_[quote] =
                        Instrument(spec.today_, original->TimeSpan().second, block, original->MarketRate() + bump);
                    return PriceTrades({euro}, XccyPricingMarket(input, Dal::CalibrateJointMultiCurve(input, options)));
                };
                const int ordinal = calibrated.residualRanges_[block].offset_ + quote;
                ASSERT_EQ(risk.buckets_[ordinal].actualPvCcy_, Dal::Ccy_("EUR"));
                const std::array<Observation_, 5> prices{base, shifted(1.0e-6), shifted(-1.0e-6), shifted(1.0e-4), shifted(-1.0e-4)};
                AssertOracleBucket(risk.buckets_[ordinal], prices, spec, options, 5, false);
            }
    }
}

TEST(JointQuoteRiskTest, TestClonedXccySlotsRejectBeforeAnySweep) {
    using namespace JointQuoteRiskFixtures;
    const auto spec = Spec();
    const auto options = InverseOptions();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    const auto market = XccyPricingMarket(spec, calibrated);
    const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, Config(2));
    auto cloneResult = calibrated;
    const auto& original = dynamic_cast<const Dal::Tape::DiscountPWC_<double>&>(*calibrated.forwardCurves_.begin()->second);
    cloneResult.forwardCurves_.begin()->second = Dal::Handle_<Dal::DiscountCurve_>(
        Dal::NewDiscountPWC(original.Name(), "USD", Dal::PiecewiseConstant_(original.KnotDates(), original.FRight())));
    auto wrong = XccyPricingMarket(spec, cloneResult);
    wrong.curveComponents_[BlockKey(1)] = market.curveComponents_.at(BlockKey(1));
    ASSERT_THROW(Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, wrong, Config(2)), Dal::Exception_);
    const int before = Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load();
    const auto risk = Dal::AggregateRatePortfolioQuoteRisk({EuroXccyTrade(spec)}, wrong, {provenance});
    ASSERT_TRUE(risk.buckets_.empty());
    ASSERT_EQ(risk.provenanceFailures_.size(), 1);
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), before);
}

TEST(JointQuoteRiskTest, TestReorderedBlocksKeepNativeAndQuoteRangesInDeclarationOrder) {
    using namespace JointQuoteRiskFixtures;
    const auto originalSpec = Spec(10, 3);
    const auto options = InverseOptions();
    const auto originalResult = Dal::CalibrateJointMultiCurve(originalSpec, options);
    const auto originalMarket = Market(originalSpec, originalResult);
    const auto originalProvenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(originalSpec, originalResult, options, originalMarket, Config(3));
    const auto trade = Irs(originalSpec);
    const auto expected = Dal::AggregateRatePortfolioQuoteRisk({trade}, originalMarket, {originalProvenance});
    auto spec = originalSpec;
    const std::array<int, 3> originalBlocks{2, 0, 1};
    for (int block = 0; block < 3; ++block)
        spec.curves_[block] = originalSpec.curves_[originalBlocks[block]];
    const auto result = Dal::CalibrateJointMultiCurve(spec, options);
    const auto market = Market(spec, result);
    const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, result, options, market, Config(3));
    auto routed = trade;
    auto& terms = std::get<Dal::IrsTradeTerms_>(routed.terms_).value_;
    terms.discountComponentKey_ = BlockKey(1);
    terms.forecastComponentKey_ = BlockKey(2);
    const auto actual = Dal::AggregateRatePortfolioQuoteRisk({routed}, market, {provenance});
    ASSERT_EQ(actual.buckets_.size(), 10);
    ASSERT_TRUE(actual.meta_[0].eligible_);
    for (int block = 0; block < 3; ++block) {
        const auto& range = result.residualRanges_[block];
        ASSERT_EQ(provenance.Axis().parameterRanges_[block].blockKey_, BlockKey(block));
        ASSERT_EQ(range.curveIndex_, block);
        for (int quote = 0; quote < range.size_; ++quote) {
            const int reference = originalResult.residualRanges_[originalBlocks[block]].offset_ + quote;
            ASSERT_NEAR(actual.buckets_[range.offset_ + quote].dPvDDecimalQuote_, expected.buckets_[reference].dPvDDecimalQuote_, 1.0e-5);
        }
    }
}

TEST(JointQuoteRiskTest, TestOffsettingTradesAndIndependentSiblingProvenances) {
    using namespace JointQuoteRiskFixtures;
    const auto spec = Spec(5, 2, Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true);
    const auto options = InverseOptions();
    const auto first = Dal::CalibrateJointMultiCurve(spec, options);
    const auto second = Dal::CalibrateJointMultiCurve(spec, options);
    auto market = Market(spec, first);
    market.curveComponents_["other-discount"] = second.discountCurves_.begin()->second;
    market.curveComponents_["other-forward"] = second.forwardCurves_.begin()->second;
    auto config = Config(2);
    const auto firstProvenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, first, options, market, config);
    config.calibrationId_ = "sibling";
    config.componentKeyByParameterBlock_ = {{BlockKey(0), "other-discount"}, {BlockKey(1), "other-forward"}};
    const auto secondProvenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, second, options, market, config);
    const auto positive = Irs(spec);
    auto negative = Irs(spec, 1, -1000000.0);
    negative.instrumentId_ = "offsetting";
    const auto offsetting = Dal::AggregateRatePortfolioQuoteRisk({positive, negative}, market, {firstProvenance});
    ASSERT_TRUE(offsetting.meta_[0].eligible_ && offsetting.meta_[1].eligible_);
    ASSERT_LT(offsetting.meta_[1].pv_, 0.0);
    ASSERT_DOUBLE_EQ(offsetting.pvByActualPvCcy_.at("USD"), 0.0);
    for (const auto& bucket : offsetting.buckets_)
        ASSERT_DOUBLE_EQ(bucket.dv01_, 0.0);
    auto sibling = positive;
    sibling.instrumentId_ = "sibling-trade";
    auto& terms = std::get<Dal::IrsTradeTerms_>(sibling.terms_).value_;
    terms.discountComponentKey_ = "other-discount";
    terms.forecastComponentKey_ = "other-forward";
    const auto expected = Dal::AggregateRatePortfolioQuoteRisk({sibling}, market, {secondProvenance});
    const auto& discount = dynamic_cast<const Dal::Tape::DiscountPWC_<double>&>(*market.curveComponents_.at(BlockKey(0)));
    market.curveComponents_[BlockKey(0)] = Dal::Handle_<Dal::DiscountCurve_>(
        Dal::NewDiscountPWC(discount.Name(), "USD", Dal::PiecewiseConstant_(discount.KnotDates(), discount.FRight())));
    const int preparations = Dal::RateCashflowPricingInternal::g_quoteRiskProvenancePreparationCount.load();
    const auto actual = Dal::AggregateRatePortfolioQuoteRisk({positive, sibling}, market, {firstProvenance, secondProvenance});
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_quoteRiskProvenancePreparationCount.load() - preparations, 2);
    ASSERT_EQ(actual.provenanceFailures_.size(), 1);
    ASSERT_EQ(actual.provenanceFailures_[0].calibrationId_, firstProvenance.CalibrationId());
    ASSERT_EQ(actual.meta_.size(), 2);
    ASSERT_TRUE(actual.meta_[0].structuralZero_);
    ASSERT_TRUE(actual.meta_[1].eligible_);
    ASSERT_EQ(actual.buckets_.size(), expected.buckets_.size());
    for (int quote = 0; quote < static_cast<int>(actual.buckets_.size()); ++quote)
        ASSERT_DOUBLE_EQ(actual.buckets_[quote].dPvDDecimalQuote_, expected.buckets_[quote].dPvDDecimalQuote_);
}

TEST(JointQuoteRiskTest, TestUnavailableModesFixingStateAndAnalyticFallback) {
    using namespace JointQuoteRiskFixtures;
    auto spec = Spec();
    for (bool request : {false, true}) {
        auto options = InverseOptions();
        options.computeEffJacobianInverse_ = request;
        spec.solveMode_ = Dal::CurveSolveMode_::Value_::APPROXIMATE;
        const auto result = Dal::CalibrateJointMultiCurve(spec, options);
        const auto market = Market(spec, result);
        const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, result, options, market, Config(2));
        ASSERT_FALSE(provenance.Available());
        ASSERT_EQ(provenance.Reason(), request ? "QUOTE_RISK_NOT_AVAILABLE_FOR_SOLVE_MODE" : "QUOTE_RISK_INVERSE_NOT_REQUESTED");
    }
    spec = Spec();
    spec.liborBasis_ = Dal::DayBasis_("ACT_360");
    const auto options = InverseOptions();
    const auto result = Dal::CalibrateJointMultiCurve(spec, options);
    ASSERT_EQ(result.jacobianModeUsed_, "BUMPED");
    auto market = Market(spec, result);
    const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, result, options, market, Config(2));
    ASSERT_TRUE(provenance.Available());
    Dal::MarketFixingSnapshot_::values_t values;
    values["unconsumed-historical-fixing"][Dal::DateTime_(spec.today_.AddDays(-1), 11, 0)] = 0.01;
    market.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(values));
    const int before = Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load();
    const auto risk = Dal::AggregateRatePortfolioQuoteRisk({Irs(spec)}, market, {provenance});
    ASSERT_EQ(risk.provenanceFailures_.size(), 1);
    ASSERT_TRUE(risk.buckets_.empty());
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), before);
}

TEST(JointQuoteRiskTest, TestSharedPassiveProvenanceUsesIndependentCallerTapes) {
    using namespace JointQuoteRiskFixtures;
    const auto spec = Spec(5, 2, Dal::CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD, true);
    const auto options = InverseOptions();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    const auto market = Market(spec, calibrated);
    const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, Config(2));
    const auto trades = Trades(spec);
    const auto expected = Dal::AggregateRatePortfolioQuoteRisk(trades, market, {provenance});
    const auto run = [&]() {
        Dal::RatePortfolioQuoteRisk_ risk;
        for (int iteration = 0; iteration < 4; ++iteration)
            risk = Dal::AggregateRatePortfolioQuoteRisk(trades, market, {provenance});
        return risk;
    };
    auto first = std::async(std::launch::async, run);
    auto second = std::async(std::launch::async, run);
    for (const auto& actual : {first.get(), second.get()}) {
        ASSERT_TRUE(actual.provenanceFailures_.empty());
        ASSERT_EQ(actual.buckets_.size(), expected.buckets_.size());
        for (int quote = 0; quote < static_cast<int>(actual.buckets_.size()); ++quote)
            ASSERT_DOUBLE_EQ(actual.buckets_[quote].dPvDDecimalQuote_, expected.buckets_[quote].dPvDDecimalQuote_);
    }
}
